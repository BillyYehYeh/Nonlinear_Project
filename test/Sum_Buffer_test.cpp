#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "Sum_Buffer.h"

#include "../src/Sum_Buffer.cpp"

SC_MODULE(Sum_Buffer_TestBench) {
    sc_clock clk_sig{"clk_sig", 10, SC_NS};        // 10ns clock period
    sc_signal<bool>   reset_sig;
    sc_signal<sc_uint32> Data_In_sig;
    sc_signal<bool>   write_sig;
    sc_signal<sc_uint32> Data_Out_sig;

    Sum_Buffer *dut;
    int cycle_count = 0;

    SC_CTOR(Sum_Buffer_TestBench) {
        dut = new Sum_Buffer("Sum_Buffer_DUT");
        dut->clk(clk_sig);
        dut->reset(reset_sig);
        dut->Data_In(Data_In_sig);
        dut->write(write_sig);
        dut->Data_Out(Data_Out_sig);

        SC_THREAD(test_stimulus);
    }

    void print_cycle_header(const std::string& description = "") {
        printf("\n%s\n", std::string(100, '-').c_str());
        printf("CYCLE #%d | Time: %s\n", cycle_count, sc_time_stamp().to_string().c_str());
        if (!description.empty()) {
            printf("Description: %s\n", description.c_str());
        }
        printf("%s\n", std::string(100, '-').c_str());
    }

    void print_signals(const std::string& operation = "") {
        printf("INPUTS:\n");
        printf("  %-18s %s\n", "Reset Signal:", reset_sig.read() ? "1 (RESET)" : "0 (NORMAL)");
        printf("  %-18s %s\n", "Write Signal:", write_sig.read() ? "1 (WRITE)" : "0 (READ)");
        
        uint32_t data_in = Data_In_sig.read();
        printf("  %-18s 0x%08x\n", "Data_In:", data_in);
        
        printf("OUTPUTS:\n");
        uint32_t data_out = Data_Out_sig.read();
        printf("  %-18s 0x%08x\n", "Data_Out:", data_out);
        
        if (!operation.empty()) {
            printf("Operation: %s\n", operation.c_str());
        }
    }

    void test_stimulus() {
        int test_count = 0;
        int passed = 0;

        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "SUM_BUFFER TESTBENCH - Single uint32 Storage" << std::endl;
        std::cout << std::string(100, '=') << std::endl;

        // ===== Test 1: Initial State =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 1: Initial State - Buffer Empty" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        cycle_count++;
        
        write_sig.write(0);  // Read mode
        reset_sig.write(0);  // Normal operation
        wait(1, SC_NS);
        print_cycle_header("Initial state");
        print_signals("Buffer should be empty (0x00000000)");
        
        std::cout << "\n[RESULT]";
        if (Data_Out_sig.read() == 0) {
            std::cout << " [PASS] Buffer initialized to 0x00000000" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Buffer should be 0x00000000" << std::endl;
        }

        // ===== Test 2: Write Single Value =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 2: Write Single Value" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        std::cout << "Writing: 0x12345678 to buffer" << std::endl;
        
        cycle_count++;
        write_sig.write(1);  // Write mode
        Data_In_sig.write(0x12345678);
        wait(10, SC_NS);  // Wait for clock edge
        print_cycle_header("Write operation - clock edge active");
        print_signals("Write signal high, data being written");
        
        cycle_count++;
        write_sig.write(0);  // Switch to read mode
        wait(1, SC_NS);
        print_cycle_header("After write operation - data committed");
        print_signals("Write completed on clock edge");
        
        std::cout << "\n[RESULT]";
        if (Data_Out_sig.read() == 0x12345678) {
            std::cout << " [PASS] Write and read successful" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Data mismatch (expected 0x12345678, got 0x" 
                      << std::hex << std::setfill('0') << std::setw(8) 
                      << Data_Out_sig.read() << std::dec << ")" << std::endl;
        }

        // ===== Test 3: Write Multiple Values (Overwrite) =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 3: Write Multiple Values (Overwrite)" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        std::cout << "Writing sequence: 0xAAAAAAAA -> 0xBBBBBBBB -> 0xCCCCCCCC" << std::endl;
        
        cycle_count++;
        write_sig.write(1);
        Data_In_sig.write(0xAAAAAAAA);
        wait(10, SC_NS);
        print_cycle_header("Write 0xAAAAAAAA");
        print_signals("Write signal high");
        
        cycle_count++;
        Data_In_sig.write(0xBBBBBBBB);
        wait(10, SC_NS);
        print_cycle_header("Write 0xBBBBBBBB");
        print_signals("Write signal high");
        
        cycle_count++;
        Data_In_sig.write(0xCCCCCCCC);
        wait(10, SC_NS);
        print_cycle_header("Write 0xCCCCCCCC");
        print_signals("Write signal high");
        
        cycle_count++;
        write_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("Read after sequence");
        print_signals("Should show last written value");
        
        std::cout << "\n[RESULT]";
        if (Data_Out_sig.read() == 0xCCCCCCCC) {
            std::cout << " [PASS] Overwrite works correctly" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Expected 0xCCCCCCCC" << std::endl;
        }

        // ===== Test 4: Reset Function =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 4: Reset Function" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        std::cout << "Before reset: buffer = 0x" << std::hex << std::setfill('0') << std::setw(8) 
                  << Data_Out_sig.read() << std::dec << std::endl;
        
        cycle_count++;
        reset_sig.write(1);  // Assert reset
        wait(10, SC_NS);
        print_cycle_header("Reset signal asserted");
        print_signals("Reset signal high - clearing buffer");
        
        cycle_count++;
        reset_sig.write(0);  // Deassert reset
        write_sig.write(0);  // Trigger combinational update by toggling write
        wait(1, SC_NS);
        print_cycle_header("After reset on clock edge");
        print_signals("Buffer should be cleared");
        
        std::cout << "\n[RESULT]";
        if (Data_Out_sig.read() == 0) {
            std::cout << " [PASS] Reset works correctly" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Reset failed" << std::endl;
        }

        // ===== Test 5: Large Value =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 5: Write Large Value (Maximum uint32)" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        write_sig.write(1);
        Data_In_sig.write(0xFFFFFFFF);
        wait(10, SC_NS);
        print_cycle_header("Write maximum value");
        print_signals("Write signal high");
        
        cycle_count++;
        write_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("Read maximum value");
        print_signals("Should read 0xFFFFFFFF");
        
        std::cout << "\n[RESULT]";
        if (Data_Out_sig.read() == 0xFFFFFFFF) {
            std::cout << " [PASS] Maximum value handled correctly" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Maximum value error" << std::endl;
        }

        // ===== Test Summary =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "SIMULATION COMPLETE - TEST SUMMARY" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        std::cout << "Total Cycles Executed: " << cycle_count << std::endl;
        std::cout << "Total Tests: " << test_count << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << (test_count - passed) << std::endl;
        
        if (passed == test_count) {
            std::cout << "\n[SUCCESS] All tests passed!" << std::endl;
        } else {
            std::cout << "\n[FAILURE] Some tests failed" << std::endl;
        }
        std::cout << std::string(100, '=') << std::endl;
        
        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    Sum_Buffer_TestBench tb("Sum_Buffer_TestBench");
    sc_start();
    return 0;
}
