#ifndef DIVIDER_H
#define DIVIDER_H

#include <systemc.h>
#include <iostream>
#include <cstdint>

using sc_uint4 = sc_dt::sc_uint<4>;
using sc_uint5 = sc_dt::sc_uint<5>;
using sc_uint16 = sc_dt::sc_uint<16>;
using sc_uint1 = sc_dt::sc_uint<1>;
using sc_uint10 = sc_dt::sc_uint<10>;

/**
 * @brief SystemC Module for Divider
 * 
 * This module performs divider operation:
 * 1. Calculate difference: diff = ky - ks
 * 2. Extract exponent from Mux_Result (FP16 format, bits 14:10)
 * 3. Modify exponent: new_exp = exp - diff
 * 4. Output shifted FP16 value with modified exponent
 * 
 * FP16 Format: [sign(1) | exponent(5) | mantissa(10)]
 * 
 * Ports:
 *   - ky: 4-bit input (right shift bits)
 *   - ks: 4-bit input (left shift bits)
 *   - Mux_Result: 16-bit input (FP16 format)
 *   - Divider_Output: 16-bit output (FP16 format with modified exponent)
 */
SC_MODULE(Divider_Module) {
    // Ports
    sc_in<sc_uint4>  ky;                   ///< 4-bit input ky (right shift bits)
    sc_in<sc_uint4>  ks;                   ///< 4-bit input ks (left shift bits)
    sc_in<sc_uint16> Mux_Result;           ///< 16-bit input (FP16 format)
    sc_out<sc_uint16> Divider_Output;      ///< 16-bit output (FP16 format)

    // Constructor
    SC_CTOR(Divider_Module) {
        SC_METHOD(compute_divider);
        sensitive << ky << ks << Mux_Result;
    }

    // Methods
    void compute_divider();  ///< Combinational logic to compute output
};

#endif // DIVIDER_H
