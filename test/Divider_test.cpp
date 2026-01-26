#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <bitset>
#include <cmath>
#include "Divider.h"

// Include the implementation from Divider.cpp
#include "../src/Divider.cpp"

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
    sc_signal<sc_uint4>  ky_sig;
    sc_signal<sc_uint4>  ks_sig;
    sc_signal<sc_uint16> mux_result_sig;
    sc_signal<sc_uint16> divider_output_sig;

    Divider_Module *dut;

    SC_CTOR(Divider_TestBench) {
        dut = new Divider_Module("Divider_DUT");
        dut->ky(ky_sig);
        dut->ks(ks_sig);
        dut->Mux_Result(mux_result_sig);
        dut->Divider_Output(divider_output_sig);

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

    void print_test_header(int test_num, sc_uint4 ky, sc_uint4 ks, float s, sc_uint16 mux, 
                          float qy_golden, float s_golden, float qy_s_golden) {
        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "Test " << test_num << ": ";
        std::cout << "ky=" << ky.to_uint() << ", ks=" << ks.to_uint() << ", s=" << std::fixed << std::setprecision(3) << s;
        std::cout << ", Mux_Result=0x" << std::hex << mux.to_uint() << std::dec << std::endl;
        std::cout << std::string(120, '=') << std::endl;
    }

    void print_all_signals(sc_uint4 ky, sc_uint4 ks, float s, sc_uint16 mux_in, sc_uint16 divider_out, 
                          float qy_golden, float s_golden, float qy_s_golden) {
        int sign_in = (mux_in.to_uint() >> 15) & 0x1;
        int exp_in = (mux_in.to_uint() >> 10) & 0x1F;
        int mant_in = mux_in.to_uint() & 0x3FF;
        
        int sign_out = (divider_out.to_uint() >> 15) & 0x1;
        int exp_out = (divider_out.to_uint() >> 10) & 0x1F;
        int mant_out = divider_out.to_uint() & 0x3FF;
        
        std::cout << "\n[INPUT SIGNALS]" << std::endl;
        std::cout << "  ky (4-bit):        " << std::setw(3) << ky.to_uint() << "   (0x" << std::hex << ky.to_uint() << std::dec << ")" << std::endl;
        std::cout << "  ks (4-bit):        " << std::setw(3) << ks.to_uint() << "   (0x" << std::hex << ks.to_uint() << std::dec << ")" << std::endl;
        std::cout << "  s (4-bit parameter): " << std::fixed << std::setprecision(3) << s << std::endl;
        std::cout << "  Mux_Result (16-bit): 0x" << std::hex << std::setw(4) << std::setfill('0') << mux_in.to_uint() << std::dec << std::setfill(' ') << std::endl;
        std::cout << "    └─ Binary:     "; print_binary_16(mux_in.to_uint());
        std::cout << "    └─ Components: Sign=" << sign_in << ", Exponent=" << exp_in << ", Mantissa=" << mant_in << std::endl;
        std::cout << "    └─ Float value (1+s): " << std::fixed << std::setprecision(6) << fp16_to_float(mux_in.to_uint()) << std::endl;
        
        std::cout << "\n[GOLDEN DATA CALCULATION]" << std::endl;
        std::cout << "  Qy = 2^(-ky) = 2^(-" << ky.to_uint() << ") = " << std::scientific << std::setprecision(6) << qy_golden << std::endl;
        std::cout << "  S = (2^ks) * (1 + s) = (2^" << ks.to_uint() << ") * (1 + " << std::fixed << std::setprecision(3) << s << ")" << std::endl;
        std::cout << "      = " << std::scientific << std::setprecision(6) << s_golden << std::endl;
        std::cout << "  Qy/S = " << std::scientific << std::setprecision(6) << qy_s_golden << " (GOLDEN DATA)" << std::endl;
        
        // Internal signals calculation
        int right_shift = ky.to_uint() + ks.to_uint();
        int new_exp_val = exp_in - right_shift;
        if (new_exp_val < 0) {
            new_exp_val = 0;
        } else if (new_exp_val > 31) {
            new_exp_val = 31;
        }
        
        std::cout << "\n[INTERNAL SIGNALS - MODULE COMPUTATION]" << std::endl;
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
        std::vector<int> test_ky_list;
        std::vector<int> test_ks_list;
        std::vector<float> test_s_list;
        std::vector<float> error_list;
        std::vector<std::string> overflow_underflow_list;

        std::cout << "\n" << std::string(140, '=') << std::endl;
        std::cout << "DIVIDER MODULE TEST - COMPREHENSIVE TEST SUITE" << std::endl;
        std::cout << "Testing ky and ks combinations (0-15 range for both 4-bit inputs)" << std::endl;
        std::cout << "Formula: Qy = 2^(-ky), S = (2^ks) * (1 + s), Golden Data = Qy/S" << std::endl;
        std::cout << std::string(140, '=') << std::endl;

        // Helper lambda to run a test case
        auto run_test = [&](sc_uint4 ky_val, sc_uint4 ks_val, float s_val) {
            test_count++;
            
            // Determine Mux_Result based on s value
            sc_uint16 mux = (s_val < 0.5f) ? sc_uint16(0x388B) : sc_uint16(0x3A8B);
            
            // Calculate golden data
            float qy_golden = std::pow(2.0f, -(float)ky_val.to_uint());
            float s_golden = std::pow(2.0f, (float)ks_val.to_uint()) * (1.0f + s_val);
            float qy_s_golden = qy_golden / s_golden;
            
            print_test_header(test_count, ky_val, ks_val, s_val, mux, qy_golden, s_golden, qy_s_golden);
            
            ky_sig.write(ky_val);
            ks_sig.write(ks_val);
            mux_result_sig.write(mux);
            wait(1, SC_NS);
            
            uint16_t output = divider_output_sig.read().to_uint();
            float output_float = fp16_to_float(output);
            
            // Check error percentage
            float error = std::abs((output_float - qy_s_golden) / qy_s_golden) * 100.0f;
            
            print_all_signals(ky_val, ks_val, s_val, mux, output, qy_golden, s_golden, qy_s_golden);
            std::cout << "  Error percentage: " << std::fixed << std::setprecision(2) << error << "%" << std::endl;
            
            // Determine if overflow/underflow occurred
            int exp_in = (mux.to_uint() >> 10) & 0x1F;
            int right_shift = ky_val.to_uint() + ks_val.to_uint();
            int new_exp_val = exp_in - right_shift;
            std::string saturation_status = "";
            
            if (new_exp_val < 0) {
                saturation_status = "[UNDERFLOW]";
            } else if (new_exp_val > 31) {
                saturation_status = "[OVERFLOW]";
            }
            
            // Store test information for summary
            test_ky_list.push_back(ky_val.to_uint());
            test_ks_list.push_back(ks_val.to_uint());
            test_s_list.push_back(s_val);
            error_list.push_back(error);
            overflow_underflow_list.push_back(saturation_status);
        };

        // Edge cases and important combinations
        std::cout << "\n" << std::string(140, '=') << std::endl;
        std::cout << "EDGE CASES (ky=0 or ks=0)" << std::endl;
        std::cout << std::string(140, '=') << std::endl;
        
        run_test(sc_uint4(0), sc_uint4(0), 0.3f);    // ky=0, ks=0, s<0.5
        run_test(sc_uint4(0), sc_uint4(1), 0.6f);    // ky=0, ks=1, s>=0.5
        run_test(sc_uint4(1), sc_uint4(0), 0.4f);    // ky=1, ks=0, s<0.5
        run_test(sc_uint4(2), sc_uint4(0), 0.7f);    // ky=2, ks=0, s>=0.5

        std::cout << "\n" << std::string(140, '=') << std::endl;
        std::cout << "SMALL VALUES (ky, ks = 1-3)" << std::endl;
        std::cout << std::string(140, '=') << std::endl;
        
        run_test(sc_uint4(1), sc_uint4(1), 0.25f);   // ky=1, ks=1, s<0.5
        run_test(sc_uint4(1), sc_uint4(2), 0.5f);    // ky=1, ks=2, s=0.5
        run_test(sc_uint4(2), sc_uint4(1), 0.45f);   // ky=2, ks=1, s<0.5
        run_test(sc_uint4(2), sc_uint4(2), 0.75f);   // ky=2, ks=2, s>=0.5
        run_test(sc_uint4(3), sc_uint4(1), 0.1f);    // ky=3, ks=1, s<0.5
        run_test(sc_uint4(3), sc_uint4(3), 0.6f);    // ky=3, ks=3, s>=0.5

        std::cout << "\n" << std::string(140, '=') << std::endl;
        std::cout << "MEDIUM VALUES (ky, ks = 4-7)" << std::endl;
        std::cout << std::string(140, '=') << std::endl;
        
        run_test(sc_uint4(4), sc_uint4(4), 0.2f);    // ky=4, ks=4, s<0.5
        run_test(sc_uint4(5), sc_uint4(3), 0.8f);    // ky=5, ks=3, s>=0.5
        run_test(sc_uint4(6), sc_uint4(5), 0.35f);   // ky=6, ks=5, s<0.5
        run_test(sc_uint4(7), sc_uint4(6), 0.65f);   // ky=7, ks=6, s>=0.5

        std::cout << "\n" << std::string(140, '=') << std::endl;
        std::cout << "LARGE VALUES (ky, ks = 8-15)" << std::endl;
        std::cout << std::string(140, '=') << std::endl;
        
        run_test(sc_uint4(8), sc_uint4(7), 0.15f);   // ky=8, ks=7, s<0.5
        run_test(sc_uint4(9), sc_uint4(8), 0.55f);   // ky=9, ks=8, s>=0.5
        run_test(sc_uint4(10), sc_uint4(9), 0.4f);   // ky=10, ks=9, s<0.5
        run_test(sc_uint4(12), sc_uint4(10), 0.9f);  // ky=12, ks=10, s>=0.5
        run_test(sc_uint4(15), sc_uint4(15), 0.5f);  // ky=15, ks=15, s=0.5 (boundary)

        std::cout << "\n" << std::string(140, '=') << std::endl;
        std::cout << "MIXED COMBINATIONS (varied ky/ks ratios)" << std::endl;
        std::cout << std::string(140, '=') << std::endl;
        
        run_test(sc_uint4(3), sc_uint4(8), 0.22f);   // ky<ks, s<0.5
        run_test(sc_uint4(5), sc_uint4(2), 0.77f);   // ky>ks, s>=0.5
        run_test(sc_uint4(7), sc_uint4(4), 0.39f);   // ky>ks, s<0.5
        run_test(sc_uint4(4), sc_uint4(11), 0.68f);  // ky<<ks, s>=0.5
        run_test(sc_uint4(14), sc_uint4(1), 0.12f);  // ky>>ks, s<0.5

        std::cout << "\n" << std::string(140, '=') << std::endl;
        std::cout << "BOUNDARY CASES (s = 0 or s = 1)" << std::endl;
        std::cout << std::string(140, '=') << std::endl;
        
        run_test(sc_uint4(2), sc_uint4(3), 0.0f);    // ky=2, ks=3, s=0 (use 0x388B since 0 < 0.5)
        run_test(sc_uint4(1), sc_uint4(4), 1.0f);    // ky=1, ks=4, s=1 (use 0x3A8B since 1 >= 0.5)
        run_test(sc_uint4(5), sc_uint4(1), 0.0f);    // ky=5, ks=1, s=0
        run_test(sc_uint4(3), sc_uint4(2), 1.0f);    // ky=3, ks=2, s=1

        // Calculate averages
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
        float average_error = total_error / test_count;
        float average_error_no_saturation = (count_no_saturation > 0) ? (total_error_no_saturation / count_no_saturation) : 0.0f;

        // Print summary table
        std::cout << "\n" << std::string(140, '=') << std::endl;
        std::cout << "TEST SUMMARY - ERROR PERCENTAGE TABLE" << std::endl;
        std::cout << std::string(140, '=') << std::endl;
        
        std::cout << std::setw(5) << "Test" 
                  << std::setw(8) << "ky" 
                  << std::setw(8) << "ks" 
                  << std::setw(10) << "s"
                  << std::setw(20) << "Error (%)"
                  << std::setw(25) << "Saturation Status" << std::endl;
        std::cout << std::string(140, '-') << std::endl;
        
        for (int i = 0; i < test_count; i++) {
            std::cout << std::setw(5) << (i + 1)
                      << std::setw(8) << test_ky_list[i]
                      << std::setw(8) << test_ks_list[i]
                      << std::setw(10) << std::fixed << std::setprecision(3) << test_s_list[i]
                      << std::setw(20) << std::fixed << std::setprecision(2) << error_list[i] << "%"
                      << std::setw(25) << overflow_underflow_list[i]
                      << std::endl;
        }
        
        std::cout << std::string(140, '=') << std::endl;
        std::cout << "OVERALL STATISTICS" << std::endl;
        std::cout << std::string(140, '=') << std::endl;
        std::cout << "Total Tests Executed:                          " << test_count << std::endl;
        std::cout << "Average Error Percentage:                      " << std::fixed << std::setprecision(2) << average_error << "%" << std::endl;
        std::cout << "Average Error Percentage (Except Saturation):  " << std::fixed << std::setprecision(2) << average_error_no_saturation << "%" << std::endl;
        std::cout << "Minimum Error:                                 " << std::fixed << std::setprecision(2) << *std::min_element(error_list.begin(), error_list.end()) << "%" << std::endl;
        std::cout << "Maximum Error:                                 " << std::fixed << std::setprecision(2) << *std::max_element(error_list.begin(), error_list.end()) << "%" << std::endl;
        std::cout << "Tests with Overflow/Underflow:                 " << (test_count - count_no_saturation) << std::endl;
        std::cout << "Tests without Overflow/Underflow:              " << count_no_saturation << std::endl;
        std::cout << std::string(140, '=') << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    Divider_TestBench tb("Divider_TestBench");
    sc_start();
    return 0;
}
