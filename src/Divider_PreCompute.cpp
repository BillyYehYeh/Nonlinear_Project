#include "Divider_PreCompute.h"
#include <cmath>
#include <cstring>

/**
 * @brief Find the position of the leading one bit
 * 
 * Scans from bit 31 (MSB) down to bit 0 (LSB) to find the first '1' bit.
 * Returns the bit position (31 = MSB, 0 = LSB).
 * If input is 0, returns 0.
 */
sc_uint4 find_leading_one_pos(sc_uint32 input) {
    uint32_t val = input.to_uint();
    
    // If input is 0, return 0
    if (val == 0) {
        return sc_uint4(0);
    }
    
    // Find the position of the most significant bit set to 1
    // Scan from bit 31 down to bit 0
    for (int i = 31; i >= 0; i--) {
        if ((val >> i) & 1) {
            return sc_uint4(i);
        }
    }
    
    return sc_uint4(0);
}


/**
 * @brief Compute threshold result based on leading one position
 * 
 * This method:
 * 1. Finds the position of the leading one bit
 * 2. Extracts the bit at (Leading_One_Pos - 1)
 * 3. Selects threshold value based on that bit
 */
void Divider_PreCompute_Module::compute_threshold() {
    sc_uint32 input_val = input.read();
    
    // Step 1: Find the leading one position
    sc_uint4 leading_pos = find_leading_one_pos(input_val);
    Leading_One_Pos.write(leading_pos);
    
    // Step 2: Extract the bit at position (leading_pos - 1)
    sc_uint1 is_over_half;
    
    if (leading_pos == 0) {
        // If leading position is 0, bit at position -1 doesn't exist, set to 0
        is_over_half = sc_uint1(0);
    } else {
        // Get the bit at position (leading_pos - 1)
        uint32_t val = input_val.to_uint();
        int bit_pos = leading_pos.to_uint() - 1;
        is_over_half = sc_uint1((val >> bit_pos) & 1);
    }
    
    // Step 3: Select threshold based on Is_Over_Half
    sc_uint16 threshold;
    
    if (is_over_half == 1) {
        // Is_Over_Half = 1: use fp16(0.818) = 0011101010001010
        threshold = sc_uint16(0x3A8A);
    } else {
        // Is_Over_Half = 0: use fp16(0.568) = 0011100010001111
        threshold = sc_uint16(0x388F);
    }
    
    Threshold_Result.write(threshold);
}
