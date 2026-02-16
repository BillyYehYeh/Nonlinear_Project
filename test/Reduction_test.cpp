#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include "Reduction.h"

using sc_uint32 = sc_dt::sc_uint<32>;
using sc_uint16 = sc_dt::sc_uint<16>;

// Helper function to extract signed 4-bit values from 16-bit packed input
int extract_4bit(uint16_t packed, int index) {
    // Extract 4-bit signed value at position index (0-3)
    int shift = index * 4;
    int value = (packed >> shift) & 0xF;
    // Convert to signed (if MSB is set, it's negative)
    if (value & 0x8) {
        value = value - 16;  // Two's complement for 4-bit
    }
    return value;
}

// Helper function to calculate expected Σ(2^(-x_i))
double calculate_golden(uint16_t input_vector) {
    double sum = 0.0;
    for (int i = 0; i < 4; i++) {
        int x = extract_4bit(input_vector, i);
        sum += pow(2.0, -x);
    }
    return sum;
}

// Convert fixed-point 16.16 to float for display
double fixed_to_double(uint32_t fixed_point) {
    return (double)fixed_point / 65536.0;
}

SC_MODULE(Reduction_TestBench) {
    sc_clock clk{"clk", 10, SC_NS};
    sc_signal<bool> rst;
    sc_signal<sc_uint16> Input_Vector;
    sc_signal<sc_uint32> Output_Sum;

    Reduction_Module *dut;

    SC_CTOR(Reduction_TestBench) {
        dut = new Reduction_Module("Reduction_DUT");
        dut->clk(clk);
        dut->rst(rst);
        dut->Input_Vector(Input_Vector);
        dut->Output_Sum(Output_Sum);

        SC_THREAD(test_stimulus);
    }

    void print_separator(const std::string& title, int width = 100) {
        std::cout << "\n" << std::string(width, '=') << std::endl;
        std::cout << title << std::endl;
        std::cout << std::string(width, '=') << std::endl;
    }

    void print_input_breakdown(uint16_t input) {
        std::cout << "Input Vector Breakdown (16-bit): 0x" << std::hex << std::setw(4) << std::setfill('0') << input << std::dec << std::setfill(' ') << std::endl;
        
        // Extract 4-bit signed values
        std::cout << "  x0[3:0]   = " << std::setw(3) << extract_4bit(input, 0) << " -> 2^(-x0) = ";
        std::cout << std::fixed << std::setprecision(6) << pow(2.0, -extract_4bit(input, 0)) << std::endl;
        
        std::cout << "  x1[7:4]   = " << std::setw(3) << extract_4bit(input, 1) << " -> 2^(-x1) = ";
        std::cout << std::fixed << std::setprecision(6) << pow(2.0, -extract_4bit(input, 1)) << std::endl;
        
        std::cout << "  x2[11:8]  = " << std::setw(3) << extract_4bit(input, 2) << " -> 2^(-x2) = ";
        std::cout << std::fixed << std::setprecision(6) << pow(2.0, -extract_4bit(input, 2)) << std::endl;
        
        std::cout << "  x3[15:12] = " << std::setw(3) << extract_4bit(input, 3) << " -> 2^(-x3) = ";
        std::cout << std::fixed << std::setprecision(6) << pow(2.0, -extract_4bit(input, 3)) << std::endl;
    }

    void test_stimulus() {
        int test_count = 0;
        int passed = 0;

        print_separator("REDUCTION MODULE TEST BENCH - Exponential Sum Pipeline");
        std::cout << "Module: Σ(2^(-x_i)) for 4 signed 4-bit inputs" << std::endl;
        std::cout << "Output Format: 16.16 Fixed-Point (32-bit)" << std::endl;
        std::cout << "Pipeline Depth: 2 cycles" << std::endl;

        // ===== Test 1: All zeros (x0=0, x1=0, x2=0, x3=0) =====
        print_separator("TEST 1: All Zeros [x0=0, x1=0, x2=0, x3=0]");
        test_count++;
        
        rst.write(true);
        wait(1, SC_NS);
        rst.write(false);
        wait(1, SC_NS);
        
        uint16_t test1_input = 0x0000;
        Input_Vector.write(test1_input);
        print_input_breakdown(test1_input);
        wait(25, SC_NS);
        
        double golden1 = calculate_golden(test1_input);
        uint32_t expected1 = (uint32_t)(golden1 * 65536.0);
        uint32_t actual1 = Output_Sum.read().to_uint();
        
        std::cout << "\nExpected: Σ(2^(-0)) = 4.0" << std::endl;
        std::cout << "Expected Output (Fixed-Point): 0x" << std::hex << std::setw(8) << std::setfill('0') << expected1 
                  << std::dec << std::setfill(' ') << " (" << std::fixed << std::setprecision(6) << fixed_to_double(expected1) << ")" << std::endl;
        std::cout << "Actual Output (Fixed-Point):   0x" << std::hex << std::setw(8) << std::setfill('0') << actual1 
                  << std::dec << std::setfill(' ') << " (" << std::fixed << std::setprecision(6) << fixed_to_double(actual1) << ")" << std::endl;
        
        double error = std::abs(fixed_to_double(actual1) - golden1);
        std::cout << "Error: " << std::scientific << std::setprecision(6) << error << std::endl;
        
        if (error < 0.001) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        wait(1, SC_NS);

        // ===== Test 2: Mixed Positive Values [x0=0, x1=2, x2=1, x3=3] =====
        print_separator("TEST 2: Mixed Positive [x0=0, x1=2, x2=1, x3=3]");
        test_count++;
        
        rst.write(true);
        wait(1, SC_NS);
        rst.write(false);
        wait(1, SC_NS);
        
        uint16_t test2_input = 0x3120;
        Input_Vector.write(test2_input);
        print_input_breakdown(test2_input);
        wait(25, SC_NS);
        
        double golden2 = calculate_golden(test2_input);
        uint32_t expected2 = (uint32_t)(golden2 * 65536.0);
        uint32_t actual2 = Output_Sum.read().to_uint();
        
        std::cout << "\nExpected: 2^0 + 2^(-2) + 2^(-1) + 2^(-3) = " << std::fixed << std::setprecision(6) << golden2 << std::endl;
        std::cout << "Expected Output (Fixed-Point): 0x" << std::hex << std::setw(8) << std::setfill('0') << expected2 
                  << std::dec << std::setfill(' ') << " (" << std::fixed << std::setprecision(6) << fixed_to_double(expected2) << ")" << std::endl;
        std::cout << "Actual Output (Fixed-Point):   0x" << std::hex << std::setw(8) << std::setfill('0') << actual2 
                  << std::dec << std::setfill(' ') << " (" << std::fixed << std::setprecision(6) << fixed_to_double(actual2) << ")" << std::endl;
        
        error = std::abs(fixed_to_double(actual2) - golden2);
        std::cout << "Error: " << std::scientific << std::setprecision(6) << error << std::endl;
        
        if (error < 0.001) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        wait(1, SC_NS);

        // ===== Test 3: All ones (x0=1, x1=1, x2=1, x3=1) =====
        print_separator("TEST 3: All Ones [x0=1, x1=1, x2=1, x3=1]");
        test_count++;
        
        rst.write(true);
        wait(1, SC_NS);
        rst.write(false);
        wait(1, SC_NS);
        
        uint16_t test3_input = 0x1111;
        Input_Vector.write(test3_input);
        print_input_breakdown(test3_input);
        wait(25, SC_NS);
        
        double golden3 = calculate_golden(test3_input);
        uint32_t expected3 = (uint32_t)(golden3 * 65536.0);
        uint32_t actual3 = Output_Sum.read().to_uint();
        
        std::cout << "\nExpected: 4 × 2^(-1) = 2.0" << std::endl;
        std::cout << "Expected Output (Fixed-Point): 0x" << std::hex << std::setw(8) << std::setfill('0') << expected3 
                  << std::dec << std::setfill(' ') << " (" << std::fixed << std::setprecision(6) << fixed_to_double(expected3) << ")" << std::endl;
        std::cout << "Actual Output (Fixed-Point):   0x" << std::hex << std::setw(8) << std::setfill('0') << actual3 
                  << std::dec << std::setfill(' ') << " (" << std::fixed << std::setprecision(6) << fixed_to_double(actual3) << ")" << std::endl;
        
        error = std::abs(fixed_to_double(actual3) - golden3);
        std::cout << "Error: " << std::scientific << std::setprecision(6) << error << std::endl;
        
        if (error < 0.001) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        wait(1, SC_NS);

        // ===== Test 4: Large Positive Values [x0=4, x1=5, x2=6, x3=7] =====
        print_separator("TEST 4: Large Positive [x0=4, x1=5, x2=6, x3=7]");
        test_count++;
        
        rst.write(true);
        wait(1, SC_NS);
        rst.write(false);
        wait(1, SC_NS);
        
        uint16_t test4_input = 0x7654;
        Input_Vector.write(test4_input);
        print_input_breakdown(test4_input);
        wait(25, SC_NS);
        
        double golden4 = calculate_golden(test4_input);
        uint32_t expected4 = (uint32_t)(golden4 * 65536.0);
        uint32_t actual4 = Output_Sum.read().to_uint();
        
        std::cout << "\nExpected: 2^(-4) + 2^(-5) + 2^(-6) + 2^(-7) = " << std::fixed << std::setprecision(6) << golden4 << std::endl;
        std::cout << "Expected Output (Fixed-Point): 0x" << std::hex << std::setw(8) << std::setfill('0') << expected4 
                  << std::dec << std::setfill(' ') << " (" << std::fixed << std::setprecision(6) << fixed_to_double(expected4) << ")" << std::endl;
        std::cout << "Actual Output (Fixed-Point):   0x" << std::hex << std::setw(8) << std::setfill('0') << actual4 
                  << std::dec << std::setfill(' ') << " (" << std::fixed << std::setprecision(6) << fixed_to_double(actual4) << ")" << std::endl;
        
        error = std::abs(fixed_to_double(actual4) - golden4);
        std::cout << "Error: " << std::scientific << std::setprecision(6) << error << std::endl;
        
        if (error < 0.001) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // ===== Test 5: Positive increasing sequence (x0=0, x1=1, x2=2, x3=3) =====
        print_separator("TEST 5: Positive Increasing [x0=0, x1=1, x2=2, x3=3]");
        test_count++;
        
        rst.write(true);
        wait(1, SC_NS);
        rst.write(false);
        wait(1, SC_NS);
        
        uint16_t test5_input = 0x3210;
        Input_Vector.write(test5_input);
        print_input_breakdown(test5_input);
        wait(25, SC_NS);
        
        double golden5 = calculate_golden(test5_input);
        uint32_t expected5 = (uint32_t)(golden5 * 65536.0);
        uint32_t actual5 = Output_Sum.read().to_uint();
        
        std::cout << "\nExpected: 2^0 + 2^(-1) + 2^(-2) + 2^(-3) = " << std::fixed << std::setprecision(6) << golden5 << std::endl;
        std::cout << "Expected Output (Fixed-Point): 0x" << std::hex << std::setw(8) << std::setfill('0') << expected5 
                  << std::dec << std::setfill(' ') << " (" << std::fixed << std::setprecision(6) << fixed_to_double(expected5) << ")" << std::endl;
        std::cout << "Actual Output (Fixed-Point):   0x" << std::hex << std::setw(8) << std::setfill('0') << actual5 
                  << std::dec << std::setfill(' ') << " (" << std::fixed << std::setprecision(6) << fixed_to_double(actual5) << ")" << std::endl;
        
        error = std::abs(fixed_to_double(actual5) - golden5);
        std::cout << "Error: " << std::scientific << std::setprecision(6) << error << std::endl;
        
        if (error < 0.001) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // ===== Test Summary =====
        print_separator("TEST SUMMARY", 100);
        std::cout << "Total Tests: " << test_count << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << (test_count - passed) << std::endl;
        
        if (passed == test_count) {
            std::cout << "Result: ✓ ALL TESTS PASSED" << std::endl;
        } else {
            std::cout << "Result: ✗ SOME TESTS FAILED" << std::endl;
        }
        std::cout << std::string(100, '=') << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "\n" << std::string(100, '=') << std::endl;
    std::cout << "REDUCTION MODULE TEST BENCH" << std::endl;
    std::cout << "Specification: Exponential-Sum Reduction with Binary Adder Tree Pipeline" << std::endl;
    std::cout << "Function: Output = Σ(2^(-x_i)) for i=0,1,2,3" << std::endl;
    std::cout << "Input: 16-bit packed vector (4×4-bit signed integers)" << std::endl;
    std::cout << "Output: 32-bit (16.16 fixed-point format)" << std::endl;
    std::cout << std::string(100, '=') << std::endl;

    Reduction_TestBench tb("Reduction_TestBench");

    sc_start();

    return 0;
}
