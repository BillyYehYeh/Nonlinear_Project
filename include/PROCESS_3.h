#ifndef PROCESS_3_H
#define PROCESS_3_H

#include <systemc.h>
#include <iostream>
#include "Log2Exp.h"
#include "Divider.h"

using sc_uint4 = sc_dt::sc_uint<4>;
using sc_uint16 = sc_dt::sc_uint<16>;
using sc_uint32 = sc_dt::sc_uint<32>;
using sc_uint64 = sc_dt::sc_uint<64>;

/**
 * @brief FP16 Subtraction Function
 * 
 * Performs IEEE 754 FP16 (half-precision floating point) subtraction:
 * result = a_bits - b_bits
 * 
 * Implementation note: Subtraction is performed by negating b_bits
 * (flipping sign bit: XOR with 0x8000) and then adding to a_bits
 * using the fp16_add utility function.
 * 
 * @param a_bits First operand as 16-bit FP16 value
 * @param b_bits Second operand as 16-bit FP16 value (will be subtracted from a)
 * @return Result of (a_bits - b_bits) as 16-bit FP16 value
 * 
 * @note Performs sign-preserving subtraction with proper IEEE 754 handling
 */
sc_uint16 fp16_subtract(sc_uint16 a_bits, sc_uint16 b_bits);


/**
 * @struct Stage1_Data
 * @brief Pipeline Stage 1: Subtraction Result
 * 
 * Holds the result of FP16 subtraction (Local_Max - Global_Max)
 * passed through the first pipeline stage.
 * 
 * This value is used in Log2Exp transformation in Stage 2.
 */
struct Stage1_Data {
    sc_uint16 Sub_Result;  ///< FP16 subtraction result (16-bit)
};

/**
 * @struct Stage2_Data
 * @brief Pipeline Stage 2: Power/Exponent Value
 * 
 * Holds the 4-bit exponent output from Log2Exp transformation.
 * Represents log2 of the input with reduced precision.
 */
struct Stage2_Data {
    sc_uint4 Power;        ///< 4-bit exponent from Log2Exp (range 0-15)
};

/**
 * @struct Stage3_Data
 * @brief Pipeline Stage 3: Division Parameters
 * 
 * Holds the multiplexer result (threshold), scale factor (ks),
 * and pre-computed coefficients (ky[0..3]) for division operations.
 * 
 * These values control the behavior of the 4 Divider modules.
 */
struct Stage3_Data {
    sc_uint16 Mux_Result;  ///< 16-bit threshold value (fp16)
    sc_uint4  ks;          ///< 4-bit scale factor
    sc_uint4  ky[4];       ///< 4 x 4-bit pre-computed coefficients
};

/**
 * @class PROCESS_3_Module
 * @brief Exponential Processing & Division Pipeline (5-Stage)
 * 
 * **Functional Description:**
 * ==========================
 * PROCESS_3 implements a pipelined exponential processing and division
 * computation for CNN feature normalization:
 * 
 * **Pipeline Stages (5 total):**
 * 
 * **Stage 0 (Combinational):**
 *   - Input Ports receive: Local_Max, Global_Max, ks_In, Mux_Result_In, etc.
 *   - Each input drives corresponding combinational stage
 * 
 * **Stage 1 (Register):**
 *   - Stores FP16 subtraction: Sub_Result = Local_Max - Global_Max
 *   - Output registered at end of cycle
 * 
 * **Stage 2 (Register):**
 *   - Inputs Stage1_Reg.Sub_Result to Log2Exp unit
 *   - Log2Exp outputs 4-bit approximation of log2 transformation
 *   - Result stored as Power in Stage2_Reg
 * 
 * **Stage 3 (Register):**
 *   - Combines Stage2_Reg.Power with ky[i] coefficients
 *   - Computes ky[i] + Power for each of 4 channels
 *   - Stores Mux_Result, ks, and computed ky values
 * 
 * **Stage 4 (Combinational):**
 *   - 4x Divider modules process Stage3_Reg data
 *   - Each divider outputs normalized 16-bit FP16 values
 * 
 * **Final Output:**
 *   - Packs 4 x 16-bit FP16 results into 64-bit Output_Vector
 *   - Format: [Output[3]|Output[2]|Output[1]|Output[0]] (16-bit each)
 * 
 * **Latency**: 5 clock cycles end-to-end
 * 
 * **Use Case**: CNN softmax/normalization: exp(x-max) / sum(exp(x[i]-max))
 * Decomposes exponential division into manageable sub-operations:
 * 1. Compute log2 approximation of base value
 * 2. Add per-channel adjustments
 * 3. Apply division via lookup table (Divider)
 * 4. Accumulate results
 * 
 * **Design Rationale:**
 * Deep pipeline (5 stages) enables:
 * - High clock frequency operation
 * - Multiple iterations per cycle through stages
 * - Complex multi-channel processing without critical path issues
 */
