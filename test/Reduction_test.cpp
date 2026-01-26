#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <cassert>
#include "Reduction.h"

// Include the implementation from Reduction.cpp to get the module definition and functions
#include "../src/Reduction.cpp"

SC_MODULE(Reduction_TestBench) {
    sc_signal<sc_uint4> X_sig;
    sc_signal<sc_uint32> Y_sig;

    Reduction_Module *dut;

    SC_CTOR(Reduction_TestBench) {
        dut = new Reduction_Module("Reduction_DUT");
        dut->X_in(X_sig);
        dut->Y_out(Y_sig);

        SC_THREAD(test_stimulus);
    }

    void test_stimulus() {
        int test_count = 0;
        int passed = 0;

        // ===== Test 1: X = 0 (2^0 = 1, shifted left by 16 bits = 0x00010000) =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Test 1: X = 0 (Expected: 2^0 = 1 << 16 = 65536)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        test_count++;
        
        X_sig.write(0);
        wait(1, SC_NS);
        
        sc_uint32 expected = 1U << 16;
        sc_uint32 actual = Y_sig.read();
        
        std::cout << "Input X: " << X_sig.read().to_uint() << std::endl;
        std::cout << "Expected Output: " << expected.to_uint() << " (0x" << std::hex << expected.to_uint() << std::dec << ")" << std::endl;
        std::cout << "Actual Output: " << actual.to_uint() << " (0x" << std::hex << actual.to_uint() << std::dec << ")" << std::endl;
        
        if (actual == expected) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // ===== Test 2: X = 1 (2^1 = 2, shifted left by 16 bits = 0x00020000) =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Test 2: X = 1 (Expected: 2^1 = 2 << 16 = 131072)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        test_count++;
        
        X_sig.write(1);
        wait(1, SC_NS);
        
        expected = 2U << 16;
        actual = Y_sig.read();
        
        std::cout << "Input X: " << X_sig.read().to_uint() << std::endl;
        std::cout << "Expected Output: " << expected.to_uint() << " (0x" << std::hex << expected.to_uint() << std::dec << ")" << std::endl;
        std::cout << "Actual Output: " << actual.to_uint() << " (0x" << std::hex << actual.to_uint() << std::dec << ")" << std::endl;
        
        if (actual == expected) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // ===== Test 3: X = 4 (2^4 = 16, shifted left by 16 bits = 0x00100000) =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Test 3: X = 4 (Expected: 2^4 = 16 << 16 = 1048576)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        test_count++;
        
        X_sig.write(4);
        wait(1, SC_NS);
        
        expected = 16U << 16;
        actual = Y_sig.read();
        
        std::cout << "Input X: " << X_sig.read().to_uint() << std::endl;
        std::cout << "Expected Output: " << expected.to_uint() << " (0x" << std::hex << expected.to_uint() << std::dec << ")" << std::endl;
        std::cout << "Actual Output: " << actual.to_uint() << " (0x" << std::hex << actual.to_uint() << std::dec << ")" << std::endl;
        
        if (actual == expected) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // ===== Test 4: X = 8 (2^8 = 256, shifted left by 16 bits = 0x01000000) =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Test 4: X = 8 (Expected: 2^8 = 256 << 16 = 16777216)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        test_count++;
        
        X_sig.write(8);
        wait(1, SC_NS);
        
        expected = 256U << 16;
        actual = Y_sig.read();
        
        std::cout << "Input X: " << X_sig.read().to_uint() << std::endl;
        std::cout << "Expected Output: " << expected.to_uint() << " (0x" << std::hex << expected.to_uint() << std::dec << ")" << std::endl;
        std::cout << "Actual Output: " << actual.to_uint() << " (0x" << std::hex << actual.to_uint() << std::dec << ")" << std::endl;
        
        if (actual == expected) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // ===== Test 5: X = 15 (2^15 = 32768, shifted left by 16 bits = 0x80000000) =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Test 5: X = 15 (Expected: 2^15 = 32768 << 16 = 2147483648)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        test_count++;
        
        X_sig.write(15);
        wait(1, SC_NS);
        
        expected = 32768U << 16;
        actual = Y_sig.read();
        
        std::cout << "Input X: " << X_sig.read().to_uint() << std::endl;
        std::cout << "Expected Output: " << expected.to_uint() << " (0x" << std::hex << expected.to_uint() << std::dec << ")" << std::endl;
        std::cout << "Actual Output: " << actual.to_uint() << " (0x" << std::hex << actual.to_uint() << std::dec << ")" << std::endl;
        
        if (actual == expected) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // ===== Test 6: X = 10 (2^10 = 1024, shifted left by 16 bits = 0x04000000) =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "Test 6: X = 10 (Expected: 2^10 = 1024 << 16 = 67108864)" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        test_count++;
        
        X_sig.write(10);
        wait(1, SC_NS);
        
        expected = 1024U << 16;
        actual = Y_sig.read();
        
        std::cout << "Input X: " << X_sig.read().to_uint() << std::endl;
        std::cout << "Expected Output: " << expected.to_uint() << " (0x" << std::hex << expected.to_uint() << std::dec << ")" << std::endl;
        std::cout << "Actual Output: " << actual.to_uint() << " (0x" << std::hex << actual.to_uint() << std::dec << ")" << std::endl;
        
        if (actual == expected) {
            std::cout << "✓ PASSED" << std::endl;
            passed++;
        } else {
            std::cout << "✗ FAILED" << std::endl;
        }

        // ===== Test Summary =====
        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Total Tests: " << test_count << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << (test_count - passed) << std::endl;
        
        if (passed == test_count) {
            std::cout << "Result: ✓ ALL TESTS PASSED" << std::endl;
        } else {
            std::cout << "Result: ✗ SOME TESTS FAILED" << std::endl;
        }
        std::cout << std::string(70, '=') << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    std::cout << "\n========================================" << std::endl;
    std::cout << "Reduction Module Test Bench" << std::endl;
    std::cout << "Function: Y = 2^X << 16 (X is 4-bit, 2^X is 16-bit, Y is 32-bit with 16-bit decimal zeros)" << std::endl;
    std::cout << "========================================" << std::endl;

    Reduction_TestBench tb("Reduction_TestBench");

    sc_start();

    return 0;
}
