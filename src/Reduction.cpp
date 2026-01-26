#include "Reduction.h"

/**
 * @brief Implementation of power_of_two - Compute 2^X
 */
sc_uint32 power_of_two(sc_uint4 X) {
    // X is a 4-bit value (0-15)
    // 2^X ranges from 2^0=1 to 2^15=32768
    unsigned int x_val = X.to_uint();
    
    // Use left shift to compute 2^X (1 << x = 2^x)
    // Then shift left by 16 bits to attach 16 bits of decimal zeros
    sc_uint<16> power_result = 1U << x_val;
    sc_uint32 result = ((sc_uint32)power_result << 16);
    return result;
}

/**
 * @brief Compute power combinational logic
 * 
 * This method is triggered whenever X_in changes.
 * It computes Y_out = 2^X_in using the power_of_two function.
 */
void Reduction_Module::compute_power() {
    sc_uint4 x_input = X_in.read();
    sc_uint32 y_output = power_of_two(x_input);
    Y_out.write(y_output);
}
