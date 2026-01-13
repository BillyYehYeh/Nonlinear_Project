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
    
    // Step 1: Calculate difference (ky - ks)
    // Both are 4-bit, so we need to handle signed arithmetic
    // Cast to signed integers for proper subtraction
    int ky_signed = (int)ky_val.to_uint();
    int ks_signed = (int)ks_val.to_uint();
    int diff = ky_signed - ks_signed;
    
    // Step 2: Extract exponent from FP16 (bits 14:10)
    sc_uint5 exp_bits = sc_uint5((mux_result.to_uint() >> 10) & 0x1F);
    int exp_val = (int)exp_bits.to_uint();
    
    // Step 3: Calculate new exponent with saturation
    // new_exp = exp - diff
    int new_exp_val = exp_val - diff;
    
    // Saturate to valid range (0-31 for 5-bit exponent)
    if (new_exp_val < 0) {
        new_exp_val = 0;  // Underflow
    } else if (new_exp_val > 31) {
        new_exp_val = 31;  // Overflow
    }
    
    // Step 4: Extract other components of FP16
    sc_uint1 sign = sc_uint1((mux_result.to_uint() >> 15) & 0x1);
    sc_uint10 mantissa = sc_uint10(mux_result.to_uint() & 0x3FF);
    
    // Step 5: Reconstruct FP16 with new exponent
    // [sign(15) | exponent(14:10) | mantissa(9:0)]
    uint16_t output_val = 0;
    output_val |= (sign.to_uint() << 15);
    output_val |= ((new_exp_val & 0x1F) << 10);
    output_val |= (mantissa.to_uint() & 0x3FF);
    
    Divider_Output.write(sc_uint16(output_val));
}
