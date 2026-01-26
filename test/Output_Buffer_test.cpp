#include <systemc.h>
#include <iostream>
#include <iomanip>
#include "Output_Buffer.h"

#include "../src/Output_Buffer.cpp"

SC_MODULE(Output_Buffer_TestBench) {
    sc_clock clk_sig{"clk_sig", 10, SC_NS};        // 10ns clock period
    sc_signal<bool>   reset_sig;
    sc_signal<sc_uint16> Data_In_sig;
    sc_signal<sc_uint10> index_sig;
    sc_signal<bool>   write_sig;
    sc_signal<sc_uint16> Data_Out_sig;

    Output_Buffer *dut;
    int cycle_count = 0;

    SC_CTOR(Output_Buffer_TestBench) {
        dut = new Output_Buffer("Output_Buffer_DUT");
        dut->clk(clk_sig);
        dut->reset(reset_sig);
        dut->Data_In(Data_In_sig);
        dut->index(index_sig);
        dut->write(write_sig);
        dut->Data_Out(Data_Out_sig);

        SC_THREAD(test_stimulus);
    }

    void print_cycle_header(const std::string& description = "") {
        std::cout << "\n" << std::string(100, '-') << std::endl;
        std::cout << "CYCLE #" << cycle_count << " | Time: " << sc_time_stamp() << std::endl;
        if (!description.empty()) {
            std::cout << "Description: " << description << std::endl;
        }
        std::cout << std::string(100, '-') << std::endl;
    }

    void print_signals(const std::string& operation = "") {
        std::cout << std::left << std::setw(20) << "INPUTS:" << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Reset Signal:" 
                  << (reset_sig.read() ? "1 (RESET)" : "0 (NORMAL)") << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Write Signal:" 
                  << (write_sig.read() ? "1 (WRITE)" : "0 (READ)") << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Index:" 
                  << std::dec << (int)index_sig.read() << " (0x" << std::hex 
                  << std::setfill('0') << std::setw(3) << (int)index_sig.read() << std::dec << ")" 
                  << std::setfill(' ') << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Data_In:" 
                  << "0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << Data_In_sig.read() << std::dec << std::setfill(' ') << std::endl;
        
        std::cout << std::left << std::setw(20) << "OUTPUTS:" << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Data_Out:" 
                  << "0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << Data_Out_sig.read() << std::dec << std::setfill(' ') << std::endl;
        
        if (!operation.empty()) {
            std::cout << "Operation: " << operation << std::endl;
        }
    }

    void test_stimulus() {
        int test_count = 0;
        int passed = 0;

        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "OUTPUT_BUFFER TESTBENCH - 1024-entry uint16 Storage" << std::endl;
        std::cout << std::string(100, '=') << std::endl;

        // ===== Test 1: Initial State =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 1: Initial State - Buffer Empty" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        cycle_count++;
        
        write_sig.write(0);  // Read mode
        reset_sig.write(0);  // Normal operation
        index_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("Initial state");
        print_signals("Buffer at index 0 should be 0x0000");
        
        std::cout << "\n[RESULT]";
        if (Data_Out_sig.read() == 0) {
            std::cout << " [PASS] Buffer initialized to 0x0000" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Buffer should be 0x0000" << std::endl;
        }

        // ===== Test 2: Write and Read Single Value =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 2: Write Single Value to Index 0" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        std::cout << "Writing: 0x1234 to index 0" << std::endl;
        
        cycle_count++;
        write_sig.write(1);  // Write mode
        index_sig.write(0);
        Data_In_sig.write(0x1234);
        wait(10, SC_NS);
        print_cycle_header("Write operation - clock edge active");
        print_signals("Write signal high, data being written");
        
        cycle_count++;
        write_sig.write(0);  // Switch to read mode
        wait(1, SC_NS);
        print_cycle_header("After write operation - data committed");
        print_signals("Write completed on clock edge");
        
        std::cout << "\n[RESULT]";
        if (Data_Out_sig.read() == 0x1234) {
            std::cout << " [PASS] Write and read successful" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Data mismatch" << std::endl;
        }

        // ===== Test 3: Write Multiple Locations =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 3: Write to Multiple Locations" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        std::cout << "Writing: 0x2222 to index 10, 0x3333 to index 20, 0x4444 to index 30" << std::endl;
        
        cycle_count++;
        write_sig.write(1);
        index_sig.write(10);
        Data_In_sig.write(0x2222);
        wait(10, SC_NS);
        print_cycle_header("Write to index 10");
        print_signals("Write signal high");
        
        cycle_count++;
        index_sig.write(20);
        Data_In_sig.write(0x3333);
        wait(10, SC_NS);
        print_cycle_header("Write to index 20");
        print_signals("Write signal high");
        
        cycle_count++;
        index_sig.write(30);
        Data_In_sig.write(0x4444);
        wait(10, SC_NS);
        print_cycle_header("Write to index 30");
        print_signals("Write signal high");
        
        // Read back from all three locations
        cycle_count++;
        write_sig.write(0);
        index_sig.write(10);
        wait(1, SC_NS);
        print_cycle_header("Read from index 10");
        print_signals("Verify written data");
        
        sc_uint16 val_10 = Data_Out_sig.read();
        
        cycle_count++;
        index_sig.write(20);
        wait(1, SC_NS);
        print_cycle_header("Read from index 20");
        print_signals("Verify written data");
        
        sc_uint16 val_20 = Data_Out_sig.read();
        
        cycle_count++;
        index_sig.write(30);
        wait(1, SC_NS);
        print_cycle_header("Read from index 30");
        print_signals("Verify written data");
        
        sc_uint16 val_30 = Data_Out_sig.read();
        
        std::cout << "\n[RESULT]";
        if (val_10 == 0x2222 && val_20 == 0x3333 && val_30 == 0x4444) {
            std::cout << " [PASS] Multiple writes successful" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Data mismatch in multiple writes" << std::endl;
        }

        // ===== Test 4: Reset Function =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 4: Reset Function" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        std::cout << "Before reset: reading from indices 0, 10, 20, 30" << std::endl;
        
        cycle_count++;
        write_sig.write(0);
        index_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("Read index 0 before reset");
        print_signals("Should have 0x1234");
        
        cycle_count++;
        reset_sig.write(1);
        wait(10, SC_NS);
        print_cycle_header("Reset signal asserted");
        print_signals("Reset signal high - clearing buffer");
        
        cycle_count++;
        reset_sig.write(0);
        index_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("Read after reset");
        print_signals("Buffer should be cleared");
        
        sc_uint16 after_reset_idx0 = Data_Out_sig.read();
        
        cycle_count++;
        index_sig.write(10);
        wait(1, SC_NS);
        print_cycle_header("Read index 10 after reset");
        print_signals("Should be 0x0000");
        
        sc_uint16 after_reset_idx10 = Data_Out_sig.read();
        
        std::cout << "\n[RESULT]";
        if (after_reset_idx0 == 0 && after_reset_idx10 == 0) {
            std::cout << " [PASS] Reset works correctly" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Reset failed" << std::endl;
        }

        // ===== Test 5: High Address Access =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 5: High Address Access (Index 1023)" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        write_sig.write(1);
        index_sig.write(1023);
        Data_In_sig.write(0x9999);
        wait(10, SC_NS);
        print_cycle_header("Write to index 1023");
        print_signals("Write signal high - maximum index");
        
        cycle_count++;
        write_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("Read from index 1023");
        print_signals("Verify high address");
        
        std::cout << "\n[RESULT]";
        if (Data_Out_sig.read() == 0x9999) {
            std::cout << " [PASS] High address access works" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] High address access failed" << std::endl;
        }

        // ===== Test 6: Maximum Value =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 6: Write Maximum Value (0xFFFF)" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        write_sig.write(1);
        index_sig.write(100);
        Data_In_sig.write(0xFFFF);
        wait(10, SC_NS);
        print_cycle_header("Write 0xFFFF");
        print_signals("Maximum uint16 value - write signal high");
        
        cycle_count++;
        write_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("Read maximum value");
        print_signals("Should be 0xFFFF");
        
        std::cout << "\n[RESULT]";
        if (Data_Out_sig.read() == 0xFFFF) {
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
    Output_Buffer_TestBench tb("Output_Buffer_TestBench");
    sc_start();
    return 0;
}
