#include "PROCESS_1.h"
#include "utils.h"
#include <cmath>

/**
 * @brief FP16 subtraction implementation
 * 
 * Performs subtraction of two FP16 values using utility function.
 */
sc_uint16 fp16_subtract(sc_uint16 a_bits, sc_uint16 b_bits) {
    sc_uint16 r = fp16_add((fp16_t)(a_bits.to_uint()), (fp16_t)(b_bits.to_uint()) ^ 0x8000);
    return r;
}

/**
 * @brief Unpack 64-bit input into 4 fp16 values (Combinational)
 * 
 * Splits the 64-bit input into 4 16-bit fp16 values:
 * - DataIn_unpacked[0] = bits [15:0]
 * - DataIn_unpacked[1] = bits [31:16]
 * - DataIn_unpacked[2] = bits [47:32]
 * - DataIn_unpacked[3] = bits [63:48]
 */
void PROCESS_1_Module::unpack_input() {
    sc_uint64 input = DataIn_64bits.read();
    
    for (int i = 0; i < 4; i++) {
        sc_uint16 unpacked = (input >> (i * 16)) & 0xFFFF;
        DataIn_unpacked[i].write(unpacked);
    }
}

/**
 * @brief Stage 1 Combinational Logic
 * 
 * Captures 4 fp16 input values from unpacked input.
 * Generates Stage1_Next (Stage1_Data structure).
 */
void PROCESS_1_Module::Stage1_Comb() {
    Stage1_Data stage1_data;
    for (int i = 0; i < 4; i++) {
        stage1_data.DataIn[i] = DataIn_unpacked[i].read();
    }
    
    Stage1_Next.write(stage1_data);
}

/**
 * @brief Stage 2 Combinational Logic
 * 
 * Combines Maximum value from MaxUnit with DataIn[4] values.
 * Generates Stage2_Next (Stage2_Data structure).
 */
void PROCESS_1_Module::Stage2_Comb() {
    Stage1_Data stage1_data = Stage1_Reg.read();
    Stage2_Data stage2_data;
    
    // Copy DataIn values from Stage1_Reg
    for (int i = 0; i < 4; i++) {
        stage2_data.DataIn[i] = stage1_data.DataIn[i];
    }
    
    // Copy Max_Out from MaxUnit
    stage2_data.Max_Out = Max_Out_comb.read();
    
    Stage2_Next.write(stage2_data);
    
    // Output Local Maximum to Max Buffer
    Local_Max_Output.write(Max_Out_comb.read());
}

/**
 * @brief Stage 3 Combinational Logic
 * 
 * Computes 5 fp16 differences:
 * - diff[0..3]: DataIn[i] - Max_Out
 * - diff[4]: Max_Out - Global_Max
 * Generates Stage3_Next (Stage3_Data structure).
 */
void PROCESS_1_Module::Stage3_Comb() {
    Stage2_Data stage2_data = Stage2_Reg.read();
    sc_uint16 global_max = Global_Max.read();
    Stage3_Data stage3_data;
    
    // Compute 4 differences: DataIn[i] - Max_Out
    for (int i = 0; i < 4; i++) {
        sc_uint16 datain = stage2_data.DataIn[i];
        stage3_data.diff[i] = fp16_subtract(datain, stage2_data.Max_Out);
    }
    
    // Compute Max_Out - Global_Max
    stage3_data.diff[4] = fp16_subtract(stage2_data.Max_Out, global_max);
    
    Stage3_Next.write(stage3_data);
}

/**
 * @brief Stage 4 Combinational Logic 
 * 
 * Routes the 5 fp16 differences from Stage3_Reg to Log2Exp module inputs.
 * Collects 5 4-bit outputs from Log2Exp modules.
 * Generates Stage4_Next (Stage4_Data structure with Log2Exp outputs).
 */
void PROCESS_1_Module::Stage4_Comb() {
    
    // Route each fp16 difference to the corresponding Log2Exp input
    Stage3_Data stage3_data = Stage3_Reg.read();
    for (int i = 0; i < 5; i++) {
        log2exp_in[i].write(stage3_data.diff[i]);
    }
    
    // Note: Log2Exp outputs are combinational, so we read them here for Stage4_Next
    Stage4_Data stage4_data;
    for (int i = 0; i < 5; i++) {
        stage4_data.power[i] = log2exp_out[i].read();
    }
    
    Stage4_Next.write(stage4_data);
}

