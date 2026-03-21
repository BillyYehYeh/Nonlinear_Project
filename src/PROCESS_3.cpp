#include "PROCESS_3.h"
#include "utils.hpp"
#include <cmath>
#include <iostream>
#include <iomanip>

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
    sc_uint16 local_max = Local_Max.read();
    sc_uint16 global_max = Global_Max.read();
    
    stage1_data.Sub_Result = fp16_subtract(local_max, global_max);
    stage1_data.data_valid = data_valid.read();  // Capture input data validity

    Stage1_Next.write(stage1_data);
    
    // ===== DEBUG TRACE: Track FIFO inputs =====
    static int debug_stage1_cnt = 0;
    static int debug_all_cnt = 0;
    debug_all_cnt++;
    
    //if (debug_all_cnt <= 150) {
    //    std::cout << "[PROCESS3_STAGE1_DEBUG] cnt=" << debug_all_cnt
    //              << " data_valid=" << stage1_data.data_valid
    //              << " local_max=0x" << std::hex << std::setfill('0') << std::setw(4) << (int)local_max.to_uint()
    //              << " global_max=0x" << std::setfill('0') << std::setw(4) << (int)global_max.to_uint()
    //              << std::dec << " @" << sc_time_stamp() << std::endl;
    //}
    
    /*if (stage1_data.data_valid) {
        debug_stage1_cnt++;
        if (debug_stage1_cnt <= 110) {
            std::cout << "[PROCESS3_STAGE1] " << debug_stage1_cnt
                      << ": valid=" << stage1_data.data_valid
                      << " @" << sc_time_stamp() << std::endl;
        }
    }*/
}


void PROCESS_3_Module::Stage2_Comb() {
    process3_pipeline::Stage1_Data stage1_data = Stage1_Reg.read();
    process3_pipeline::Stage2_Data stage2_data;
    stage2_data.Power = log2exp_out.read();
    stage2_data.data_valid = stage1_data.data_valid;  // Propagate validity from Stage1
    Stage2_Next.write(stage2_data);

    stage1_valid.write(stage1_data.data_valid);
    
    /*// DEBUG: Track stage2 validity
    if (stage2_data.data_valid) {
        static int s2_cnt = 0;
        s2_cnt++;
        if (s2_cnt <= 110) {
            std::cout << "[PROCESS3_STAGE2] " << s2_cnt << " @" << sc_time_stamp() << std::endl;
        }
    }*/
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
        // Extend to 5-bit for addition, then clamp to 0-15
        sc_uint5 sum = ((sc_uint5)stage2_data.Power) + ((sc_uint5)ky);
        
        sc_uint4 add_result = (sum > 15) ? (sc_uint4)15 : (sc_uint4)sum;
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
        
        process3_pipeline::Stage4_Data reset_data4;
        reset_data4.Output = 0;
        reset_data4.data_valid = false;
        Stage4_Reg.write(reset_data4);
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
        // Stage4_Reg is NOT updated
    }
    else {
        // NORMAL MODE: Update all pipeline stages with next stage data
        Stage1_Reg.write(Stage1_Next.read());
        Stage2_Reg.write(Stage2_Next.read());
        Stage3_Reg.write(Stage3_Next.read());
        Stage4_Reg.write(Stage4_Next.read());
    }
}


void PROCESS_3_Module::Stage4_Comb() {
    process3_pipeline::Stage4_Data stage4_data;
    Stage3_Data stage3_data = Stage3_Reg.read();
    sc_uint64 packed = 0;
    for (int i = 0; i < 4; i++) {
        sc_uint16 divider_out = Divider_Output[i].read();
        sc_uint64 temp = (sc_uint64)(divider_out.to_uint());
        packed = packed | (temp << (i * 16));
    }
    stage4_data.Output = packed;
    stage4_data.data_valid = stage3_data.data_valid;  // Propagate validity from Stage3
    Stage4_Next.write(stage4_data);
    
    // Output stage3_valid flag from Stage3 register (for AXI write control)
    stage3_valid.write(stage3_data.data_valid);
    
    // ===== DEBUG TRACE =====
    /*static int debug_output_cnt = 0;
    if (stage3_data.data_valid && debug_output_cnt < 100) {
        std::cout << "[PROCESS3_OUTPUT] Cycle " << (++debug_output_cnt) 
                  << ": Output_Vector=0x" << std::hex << std::setfill('0') << std::setw(16) 
                  << packed.to_uint64() << std::dec
                  << " Valid=" << stage3_data.data_valid
                  << " @" << sc_time_stamp() << std::endl;
        
        // Also show individual 16-bit outputs
        if (debug_output_cnt <= 5 || debug_output_cnt > 95) {
            std::cout << "    [Details] Divider outputs: ";
            for (int j = 0; j < 4; j++) {
                std::cout << "out[" << j << "]=0x" << std::hex 
                         << std::setfill('0') << std::setw(4) 
                         << Divider_Output[j].read().to_uint() << " ";
            }
            std::cout << std::dec << std::endl;
        }
    }*/
}
void PROCESS_3_Module::Output_Comb() {
    Stage4_Data stage4_data = Stage4_Reg.read();
    Output_Vector.write(stage4_data.Output);
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

// Print all pipeline stage registers on clock edge in yellow
void PROCESS_3_Module::Print_Stage_Regs() {
    if(enable.read())
    {
        const char* YELLOW = "\033[33m";
        const char* RESET  = "\033[0m";

        auto s1 = Stage1_Reg.read();
        auto s2 = Stage2_Reg.read();
        auto s3 = Stage3_Reg.read();
        auto s4 = Stage4_Reg.read();

        std::cerr << YELLOW;
        std::cerr << "[PIPELINE DUMP] (Process3) @" << sc_time_stamp() << "\n";
        std::cerr << " Stage1: " << s1 << "\n";
        std::cerr << " Stage2: " << s2 << "\n";
        std::cerr << " Stage3: " << s3 << "\n";
        std::cerr << " Stage4: " << s4 << "\n";
        //std::cerr << " Output_Vector: " << Output_Vector.read() << "\n";
        std::cerr << RESET << std::flush;
    }
}
