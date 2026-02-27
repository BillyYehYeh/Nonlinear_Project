#include "PROCESS_2.h"
#include "utils.h"
#include <cmath>

/**
 * @brief Input Combinational Logic
 * 
 * Routes the input port Pre_Compute_In to the internal signal Pre_Compute_In_Signal
 * which drives the embedded Divider_PreCompute_Module.
 * 
 * This is a simple combinational pass-through path.
 * 
 * Triggered by: Pre_Compute_In port changes
 * Latency: 0 ns (combinational)
 */
void PROCESS_2_Module::Input_Comb() {
    Pre_Compute_In_Signal.write(Pre_Compute_In.read());
}

/**
 * @brief Output Combinational Logic
 * 
 * Routes the stall registers (Output_Reg_Lo_Pos and Output_Reg_Mux_Result)
 * to the module output ports (Leading_One_Pos_Out and Mux_Result_Out).
 * 
 * This is a simple combinational pass-through from register to port.
 * 
 * Triggered by: Output register changes
 * Latency: 0 ns (combinational)
 */
void PROCESS_2_Module::Output_Comb() {
    Leading_One_Pos_Out.write(Output_Reg_Lo_Pos.read());
    Mux_Result_Out.write(Output_Reg_Mux_Result.read());
}

/**
 * @brief Stall Control Logic (Sequential Process)
 * 
 * Controls when the output registers update based on the stall_output signal.
 * This mechanism allows pipeline control where outputs can be held at their
 * current values while downstream stages catch up.
 * 
 * **Operation:**
 * - If rst=1: Force all registers to 0 (reset state)
 * - Else if stall_output=0: Sample new values from Divider_PreCompute outputs
 *   (registers continuously update with new data)
 * - Else (stall_output=1): Registers retain their current values unchanged
 *   (pipeline hold - prevents further propagation of data)
 * 
 * **Example Timeline:**
 * Cycle 0: Input=0x00010000, Stall=0 → Reg=1, Output=1
 * Cycle 1: Input=0x00020000, Stall=1 → Reg=1 (unchanged), Output=1
 * Cycle 2: Input=0x00040000, Stall=1 → Reg=1 (unchanged), Output=1
 * Cycle 3: Input=0x00040000, Stall=0 → Reg=2, Output=2
 * 
 * Triggered by: clk edges, rst signal, or Divider output changes
 * Latency: Guarded by stall signal (0 or 1 cycle delay)
 */
void PROCESS_2_Module::Output_Stall() {
    if (rst.read()) {
        // RESET: Force all output registers to initial state (0)
        Output_Reg_Lo_Pos.write(0);
        Output_Reg_Mux_Result.write(0);
    } 
    else if (stall_output.read() == false) {
        // NO STALL: Update registers with new values from Divider_PreCompute
        // stall_output=0 allows new data to propagate through the pipeline
        Output_Reg_Lo_Pos.write(Leading_One_Pos_Out_Signal.read());
        Output_Reg_Mux_Result.write(Mux_Result_Out_Signal.read());
    }
    // STALL: else condition - registers maintain current values (no write occurs)
    // This prevents data propagation downstream when pipeline needs to be held
}

