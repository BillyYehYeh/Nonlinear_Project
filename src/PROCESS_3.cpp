#include "PROCESS_3.h"
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


void PROCESS_1_Module::Stage1_Comb() {
    Stage1_Data stage1_data;
    stage1_data.Sub_Result = fp16_subtract(Local_Max, Global_Max);
    Stage1_Next.write(stage1_data);
}


void PROCESS_1_Module::Stage2_Comb() {
    Stage2_Data stage2_data;
    stage2_data.Power = log2exp_out.read();
    Stage2_Next.write(stage2_data);
}

void PROCESS_1_Module::Stage3_Comb() {
    Stage2_Data stage2_data = Stage2_Reg.read();
    sc_uint16 Mux_Result = Mux_Result_In.read();
    sc_uint4 ks = ks_In.read();
    sc_uint16 ky_4 = Output_Buffer_In.read();

    Stage3_Data stage3_data;
    stage3_data.Mux_Result = Mux_Result;
    stage3_data.ks = ks;
    for (int i = 0; i < 4; i++) {
        sc_uint4 ky = ky_4.range(i*4+3, i*4);
        sc_uint4 add_result = stage2_data.Power + ky;
        stage3_data.ky[i].write(add_result)
    }
    
    Stage3_Next.write(stage3_data);
}


void PROCESS_1_Module::Pipeline_Update() {
    if (rst.read()) {
        // Reset stage 1 Pipline
        Stage1_Data reset_data1;
        reset_data1.Sub_Result = 0;
        Stage1_Reg.write(reset_data1);

        // Reset stage 2 Pipline
        Stage2_Data reset_data2;
        reset_data2.Power = 0;
        Stage2_Reg.write(reset_data2);

        // Reset stage 3 Pipline
        Stage3_Data reset_data3;
        reset_data3.Mux_Result = 0;
        reset_data3.ks = 0;
        for (int i = 0; i < 4; i++) {
            reset_data3.ky[i] = 0;
        }
        Stage3_Reg.write(reset_data3);
    } else {
        // Update pipeline registers
        Stage1_Reg.write(Stage1_Next.read());
        Stage2_Reg.write(Stage2_Next.read());
        Stage3_Reg.write(Stage3_Next.read());
    }
}


void PROCESS_1_Module::Output_Comb() {
    sc_uint64 packed = 0;
    for (int i = 0; i < 4; i++) {
        sc_uint64 temp = (sc_uint64)(Divider_Output[i].to_uint());
        packed = packed | (temp << (i * 16));
    }
    Output_Vector.write(packed);
}

