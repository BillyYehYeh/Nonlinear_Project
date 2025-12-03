#include "MaxUnit.h"

/**
 * @brief Implementation of fp16_max - Binary FP16 maximum operation
 */
sc_uint16 fp16_max(sc_uint16 a_bits, sc_uint16 b_bits) {
    // Get Sign Bit
    bool a_sign = a_bits[15];
    bool b_sign = b_bits[15];
    
    // Get Exponent + Mantissa
    sc_uint<15> a_rest = a_bits.range(14, 0);
    sc_uint<15> b_rest = b_bits.range(14, 0);

    // (Sign Bit) compare
    if (a_sign == 0 && b_sign == 1) {
        return a_bits;
    }
    if (a_sign == 1 && b_sign == 0) {
        return b_bits;
    }

    // (Exponent + Mantissa) compare
    if (a_sign == 0 && b_sign == 0) {       // Positive
        return (a_rest >= b_rest) ? a_bits : b_bits;
    } else {                                // Negative
        return (a_rest <= b_rest) ? a_bits : b_bits;
    }
}
/**
 * @brief Stage 1 Combinational Logic - Parallel FP16 Comparisons
 * 
 * Compares A vs B and C vs D in parallel using fp16_max function.
 * Results are stored in R1_next and R2_next, which will be latched
 * on the next clock edge.
 */
void Max4_FP16_Pipeline::stage1_comb_logic() {
    R1_next.write(fp16_max(A.read(), B.read()));
    R2_next.write(fp16_max(C.read(), D.read()));
}

/**
 * @brief Stage 1 Register Update - Clock-driven Update
 * 
 * Updates pipeline registers R1_reg and R2_reg on positive clock edge.
 * On reset (rst=1), registers are cleared to 0.
 * Otherwise, registers capture the combinational logic outputs.
 */
void Max4_FP16_Pipeline::stage1_register_update() {
    if (rst.read() == true) {   // Reset
        R1_reg.write(0);
        R2_reg.write(0);
    } else { 
        R1_reg.write(R1_next.read());
        R2_reg.write(R2_next.read());
    }
}

/**
 * @brief Stage 2 Combinational Logic - Final Comparison
 * 
 * Compares the two pipeline register values to produce the final maximum.
 * No latching occurs at this stage - output is combinational.
 */
void Max4_FP16_Pipeline::stage2_comb_logic() {
    Max_Out.write(fp16_max(R1_reg.read(), R2_reg.read()));
}
