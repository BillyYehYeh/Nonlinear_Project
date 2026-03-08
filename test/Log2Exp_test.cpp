#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <bitset>
#include <cmath>
#include "Log2Exp.h"

#define FP16_INPUT 0x8001  

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

SC_MODULE(Log2Exp_TestBench) {
    sc_signal<sc_dt::sc_uint<16>> fp16_in_sig;
    sc_signal<sc_dt::sc_uint<4>>  result_out_sig;

    Log2Exp *dut;
    
    // Test tracking
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;

    SC_CTOR(Log2Exp_TestBench) {
        dut = new Log2Exp("Log2Exp_DUT");
        dut->fp16_in(fp16_in_sig);
        dut->result_out(result_out_sig);

        SC_THREAD(test_stimulus);
    }

    void print_fp16_info(uint16_t bits) {
        int sign = (bits >> 15) & 0x1;
        int exp_bits = (bits >> 10) & 0x1F;
        int mant_bits = bits & 0x3FF;
        
        std::cout << "  Sign: " << sign << ", Exponent: " << exp_bits << ", Mantissa: " << mant_bits << std::endl;
        std::cout << "  Float value: " << fp16_to_float(bits) << std::endl;
    }

    bool run_test(const std::string& test_name, uint16_t input_val, uint16_t* expected_output = nullptr) {
        total_tests++;
        
        std::cout << "\n[TEST " << total_tests << "] " << test_name << std::endl;
        std::cout << std::string(80, '-') << std::endl;
        std::cout << "Input (hex): 0x" << std::hex << std::setw(4) << std::setfill('0') << input_val << std::dec << std::endl;
        std::cout << "Input (binary): " << std::bitset<16>(input_val) << std::endl;
        
        print_fp16_info(input_val);
        
        fp16_in_sig.write(sc_dt::sc_uint<16>(input_val));
        wait(1, SC_NS);
        
        sc_dt::sc_uint<4> result = result_out_sig.read();
        uint16_t result_int = result.to_uint();

        float x = fp16_to_float(input_val);
        float Exp_TRUE = std::exp(x); 
        double Exp_APPROXIMATE = std::exp2(x * (1.4375));
        double Exp_APPROXIMATE_OUTPUT = std::exp2(-result_int);

        std::cout << "-------------MODULE OUTPUT--------------" << std::endl;
        std::cout << " Output (4-bit):   " << std::bitset<4>(result_int) << std::endl;
        std::cout << " Output (decimal):   " << result_int << std::endl;
        std::cout << " Scaling (x * -1.4375): " << x * (-1.4375) << std::endl;

        std::cout << "-------------E^x Value--------------" << std::endl;
        std::cout << " Exp_TRUE: " << Exp_TRUE << std::endl;
        std::cout << " Exp_APPROXIMATE: " << std::scientific << Exp_APPROXIMATE << std::defaultfloat << std::endl;
        std::cout << " Exp_APPROXIMATE_OUTPUT: " << std::scientific << Exp_APPROXIMATE_OUTPUT << std::defaultfloat << std::endl;
        
        bool test_passed = true;
        if (expected_output != nullptr && result_int != *expected_output) {
            std::cout << " [FAIL] Expected: " << *expected_output << ", Got: " << result_int << std::endl;
            test_passed = false;
            failed_tests++;
        } else {
            std::cout << " [PASS]" << std::endl;
            passed_tests++;
        }
        
        return test_passed;
    }

    void test_stimulus() {
        std::cout << std::string(80, '=') << std::endl;
        std::cout << "\n  LOG2EXP MODULE - COMPREHENSIVE TEST SUITE  " << std::endl;
        std::cout << "  Module: Converts FP16 input to 4-bit logarithmic output" << std::endl;
        std::cout << "  Input: 16-bit FP16 value (fp16_in)" << std::endl;
        std::cout << "  Output: 4-bit result (result_out)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        // Test 1: Zero
        run_test("Zero (0x0000)", 0x0000);
        
        // Test 2: Positive one (1.0 in FP16 = 0x3C00)
        run_test("Positive One (0x3C00)", 0x3C00);
        
        // Test 3: Small positive (0.5 in FP16 = 0x3800)
        run_test("Small Positive 0.5 (0x3800)", 0x3800);
        
        // Test 4: Larger positive (2.0 in FP16 = 0x4000)
        run_test("Larger Positive 2.0 (0x4000)", 0x4000);
        
        // Test 5: Even larger (4.0 in FP16 = 0x4400)
        run_test("Even Larger 4.0 (0x4400)", 0x4400);
        
        // Test 6: Large positive (8.0 in FP16 = 0x4800)
        run_test("Large Positive 8.0 (0x4800)", 0x4800);
        
        // Test 7: Very small positive (0.125 in FP16 = 0x2E00)
        run_test("Very Small Positive 0.125 (0x2E00)", 0x2E00);
        
        // Test 8: Maximum finite value (65504 in FP16 = 0x7BFF)
        run_test("Maximum Finite Value (0x7BFF)", 0x7BFF);
        
        // Test 9: Minimal positive normalized (0x0400)
        run_test("Minimal Positive Normalized (0x0400)", 0x0400);
        
        // Test 10: Custom test (0x8001 - negative)
        run_test("Original Test Input (0x8001)", 0x8001);

        // Test 11: Negative one (-1.0 in FP16 = 0xBC00)
        run_test("Negative One (-1.0) (0xBC00)", 0xBC00);
        
        // Print summary
        print_test_summary();
        sc_stop();
    }
    
    void print_test_summary() {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "                       TEST SUMMARY                      " << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        std::cout << " Total Tests:  " << total_tests << std::endl;
        std::cout << " Passed:       " << passed_tests << " (" << (total_tests > 0 ? (passed_tests * 100) / total_tests : 0) << "%)" << std::endl;
        std::cout << " Failed:       " << failed_tests << " (" << (total_tests > 0 ? (failed_tests * 100) / total_tests : 0) << "%)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        
        if (failed_tests == 0) {
            std::cout << "                    ✓ ALL TESTS PASSED" << std::endl;
        } else {
            std::cout << "                    ✗ SOME TESTS FAILED" << std::endl;
        }
        std::cout << std::string(80, '=') << std::endl;
    }
};

int sc_main(int argc, char* argv[]) {
    Log2Exp_TestBench tb("Log2Exp_TestBench");
    sc_start();
    return 0;
}
