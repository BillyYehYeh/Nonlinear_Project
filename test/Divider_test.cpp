#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <bitset>
#include <cmath>
#include <limits>
#include "Divider.h"
#include "Divider_PreCompute.h"

// Include the implementations
#include "../src/Divider.cpp"
#include "../src/Divider_PreCompute.cpp"

// Helper function to convert FP16 bits to float
float fp16_to_float(uint16_t bits) {
    int sign = (bits >> 15) & 0x1;
    int exp_bits = (bits >> 10) & 0x1F;
    int mant_bits = bits & 0x3FF;
    
    if (exp_bits == 0 && mant_bits == 0) {
        return 0.0f;
    }
    
    float mantissa = 1.0f;
    if (exp_bits != 0) {
        mantissa = 1.0f + mant_bits / 1024.0f;
    } else {
        mantissa = mant_bits / 1024.0f;
    }
    
    int exponent = exp_bits - 15;
    return (sign ? -1.0f : 1.0f) * std::pow(2.0f, (float)exponent) * mantissa;
}

// Helper function to convert float to FP16
uint16_t float_to_fp16(float value) {
    uint32_t bits = *(uint32_t*)&value;
    uint32_t sign = (bits >> 31) & 0x1;
    uint32_t exp = (bits >> 23) & 0xFF;
    uint32_t mant = bits & 0x7FFFFF;
    
    // Simplified conversion: just use the high 16 bits as approximation
    uint16_t fp16 = (sign << 15) | ((exp - 112) << 10) | (mant >> 13);
    return fp16;
}

