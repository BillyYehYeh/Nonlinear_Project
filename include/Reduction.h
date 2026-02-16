#ifndef REDUCTION_H
#define REDUCTION_H

#include <systemc.h>
#include <iostream>
#include <vector>

using sc_int4 = sc_dt::sc_int<4>;
using sc_uint32 = sc_dt::sc_uint<32>;

/**
 * @brief Exponential-Sum Reduction Module (指數加總歸約模組)
 * 
 * Computes the sum of exponential values: Output = Σ(2^(-x_i))
 * 
 * Architecture:
 * - 4 parallel Power-of-Two units compute 2^(-x_i) for each 4-bit input
 * - Binary adder tree with pipeline register for timing optimization
 * - Stage 1: Two 32-bit adders (parallel addition of input pairs)
 * - Pipeline Register: For timing optimization
 * - Stage 2: One 32-bit adder (final reduction)
 * 
 * Data Format:
 * - Input: 4×4 bits (signed int4 values)
 * - Output: 32 bits (16.16 fixed-point)
 *   - Bits [31:16]: Integer part
 *   - Bits [15:0]: Fractional part
 *   - Resolution: 2^(-16)
 * 
 * Ports:
 *   - clk: Clock input
 *   - rst: Reset signal
 *   - Input_Vector: 4×4 bits input vector (4 signed integers)
 *   - Output_Sum: 32 bits output (16.16 fixed-point format)
 */
SC_MODULE(Reduction_Module) {
    // Ports
    sc_in<bool>                      clk;             ///< Clock input
    sc_in<bool>                      rst;             ///< Reset signal
    sc_in<sc_dt::sc_uint<16>>        Input_Vector;    ///< 4×4 bits packed input
    sc_out<sc_uint32>                Output_Sum;      ///< 32-bit output (16.16 fixed-point)

    // Internal signals for pipeline stages
    sc_signal<sc_uint32> exp_out[4];                  ///< Outputs of 4 exponential units
    sc_signal<sc_uint32> stage1_add0;                 ///< First adder output (exp[0] + exp[1])
    sc_signal<sc_uint32> stage1_add1;                 ///< Second adder output (exp[2] + exp[3])
    sc_signal<sc_uint32> pipe_add0_reg;               ///< Pipeline register for stage1_add0
    sc_signal<sc_uint32> pipe_add1_reg;               ///< Pipeline register for stage1_add1
    sc_signal<sc_uint32> final_sum;                   ///< Final sum output

    // Constructor
    SC_HAS_PROCESS(Reduction_Module);
    Reduction_Module(sc_core::sc_module_name name) : sc_core::sc_module(name) {
        // Register processes
        SC_METHOD(compute_exponentials);
        sensitive << Input_Vector;
        
        SC_METHOD(stage1_additions);
        sensitive << exp_out[0] << exp_out[1] << exp_out[2] << exp_out[3];
        
        SC_METHOD(pipeline_register_update);
        sensitive << clk.pos();
        
        SC_METHOD(stage2_addition);
        sensitive << pipe_add0_reg << pipe_add1_reg;
        
        SC_METHOD(update_output);
        sensitive << final_sum;
    }

    // Methods
    void compute_exponentials();        ///< Compute 2^(-x_i) for each input
    void stage1_additions();             ///< Parallel addition in stage 1
    void pipeline_register_update();    ///< Update pipeline registers on clock edge
    void stage2_addition();              ///< Final addition in stage 2
    void update_output();                ///< Output the final sum

private:

};

#endif // REDUCTION_H
