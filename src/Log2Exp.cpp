#include "Log2Exp.h"

void Log2Exp::process() {
    // Extract exponent (bits 14:10) and mantissa (bits 9:0)
    sc_bv<5> exp_bits = fp16_in.read().range(14, 10);
    sc_bv<10> mant_bits = fp16_in.read().range(9, 0);

    // Store exponent as integer for comparison
    int exponent_val = (int)exp_bits.to_uint();  // Explicitly convert to unsigned first
    exponent.write(exp_bits);

    // Extract mantissa: implicit bit (1) + 4 MSB of mantissa = 5 bits total
    // Implicit bit is always 1 for normalized numbers
    sc_bv<5> mantissa;
    mantissa[4] = 1;  // Implicit bit
    mantissa[3] = mant_bits[9];  // 4 MSB of mantissa
    mantissa[2] = mant_bits[8];
    mantissa[1] = mant_bits[7];
    mantissa[0] = mant_bits[6];
    
    mantissa_extract.write(mantissa);

    // Perform three operations on mantissa (Q1.4 fixed-point format):
    // Mantissa is in Q1.4 format: 1 integer bit + 4 fractional bits
    // 1. Mantissa_extract (5 bits in Q1.4)
    // 2. Mantissa_extract << 1 (result in Q2.4 - shifts decimal point right by 1)
    // 3. Mantissa_extract << 4 (result in Q5.4 - shifts decimal point right by 4)
    // Then add them together in a common fixed-point format
    
    int mant_val = mantissa.to_uint();                     // Mantissa as integer (Q1.4)
    int op1 = mant_val;                                     // Q1.4 format
    int op2 = (mant_val << 1) & 0x3FF;                     // Q2.4 format (left shift 1 bit)
    int op3 = (mant_val << 4) & 0x3FF;                     // Q5.4 format (left shift 4 bits)

    // Sum all three operations (result will be in Q5.4 format due to op3)
    // Decimal point is at position 4 (fractional bits)
    int sum = op1 + op2 + op3;
    
    // Keep only the lower 10 bits
    sc_bv<10> sum_10bits = sum & 0x3FF;
    sum_result.write(sum_10bits);

    // Shift the sum_result by the exponent
    // FP16 bias is 15, so actual_exponent = exponent_val - 15
    // The result is shifted right by actual_exponent
    // Note: when shifting, we're working with fixed-point, decimal point is at bit 4
    int actual_exp_val = exponent_val - 15;
    actual_exponent.write(actual_exp_val);  // Write to signal for test observation
    
    int shifted_value;
    if (actual_exp_val <= 0) {
        // If actual exponent is negative or zero, shift LEFT (move decimal point right)
        shifted_value = (sum_10bits.to_uint() >> (-actual_exponent));
    } else {
        // If actual exponent is positive, shift RIGHT (move decimal point left)
        shifted_value = (sum_10bits.to_uint() << actual_exponent);
    }
    shifted_value_sig.write(shifted_value);  // Write to signal for test observation

    // Keep only the 4 bits LSB as integer part of the fixed-point result
    // The decimal point is at position 4, so bits 4-7 are the integer part,
    // but we only want the lower 4 bits of the integer representation
    int shift_res_int = (shifted_value >> 4) & 0xF;  // Extract bits [7:4] (integer bits) and keep 4 bits
    shift_res_int_sig.write(shift_res_int);  // Write to signal for test observation
    
    sc_bv<4> shift_res = shift_res_int;
    shift_result.write(shift_res);

    // Three-input multiplexer logic
    sc_bv<4> output;
    if (exponent_val <= 9) {
        output = 0x0;  // 0000
    } else if (exponent_val >= 15) {
        output = 0xF;  // 1111
    } else {
        output = shift_res;  // Use local variable instead of reading signal
    }

    result_out.write(output);
}
