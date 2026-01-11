#ifndef DIVIDER_PRECOMPUTE_H
#define DIVIDER_PRECOMPUTE_H

#include <systemc.h>
#include <iostream>
#include <cstdint>

using sc_uint4 = sc_dt::sc_uint<4>;
using sc_uint32 = sc_dt::sc_uint<32>;
using sc_uint16 = sc_dt::sc_uint<16>;
using sc_uint1 = sc_dt::sc_uint<1>;

/**
 * @brief Find the position of the leading (most significant) one bit in a 32-bit value
 * 
 * Scans from MSB to LSB to find the position of the first '1' bit.
 * Position 31 is the MSB (2^31), position 0 is the LSB.
 * Returns 0 if input is 0.
 * 
 * @param input 32-bit unsigned integer
 * @return sc_uint4 4-bit position of leading one (0-31, stored in 4 bits)
 */
sc_uint4 find_leading_one_pos(sc_uint32 input);

/**
 * @brief SystemC Module for Divider PreComputation
 * 
 * This module performs preprocessing for a divider circuit:
 * 1. Finds the position of the leading one bit (0-31)
 * 2. Extracts the bit at position (Leading_One_Pos - 1)
 * 3. Uses this bit to select between two threshold values (fp16)
 * 
 * Ports:
 *   - input: 32-bit unsigned integer input
 *   - Leading_One_Pos: 4-bit output (position of leading one, 0-15)
 *   - Threshold_Result: 16-bit output (fp16 format)
 *       - If bit at (Leading_One_Pos - 1) = 1: outputs fp16(0.818)
 *       - If bit at (Leading_One_Pos - 1) = 0: outputs fp16(0.568)
 */
SC_MODULE(Divider_PreCompute_Module) {
    // Ports
    sc_in<sc_uint32>  input;                ///< 32-bit input
    sc_out<sc_uint4>  Leading_One_Pos;      ///< 4-bit output: position of leading one
    sc_out<sc_uint16> Threshold_Result;     ///< 16-bit output: fp16 threshold value

    // Constructor
    SC_CTOR(Divider_PreCompute_Module) {
        SC_METHOD(compute_threshold);
        sensitive << input;
    }

    // Methods
    void compute_threshold();  ///< Combinational logic to compute outputs
};

#endif // DIVIDER_PRECOMPUTE_H