/**
 * @brief Stage 5 Combinational Logic 
 * 
 * Packs the 4 4-bit exponents (from power[0..3]) into a 16-bit vector:
 * - bits [3:0]   = power[0]
 * - bits [7:4]   = power[1]
 * - bits [11:8]  = power[2]
 * - bits [15:12] = power[3]
 * 
 * Connects packed power vector to Reduction module input.
 * Extracts the 5th 4-bit value (power[4]) as right shift amount.
 * Generates Stage5_Next (Stage5_Data structure).
 */
void PROCESS_1_Module::Stage5_Comb() {
    Stage4_Data stage4_data = Stage4_Reg.read();

    // Pack 4 4-bit values into 16-bit power vector
    sc_uint16 packed = 0;
    for (int i = 0; i < 4; i++) {
        sc_uint16 temp = (sc_uint16)(stage4_data.power[i].to_uint());
        packed = packed | (temp << (i * 4));
    }

    // Connect packed power vector to Reduction module input
    Reduction_In.write(packed);
    
    // Extract the 5th 4-bit value as right shift amount
    Stage5_Data stage5_data;
    stage5_data.Power_of_Two_Vector = packed;
    stage5_data.Right_Shift_Num = stage4_data.power[4];
    
    Stage5_Next.write(stage5_data);
}

/**
 * @brief Pipeline Update (Sequential)
 * 
 * Updates all pipeline registers on positive clock edge:
 * Stage1_Reg <= Stage1_Next
 * Stage2_Reg <= Stage2_Next
 * Stage3_Reg <= Stage3_Next
 * Stage4_Reg <= Stage4_Next
 * Stage5_Reg <= Stage5_Next
 */
void PROCESS_1_Module::Pipeline_Update() {
    if (rst.read()) {
        // Reset stage 1 Pipline
        Stage1_Data reset_data1;
        for (int i = 0; i < 4; i++) {
            reset_data1.DataIn[i] = 0;
        }
        Stage1_Reg.write(reset_data1);
        // Reset stage 2 Pipline
        Stage2_Data reset_data2;
        reset_data2.Max_Out = 0;
        for (int i = 0; i < 4; i++) {
            reset_data2.DataIn[i] = 0;
        }
        Stage2_Reg.write(reset_data2);
        // Reset stage 3 Pipline
        Stage3_Data reset_data3;
        for (int i = 0; i < 5; i++) {
            reset_data3.diff[i] = 0;
        }
        Stage3_Reg.write(reset_data3);
        // Reset stage 4 Pipline
        Stage4_Data reset_data4;
        for (int i = 0; i < 5; i++) {
            reset_data4.power[i] = 0;
        }
        Stage4_Reg.write(reset_data4);
        // Reset stage 5 Pipline
        Stage5_Data reset_data5;
        reset_data5.Power_of_Two_Vector = 0;
        reset_data5.Right_Shift_Num = 0;
        Stage5_Reg.write(reset_data5);
    } else {
        // Update pipeline registers
        Stage1_Reg.write(Stage1_Next.read());
        Stage2_Reg.write(Stage2_Next.read());
        Stage3_Reg.write(Stage3_Next.read());
        Stage4_Reg.write(Stage4_Next.read());
        Stage5_Reg.write(Stage5_Next.read());
    }
}

/**
 * @brief Output Combinational Logic
 * 
 * Generates final outputs:
 * 1. Power_of_Two_Vector: From Stage5_Reg
 * 2. Sum_Buffer_Update: (Sum_Buffer_In >> Right_Shift_Num) + Reduction_Output
 */
void PROCESS_1_Module::Output_Comb() {
    Stage5_Data stage5_data = Stage5_Reg.read();
    
    // Output 1: Power_of_Two_Vector
    Power_of_Two_Vector.write(stage5_data.Power_of_Two_Vector);
    
    // Output 2: Sum_Buffer_Update
    sc_uint32 sum_buffer_in = Sum_Buffer_In.read();
    sc_uint32 reduction_output = Reduction_Output.read();
    uint32_t shift_val = stage5_data.Right_Shift_Num.to_uint();
    
    // Perform right shift
    sc_uint32 shifted_value = sum_buffer_in >> shift_val;
    
    // Add the reduction output
    sc_uint32 final_result = shifted_value + reduction_output;
    
    Sum_Buffer_Update.write(final_result);
}

