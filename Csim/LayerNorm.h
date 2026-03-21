#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#define LUT_BITS 5
#define LUT_SIZE (1<<(LUT_BITS))
#define LUT_QBITS  14            // Q1.14 format
#define LAYERNORM_TILED_N 10     // Number of elements per tile

typedef float DATA_TYPE;

uint16_t recip_sqrt_LUT[LUT_SIZE];
int recip_sqrt_LUT_initialized = 0;

float fp16_clip(float x_val) {
    union {
        float f;
        uint32_t u;
    } in, out;

    in.f = x_val;

    uint32_t sign = (in.u >> 31) & 0x1;
    int32_t exp  = (in.u >> 23) & 0xFF;
    uint32_t mant = in.u & 0x7FFFFF;

    uint16_t h; // half precision bits

    // Handle NaN / Inf
    if (exp == 0xFF) {
        if (mant) { // NaN
            h = (sign << 15) | (0x1F << 10) | (mant ? 0x200 : 0);
        } else { // Inf
            h = (sign << 15) | (0x1F << 10);
        }
    } else if (exp > 0x70 + 0x1E) {
        // Overflow: too large for FP16 (max exp=0x1F)
        h = (sign << 15) | (0x1F << 10);
    } else if (exp < 0x70 - 10) {
        // Underflow: too small for FP16
        h = (sign << 15); // ±0
    } else {
        // Normalized range
        int32_t new_exp = exp - 0x70 + 0xF;  // bias adjust (127 -> 15)
        uint32_t new_mant = mant >> 13;      // 23 → 10 bits
        // round to nearest, ties to even
        uint32_t round_bit = (mant >> 12) & 1;
        uint32_t sticky_bits = mant & 0xFFF;
        if (round_bit && (sticky_bits || (new_mant & 1)))
            new_mant++;

        // handle mantissa overflow
        if (new_mant == 0x400) {
            new_mant = 0;
            new_exp++;
        }

        if (new_exp >= 0x1F) { // overflow again
            h = (sign << 15) | (0x1F << 10);
        } else if (new_exp <= 0) { // subnormal
            if (new_exp < -10)
                h = (sign << 15); // too small
            else {
                new_mant = (new_mant | 0x400) >> (1 - new_exp);
                h = (sign << 15) | (new_mant & 0x3FF);
            }
        } else {
            h = (sign << 15) | ((new_exp & 0x1F) << 10) | (new_mant & 0x3FF);
        }
    }

    // Convert back to float
    // Expand half back to single precision (approximate, keeping FP16 precision)
    uint32_t sign32 = (h >> 15) & 0x1;
    uint32_t exp16  = (h >> 10) & 0x1F;
    uint32_t mant16 = h & 0x3FF;

    uint32_t exp32, mant32;
    if (exp16 == 0) {
        if (mant16 == 0) {
            exp32 = 0;
            mant32 = 0;
        } else {
            // subnormal
            int shift = 0;
            while ((mant16 & 0x400) == 0) {
                mant16 <<= 1;
                shift++;
            }
            mant16 &= 0x3FF;
            exp32 = 127 - 15 - shift;
            mant32 = mant16 << 13;
        }
    } else if (exp16 == 0x1F) {
        exp32 = 0xFF;
        mant32 = mant16 ? 0x7FFFFF : 0;
    } else {
        exp32 = exp16 - 15 + 127;
        mant32 = mant16 << 13;
    }

    out.u = (sign32 << 31) | (exp32 << 23) | mant32;
    return out.f;
}

void init_recip_sqrt_LUT() {
    for (int i = 0; i < LUT_SIZE; i++) {
        double m = 1.0 + (i + 0.5) * (1.0 / LUT_SIZE); // m ∈ [1,2)
        double invsqrt = 1.0 / sqrt(m);
        int q = (int)round(invsqrt * (1 << LUT_QBITS));
        if (q >= (1 << 15)) q = (1 << 15) - 1;
        recip_sqrt_LUT[i] = (uint16_t)q;
    }
}

