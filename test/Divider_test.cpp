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

    void test_stimulus() {
        int test_count = 0;
        int passed = 0;

        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "DIVIDER MODULE TEST" << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        // Test 1: Basic test with ky=8, ks=3 (diff=5)
        // Use a FP16 value with exponent=15 and modify it by -5
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 1: ky=8, ks=3, diff=5 (reduce exponent by 5)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        sc_uint4 test_ky = 8;
        sc_uint4 test_ks = 3;
        sc_uint16 test_mux = 0x3C00;  // FP16 with sign=0, exp=15, mantissa=0 (value ≈ 1.0)
        
        ky_sig.write(test_ky);
        ks_sig.write(test_ks);
        mux_result_sig.write(test_mux);
        wait(1, SC_NS);
        
        uint16_t output = divider_output_sig.read().to_uint();
        int exp_out = (output >> 10) & 0x1F;
        
        std::cout << "ky: " << test_ky.to_uint() << ", ks: " << test_ks.to_uint() << std::endl;
        std::cout << "Difference (ky - ks): " << (int)test_ky.to_uint() - (int)test_ks.to_uint() << std::endl;
        std::cout << "Input Mux_Result: 0x" << std::hex << test_mux.to_uint() << std::dec;
        print_binary_16(test_mux.to_uint());
        print_fp16_components(test_mux.to_uint());
        
        std::cout << "Expected new exponent: " << (15 - 5) << " (15 - 5 = 10)" << std::endl;
        std::cout << "Output Divider_Output: 0x" << std::hex << output << std::dec;
        print_binary_16(output);
        print_fp16_components(output);
        std::cout << "Actual exponent: " << exp_out << std::endl;
        
        if (exp_out == 10) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // Test 2: ky=5, ks=8 (diff=-3, increase exponent by 3)
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 2: ky=5, ks=8, diff=-3 (increase exponent by 3)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        test_ky = 5;
        test_ks = 8;
        test_mux = 0x3C00;  // exp=15
        
        ky_sig.write(test_ky);
        ks_sig.write(test_ks);
        mux_result_sig.write(test_mux);
        wait(1, SC_NS);
        
        output = divider_output_sig.read().to_uint();
        exp_out = (output >> 10) & 0x1F;
        
        std::cout << "ky: " << test_ky.to_uint() << ", ks: " << test_ks.to_uint() << std::endl;
        std::cout << "Difference (ky - ks): " << (int)test_ky.to_uint() - (int)test_ks.to_uint() << std::endl;
        std::cout << "Input Mux_Result: 0x" << std::hex << test_mux.to_uint() << std::dec;
        print_binary_16(test_mux.to_uint());
        print_fp16_components(test_mux.to_uint());
        
        std::cout << "Expected new exponent: " << (15 + 3) << " (15 - (-3) = 18)" << std::endl;
        std::cout << "Output Divider_Output: 0x" << std::hex << output << std::dec;
        print_binary_16(output);
        print_fp16_components(output);
        std::cout << "Actual exponent: " << exp_out << std::endl;
        
        if (exp_out == 18) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // Test 3: Exponent underflow protection (diff=20, exp should saturate to 0)
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 3: ky=15, ks=0, diff=15 (exponent underflow protection)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        test_ky = 15;
        test_ks = 0;
        test_mux = 0x3800;  // exp=7
        
        ky_sig.write(test_ky);
        ks_sig.write(test_ks);
        mux_result_sig.write(test_mux);
        wait(1, SC_NS);
        
        output = divider_output_sig.read().to_uint();
        exp_out = (output >> 10) & 0x1F;
        
        std::cout << "ky: " << test_ky.to_uint() << ", ks: " << test_ks.to_uint() << std::endl;
        std::cout << "Difference (ky - ks): " << (int)test_ky.to_uint() - (int)test_ks.to_uint() << std::endl;
        std::cout << "Input Mux_Result: 0x" << std::hex << test_mux.to_uint() << std::dec;
        print_binary_16(test_mux.to_uint());
        print_fp16_components(test_mux.to_uint());
        
        std::cout << "Expected new exponent: 0 (7 - 15 = -8, saturated to 0)" << std::endl;
        std::cout << "Output Divider_Output: 0x" << std::hex << output << std::dec;
        print_binary_16(output);
        print_fp16_components(output);
        std::cout << "Actual exponent: " << exp_out << std::endl;
        
        if (exp_out == 0) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // Test 4: Exponent overflow protection (negative diff with low initial exp)
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 4: ky=2, ks=15, diff=-13 (exponent increase without overflow)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        test_ky = 2;
        test_ks = 15;
        test_mux = 0x3E00;  // exp=15
        
        ky_sig.write(test_ky);
        ks_sig.write(test_ks);
        mux_result_sig.write(test_mux);
        wait(1, SC_NS);
        
        output = divider_output_sig.read().to_uint();
        exp_out = (output >> 10) & 0x1F;
        
        std::cout << "ky: " << test_ky.to_uint() << ", ks: " << test_ks.to_uint() << std::endl;
        std::cout << "Difference (ky - ks): " << (int)test_ky.to_uint() - (int)test_ks.to_uint() << std::endl;
        std::cout << "Input Mux_Result: 0x" << std::hex << test_mux.to_uint() << std::dec;
        print_binary_16(test_mux.to_uint());
        print_fp16_components(test_mux.to_uint());
        
        std::cout << "Expected new exponent: 28 (15 - (-13) = 28)" << std::endl;
        std::cout << "Output Divider_Output: 0x" << std::hex << output << std::dec;
        print_binary_16(output);
        print_fp16_components(output);
        std::cout << "Actual exponent: " << exp_out << std::endl;
        
        if (exp_out == 28) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // Test 5: Sign preservation with mantissa variation
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 5: Sign and mantissa preservation" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        test_ky = 10;
        test_ks = 6;
        test_mux = 0xBC00;  // sign=1, exp=15, mantissa=0 (negative value)
        
        ky_sig.write(test_ky);
        ks_sig.write(test_ks);
        mux_result_sig.write(test_mux);
        wait(1, SC_NS);
        
        output = divider_output_sig.read().to_uint();
        int sign_out = (output >> 15) & 0x1;
        exp_out = (output >> 10) & 0x1F;
        int mant_out = output & 0x3FF;
        
        std::cout << "ky: " << test_ky.to_uint() << ", ks: " << test_ks.to_uint() << std::endl;
        std::cout << "Difference (ky - ks): " << (int)test_ky.to_uint() - (int)test_ks.to_uint() << std::endl;
        std::cout << "Input Mux_Result: 0x" << std::hex << test_mux.to_uint() << std::dec;
        print_binary_16(test_mux.to_uint());
        print_fp16_components(test_mux.to_uint());
        
        std::cout << "Expected: sign=1, exponent=" << (15-4) << ", mantissa=0" << std::endl;
        std::cout << "Output Divider_Output: 0x" << std::hex << output << std::dec;
        print_binary_16(output);
        print_fp16_components(output);
        std::cout << "Actual: sign=" << sign_out << ", exponent=" << exp_out << ", mantissa=" << mant_out << std::endl;
        
        if (sign_out == 1 && exp_out == 11 && mant_out == 0) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        std::cout << "Total Tests: " << test_count << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << (test_count - passed) << std::endl;

        if (passed == test_count) {
            std::cout << "\n✓ ALL TESTS PASSED!" << std::endl;
        } else {
            std::cout << "\n✗ SOME TESTS FAILED!" << std::endl;
        }
        std::cout << std::string(80, '=') << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    Divider_TestBench tb("Divider_TestBench");
    sc_start();
    return 0;
}
