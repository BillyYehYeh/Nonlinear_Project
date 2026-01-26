#include "Divider.h"

/**
 * @brief Compute the divider operation
 * 
 * This method:
 * 1. Calculates diff = ky - ks
 * 2. Extracts the exponent from Mux_Result (bits 14:10 in FP16)
 * 3. Computes new_exp = exp - diff (with saturation)
 * 4. Reconstructs the FP16 output with the modified exponent
 * 
 * FP16 format: [sign(15) | exponent(14:10) | mantissa(9:0)]
 */
void Divider_Module::compute_divider() {
    sc_uint4 ky_val = ky.read();
    sc_uint4 ks_val = ks.read();
    sc_uint16 mux_result = Mux_Result.read();
    
    // Step 1: Calculate difference (ky + ks)
    // Both are 4-bit, so we need to handle signed arithmetic
    // Cast to signed integers for proper subtraction
    int ky_signed = (int)ky_val.to_uint();
    int ks_signed = (int)ks_val.to_uint();
    int right_shift = ky_signed + ks_signed;
    
    // Step 2: Extract exponent from FP16 (bits 14:10)
    sc_uint<5> exponent = mux_result.range(14, 10);
    int exp_val = (int)exponent.to_uint();
    
    // Step 3: Calculate new exponent with saturation
    // new_exp = exp - right_shift
    int new_exp_val = exp_val - right_shift;
    
    // Saturate to valid range (0-31 for 5-bit exponent)
    if (new_exp_val < 0) {
        new_exp_val = 0;  // Underflow
    } else if (new_exp_val > 31) {
        new_exp_val = 31;  // Overflow
    }
    
    // Step 4: Extract other components of FP16
    sc_uint<1> sign = (bool)mux_result[15];
    sc_uint<10> mantissa = mux_result.range(9, 0);
    
    // Step 5: Reconstruct FP16 with new exponent
    // [sign(15) | exponent(14:10) | mantissa(9:0)]
    sc_uint<16> output_val;
    output_val = (sign, (sc_uint<5>)new_exp_val, mantissa);
    
    Divider_Output.write(sc_uint16(output_val));
}
