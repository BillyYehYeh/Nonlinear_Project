#include "LayerNorm.h"


static float fp16_to_float(uint16_t h) {
  uint32_t sign = (uint32_t)(h >> 15) & 0x1u;
  uint32_t exp16 = (uint32_t)(h >> 10) & 0x1Fu;
  uint32_t mant16 = (uint32_t)h & 0x3FFu;

  uint32_t exp32;
  uint32_t mant32;

  if (exp16 == 0) {
    if (mant16 == 0) {
      exp32 = 0;
      mant32 = 0;
    } else {
      int shift = 0;
      while ((mant16 & 0x400u) == 0) {
        mant16 <<= 1;
        shift++;
      }
      mant16 &= 0x3FFu;
      exp32 = (uint32_t)(127 - 15 - shift);
      mant32 = mant16 << 13;
    }
  } else if (exp16 == 0x1Fu) {
    exp32 = 0xFFu;
    mant32 = mant16 ? 0x7FFFFFu : 0;
  } else {
    exp32 = exp16 - 15 + 127;
    mant32 = mant16 << 13;
  }

  union {
    uint32_t u;
    float f;
  } out;
  out.u = (sign << 31) | (exp32 << 23) | mant32;
  return out.f;
}

static int read_fp16_binary(const char *path, float **out_data, size_t *out_len) {
  FILE *fp = fopen(path, "rb");
  if (fp == NULL) {
    perror("fopen");
    return -1;
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    perror("fseek");
    fclose(fp);
    return -1;
  }
  long file_size = ftell(fp);
  if (file_size < 0) {
    perror("ftell");
    fclose(fp);
    return -1;
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    perror("fseek");
    fclose(fp);
    return -1;
  }

  if ((file_size % (long)sizeof(uint16_t)) != 0) {
    fprintf(stderr, "Invalid fp16 binary size: %ld bytes\n", file_size);
    fclose(fp);
    return -1;
  }

  size_t elem_count = (size_t)file_size / sizeof(uint16_t);
  uint16_t *raw = (uint16_t *)malloc(elem_count * sizeof(uint16_t));
  float *data = (float *)malloc(elem_count * sizeof(float));
  if (raw == NULL || data == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    free(raw);
    free(data);
    fclose(fp);
    return -1;
  }

  size_t n_read = fread(raw, sizeof(uint16_t), elem_count, fp);
  fclose(fp);
  if (n_read != elem_count) {
    fprintf(stderr, "Failed to read binary file: %s\n", path);
    free(raw);
    free(data);
    return -1;
  }

  for (size_t i = 0; i < elem_count; i++) {
    data[i] = fp16_to_float(raw[i]);
  }

  free(raw);
  *out_data = data;
  *out_len = elem_count;
  return 0;
}

static float cosine_similarity(const float *a, const float *b, size_t len) {
  double dot = 0.0;
  double norm_a = 0.0;
  double norm_b = 0.0;

  for (size_t i = 0; i < len; i++) {
    double av = (double)a[i];
    double bv = (double)b[i];
    dot += av * bv;
    norm_a += av * av;
    norm_b += bv * bv;
  }

  if (norm_a == 0.0 || norm_b == 0.0) {
    return 0.0f;
  }

  return (float)(dot / (sqrt(norm_a) * sqrt(norm_b)));
}

static size_t count_nonfinite(const float *data, size_t len) {
  size_t cnt = 0;
  for (size_t i = 0; i < len; i++) {
    if (!isfinite(data[i])) {
      cnt++;
    }
  }
  return cnt;
}

static void print_vector_float(const char *name, const float *data, size_t len) {
  printf("%s (float):\n", name);
  for (size_t i = 0; i < len; i++) {
    printf("  [%zu] %.8f\n", i, data[i]);
  }
}

