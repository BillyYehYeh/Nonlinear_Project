#ifndef CSIM_H
#define CSIM_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>

#define EXP_QBIS 4
#define QBITS ((uint64_t)1 << EXP_QBIS)   // 16
#define Q_ONE ((uint64_t)1 << QBITS)      // 2^16

#define DATA_TYPE float

typedef uint32_t qtype;

int approximate_log2(float x) {
  int n = (int)(-x);

  if (n > QBITS) {
    return QBITS;
  }

  return n;
}

float approximate_divide(int shift, qtype sum_i) {
  // leading one of sum_i(16.16 fixed-point)
  int leading_one_pos = 0;
  for (int bit = 31; bit >= 0; bit--) {     
    if ( (sum_i >> bit) & 0x1 ) {
      leading_one_pos = bit;
      break;
    }
  }

  uint32_t sum_i_approx = sum_i >> (leading_one_pos - 1);
  float Mux_Result = (sum_i_approx & 0x1)? 0.568f : 0.818f;
  float Divisor_Result = (float)((uint64_t)1 << ((leading_one_pos - QBITS) + shift));
  return Mux_Result / Divisor_Result;
}

// SOLE softmax
void SOLE_softmax(float* out, float* x, int size)  {
  assert(out != NULL);
  assert(x != NULL);

  float *y = (float*)malloc(sizeof(float) * size);
  int *m = (int*)malloc(sizeof(int) * size);
  assert(y != NULL);
  assert(m != NULL);

  int i;
  // find max value (for numerical stability)
  DATA_TYPE max_val = x[0];
  for (i = 1; i < size; i++) {
    if (x[i] > max_val) {
      max_val = x[i];
    }
  }

  float max_val_q = (int)(max_val * Q_ONE) / (float)(Q_ONE);;
  qtype sum_i = 0;
  const float log2exp_approxiamte = 1.4375f; // log2(e) approximate to 23/16
  for (i = 0; i < size; i++) {
    float x_q = (int)(x[i] * Q_ONE) / (float)(Q_ONE);
    y[i] = (x_q - max_val_q) * log2exp_approxiamte;   //左移次數(-)(fp)
    
    m[i] = approximate_log2(y[i]);                    //右移次數(+)(uint4)

    //printf("x_q: %f, max_val_q: %f, y[i]: %f, m[i]: %d\n", x_q, max_val_q, y[i], m[i]);
    sum_i += (uint64_t)(Q_ONE >> m[i]);
  }

  // normalize
  for (int i = 0; i < size; i++) {
    out[i] = approximate_divide(m[i], sum_i);
  }

  free(y);
  free(m);
}

// SOFTWARE standard softmax
void SOFTWARE_softmax(float* out, float* x, int size) {
  assert(out != NULL);
  assert(x != NULL);

  float max_val = x[0];
  for (int i = 1; i < size; i++) {
    if (x[i] > max_val) {
      max_val = x[i];
    }
  }

  float sum = 0.0f;
  for (int i = 0; i < size; i++) {
    out[i] = expf(x[i] - max_val);
    sum += out[i];
  }

  for (int i = 0; i < size; i++) {
    out[i] /= sum;
  }
}

#endif // CSIM_H
