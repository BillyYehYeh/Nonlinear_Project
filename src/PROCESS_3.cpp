#include "PROCESS_3.h"
#include "utils.h"
#include <cmath>

using namespace process3_pipeline;

/**
 * @brief FP16 subtraction implementation
 * 
 * Performs subtraction of two FP16 values using utility function.
 */
static sc_uint16 fp16_subtract(sc_uint16 a_bits, sc_uint16 b_bits) {
    sc_uint16 r = fp16_add((fp16_t)(a_bits.to_uint()), (fp16_t)(b_bits.to_uint()) ^ 0x8000);
    return r;
}


void PROCESS_3_Module::Stage1_Comb() {
    process3_pipeline::Stage1_Data stage1_data;
    stage1_data.Sub_Result = fp16_subtract(Local_Max, Global_Max);
    stage1_data.data_valid = data_valid.read();  // Capture input data validity
    Stage1_Next.write(stage1_data);
}


void PROCESS_3_Module::Stage2_Comb() {
    process3_pipeline::Stage1_Data stage1_data = Stage1_Reg.read();
    process3_pipeline::Stage2_Data stage2_data;
    stage2_data.Power = log2exp_out.read();
    stage2_data.data_valid = stage1_data.data_valid;  // Propagate validity from Stage1
    Stage2_Next.write(stage2_data);
}

void PROCESS_3_Module::Stage3_Comb() {
    process3_pipeline::Stage2_Data stage2_data = Stage2_Reg.read();
    sc_uint16 Mux_Result = Mux_Result_In.read();
    sc_uint4 ks = ks_In.read();
    sc_uint16 ky_4 = Output_Buffer_In.read();

    process3_pipeline::Stage3_Data stage3_data;
    stage3_data.Mux_Result = Mux_Result;
    stage3_data.ks = ks;
    stage3_data.data_valid = stage2_data.data_valid;  // Propagate validity from Stage2
    for (int i = 0; i < 4; i++) {
        sc_uint4 ky = ky_4.range(i*4+3, i*4);
        sc_uint4 add_result = stage2_data.Power + ky;
        stage3_data.ky[i] = add_result;
    }
    
    Stage3_Next.write(stage3_data);
}


void PROCESS_3_Module::Pipeline_Update() {
    if (rst.read()) {
        // Reset all pipeline stages
        process3_pipeline::Stage1_Data reset_data1;
        reset_data1.Sub_Result = 0;
        reset_data1.data_valid = false;
        Stage1_Reg.write(reset_data1);

        process3_pipeline::Stage2_Data reset_data2;
        reset_data2.Power = 0;
        reset_data2.data_valid = false;
        Stage2_Reg.write(reset_data2);

        process3_pipeline::Stage3_Data reset_data3;
        reset_data3.Mux_Result = 0;
        reset_data3.ks = 0;
        reset_data3.data_valid = false;
        for (int i = 0; i < 4; i++) {
            reset_data3.ky[i] = 0;
        }
        Stage3_Reg.write(reset_data3);
    }
    else if (enable.read() == false) {
        // ENABLE LOW: Module disabled, hold all pipeline stages (stall)
        // All pipeline registers maintain current values (no update occurs)
    }
    else if (stall.read() == true) {
        // STALL HIGH: Freeze ALL pipeline stages
        // This prevents data loss when AXI write handshake fails
        // All stages (1, 2, 3) are held and do not update
        // Stage1_Reg is NOT updated
        // Stage2_Reg is NOT updated
        // Stage3_Reg is NOT updated
    }
    else {
        // NORMAL MODE: Update all pipeline stages with next stage data
        Stage1_Reg.write(Stage1_Next.read());
        Stage2_Reg.write(Stage2_Next.read());
        Stage3_Reg.write(Stage3_Next.read());
    }
}


void PROCESS_3_Module::Output_Comb() {
    sc_uint64 packed = 0;
    for (int i = 0; i < 4; i++) {
        sc_uint16 divider_out = Divider_Output[i].read();
        sc_uint64 temp = (sc_uint64)(divider_out.to_uint());
        packed = packed | (temp << (i * 16));
    }
    Output_Vector.write(packed);
    
    // Output stage3_valid flag from Stage3 register (for AXI write control)
    Stage3_Data stage3_data = Stage3_Reg.read();
    stage3_valid.write(stage3_data.data_valid);
}

void PROCESS_3_Module::Extract_Stage1_Data() {
    process3_pipeline::Stage1_Data stage1_data = Stage1_Reg.read();
    // Pass Stage1_Reg.Sub_Result (sc_uint16) directly to Log2Exp input
    stage1_sub_result_sig.write(stage1_data.Sub_Result);
}

void PROCESS_3_Module::Extract_Stage3_Data() {
    process3_pipeline::Stage3_Data stage3_data = Stage3_Reg.read();
    // Convert Stage3_Reg members to correct types for Divider inputs
    // Note: Divider expects sc_uint types, not sc_bv
    stage3_mux_result_sig.write(stage3_data.Mux_Result);
    stage3_ks_sig.write(stage3_data.ks);
    for (int i = 0; i < 4; i++) {
        stage3_ky_sig[i].write(stage3_data.ky[i]);
    }
}