SC_MODULE(PROCESS_3_Module) {
    // ===== Input Ports =====
    sc_in<bool>              clk;                  ///< Clock signal (synchronizes all pipeline registers)
    sc_in<bool>              rst;                  ///< Reset signal (clears all pipeline stages)
    sc_in<sc_uint16>         Local_Max;            ///< 16-bit FP16 local maximum value  
    sc_in<sc_uint16>         Global_Max;           ///< 16-bit FP16 global maximum value
    sc_in<sc_uint4>          ks_In;                ///< 4-bit scale factor from Divider_PreCompute
    sc_in<sc_uint16>         Mux_Result_In;        ///< 16-bit threshold from Divider_PreCompute
    sc_in<sc_uint16>         Output_Buffer_In;     ///< 16-bit pre-computed coefficient buffer
    
    // ===== Output Ports =====
    sc_out<sc_uint64>        Output_Vector;         ///< 64-bit packed output (4 x 16-bit FP16 results)

    // ===== Internal Signals & Modules =====
    
    /** 4 instances of Divider module for parallel division computations */
    Divider_Module           *divider_unit[4];      ///< Array of Divider module pointers
    
    /** Output signals from each Divider module */
    sc_signal<sc_uint16>     Divider_Output[4];     ///< 4 x 16-bit normalized results
    
    /** Log2Exp unit for exponential approximation transformation */
    Log2Exp                  *log2exp_units;       ///< Pointer to Log2Exp module instance
    
    /** 4-bit exponent output from Log2Exp */
    sc_signal<sc_uint4>      log2exp_out;          ///< Log2 transformation result
    
    // ===== Pipeline Stage 1 Signals =====
    // (Stores subtraction results)
    sc_signal<Stage1_Data>   Stage1_Next;          ///< Stage 1 combinational output
    sc_signal<Stage1_Data>   Stage1_Reg;           ///< Stage 1 registered output
    
    // ===== Pipeline Stage 2 Signals =====
    // (Stores exponent results from Log2Exp + Log2Exp input)
    sc_signal<Stage2_Data>   Stage2_Next;          ///< Stage 2 combinational output
    sc_signal<Stage2_Data>   Stage2_Reg;           ///< Stage 2 registered output
    
    // ===== Pipeline Stage 3 Signals =====
    // (Stores division parameters and coefficients)
    sc_signal<Stage3_Data>   Stage3_Next;          ///< Stage 3 combinational output
    sc_signal<Stage3_Data>   Stage3_Reg;           ///< Stage 3 registered output
    
    // ===== Process Methods =====
    
    /**
     * @brief Stage 1 Combinational Logic
     * 
     * Computes FP16 subtraction:
     *   Stage1_Next.Sub_Result = Local_Max - Global_Max
     * 
     * This feeds the Log2Exp unit in the next stage.
     * Triggered by: Local_Max or Global_Max changes
     * Latency: 0 ns (combinational)
     */
    void Stage1_Comb();
    
    /**
     * @brief Stage 2 Combinational Logic
     * 
     * Captures the exponent result from Log2Exp transformation:
     *   Stage2_Next.Power = log2exp_out
     * 
     * The Log2Exp unit processes Stage1_Reg.Sub_Result, and this captures
     * its output for the next pipeline stage.
     * Triggered by: log2exp_out or Stage1_Reg changes
     * Latency: 0 ns (combinational)
     */
    void Stage2_Comb();
    
    /**
     * @brief Stage 3 Combinational Logic
     * 
     * Prepares division parameters by combining:
     * - Mux_Result and ks inputs (routing)
     * - Stage2_Reg.Power + ky[i] computations for each channel
     * 
     * Outputs prepared values to Stage3_Next for pipeline register.
     * This stage computes: ky[i]+Power values used by Divider modules.
     * Triggered by: Stage2_Reg, ks_In, Mux_Result_In, or Output_Buffer_In
     * Latency: 0 ns (combinational)
     */
    void Stage3_Comb();
    
    /**
     * @brief Pipeline Register Update (Sequential on Clock)
     * 
     * Synchronized to rising clock edge, updates all pipeline registers:
     * - Stage1_Reg ← Stage1_Next
     * - Stage2_Reg ← Stage2_Next
     * - Stage3_Reg ← Stage3_Next
     * 
     * On reset (rst=1), clears all stages to default (0) values.
     * This creates the pipelined data flow through 5 stages.
     * Triggered by: clk.pos() (rising clock edge)
     * Latency: 1 clock cycle per update
     */
    void Pipeline_Update();
    
    /**
     * @brief Output Combinational Logic
     * 
     * Packs outputs from the 4 Divider modules into a 64-bit vector:
     *   Output_Vector[15:0]   = Divider_Output[0]
     *   Output_Vector[31:16]  = Divider_Output[1]
     *   Output_Vector[47:32]  = Divider_Output[2]
     *   Output_Vector[63:48]  = Divider_Output[3]
     * 
     * This is the final combinational output stage.
     * Triggered by: Divider_Output[0..3] changes
     * Latency: 0 ns (combinational)
     */
    void Output_Comb();
    
    // ===== Constructor =====
    SC_HAS_PROCESS(PROCESS_3_Module);
    PROCESS_3_Module(sc_core::sc_module_name name) : sc_core::sc_module(name) {
        // Instantiate Log2Exp
        log2exp_units = new Log2Exp("max_unit");
        log2exp_units->fp16_in(Stage1_Reg.Sub_Result);
        log2exp_units->result_out(log2exp_out);
        
        // Instantiate 4 Divider modules
        for (int i = 0; i < 4; i++) {
            std::stringstream ss;
            ss << "log2exp_" << i;
            divider_unit[i] = new Divider_Module(ss.str().c_str());
            divider_unit[i]->ky(Stage3_Reg.ky[i]);
            divider_unit[i]->ks(Stage3_Reg.ks);
            divider_unit[i]->Mux_Result(Stage3_Reg.Mux_Result);
            divider_unit[i]->Divider_Output(Divider_Output[i]);
        }
        
        // Register process methods
        SC_METHOD(Stage1_Comb);
        sensitive << Local_Max << Global_Max;

        SC_METHOD(Stage2_Comb);
        sensitive << Stage1_Reg;
        
        SC_METHOD(Stage3_Comb);
        sensitive << Stage2_Reg << ks_In << Mux_Result_In << Output_Buffer_In;
        
        SC_METHOD(Pipeline_Update);
        sensitive << clk.pos();
        
        SC_METHOD(Output_Comb);
        sensitive << Stage3_Reg;
    }
};

#endif // PROCESS_3_H