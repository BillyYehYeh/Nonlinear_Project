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

    void print_test_case(int test_num, const std::string& description, uint32_t input, 
                        int expected_pos, int expected_is_over_half, uint16_t expected_threshold) {
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "Test " << test_num << ": " << description << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        std::cout << "Input: 0x" << std::hex << std::setfill('0') << std::setw(8) << input << std::dec << std::endl;
        std::cout << "Binary (32-bit): " << std::bitset<32>(input) << std::endl;
        std::cout << "                 " << "||||||||||||||||||XXXXXXXXXXXXXXXX" << std::endl;
        std::cout << "                 " << "Integer(31-16)   Decimal(15-0)" << std::endl;
        
        // Show integer and decimal parts
        uint32_t integer_part = (input >> 16) & 0xFFFF;
        uint32_t decimal_part = input & 0xFFFF;
        std::cout << "\nInteger Part (bits 31-16):" << std::endl;
        std::cout << "  Hex: 0x" << std::hex << std::setfill('0') << std::setw(4) << integer_part << std::dec << std::endl;
        std::cout << "  Decimal: " << integer_part << std::endl;
        std::cout << "  Binary: " << std::bitset<16>(integer_part) << std::endl;
        
        std::cout << "\nDecimal Part (bits 15-0):" << std::endl;
        std::cout << "  Hex: 0x" << std::hex << std::setfill('0') << std::setw(4) << decimal_part << std::dec << std::endl;
        std::cout << "  Decimal: " << decimal_part << std::endl;
        std::cout << "  Binary: " << std::bitset<16>(decimal_part) << std::endl;
        
        std::cout << "\nExpected Results:" << std::endl;
        std::cout << "  Leading_One_Pos: " << expected_pos << std::endl;
        if (expected_pos == 0) {
            std::cout << "    (Integer part is all zeros, using first decimal place bit)" << std::endl;
        } else {
            std::cout << "    (Leading one found at bit " << (16 + expected_pos) << " in 32-bit input)" << std::endl;
        }
        std::cout << "  Is_Over_Half: " << expected_is_over_half << std::endl;
        if (expected_pos == 0) {
            int bit_15_val = (input >> 15) & 1;
            std::cout << "    (From bit 15 [first decimal place]: " << bit_15_val << ")" << std::endl;
        } else {
            int bit_pos = 16 + expected_pos - 1;
            int bit_val = (input >> bit_pos) & 1;
            std::cout << "    (From bit " << bit_pos << " [one below leading one]: " << bit_val << ")" << std::endl;
        }
        std::cout << "  Threshold: 0x" << std::hex << std::setfill('0') << std::setw(4) << expected_threshold 
                  << std::dec << " (" << (expected_threshold == 0x3A8B ? "fp16(0.818)" : "fp16(0.568)") << ")" << std::endl;
    }

    void run_test(int& test_count, int& passed, int test_num, const std::string& description, 
                  uint32_t input, int expected_pos, int expected_is_over_half, uint16_t expected_threshold) {
        print_test_case(test_num, description, input, expected_pos, expected_is_over_half, expected_threshold);
        
        input_sig.write(sc_uint32(input));
        wait(1, SC_NS);
        
        sc_uint4 actual_pos = leading_one_pos_sig.read();
        sc_uint16 actual_threshold = threshold_result_sig.read();
        
        std::cout << "\nActual Results:" << std::endl;
        std::cout << "  Leading_One_Pos: " << actual_pos.to_uint() << std::endl;
        std::cout << "  Threshold: 0x" << std::hex << std::setfill('0') << std::setw(4) << actual_threshold.to_uint() 
                  << std::dec << " (" << (actual_threshold.to_uint() == 0x3A8B ? "fp16(0.818)" : "fp16(0.568)") << ")" << std::endl;
        
        std::cout << "\nComparison:" << std::endl;
        bool pos_match = (actual_pos.to_uint() == expected_pos);
        bool threshold_match = (actual_threshold.to_uint() == expected_threshold);
        
        std::cout << "  Leading_One_Pos: " << (pos_match ? "✓ MATCH" : "✗ MISMATCH") 
                  << " (expected " << expected_pos << ", got " << actual_pos.to_uint() << ")" << std::endl;
        std::cout << "  Threshold: " << (threshold_match ? "✓ MATCH" : "✗ MISMATCH") 
                  << " (expected 0x" << std::hex << expected_threshold << std::dec 
                  << ", got 0x" << std::hex << actual_threshold.to_uint() << std::dec << ")" << std::endl;
        
        test_count++;
        if (pos_match && threshold_match) {
            std::cout << "\n✓ TEST PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "\n✗ TEST FAILED" << std::endl;
        }
    }

    void test_stimulus() {
        int test_count = 0;
        int passed = 0;

        // Test 1: Integer part = 0x0001, leading one at position 0
        // is_over_half comes from bit 15 (first decimal place) = 0
        run_test(test_count, passed, 1, "Integer=0x0001, leading at pos 0, decimal_bit[15]=0",
                0x00010000, 0, 0, 0x388B);

        // Test 2: Integer part = 0x0001, leading one at position 0
        // is_over_half comes from bit 15 (first decimal place) = 1
        run_test(test_count, passed, 2, "Integer=0x0001, leading at pos 0, decimal_bit[15]=1",
                0x00018000, 0, 1, 0x3A8B);

        // Test 3: Integer part = 0x0002, leading one at position 1
        // is_over_half from bit 0 (position below leading one) = 0
        run_test(test_count, passed, 3, "Integer=0x0002, leading at pos 1, bit[0]=0",
                0x00020000, 1, 0, 0x388B);

        // Test 4: Integer part = 0x0003, leading one at position 1
        // is_over_half from bit 0 = 1
        run_test(test_count, passed, 4, "Integer=0x0003, leading at pos 1, bit[0]=1",
                0x00030000, 1, 1, 0x3A8B);

        // Test 5: Integer part = 0x0004, leading one at position 2
        // is_over_half from bit 1 = 0
        run_test(test_count, passed, 5, "Integer=0x0004, leading at pos 2, bit[1]=0",
                0x00040000, 2, 0, 0x388B);

        // Test 6: Integer part = 0x0006, leading one at position 2
        // is_over_half from bit 1 = 1
        run_test(test_count, passed, 6, "Integer=0x0006, leading at pos 2, bit[1]=1",
                0x00060000, 2, 1, 0x3A8B);

        // Test 7: Integer part = 0x0008, leading one at position 3
        // is_over_half from bit 2 = 0
        run_test(test_count, passed, 7, "Integer=0x0008, leading at pos 3, bit[2]=0",
                0x00080000, 3, 0, 0x388B);

        // Test 8: Integer part = 0x000C, leading one at position 3
        // is_over_half from bit 2 = 1
        run_test(test_count, passed, 8, "Integer=0x000C, leading at pos 3, bit[2]=1",
                0x000C0000, 3, 1, 0x3A8B);

        // Test 9: Integer part = 0x00FF, leading one at position 7
        // is_over_half from bit 6 = 1
        run_test(test_count, passed, 9, "Integer=0x00FF, leading at pos 7, bit[6]=1",
                0x00FF0000, 7, 1, 0x3A8B);

        // Test 10: Integer part = 0x0080, leading one at position 7
        // is_over_half from bit 6 = 0
        run_test(test_count, passed, 10, "Integer=0x0080, leading at pos 7, bit[6]=0",
                0x00800000, 7, 0, 0x388B);

        // Test 11: Integer part = 0x0100, leading one at position 8
        // is_over_half from bit 7 = 0
        run_test(test_count, passed, 11, "Integer=0x0100, leading at pos 8, bit[7]=0",
                0x01000000, 8, 0, 0x388B);

        // Test 12: Integer part = 0x0180, leading one at position 8
        // is_over_half from bit 7 = 1
        run_test(test_count, passed, 12, "Integer=0x0180, leading at pos 8, bit[7]=1",
                0x01800000, 8, 1, 0x3A8B);

        // Test 13: Integer part = 0x4000, leading one at position 14
        // is_over_half from bit 13 = 0
        run_test(test_count, passed, 13, "Integer=0x4000, leading at pos 14, bit[13]=0",
                0x40000000, 14, 0, 0x388B);

        // Test 14: Integer part = 0x6000, leading one at position 14
        // is_over_half from bit 13 = 1
        run_test(test_count, passed, 14, "Integer=0x6000, leading at pos 14, bit[13]=1",
                0x60000000, 14, 1, 0x3A8B);

        // Test 15: Integer part = 0x8000, leading one at position 15
        // is_over_half from bit 14 = 0
        run_test(test_count, passed, 15, "Integer=0x8000, leading at pos 15, bit[14]=0",
                0x80000000, 15, 0, 0x388B);

        // Test 16: Integer part = 0xC000, leading one at position 15
        // is_over_half from bit 14 = 1
        run_test(test_count, passed, 16, "Integer=0xC000, leading at pos 15, bit[14]=1",
                0xC0000000, 15, 1, 0x3A8B);

        // Test 17: Integer part = 0x0000 with decimal part = 0x0001
        // leading_pos = 0, is_over_half from bit 15 = 0
        run_test(test_count, passed, 17, "Integer=0x0000, leading at pos 0, decimal_bit[15]=0",
                0x00000001, 0, 0, 0x388B);

        // Test 18: Integer part = 0x0000 with decimal part = 0x8000
        // leading_pos = 0, is_over_half from bit 15 = 1
        run_test(test_count, passed, 18, "Integer=0x0000, leading at pos 0, decimal_bit[15]=1",
                0x00008000, 0, 1, 0x3A8B);

        // Test 19: Integer part = 0x0000 with decimal part = 0x0000
        // leading_pos = 0, is_over_half from bit 15 = 0
        run_test(test_count, passed, 19, "Integer=0x0000, all zeros, leading at pos 0",
                0x00000000, 0, 0, 0x388B);

        // Summary
        std::cout << "\n\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        std::cout << "Total Tests: " << test_count << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << (test_count - passed) << std::endl;
        std::cout << "Pass Rate: " << std::fixed << std::setprecision(2) 
                  << (100.0 * passed / test_count) << "%" << std::endl;
        
        if (passed == test_count) {
            std::cout << "\n✓ ALL TESTS PASSED!" << std::endl;
        } else {
            std::cout << "\n✗ SOME TESTS FAILED!" << std::endl;
        }
        std::cout << std::string(100, '=') << std::endl;

        sc_stop();
    }
};

// Main simulation
int sc_main(int argc, char* argv[]) {
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "DIVIDER_PRECOMPUTE MODULE COMPREHENSIVE TEST" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    std::cout << "Input format: 32-bit value (16-bit integer part | 16-bit decimal part)" << std::endl;
    std::cout << "              bits 31-16: integer part, bits 15-0: decimal part" << std::endl;
    std::cout << "\nLeading_One_Pos: Position of leading one in integer part (0-15)" << std::endl;
    std::cout << "Is_Over_Half: Determined by:" << std::endl;
    std::cout << "  - If Leading_One_Pos == 0: bit 15 (first decimal place)" << std::endl;
    std::cout << "  - If Leading_One_Pos >  0: bit at (Leading_One_Pos - 1) in integer part" << std::endl;
    std::cout << "\nThreshold output:" << std::endl;
    std::cout << "  - If Is_Over_Half == 1: 0x3A8B (fp16 0.818)" << std::endl;
    std::cout << "  - If Is_Over_Half == 0: 0x388B (fp16 0.568)" << std::endl;
    std::cout << std::string(100, '=') << std::endl;
    
    Divider_PreCompute_TestBench *test = new Divider_PreCompute_TestBench("TestBench");
    
    sc_start();
    
    return 0;
}