static void print_pairwise_diff(const float *approx, const float *golden, size_t len) {
  printf("Approx vs Golden (float):\n");
  for (size_t i = 0; i < len; i++) {
    printf("  [%zu] approx=%.8f golden=%.8f diff=%.8f\n", i, approx[i], golden[i], approx[i] - golden[i]);
  }
}

static void print_usage(const char *prog) {
  printf("Usage: %s -i <input_fp16_bin> -o <golden_fp16_bin>\n", prog);
}


int main(int argc, char **argv) {
  const char *input_path = NULL;
  const char *golden_path = NULL;

  for (int i = 1; i < argc; i++) {
    if ((strcmp(argv[i], "-i") == 0) && (i + 1 < argc)) {
      input_path = argv[++i];
    } else if ((strcmp(argv[i], "-o") == 0) && (i + 1 < argc)) {
      golden_path = argv[++i];
    } else {
      print_usage(argv[0]);
      return 1;
    }
  }

  if (input_path == NULL || golden_path == NULL) {
    print_usage(argv[0]);
    return 1;
  }

  float *x = NULL;
  float *golden = NULL;
  size_t x_len = 0;
  size_t golden_len = 0;

  if (read_fp16_binary(input_path, &x, &x_len) != 0) {
    fprintf(stderr, "Failed to parse input binary: %s\n", input_path);
    return 1;
  }

  if (read_fp16_binary(golden_path, &golden, &golden_len) != 0) {
    fprintf(stderr, "Failed to parse golden binary: %s\n", golden_path);
    free(x);
    return 1;
  }

  print_vector_float("Input Data", x, x_len);
  print_vector_float("Golden Output", golden, golden_len);

  if (x_len != golden_len) {
    fprintf(stderr, "Length mismatch: input=%zu, golden=%zu\n", x_len, golden_len);
    free(x);
    free(golden);
    return 1;
  }

  const int dim = 10;
  if ((x_len % (size_t)dim) != 0) {
    fprintf(stderr, "Input length must be divisible by dim=%d, got %zu\n", dim, x_len);
    free(x);
    free(golden);
    return 1;
  }

  int n = (int)(x_len / (size_t)dim);
  int data_len = (int)x_len;
  float *out_approx = (float *)calloc(x_len, sizeof(float));
  DATA_TYPE *weight = (DATA_TYPE *)malloc((size_t)dim * sizeof(DATA_TYPE));
  DATA_TYPE *bias = (DATA_TYPE *)malloc((size_t)dim * sizeof(DATA_TYPE));
  if (out_approx == NULL || weight == NULL || bias == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    free(x);
    free(golden);
    free(out_approx);
    free(weight);
    free(bias);
    return 1;
  }

  for (int i = 0; i < dim; i++) {
    weight[i] = 1.0f;
    bias[i] = 0.0f;
  }

  layernorm(out_approx, x, weight, bias, n, dim);

  print_vector_float("Approximate LayerNorm Output", out_approx, x_len);
  print_vector_float("PyTorch Golden Output", golden, x_len);
  print_pairwise_diff(out_approx, golden, x_len);

  size_t bad_approx = count_nonfinite(out_approx, x_len);
  size_t bad_golden = count_nonfinite(golden, x_len);

  if (bad_approx > 0 || bad_golden > 0) {
    fprintf(stderr, "Non-finite values detected: approx=%zu, golden=%zu\n", bad_approx, bad_golden);
    free(x);
    free(golden);
    free(out_approx);
    free(weight);
    free(bias);
    return 1;
  }

  float cos = cosine_similarity(out_approx, golden, x_len);

  printf("Input:  %s\n", input_path);
  printf("Golden: %s\n", golden_path);
  printf("Data length: %d\n", data_len);
  printf("LayerNorm shape: n=%d, dim=%d\n", n, dim);
  printf("Cosine similarity (Approx LayerNorm vs PyTorch golden): %.8f\n", cos);

  free(x);
  free(golden);
  free(out_approx);
  free(weight);
  free(bias);

  return 0;
}