SC_MODULE(Divider_TestBench) {
    // Test bench signals
    sc_signal<sc_uint32> input_sig;          // 32-bit input to Divider_PreCompute
    sc_signal<sc_uint4>  ky_sig;             // 4-bit ky input to Divider
    
    // Internal connection signals
    sc_signal<sc_uint4>  leading_one_pos_sig;   // Leading_One_Pos output from PreCompute -> ks input to Divider
    sc_signal<sc_uint16> mux_result_sig;        // Mux_Result output from PreCompute -> Mux_Result input to Divider
    sc_signal<sc_uint16> divider_output_sig;    // Final output from Divider

    Divider_PreCompute_Module *dut_precompute;
    Divider_Module *dut_divider;

    SC_CTOR(Divider_TestBench) {
        // Instantiate Divider_PreCompute module
        dut_precompute = new Divider_PreCompute_Module("Divider_PreCompute_DUT");
        dut_precompute->input(input_sig);
        dut_precompute->Leading_One_Pos(leading_one_pos_sig);
        dut_precompute->Mux_Result(mux_result_sig);
        
        // Instantiate Divider module
        dut_divider = new Divider_Module("Divider_DUT");
        dut_divider->ky(ky_sig);
        dut_divider->ks(leading_one_pos_sig);  // Connect Leading_One_Pos to ks
        dut_divider->Mux_Result(mux_result_sig);  // Connect Mux_Result through
        dut_divider->Divider_Output(divider_output_sig);

        SC_THREAD(test_stimulus);
    }

    void print_binary_16(uint16_t val) {
        std::cout << "Binary: " << std::bitset<16>(val) << std::endl;
    }

    void print_fp16_components(uint16_t val) {
        int sign = (val >> 15) & 0x1;
        int exp_bits = (val >> 10) & 0x1F;
        int mant_bits = val & 0x3FF;
        
        std::cout << "  Sign: " << sign << ", Exponent: " << exp_bits << ", Mantissa: " << mant_bits << std::endl;
        std::cout << "  Float value: " << fp16_to_float(val) << std::endl;
    }

    void print_test_header(int test_num, sc_uint32 input_val, sc_uint4 ky, sc_uint4 ks) {
        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "Test " << test_num << ": ";
        std::cout << "input=0x" << std::hex << std::setw(8) << std::setfill('0') << input_val.to_uint() << std::dec << std::setfill(' ');
        std::cout << ", ky=" << ky.to_uint() << ", ks_computed=" << ks.to_uint() << std::endl;
        std::cout << std::string(120, '=') << std::endl;
    }

    void print_all_signals(sc_uint32 input_val, sc_uint4 ky, sc_uint4 ks, sc_uint16 mux_in, sc_uint16 divider_out, 
                          float qy_golden, float s_golden, float qy_s_golden) {
        int sign_in = (mux_in.to_uint() >> 15) & 0x1;
        int exp_in = (mux_in.to_uint() >> 10) & 0x1F;
        int mant_in = mux_in.to_uint() & 0x3FF;
        
        int sign_out = (divider_out.to_uint() >> 15) & 0x1;
        int exp_out = (divider_out.to_uint() >> 10) & 0x1F;
        int mant_out = divider_out.to_uint() & 0x3FF;
        
        std::cout << "\n[TESTBENCH INPUT (RANDOM GENERATION)]" << std::endl;
        std::cout << "  input_sig (32-bit): 0x" << std::hex << std::setw(8) << std::setfill('0') << input_val.to_uint() << std::dec << std::setfill(' ') << std::endl;
        std::cout << "  ky_sig (4-bit):     " << std::setw(3) << ky.to_uint() << "   (0x" << std::hex << ky.to_uint() << std::dec << ")" << std::endl;
        
        std::cout << "\n[DIVIDER_PRECOMPUTE OUTPUT -> DIVIDER INPUT]" << std::endl;
        std::cout << "  Leading_One_Pos (ks): " << std::setw(3) << ks.to_uint() << "   (0x" << std::hex << ks.to_uint() << std::dec << ")" << std::endl;
        std::cout << "  Mux_Result (16-bit):  0x" << std::hex << std::setw(4) << std::setfill('0') << mux_in.to_uint() << std::dec << std::setfill(' ') << std::endl;
        std::cout << "    └─ Binary:        "; print_binary_16(mux_in.to_uint());
        std::cout << "    └─ Components:    Sign=" << sign_in << ", Exponent=" << exp_in << ", Mantissa=" << mant_in << std::endl;
        std::cout << "    └─ Float value:   " << std::fixed << std::setprecision(6) << fp16_to_float(mux_in.to_uint()) << std::endl;
        
        std::cout << "\n[GOLDEN DATA CALCULATION]" << std::endl;
        uint32_t input_uint = input_val.to_uint();
        uint16_t int_part = (input_uint >> 16) & 0xFFFF;      // bits 31:16 - integer part
        uint16_t dec_part = input_uint & 0xFFFF;              // bits 15:0 - decimal part
        std::cout << "  S = input_sig (32-bit fixed point: 16 bits int + 16 bits decimal)" << std::endl;
        std::cout << "      = 0x" << std::hex << std::setw(8) << std::setfill('0') << input_uint << std::dec << std::setfill(' ') << std::endl;
        std::cout << "      = [Integer: 0x" << std::hex << std::setw(4) << std::setfill('0') << (int)int_part << ", Decimal: 0x" << std::setw(4) << std::setfill('0') << (int)dec_part << "]" << std::dec << std::setfill(' ') << std::endl;
        std::cout << "      = " << (float)int_part << " + " << (float)dec_part << "/65536" << std::endl;
        std::cout << "      = " << std::scientific << std::setprecision(6) << s_golden << std::endl;
        std::cout << "  Qy = 2^(-ky) = 2^(-" << ky.to_uint() << ") = " << std::scientific << std::setprecision(6) << qy_golden << std::endl;
        std::cout << "  Qy/S = " << std::scientific << std::setprecision(6) << qy_s_golden << " (GOLDEN DATA)" << std::endl;
        
        // Internal signals calculation
        int right_shift = ky.to_uint() + ks.to_uint();
        int new_exp_val = exp_in - right_shift;
        if (new_exp_val < 0) {
            new_exp_val = 0;
        } else if (new_exp_val > 31) {
            new_exp_val = 31;
        }
        
        std::cout << "\n[DIVIDER MODULE INTERNAL COMPUTATION]" << std::endl;
        std::cout << "  right_shift = ky + ks = " << ky.to_uint() << " + " << ks.to_uint() << " = " << right_shift << std::endl;
        std::cout << "  Input Exponent (from Mux_Result): " << exp_in << std::endl;
        std::cout << "  new_exp_val = exp - right_shift = " << exp_in << " - " << right_shift << " = " << (exp_in - right_shift);
        if ((exp_in - right_shift) < 0) {
            std::cout << " (saturated to 0 - UNDERFLOW)" << std::endl;
        } else if ((exp_in - right_shift) > 31) {
            std::cout << " (saturated to 31 - OVERFLOW)" << std::endl;
        } else {
            std::cout << std::endl;
        }
        std::cout << "  Final new_exp_val (after saturation): " << new_exp_val << std::endl;
        std::cout << "  Sign (preserved): " << sign_in << std::endl;
        std::cout << "  Mantissa (preserved): " << mant_in << std::endl;
        std::cout << "  output_val components: [Sign=" << sign_in << " | Exponent=" << new_exp_val << " | Mantissa=" << mant_in << "]" << std::endl;
        
        std::cout << "\n[OUTPUT SIGNALS]" << std::endl;
        std::cout << "  Divider_Output (16-bit): 0x" << std::hex << std::setw(4) << std::setfill('0') << divider_out.to_uint() << std::dec << std::setfill(' ') << std::endl;
        std::cout << "    └─ Binary:     "; print_binary_16(divider_out.to_uint());
        std::cout << "    └─ Components: Sign=" << sign_out << ", Exponent=" << exp_out << ", Mantissa=" << mant_out << std::endl;
        std::cout << "    └─ Float value: " << std::scientific << std::setprecision(6) << fp16_to_float(divider_out.to_uint()) << std::endl;
    }

    void test_stimulus() {
        int test_count = 0;
        std::vector<int> test_input_list;
        std::vector<int> test_ky_list;
        std::vector<int> test_ks_list;
        std::vector<float> test_s_list;
        std::vector<float> test_golden_list;
        std::vector<int> test_golden_hex_list;
        std::vector<float> test_output_float_list;
        std::vector<int> test_output_hex_list;
        std::vector<float> error_list;
        std::vector<std::string> overflow_underflow_list;

        std::cout << "\n" << std::string(160, '=') << std::endl;
        std::cout << "DIVIDER_PRECOMPUTE + DIVIDER INTEGRATED MODULE TEST" << std::endl;
        std::cout << "Testing Divider_PreCompute.cpp and Divider.cpp together" << std::endl;
        std::cout << "Connections: Leading_One_Pos (PreCompute) -> ks (Divider)" << std::endl;
        std::cout << "             Mux_Result (PreCompute) -> Mux_Result (Divider)" << std::endl;
        std::cout << "Formula: Qy = 2^(-ky), S = input_sig (32-bit fixed point), Golden Data = Qy/S" << std::endl;
        std::cout << "Constraint: REQUIRES Qy <= S in all test cases" << std::endl;
        std::cout << std::string(160, '=') << std::endl;

        // Helper lambda to run a test case
        auto run_test = [&](sc_uint32 input_val, sc_uint4 ky_val) {
            test_count++;
            
            // Apply inputs to testbench
            input_sig.write(input_val);
            ky_sig.write(ky_val);
            wait(1, SC_NS);
            
            // Read outputs from PreCompute
            sc_uint4 ks_computed = leading_one_pos_sig.read();
            sc_uint16 mux_computed = mux_result_sig.read();
            
            // Calculate golden data using input_sig as S (32-bit fixed point: 16 bits int + 16 bits decimal)
            uint32_t input_uint = input_val.to_uint();
            uint16_t int_part = (input_uint >> 16) & 0xFFFF;      // bits 31:16 - integer part
            uint16_t dec_part = input_uint & 0xFFFF;              // bits 15:0 - decimal part
            float s_golden = (float)int_part + (float)dec_part / 65536.0f;  // S = int_part + dec_part/2^16
            
            float qy_golden = std::pow(2.0f, -(float)ky_val.to_uint());
            float qy_s_golden = qy_golden / s_golden;
            
            print_test_header(test_count, input_val, ky_val, ks_computed);
            
            wait(1, SC_NS);
            
            // Read output from Divider
            uint16_t output = divider_output_sig.read().to_uint();
            float output_float = fp16_to_float(output);
            
            // Check error - absolute difference between output and golden data
            float error = std::abs(output_float - qy_s_golden);
            
            print_all_signals(input_val, ky_val, ks_computed, mux_computed, output, qy_golden, s_golden, qy_s_golden);
            std::cout << "  Error (absolute difference): " << std::scientific << std::setprecision(6) << error << std::endl;
            
            // Determine if overflow/underflow occurred
            int exp_in = (mux_computed.to_uint() >> 10) & 0x1F;
            int right_shift = ky_val.to_uint() + ks_computed.to_uint();
            int new_exp_val = exp_in - right_shift;
            std::string saturation_status = "";
            
            if (new_exp_val < 0) {
                saturation_status = "[UNDERFLOW]";
            } else if (new_exp_val > 31) {
                saturation_status = "[OVERFLOW]";
            }
            
            // Store test information for summary
            test_input_list.push_back(input_val.to_uint());
            test_ky_list.push_back(ky_val.to_uint());
            test_ks_list.push_back(ks_computed.to_uint());
            test_s_list.push_back(s_golden);  // Store the actual s value (input_sig)
            test_golden_list.push_back(qy_s_golden);  // Golden data float value
            test_golden_hex_list.push_back(float_to_fp16(qy_s_golden));  // Golden data as FP16 HEX
            test_output_float_list.push_back(output_float);  // Module output float value
            test_output_hex_list.push_back(output);  // Module output HEX
            error_list.push_back(error);
            overflow_underflow_list.push_back(saturation_status);
        };

        // Test cases with constraint: Qy = 2^(-ky) <= S (input_sig)
        // Total 30 test cases
        std::cout << "\n" << std::string(160, '=') << std::endl;
        std::cout << "TEST CASES (Constraint: Qy*8 = 8*2^(-ky) <= S, Random: input_sig and ky)" << std::endl;
        std::cout << std::string(160, '=') << std::endl;
        
        // Test 1: S=2, ky=2, Qy=0.25, Qy*8=2 (valid: 2<=2)
        run_test(sc_uint32(0x00020000), sc_uint4(2));
        
        // Test 2: S=4, ky=1, Qy=0.5, Qy*8=4 (valid: 4<=4)
        run_test(sc_uint32(0x00040000), sc_uint4(1));
        
        // Test 3: S=4, ky=2, Qy=0.25, Qy*8=2 (valid: 2<=4)
        run_test(sc_uint32(0x00040000), sc_uint4(2));
        
        // Test 4: S=8, ky=0, Qy=1, Qy*8=8 (valid: 8<=8)
        run_test(sc_uint32(0x00080000), sc_uint4(0));
        
        // Test 5: S=8, ky=1, Qy=0.5, Qy*8=4 (valid: 4<=8)
        run_test(sc_uint32(0x00080000), sc_uint4(1));
        
        // Test 6: S=8, ky=2, Qy=0.25, Qy*8=2 (valid: 2<=8)
        run_test(sc_uint32(0x00080000), sc_uint4(2));
        
        // Test 7: S=8, ky=3, Qy=0.125, Qy*8=1 (valid: 1<=8)
        run_test(sc_uint32(0x00080000), sc_uint4(3));
        
        // Test 8: S=16, ky=0, Qy=1, Qy*8=8 (valid: 8<=16)
        run_test(sc_uint32(0x00100000), sc_uint4(0));
        
        // Test 9: S=16, ky=1, Qy=0.5, Qy*8=4 (valid: 4<=16)
        run_test(sc_uint32(0x00100000), sc_uint4(1));
        
        // Test 10: S=16, ky=2, Qy=0.25, Qy*8=2 (valid: 2<=16)
        run_test(sc_uint32(0x00100000), sc_uint4(2));
        
        // Test 11: S=32, ky=0, Qy=1, Qy*8=8 (valid: 8<=32)
        run_test(sc_uint32(0x00200000), sc_uint4(0));
        
        // Test 12: S=32, ky=1, Qy=0.5, Qy*8=4 (valid: 4<=32)
        run_test(sc_uint32(0x00200000), sc_uint4(1));
        
        // Test 13: S=32, ky=2, Qy=0.25, Qy*8=2 (valid: 2<=32)
        run_test(sc_uint32(0x00200000), sc_uint4(2));
        
        // Test 14: S=64, ky=0, Qy=1, Qy*8=8 (valid: 8<=64)
        run_test(sc_uint32(0x00400000), sc_uint4(0));
        
        // Test 15: S=64, ky=1, Qy=0.5, Qy*8=4 (valid: 4<=64)
        run_test(sc_uint32(0x00400000), sc_uint4(1));
        
        // Test 16: S=64, ky=2, Qy=0.25, Qy*8=2 (valid: 2<=64)
        run_test(sc_uint32(0x00400000), sc_uint4(2));
        
        // Test 17: S=128, ky=0, Qy=1, Qy*8=8 (valid: 8<=128)
        run_test(sc_uint32(0x00800000), sc_uint4(0));
        
        // Test 18: S=128, ky=1, Qy=0.5, Qy*8=4 (valid: 4<=128)
        run_test(sc_uint32(0x00800000), sc_uint4(1));
        
        // Test 19: S=256, ky=0, Qy=1, Qy*8=8 (valid: 8<=256)
        run_test(sc_uint32(0x01000000), sc_uint4(0));
        
        // Test 20: S=512, ky=0, Qy=1, Qy*8=8 (valid: 8<=512)
        run_test(sc_uint32(0x02000000), sc_uint4(0));
        
        // Test 21: S=1024, ky=0, Qy=1, Qy*8=8 (valid: 8<=1024)
        run_test(sc_uint32(0x04000000), sc_uint4(0));
        
        // Test 22: S=2048, ky=0, Qy=1, Qy*8=8 (valid: 8<=2048)
        run_test(sc_uint32(0x08000000), sc_uint4(0));
        
        // Test 23: S=4096, ky=0, Qy=1, Qy*8=8 (valid: 8<=4096)
        run_test(sc_uint32(0x10000000), sc_uint4(0));
        
        // Test 24: S=0.5, ky=4, Qy=0.0625, Qy*8=0.5 (valid: 0.5<=0.5)
        run_test(sc_uint32(0x00008000), sc_uint4(4));
        
        // Test 25: S=1, ky=3, Qy=0.125, Qy*8=1 (valid: 1<=1)
        run_test(sc_uint32(0x00010000), sc_uint4(3));
        
        // Test 26: S=2, ky=3, Qy=0.125, Qy*8=1 (valid: 1<=2)
        run_test(sc_uint32(0x00020000), sc_uint4(3));
        
        // Test 27: S=4, ky=3, Qy=0.125, Qy*8=1 (valid: 1<=4)
        run_test(sc_uint32(0x00040000), sc_uint4(3));
        
        // Test 28: S=8, ky=4, Qy=0.0625, Qy*8=0.5 (valid: 0.5<=8)
        run_test(sc_uint32(0x00080000), sc_uint4(4));
        
        // Test 29: S=16, ky=3, Qy=0.125, Qy*8=1 (valid: 1<=16)
        run_test(sc_uint32(0x00100000), sc_uint4(3));
        
        // Test 30: S=32, ky=3, Qy=0.125, Qy*8=1 (valid: 1<=32)
        run_test(sc_uint32(0x00200000), sc_uint4(3));

        // Calculate averages (kept for reference but not used in new summary)
        float total_error = 0.0f;
        float total_error_no_saturation = 0.0f;
        int count_no_saturation = 0;
        
        for (int i = 0; i < test_count; i++) {
            total_error += error_list[i];
            if (overflow_underflow_list[i].empty()) {
                total_error_no_saturation += error_list[i];
                count_no_saturation++;
            }
        }

        // Print summary table with enhanced information
        std::cout << "\n" << std::string(220, '=') << std::endl;
        std::cout << "TEST SUMMARY - DIVIDER_PRECOMPUTE + DIVIDER INTEGRATED TEST" << std::endl;
        std::cout << "Random Data: ☆ input_sig (32-bit fixed point), ☆ ky (4-bit parameter)" << std::endl;
        std::cout << "Constraint: Qy*8 = 8*2^(-ky) <= S (golden data = Qy/S)" << std::endl;
        std::cout << std::string(220, '=') << std::endl;
        
        std::cout << std::setw(4) << "Test"
                  << "│ ☆Input(HEX)" << std::setw(14) << "│ Input(float)"
                  << "│ ☆ky" << std::setw(3) << "│ ks"
                  << "│ Golden(float)" << std::setw(9) << "│ Golden(HEX)"
                  << "│ Output(float)" << std::setw(9) << "│ Output(HEX)"
                  << "│ Error" << std::endl;
        std::cout << std::string(220, '-') << std::endl;
        
        float max_error_no_underflow = 0.0f;
        float max_error_with_underflow = 0.0f;
        bool has_underflow_errors = false;
        
        for (int i = 0; i < test_count; i++) {
            // Convert input hex to fixed-point float
            uint32_t input_uint = test_input_list[i];
            uint16_t int_part = (input_uint >> 16) & 0xFFFF;
            uint16_t dec_part = input_uint & 0xFFFF;
            float input_float = (float)int_part + (float)dec_part / 65536.0f;
            
            std::cout << std::setw(4) << (i + 1)
                      << "│ 0x" << std::hex << std::setw(8) << std::setfill('0') << test_input_list[i] << std::dec << std::setfill(' ')
                      << "│ " << std::setw(12) << std::fixed << std::setprecision(4) << input_float
                      << "│ " << std::setw(3) << test_ky_list[i]
                      << "│ " << std::setw(2) << test_ks_list[i]
                      << "│ " << std::setw(12) << std::scientific << std::setprecision(3) << test_golden_list[i]
                      << "│ 0x" << std::hex << std::setw(8) << std::setfill('0') << test_golden_hex_list[i] << std::dec << std::setfill(' ')
                      << "│ " << std::setw(12) << std::scientific << std::setprecision(3) << test_output_float_list[i]
                      << "│ 0x" << std::hex << std::setw(8) << std::setfill('0') << test_output_hex_list[i] << std::dec << std::setfill(' ')
                      << "│ " << std::setw(10) << std::scientific << std::setprecision(2) << error_list[i]
                      << std::endl;
            
            // Track max errors
            if (overflow_underflow_list[i].empty()) {
                max_error_no_underflow = std::max(max_error_no_underflow, error_list[i]);
            } else {
                max_error_with_underflow = std::max(max_error_with_underflow, error_list[i]);
                has_underflow_errors = true;
            }
        }
        
        // Calculate min error for non-underflow cases
        float min_error_no_underflow = std::numeric_limits<float>::max();
        float min_error_with_underflow = std::numeric_limits<float>::max();
        
        for (int i = 0; i < test_count; i++) {
            if (overflow_underflow_list[i].empty()) {
                min_error_no_underflow = std::min(min_error_no_underflow, error_list[i]);
            } else {
                min_error_with_underflow = std::min(min_error_with_underflow, error_list[i]);
            }
        }
        
        std::cout << std::string(220, '=') << std::endl;
        std::cout << "ERROR STATISTICS" << std::endl;
        std::cout << std::string(220, '=') << std::endl;
        std::cout << "Max Error (without underflow):  " << std::scientific << std::setprecision(6) << max_error_no_underflow << std::endl;
        std::cout << "Min Error (without underflow):  " << std::scientific << std::setprecision(6) << (min_error_no_underflow == std::numeric_limits<float>::max() ? 0.0f : min_error_no_underflow) << std::endl;
        if (has_underflow_errors) {
            std::cout << "Max Error (with underflow):    " << std::scientific << std::setprecision(6) << max_error_with_underflow << std::endl;
            std::cout << "Min Error (with underflow):    " << std::scientific << std::setprecision(6) << (min_error_with_underflow == std::numeric_limits<float>::max() ? 0.0f : min_error_with_underflow) << std::endl;
        }
        std::cout << std::string(220, '=') << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    Divider_TestBench tb("Divider_TestBench");
    sc_start();
    return 0;
}
