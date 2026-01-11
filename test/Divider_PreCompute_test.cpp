#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <bitset>
#include "Divider_PreCompute.h"

// Include the implementation from Divider_PreCompute.cpp
#include "../src/Divider_PreCompute.cpp"

SC_MODULE(Divider_PreCompute_TestBench) {
    sc_signal<sc_uint32> input_sig;
    sc_signal<sc_uint4>  leading_one_pos_sig;
    sc_signal<sc_uint16> threshold_result_sig;

    Divider_PreCompute_Module *dut;

    SC_CTOR(Divider_PreCompute_TestBench) {
        dut = new Divider_PreCompute_Module("Divider_PreCompute_DUT");
        dut->input(input_sig);
        dut->Leading_One_Pos(leading_one_pos_sig);
        dut->Threshold_Result(threshold_result_sig);

        SC_THREAD(test_stimulus);
    }

    void print_binary_32(uint32_t val) {
        std::cout << "Binary: " << std::bitset<32>(val) << std::endl;
    }

    void print_binary_16(uint16_t val) {
        std::cout << "Binary: " << std::bitset<16>(val) << std::endl;
    }

    void test_stimulus() {
        int test_count = 0;
        int passed = 0;

        // Test 1: Input = 0x80000000 (bit 31 set) -> Leading_One_Pos = 31
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 1: Input = 0x80000000 (Leading one at bit 31)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        uint32_t test_input = 0x80000000;
        input_sig.write(sc_uint32(test_input));
        wait(1, SC_NS);
        
        std::cout << "Input: 0x" << std::hex << test_input << std::dec << std::endl;
        print_binary_32(test_input);
        
        sc_uint4 expected_pos = 31;
        sc_uint4 actual_pos = leading_one_pos_sig.read();
        sc_uint16 threshold = threshold_result_sig.read();
        
        std::cout << "Expected Leading_One_Pos: " << expected_pos.to_uint() << std::endl;
        std::cout << "Actual Leading_One_Pos: " << actual_pos.to_uint() << std::endl;
        std::cout << "Threshold_Result: 0x" << std::hex << threshold.to_uint() << std::dec;
        print_binary_16(threshold.to_uint());
        
        // Bit at position 30 should be 0
        int bit_pos = expected_pos.to_uint() - 1;
        int is_over_half = (test_input >> bit_pos) & 1;
        std::cout << "Bit at position " << bit_pos << " (Is_Over_Half): " << is_over_half << std::endl;
        std::cout << "Expected Threshold: fp16(0.568)" << std::endl;
        
        if (actual_pos == expected_pos) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // Test 2: Input = 0xC0000000 (bits 31,30 set) -> Leading_One_Pos = 31, Is_Over_Half = 1
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 2: Input = 0xC0000000 (Leading one at bit 31, bit 30 = 1)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        test_input = 0xC0000000;
        input_sig.write(sc_uint32(test_input));
        wait(1, SC_NS);
        
        std::cout << "Input: 0x" << std::hex << test_input << std::dec << std::endl;
        print_binary_32(test_input);
        
        expected_pos = 31;
        actual_pos = leading_one_pos_sig.read();
        threshold = threshold_result_sig.read();
        
        std::cout << "Expected Leading_One_Pos: " << expected_pos.to_uint() << std::endl;
        std::cout << "Actual Leading_One_Pos: " << actual_pos.to_uint() << std::endl;
        std::cout << "Threshold_Result: 0x" << std::hex << threshold.to_uint() << std::dec;
        print_binary_16(threshold.to_uint());
        
        // Bit at position 30 should be 1
        bit_pos = expected_pos.to_uint() - 1;
        is_over_half = (test_input >> bit_pos) & 1;
        std::cout << "Bit at position " << bit_pos << " (Is_Over_Half): " << is_over_half << std::endl;
        std::cout << "Expected Threshold: fp16(0.818)" << std::endl;
        
        if (actual_pos == expected_pos) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // Test 3: Input = 0x00008000 (bit 15 set) -> Leading_One_Pos = 15
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 3: Input = 0x00008000 (Leading one at bit 15)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        test_input = 0x00008000;
        input_sig.write(sc_uint32(test_input));
        wait(1, SC_NS);
        
        std::cout << "Input: 0x" << std::hex << test_input << std::dec << std::endl;
        print_binary_32(test_input);
        
        expected_pos = 15;
        actual_pos = leading_one_pos_sig.read();
        threshold = threshold_result_sig.read();
        
        std::cout << "Expected Leading_One_Pos: " << expected_pos.to_uint() << std::endl;
        std::cout << "Actual Leading_One_Pos: " << actual_pos.to_uint() << std::endl;
        std::cout << "Threshold_Result: 0x" << std::hex << threshold.to_uint() << std::dec;
        print_binary_16(threshold.to_uint());
        
        // Bit at position 14 should be 0
        bit_pos = expected_pos.to_uint() - 1;
        is_over_half = (test_input >> bit_pos) & 1;
        std::cout << "Bit at position " << bit_pos << " (Is_Over_Half): " << is_over_half << std::endl;
        std::cout << "Expected Threshold: fp16(0.568)" << std::endl;
        
        if (actual_pos == expected_pos) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // Test 4: Input = 0x0000C000 (bits 15,14 set) -> Leading_One_Pos = 15, Is_Over_Half = 1
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 4: Input = 0x0000C000 (Leading one at bit 15, bit 14 = 1)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        test_input = 0x0000C000;
        input_sig.write(sc_uint32(test_input));
        wait(1, SC_NS);
        
        std::cout << "Input: 0x" << std::hex << test_input << std::dec << std::endl;
        print_binary_32(test_input);
        
        expected_pos = 15;
        actual_pos = leading_one_pos_sig.read();
        threshold = threshold_result_sig.read();
        
        std::cout << "Expected Leading_One_Pos: " << expected_pos.to_uint() << std::endl;
        std::cout << "Actual Leading_One_Pos: " << actual_pos.to_uint() << std::endl;
        std::cout << "Threshold_Result: 0x" << std::hex << threshold.to_uint() << std::dec;
        print_binary_16(threshold.to_uint());
        
        // Bit at position 14 should be 1
        bit_pos = expected_pos.to_uint() - 1;
        is_over_half = (test_input >> bit_pos) & 1;
        std::cout << "Bit at position " << bit_pos << " (Is_Over_Half): " << is_over_half << std::endl;
        std::cout << "Expected Threshold: fp16(0.818)" << std::endl;
        
        if (actual_pos == expected_pos) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // Test 5: Input = 0x00000001 (bit 0 set) -> Leading_One_Pos = 0
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 5: Input = 0x00000001 (Leading one at bit 0)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        test_input = 0x00000001;
        input_sig.write(sc_uint32(test_input));
        wait(1, SC_NS);
        
        std::cout << "Input: 0x" << std::hex << test_input << std::dec << std::endl;
        print_binary_32(test_input);
        
        expected_pos = 0;
        actual_pos = leading_one_pos_sig.read();
        threshold = threshold_result_sig.read();
        
        std::cout << "Expected Leading_One_Pos: " << expected_pos.to_uint() << std::endl;
        std::cout << "Actual Leading_One_Pos: " << actual_pos.to_uint() << std::endl;
        std::cout << "Threshold_Result: 0x" << std::hex << threshold.to_uint() << std::dec;
        print_binary_16(threshold.to_uint());
        
        std::cout << "Is_Over_Half: 0 (bit at position -1 doesn't exist)" << std::endl;
        std::cout << "Expected Threshold: fp16(0.568)" << std::endl;
        
        if (actual_pos == expected_pos) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // Test 6: Input = 0xAAAAAAAA (alternating bits) -> Leading_One_Pos = 31
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 6: Input = 0xAAAAAAAA (Alternating bits)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        test_input = 0xAAAAAAAA;
        input_sig.write(sc_uint32(test_input));
        wait(1, SC_NS);
        
        std::cout << "Input: 0x" << std::hex << test_input << std::dec << std::endl;
        print_binary_32(test_input);
        
        expected_pos = 31;
        actual_pos = leading_one_pos_sig.read();
        threshold = threshold_result_sig.read();
        
        std::cout << "Expected Leading_One_Pos: " << expected_pos.to_uint() << std::endl;
        std::cout << "Actual Leading_One_Pos: " << actual_pos.to_uint() << std::endl;
        std::cout << "Threshold_Result: 0x" << std::hex << threshold.to_uint() << std::dec;
        print_binary_16(threshold.to_uint());
        
        // Bit at position 30 should be 0
        bit_pos = expected_pos.to_uint() - 1;
        is_over_half = (test_input >> bit_pos) & 1;
        std::cout << "Bit at position " << bit_pos << " (Is_Over_Half): " << is_over_half << std::endl;
        std::cout << "Expected Threshold: fp16(0.568)" << std::endl;
        
        if (actual_pos == expected_pos) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // Test 7: Input = 0x55555555 (alternating bits, starting with 0) -> Leading_One_Pos = 30
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "Test 7: Input = 0x55555555 (Alternating bits)" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        test_count++;
        
        test_input = 0x55555555;
        input_sig.write(sc_uint32(test_input));
        wait(1, SC_NS);
        
        std::cout << "Input: 0x" << std::hex << test_input << std::dec << std::endl;
        print_binary_32(test_input);
        
        expected_pos = 30;
        actual_pos = leading_one_pos_sig.read();
        threshold = threshold_result_sig.read();
        
        std::cout << "Expected Leading_One_Pos: " << expected_pos.to_uint() << std::endl;
        std::cout << "Actual Leading_One_Pos: " << actual_pos.to_uint() << std::endl;
        std::cout << "Threshold_Result: 0x" << std::hex << threshold.to_uint() << std::dec;
        print_binary_16(threshold.to_uint());
        
        // Bit at position 29 should be 1
        bit_pos = expected_pos.to_uint() - 1;
        is_over_half = (test_input >> bit_pos) & 1;
        std::cout << "Bit at position " << bit_pos << " (Is_Over_Half): " << is_over_half << std::endl;
        std::cout << "Expected Threshold: fp16(0.818)" << std::endl;
        
        if (actual_pos == expected_pos) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // Summary
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

// Main simulation
int sc_main(int argc, char* argv[]) {
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "DIVIDER_PRECOMPUTE MODULE TEST" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    
    Divider_PreCompute_TestBench *test = new Divider_PreCompute_TestBench("TestBench");
    
    sc_start();
    
    return 0;
}
