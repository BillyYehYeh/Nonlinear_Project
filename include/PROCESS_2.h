#ifndef PROCESS_2_H
#define PROCESS_2_H

#include <systemc.h>
#include <iostream>
#include "Divider_PreCompute.h"

using sc_uint4 = sc_dt::sc_uint<4>;
using sc_uint16 = sc_dt::sc_uint<16>;
using sc_uint32 = sc_dt::sc_uint<32>;
using sc_uint64 = sc_dt::sc_uint<64>;

/**
 * @struct Output_Data
 * @brief Data structure for output register storage
 * 
 * Holds the output values from Divider_PreCompute_Module that are sampled
 * and held based on the stall_output control signal.
 */
struct Output_Data {
    sc_uint4  Leading_One_Reg;      ///< 4-bit leading one position register
    sc_uint16 Mux_Result_Reg;       ///< 16-bit FP16 threshold value register
};


/**
 * @class PROCESS_2_Module
 * @brief Divider Pre-computation Module with Output Stall Control
 * 
 * **Functional Description:**
 * ==========================
 * PROCESS_2 performs fixed-point number analysis for normalized division
 * operations in CNN processing pipelines:
 * 
 * 1. **Input Stage**: Receives 32-bit fixed-point number (16-bit integer + 16-bit decimal)
 *    and routes to embedded Divider_PreCompute_Module
 * 
 * 2. **Processing**: Divider_PreCompute_Module analyzes the number:
 *    - Detects position of leading (most significant) one bit (0-15)
 *    - Checks bit at position (Leading_One - 1) for threshold selection
 *    - Outputs: fp16(0.818)=0x3A8B if bit=1, fp16(0.568)=0x388B if bit=0
 * 
 * 3. **Output Control**: Stall mechanism for pipeline management
 *    - stall_output=0: Output registers continuously update
 *    - stall_output=1: Output registers hold previous values (pipeline hold)
 *    - rst=1: All registers cleared to 0
 * 
 * **Latency**: 1 clock cycle (combinational processing + register sampling)
 * **Use Case**: CNN preprocessing for feature normalization
 */
SC_MODULE(PROCESS_2_Module) {
    // ===== Input Ports =====
    sc_in<bool>              clk;                  ///< Clock signal (synchronizes stall register updates)
    sc_in<bool>              rst;                  ///< Reset signal (forces output registers to 0)
    sc_in<bool>              stall_output;         ///< Stall control (1=hold, 0=update)
    sc_in<sc_uint32>         Pre_Compute_In;       ///< 32-bit fixed-point input (16-bit integer|16-bit decimal)
    
    // ===== Output Ports =====
    sc_out<sc_uint4>         Leading_One_Pos_Out;   ///< 4-bit leading one position (0-15)
    sc_out<sc_uint16>        Mux_Result_Out;        ///< 16-bit FP16 threshold (0x3A8B or 0x388B)

    // ===== Internal Signals & Registers =====
    
    /** Embedded Divider_PreCompute_Module instance for core processing */
    Divider_PreCompute_Module   *Divider_Pre_units;  

    /** Input signal routing from port to embedded unit */
    sc_signal<sc_uint32>     Pre_Compute_In_Signal;
    
    /** Leading one position output from embedded unit (combinational) */
    sc_signal<sc_uint4>      Leading_One_Pos_Out_Signal; 
    
    /** Threshold value output from embedded unit (combinational) */
    sc_signal<sc_uint16>     Mux_Result_Out_Signal;

    /** Output stall register: holds leading one position when stalled */
    sc_signal<sc_uint4>      Output_Reg_Lo_Pos;
    
    /** Output stall register: holds mux result when stalled */
    sc_signal<sc_uint16>     Output_Reg_Mux_Result;

    // ===== Process Methods =====
    
    /** @brief Input combinational logic - routes input to Divider unit */
    void Input_Comb();
    
    /** @brief Output combinational logic - routes registers to output ports */
    void Output_Comb();
    
    /** @brief Sequential stall logic - controls register updates on clock */
    void Output_Stall();
    
    // ===== Constructor =====
    SC_HAS_PROCESS(PROCESS_2_Module);
    PROCESS_2_Module(sc_core::sc_module_name name) : sc_core::sc_module(name) {
        // Instantiate Divider_PreCompute_Module
        Divider_Pre_units = new Divider_PreCompute_Module("divider_pre");
        Divider_Pre_units->input(Pre_Compute_In_Signal);
        Divider_Pre_units->Leading_One_Pos(Leading_One_Pos_Out_Signal);
        Divider_Pre_units->Mux_Result(Mux_Result_Out_Signal);
        
        // Register process methods
        SC_METHOD(Input_Comb);
        sensitive << Pre_Compute_In;

        SC_METHOD(Output_Comb);
        sensitive << Output_Reg_Lo_Pos << Output_Reg_Mux_Result;
        
        SC_METHOD(Output_Stall);
        sensitive << stall_output << rst << Mux_Result_Out_Signal << Leading_One_Pos_Out_Signal;
    }
};

#endif // PROCESS_2_H