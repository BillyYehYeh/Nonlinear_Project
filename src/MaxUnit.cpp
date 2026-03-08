#include "MaxUnit.h"
#include "utils.h"

/**
 * @brief Stage 1 Combinational Logic - Parallel FP16 Comparisons
 * 
 * Compares A vs B and C vs D in parallel using fp16_max function.
 * Results are stored in R1_next and R2_next, which will be latched
 * on the next clock edge.
 */
void MaxUnit::stage1_comb_logic() {
    R1_next.write(fp16_max(A.read(), B.read()));
    R2_next.write(fp16_max(C.read(), D.read()));
}

/**
 * @brief Stage 1 Register Update - Clock-driven Update
 * 
 * Updates pipeline registers R1_reg and R2_reg on positive clock edge.
 * On reset (rst=1), registers are cleared to 0.
 * Otherwise, registers capture the combinational logic outputs.
 */
void MaxUnit::stage1_register_update() {
    if (rst.read()) {   // Reset
        R1_reg.write(0);
        R2_reg.write(0);
    } else { 
        R1_reg.write(R1_next.read());
        R2_reg.write(R2_next.read());
    }
}

/**
 * @brief Stage 2 Combinational Logic - Final Comparison
 * 
 * Compares the two pipeline register values to produce the final maximum.
 * No latching occurs at this stage - output is combinational.
 */
void MaxUnit::stage2_comb_logic() {
    Max_Out.write(fp16_max(R1_reg.read(), R2_reg.read()));
}