float approx_inv_sqrt(float x) {
  if (x <= 0.0f) return 0.0f;

  // Extract exponent & mantissa (IEEE 754 float)
  uint32_t bits;
  memcpy(&bits, &x, sizeof(bits));
  int exp = ((bits >> 23) & 0xFF) - 127;
  int mant = (bits & 0x7FFFFF) >> (23 - LUT_BITS);

  // Normalize mantissa [1,2)
  float inv_m = recip_sqrt_LUT[mant] / (float)(1 << LUT_QBITS);

  // Exponent correction: 2^(-exp/2)
  float exp_corr = ldexpf(1.0f, -(exp >> 1));
  if (exp & 1) exp_corr *= 0.70710678f; // If exp is odd, add √0.5 correction

  float result = inv_m * exp_corr;
  //check if is nan or inf
  if (!isfinite(result)) {
    printf("Warning: approx_inv_sqrt(%f) produced non-finite result. Returning 0.\n", x);
    result = 0.0f;
    
  }
  else {
    printf("approx_inv_sqrt(%f) = %f\n", x, result);
  }

  return result;
}


void layernorm(DATA_TYPE* out, DATA_TYPE* x, DATA_TYPE* w, DATA_TYPE* b, int n,
  int dim) {

  if (!recip_sqrt_LUT_initialized) {
    init_recip_sqrt_LUT();
    recip_sqrt_LUT_initialized = 1;
  }

  assert(out != NULL);
  assert(x != NULL);
  assert(w != NULL);
  assert(b != NULL);
  const DATA_TYPE eps = 1e-5;




  // Tiling parameters (can be configured)
  int tile_n = LAYERNORM_TILED_N;   // Number of elements per tile

  // Perform tiled layer normalization (on dimension dim)
  for (int n_tile = 0; n_tile < n; n_tile += tile_n) {
    int current_tile_n = (n_tile + tile_n > n) ? (n - n_tile) : tile_n;

    // Perform layer normalization for the current tile
    for (int i = 0; i < current_tile_n; i++) {
      DATA_TYPE mean = 0.0f;
      DATA_TYPE variance = 0.0f;

      DATA_TYPE Ex = 0;
      DATA_TYPE Ex2 = 0;

      // Stage 1: Static Calculation
      for (int d = 0; d < dim; d++) {
        DATA_TYPE x_val = x[d * n + i + n_tile];
        x_val = fp16_clip(x_val);
        Ex = fp16_clip(Ex + x_val);
        Ex2 = fp16_clip(Ex2 + fp16_clip(x_val * x_val));
      }

      // Ex = fp16_clip(Ex);
      // Ex2 = fp16_clip(Ex2);

      // process
      mean = Ex / dim;
      mean = fp16_clip(mean);

      variance = Ex2 / dim - mean * mean;
      variance = fp16_clip(variance);

      // standard = sqrtf(variance + eps);
      // DATA_TYPE standard_inv = 1.0f / standard;
      DATA_TYPE standard_inv = fp16_clip(approx_inv_sqrt(variance + eps));

      // Stage 2: Affine Transformation
      for (int d = 0; d < dim; d++) {
        out[d * n + i + n_tile] =  (x[d * n + i + n_tile] - mean) * standard_inv * w[d] + b[d];
      }
    }
  }
}



void SOFTWARE_layernorm(float* out, float* x, float* w, float* b, int n, int dim) {
  assert(out != NULL);
  assert(x != NULL);
  assert(w != NULL);
  assert(b != NULL);
  const float eps = 1e-5f;

  for (int i = 0; i < n; i++) {
    float mean = 0.0f;
    float variance = 0.0f;

    // Calculate mean
    for (int d = 0; d < dim; d++) {
      mean += x[d * n + i];
    }
    mean /= dim;

    // Calculate variance
    for (int d = 0; d < dim; d++) {
      float diff = x[d * n + i] - mean;
      variance += diff * diff;
    }
    variance /= dim;

    // Normalize and apply affine transformation
    float standard_inv = 1.0f / sqrtf(variance + eps);
    for (int d = 0; d < dim; d++) {
      out[d * n + i] = (x[d * n + i] - mean) * standard_inv * w[d] + b[d];
    }
  }
}