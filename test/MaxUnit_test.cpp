#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <cassert>
#include "MaxUnit.h"

// Include the implementation from MaxUnit.cpp to get the module definition and functions
#include "../src/MaxUnit.cpp"

SC_MODULE(MaxUnit_TestBench) {
    sc_signal<bool> clk_sig, rst_sig;
    sc_signal<sc_uint16> A_sig, B_sig, C_sig, D_sig;
    sc_signal<sc_uint16> Max_Out_sig;

    Max4_FP16_Pipeline *dut;

    SC_CTOR(MaxUnit_TestBench) {
        dut = new Max4_FP16_Pipeline("MaxUnit_DUT");
        dut->clk(clk_sig);
        dut->rst(rst_sig);
        dut->A(A_sig);
        dut->B(B_sig);
        dut->C(C_sig);
        dut->D(D_sig);
        dut->Max_Out(Max_Out_sig);

        SC_THREAD(clk_gen);
        SC_THREAD(test_stimulus);
    }

    void clk_gen() {
        clk_sig.write(false);
        while (true) {
            wait(5, SC_NS);
            clk_sig.write(!clk_sig.read());
        }
    }

    void test_stimulus() {
        int test_count = 0;
        int passed = 0;

        // ===== Test 1: Reset test =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Test 1: Reset Functionality" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        test_count++;
        
        rst_sig.write(true);
        A_sig.write(0x4100);  // 3.0
        B_sig.write(0x4200);  // 4.0
        C_sig.write(0x3D00);  // 0.5
        D_sig.write(0xC400);  // -3.0
        
        wait(10, SC_NS);
        rst_sig.write(false);
        wait(5, SC_NS);
        
        std::cout << "Output during reset: 0x" << std::hex << Max_Out_sig.read() 
                  << std::dec << " (Expected: 0x0000)" << std::endl;
        if (Max_Out_sig.read() == 0) {
            std::cout << "[PASS] Reset works correctly" << std::endl;
            passed++;
        } else {
            std::cout << "[FAIL] Reset failed" << std::endl;
        }

        // ===== Test 2: Max of Positive Numbers =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Test 2: Maximum of Positive Numbers" << std::endl;
        std::cout << "Input: A=3.0 (0x4100), B=4.0 (0x4200), C=0.5 (0x3D00), D=2.0 (0x4000)" << std::endl;
        std::cout << "Expected Output: 4.0 (0x4200)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        test_count++;
        
        A_sig.write(0x4100);  // 3.0
        B_sig.write(0x4200);  // 4.0
        C_sig.write(0x3D00);  // 0.5
        D_sig.write(0x4000);  // 2.0
        
        std::cout << "T=" << sc_time_stamp() << " Inputs set" << std::endl;
        wait(20, SC_NS);  // 2 clock cycles
        
        sc_uint16 result = Max_Out_sig.read();
        std::cout << "T=" << sc_time_stamp() << " Output: 0x" << std::hex << result << std::dec << std::endl;
        if (result == 0x4200) {
            std::cout << "[PASS] Correct maximum found (4.0)" << std::endl;
            passed++;
        } else {
            std::cout << "[FAIL] Incorrect result" << std::endl;
        }

        // ===== Test 3: Max with Negative Numbers =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Test 3: Maximum with Negative Numbers" << std::endl;
        std::cout << "Input: A=-3.0 (0xC400), B=-4.0 (0xC600), C=5.0 (0x4400), D=1.0 (0x3C00)" << std::endl;
        std::cout << "Expected Output: 5.0 (0x4400)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        test_count++;
        
        A_sig.write(0xC400);  // -3.0
        B_sig.write(0xC600);  // -4.0
        C_sig.write(0x4400);  // 5.0
        D_sig.write(0x3C00);  // 1.0
        
        std::cout << "T=" << sc_time_stamp() << " Inputs set" << std::endl;
        wait(20, SC_NS);
        
        result = Max_Out_sig.read();
        std::cout << "T=" << sc_time_stamp() << " Output: 0x" << std::hex << result << std::dec << std::endl;
        if (result == 0x4400) {
            std::cout << "[PASS] Correct maximum found (5.0)" << std::endl;
            passed++;
        } else {
            std::cout << "[FAIL] Incorrect result" << std::endl;
        }

        // ===== Test 4: All Negative Numbers =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Test 4: All Negative Numbers" << std::endl;
        std::cout << "Input: A=-1.0 (0xBC00), B=-2.0 (0xC000), C=-0.5 (0xB800), D=-3.0 (0xC400)" << std::endl;
        std::cout << "Expected Output: -0.5 (0xB800)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        test_count++;
        
        A_sig.write(0xBC00);  // -1.0
        B_sig.write(0xC000);  // -2.0
        C_sig.write(0xB800);  // -0.5
        D_sig.write(0xC400);  // -3.0
        
        std::cout << "T=" << sc_time_stamp() << " Inputs set" << std::endl;
        wait(20, SC_NS);
        
        result = Max_Out_sig.read();
        std::cout << "T=" << sc_time_stamp() << " Output: 0x" << std::hex << result << std::dec << std::endl;
        if (result == 0xB800) {
            std::cout << "[PASS] Correct maximum found (-0.5)" << std::endl;
            passed++;
        } else {
            std::cout << "[FAIL] Incorrect result" << std::endl;
        }

        // ===== Test 5: Mixed Positive and Negative =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Test 5: Mixed Positive and Negative Numbers" << std::endl;
        std::cout << "Input: A=-5.0 (0xC800), B=2.5 (0x4080), C=-1.5 (0xBE00), D=3.0 (0x4100)" << std::endl;
        std::cout << "Expected Output: 3.0 (0x4100)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        test_count++;
        
        A_sig.write(0xC800);  // -5.0
        B_sig.write(0x4080);  // 2.5
        C_sig.write(0xBE00);  // -1.5
        D_sig.write(0x4100);  // 3.0
        
        std::cout << "T=" << sc_time_stamp() << " Inputs set" << std::endl;
        wait(20, SC_NS);
        
        result = Max_Out_sig.read();
        std::cout << "T=" << sc_time_stamp() << " Output: 0x" << std::hex << result << std::dec << std::endl;
        if (result == 0x4100) {
            std::cout << "[PASS] Correct maximum found (3.0)" << std::endl;
            passed++;
        } else {
            std::cout << "[FAIL] Incorrect result" << std::endl;
        }

        // ===== Test Summary =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Total Tests: " << test_count << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << (test_count - passed) << std::endl;
        
        if (passed == test_count) {
            std::cout << "\n[ALL TESTS PASSED]" << std::endl;
        } else {
            std::cout << "\n[SOME TESTS FAILED]" << std::endl;
        }
        std::cout << std::string(70, '=') << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "Starting MaxUnit Test Suite\n" << std::endl;
    
    MaxUnit_TestBench tb("tb");
    sc_start();
    
    return 0;
}
