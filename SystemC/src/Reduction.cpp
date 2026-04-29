#include "Reduction.h"
#include <cmath>
#include <iomanip>

/**
 * @brief Compute exponential values for each of the 4 inputs
 * 
 * Extracts 4 signed 4-bit values from the 16-bit Input_Vector and computes
 * 2^(-x_i) for each. Results are stored in exp_out[] signals.
 * 
 * Input packing: bits [3:0]=x0, [7:4]=x1, [11:8]=x2, [15:12]=x3
 */
void Reduction_Module::compute_exponentials() {
    sc_dt::sc_uint<16> input_vec = Input_Vector.read();
    
    // Extract 4 signed 4-bit values from the 16-bit input
    for (int i = 0; i < 4; i++) {
        sc_dt::sc_uint<4> power = input_vec.range(i*4 + 3, i*4);
    
        // Compute 2^(-x_i) and store in exp_out[i]
        sc_uint32 exp_value = 0x00010000 >> power;
        exp_out[i].write(exp_value);
    }
}

/**
 * @brief Stage 1: Parallel binary additions
 * 
 * Performs two parallel 32-bit additions:
 * - stage1_add0 = exp_out[0] + exp_out[1]
 * - stage1_add1 = exp_out[2] + exp_out[3]
 */
void Reduction_Module::stage1_additions() {
    sc_uint32 exp0 = exp_out[0].read();
    sc_uint32 exp1 = exp_out[1].read();
    sc_uint32 exp2 = exp_out[2].read();
    sc_uint32 exp3 = exp_out[3].read();
    
    // Perform two parallel additions
    stage1_add0.write(exp0 + exp1);
    stage1_add1.write(exp2 + exp3);
}

/**
 * @brief Pipeline Register Update (Timing Optimization)
 * 
 * On positive clock edge, captures stage 1 addition results into
 * pipeline registers for timing optimization between stages.
 * Resets on rst signal.
 */
void Reduction_Module::pipeline_register_update() {
    if (rst.read()) {
        pipe_add0_reg.write(0);
        pipe_add1_reg.write(0);
    } else {
        pipe_add0_reg.write(stage1_add0.read());
        pipe_add1_reg.write(stage1_add1.read());
    }
}

/**
 * @brief Stage 2: Final binary addition
 * 
 * Performs the final 32-bit addition to reduce the two intermediate
 * sums to a single output.
 * 
 * final_sum = pipe_add0_reg + pipe_add1_reg
 */
void Reduction_Module::stage2_addition() {
    sc_uint32 add0 = pipe_add0_reg.read();
    sc_uint32 add1 = pipe_add1_reg.read();
    
    final_sum.write(add0 + add1);
}

/**
 * @brief Update Output Port
 * 
 * Propagates the final sum to the output port.
 */
void Reduction_Module::update_output() {
    Output_Sum.write(final_sum.read());
}
