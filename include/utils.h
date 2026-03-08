#ifndef UTILS_H
#define UTILS_H

#include <systemc.h>
typedef uint16_t fp16_t; 

using sc_uint16 = sc_dt::sc_uint<16>;

fp16_t fp16_add(fp16_t a, fp16_t b);
fp16_t fp16_mul(fp16_t a, fp16_t b);
sc_uint16 fp16_max(sc_uint16 a_bits, sc_uint16 b_bits);

#endif // UTILS_H