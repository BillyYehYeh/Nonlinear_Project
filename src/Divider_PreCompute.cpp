#include "Divider_PreCompute.h"
#include <cmath>
#include <cstring>

/**
 * @brief Find the position of the leading one bit in the integer part (bits 31-16)
 * 
 * The 32-bit input consists of:
 *   - Bits 31-16: 16-bit integer part
 *   - Bits 15-0: 16-bit decimal part
 * 
 * Scans bits 31 down to 16 to find the first '1' bit in the integer part.
 * Returns the bit position relative to bit 16 (0-15 range).
 * If the integer part is all zeros, returns 0.
 */
sc_uint4 find_leading_one_pos(sc_uint32 input) {
    uint32_t val = input.to_uint();
    
    // Extract the integer part (bits 31-16)
    uint32_t integer_part = (val >> 16) & 0xFFFF;
    
    // If integer part is 0, return 0
    if (integer_part == 0) {
        return sc_uint4(0);
    }
    
    // Find the position of the most significant bit set to 1 in the integer part
    // Scan from bit 15 down to bit 0 of the integer part
    for (int i = 15; i >= 0; i--) {
        if ((integer_part >> i) & 1) {
            return sc_uint4(i);
        }
    }
    
    return sc_uint4(0);
}


/**
 * @brief Compute threshold result based on leading one position
 * 
 * This method:
 * 1. Finds the position of the leading one bit in the integer part
 * 2. If leading_pos > 0: Extracts the bit at (leading_pos - 1) in the integer part
 * 3. If leading_pos == 0: Extracts the first decimal place (bit 15 of decimal part)
 * 4. Selects threshold value based on that bit
 */
void Divider_PreCompute_Module::compute_threshold() {
    sc_uint32 input_val = input.read();
    uint32_t val = input_val.to_uint();
    
    // Step 1: Find the leading one position
    sc_uint4 leading_pos = find_leading_one_pos(input_val);
    Leading_One_Pos.write(leading_pos);
    
    // Step 2: Extract the bit to determine Is_Over_Half
    sc_uint1 is_over_half;
    
    
    // Get the bit at position (16 + leading_pos - 1) = (15 + leading_pos)
    // This is one position below the leading one in the integer part
    int bit_pos = 15 + leading_pos.to_uint();
    is_over_half = sc_uint1((val >> bit_pos) & 1);
    
    // Step 3: Select threshold based on Is_Over_Half
    sc_uint16 threshold;
    
    if (is_over_half == 1) {
        // Is_Over_Half = 1: use fp16(0.818) = 0011101010001011
        threshold = sc_uint16(0x3A8B);
    } else {
        // Is_Over_Half = 0: use fp16(0.568) = 0011100010001011
        threshold = sc_uint16(0x388B);
    }
    
    Mux_Result.write(threshold);
}
