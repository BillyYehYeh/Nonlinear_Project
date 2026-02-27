#ifndef MAXUNIT_H
#define MAXUNIT_H

#include <systemc.h>
#include <iostream>

using sc_uint16 = sc_dt::sc_uint<16>;

/**
 * @brief Binary maximum operation for FP16 (half-precision floating point)
 * 
 * Compares two FP16 values represented as 16-bit unsigned integers and returns
 * the maximum value according to FP16 comparison rules:
 * - Positive numbers are always greater than negative numbers
 * - For numbers with the same sign, compares exponent+mantissa field
 * 
 * @param a_bits First FP16 value (16-bit representation)
 * @param b_bits Second FP16 value (16-bit representation)
 * @return sc_uint16 Maximum of the two values
 */
sc_uint16 fp16_max(sc_uint16 a_bits, sc_uint16 b_bits);

/**
 * @brief 4-Input FP16 Maximum Finder with 2-Stage Pipeline
 * 
 * This SystemC module implements a pipelined maximum finder for 4 FP16 inputs:
 * - Stage 1: Parallel comparison of (A vs B) and (C vs D)
 * - Register: Pipeline registers store intermediate results
 * - Stage 2: Final comparison of stage 1 results
 * 
 * Latency: 2 clock cycles
 * 
 * Ports:
 *   - clk: Clock input (positive edge triggered)
 *   - rst: Reset input (active high)
 *   - A, B, C, D: 4x 16-bit FP16 input ports
 *   - Max_Out: 16-bit FP16 output port
 */
SC_MODULE(MaxUnit) {
    // Ports
    sc_in<bool>      clk, rst;        ///< Clock and reset signals
    sc_in<sc_uint16> A, B, C, D;      ///< 4x FP16 input ports
    sc_out<sc_uint16> Max_Out;        ///< FP16 maximum output

    // Internal signals
    sc_signal<sc_uint16> R1_reg, R2_reg;    ///< Pipeline register values
    sc_signal<sc_uint16> R1_next, R2_next;  ///< Next pipeline register values

    // Methods
    void stage1_comb_logic();         ///< Stage 1: Parallel comparisons
    void stage1_register_update();    ///< Register update on clock edge
    void stage2_comb_logic();         ///< Stage 2: Final comparison

    /**
     * @brief Constructor - Set up sensitivity lists and method bindings
     */
    SC_CTOR(MaxUnit) {
        SC_METHOD(stage1_comb_logic);
        sensitive << A << B << C << D; 
        SC_METHOD(stage1_register_update);
        sensitive << clk.pos();
        SC_METHOD(stage2_comb_logic);
        sensitive << R1_reg << R2_reg;
    }
};

#endif // MAXUNIT_H
