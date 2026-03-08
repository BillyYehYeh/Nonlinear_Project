#include "Log2Exp.h"
#include <iostream>
#include <bitset>



void Log2Exp::process() {
    // =======================================================================
    // Step 1: Extract FP16 components
    // FP16 format: [Sign(1)] [Exponent(5 bits)] [Mantissa(10 bits)]
    // Exponent range: 0-31, with bias of 15
    // =======================================================================
    sc_dt::sc_uint<16> fp16_val = fp16_in.read();
    sc_dt::sc_uint<5> exp_bits = fp16_val.range(14, 10);
    sc_dt::sc_uint<10> mant_bits = fp16_val.range(9, 0);  
    int exponent_val = (int)exp_bits.to_uint();  // FP16 exponent value (0-31)

    // =======================================================================
    // Step 2: Construct mantissa with implicit leading 1
    // Fixed-point format Q1.4: 1 integer bit + 4 fractional bits
    // Result: [implicit_1(1)] [mant_bits(10)] [padding(4)] = 18 bits total
    // =======================================================================
    sc_dt::sc_uint<4> one_bits = 1;
    sc_dt::sc_uint<4> zero_bits = 0;
    sc_uint<18> mant_val = ((one_bits.to_uint() << 14) | (mant_bits.to_uint() << 4)) & 0x3FFFF;
    sc_uint<18> op1 = mant_val;                                     
    sc_uint<18> op2 = (mant_val >> 1) ;                             
    sc_uint<18> op3 = (mant_val >> 4) ;                             
    
    // =======================================================================
    // Step 3: Sum the three operations
    // Compute: op1 + op2 - op3
    // This approximates the logarithmic function for the mantissa
    // =======================================================================
    sc_uint<18> sum = op1 + op2 - op3;

    // =======================================================================
    // Step 4: Apply exponent shift
    // FP16 bias is 15, so: actual_exponent = exponent_val - 15
    // Shift the sum result based on the actual exponent
    // Positive exponent: shift LEFT (multiply by 2^n)
    // Negative exponent: shift RIGHT (divide by 2^n)
    // =======================================================================
    int actual_exp_val = exponent_val - 15;
    
    sc_uint<18> shifted_value;
    if (actual_exp_val <= 0) {
        // If actual exponent is negative or zero, shift RIGHT (move decimal point left)
        shifted_value = (sum >> (-actual_exp_val));
    } else {
        // If actual exponent is positive, shift LEFT (move decimal point right)
        shifted_value = (sum << actual_exp_val);         // shifted_value with exponent applied
    }

    // =======================================================================
    // Step 5: Extract result bits
    // Extract bits [17:14] to get 4-bit integer result
    // These bits represent the integer portion after fixed-point operations
    // =======================================================================
    sc_dt::sc_uint<4> shift_res = shifted_value.range(17, 14).to_uint();  // Extract bits [17:14]
    
    // =======================================================================
    // Step 6: Multiplexer logic - Select output based on exponent
    // exponent_val <= 13: Output 0x0 (very small numbers)
    // exponent_val >= 19: Output 0xF (very large numbers)
    // exponent_val == 18 AND sum[15]==1: Output 0xF (transition boundary)
    // else: Output computed shift_res
    // =======================================================================
    sc_dt::sc_uint<4> output;
    if (exponent_val <= 13) {
        output = 0x0;  // 0000 - exponent too small
    } else if (exponent_val >= 19) {
        output = 0xF;  // 1111 - exponent too large
    } else if (exponent_val == 18 && sum[15] == 1) {
        output = 0xF;  // 1111 - boundary condition: exponent at 18 with bit 15 set
    } else {
        output = shift_res;  // Normal case: use computed result
    }

    result_out.write(output);
}
