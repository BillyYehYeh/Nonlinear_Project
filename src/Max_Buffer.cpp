#include "Max_Buffer.h"

/**
 * @brief Binary maximum operation for FP16
 * 
 * Compares two FP16 values and returns the maximum according to FP16 rules.
 * Positive numbers are greater than negative numbers.
 * For same-sign numbers, compare exponent+mantissa field.
 */
sc_uint16 fp16_max_buffer(sc_uint16 a_bits, sc_uint16 b_bits) {
    // Get Sign Bit
    bool a_sign = a_bits[15];
    bool b_sign = b_bits[15];
    
    // Get Exponent + Mantissa
    sc_uint<15> a_rest = a_bits.range(14, 0);
    sc_uint<15> b_rest = b_bits.range(14, 0);

    // Sign bit compare
    if (a_sign == 0 && b_sign == 1) {
        return a_bits;  // a is positive, b is negative
    }
    if (a_sign == 1 && b_sign == 0) {
        return b_bits;  // b is positive, a is negative
    }

    // Exponent + Mantissa compare
    if (a_sign == 0 && b_sign == 0) {       // Both positive
        return (a_rest >= b_rest) ? a_bits : b_bits;
    } else {                                // Both negative
        return (a_rest <= b_rest) ? a_bits : b_bits;
    }
}

/**
 * @brief Combinational Read and Global Max Computation
 * 
 * This method performs two operations:
 * 1. Reads data from buffer at the address specified by index (combinational)
 * 2. Outputs the cached global maximum value (updated on write)
 * 
 * Both outputs are updated combinationally whenever inputs change.
 */
void Max_Buffer::compute_read_and_global_max() {
    // Combinational read from buffer at index address
    sc_uint10 addr = index.read();
    Local_Max_Out.write(buffer[addr]);

    // Output the cached global maximum
    Global_Max_Out.write(global_max);
}

/**
 * @brief Synchronous Write Operation with Global Max Update
 * 
 * On each positive clock edge:
 * - If reset=1: Clear all buffer entries and reset global_max to FP16_MIN
 * - Else if write=1: Store Local_Max_In at buffer[index]
 *   AND update global_max by comparing with the new data
 * - Else: No operation
 * 
 * Write latency is 1 clock cycle. Global max is updated immediately on write.
 */
void Max_Buffer::write_on_clock() {
    if (reset.read() == 1) {
        // Clear entire buffer
        for (int i = 0; i < 1024; i++) {
            buffer[i] = 0;
        }
        // Reset global_max to minimum FP16 value
        global_max = FP16_MIN;
    } else if (write.read() == 1) {
        sc_uint10 addr = index.read();
        sc_uint16 data = Local_Max_In.read();
        buffer[addr] = data;
        
        // Update global_max by comparing with the new data
        global_max = fp16_max_buffer(global_max, data);
    }
}
