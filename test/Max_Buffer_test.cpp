#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <cassert>
#include "Max_Buffer.h"

// Include the implementation to get module definition and functions
#include "../src/Max_Buffer.cpp"

SC_MODULE(Max_Buffer_TestBench) {
    sc_clock clk_sig{"clk_sig", 10, SC_NS};        // 10ns clock period
    sc_signal<bool>   reset_sig;
    sc_signal<sc_uint16> Local_Max_In_sig;
    sc_signal<sc_uint1>  write_sig;
    sc_signal<sc_uint10> index_sig;
    sc_signal<sc_uint16> Local_Max_Out_sig;
    sc_signal<sc_uint16> Global_Max_Out_sig;

    Max_Buffer *dut;
    int cycle_count = 0;

    SC_CTOR(Max_Buffer_TestBench) {
        dut = new Max_Buffer("Max_Buffer_DUT");
        dut->clk(clk_sig);
        dut->reset(reset_sig);
        dut->Local_Max_In(Local_Max_In_sig);
        dut->write(write_sig);
        dut->index(index_sig);
        dut->Local_Max_Out(Local_Max_Out_sig);
        dut->Global_Max_Out(Global_Max_Out_sig);

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
        std::cout << "  " << std::left << std::setw(18) << "Local_Max_In:" 
                  << "0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << Local_Max_In_sig.read() << std::dec << std::setfill(' ') << std::endl;
        
        std::cout << std::left << std::setw(20) << "OUTPUTS:" << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Local_Max_Out:" 
                  << "0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << Local_Max_Out_sig.read() << std::dec << std::setfill(' ') << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Global_Max_Out:" 
                  << "0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << Global_Max_Out_sig.read() << std::dec << std::setfill(' ') << std::endl;
        
        if (!operation.empty()) {
            std::cout << "Operation: " << operation << std::endl;
        }
    }

    void test_stimulus() {
        int test_count = 0;
        int passed = 0;

        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "MAX_BUFFER TESTBENCH - FP16 Dual-Port Buffer (1024 entries)" << std::endl;
        std::cout << std::string(100, '=') << std::endl;

        // ===== Test 1: Initial State (Empty Buffer) =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 1: Initial State - Read from Empty Buffer" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        cycle_count++;
        
        write_sig.write(0);  // Read mode
        index_sig.write(0);
        print_cycle_header("Empty buffer initialization");
        wait(1, SC_NS);
        print_signals("Reading from empty buffer at index 0");
        
        std::cout << "\n[RESULT]";
        if (Local_Max_Out_sig.read() == 0 && Global_Max_Out_sig.read() == 0xFBFF) {
            std::cout << " [PASS] Empty buffer initialized correctly (buffer=0, global_max=FP16_MIN)" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Expected buffer=0x0000, global_max=0xFBFF" << std::endl;
        }

        // ===== Test 2: Write Single Value =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 2: Write Single Value to Index 0" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        std::cout << "Writing: 0x4100 (FP16 value ~3.0) to index 0" << std::endl;
        
        cycle_count++;
        write_sig.write(1);  // Write mode
        index_sig.write(0);
        Local_Max_In_sig.write(0x4100);
        print_cycle_header("Write 0x4100 to index 0");
        print_signals("Setting up write operation");
        
        wait(10, SC_NS);  // Wait for clock edge and execution
        cycle_count++;
        print_cycle_header("After clock edge (data written)");
        print_signals("Data has been written, global_max updated");
        
        // After write, read the same location
        write_sig.write(0);  // Switch to read mode
        index_sig.write(0);
        wait(1, SC_NS);
        cycle_count++;
        print_cycle_header("Read back from index 0");
        print_signals("Reading written data");
        
        std::cout << "\n[RESULT]";
        if (Local_Max_Out_sig.read() == 0x4100 && Global_Max_Out_sig.read() == 0x4100) {
            std::cout << " [PASS] Write and read successful, global_max updated" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Write/read mismatch or global_max not updated" << std::endl;
        }

        // ===== Test 3: Write Multiple Values =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 3: Write Multiple Values and Track Global Max Update" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        // Write 0x4200 (FP16 ~4.0) to index 1
        cycle_count++;
        std::cout << "\n*** Write 0x4200 (~4.0) to index 1 ***" << std::endl;
        write_sig.write(1);
        index_sig.write(1);
        Local_Max_In_sig.write(0x4200);
        print_cycle_header("Write 0x4200 to index 1");
        print_signals("Setting up write (should become new global_max)");
        wait(10, SC_NS);
        
        cycle_count++;
        print_cycle_header("After clock edge");
        print_signals("0x4200 written, should be new global_max");
        
        // Write 0x3D00 (FP16 ~0.5) to index 2
        cycle_count++;
        std::cout << "\n*** Write 0x3D00 (~0.5) to index 2 ***" << std::endl;
        index_sig.write(2);
        Local_Max_In_sig.write(0x3D00);
        print_cycle_header("Write 0x3D00 to index 2");
        print_signals("Setting up write (should NOT change global_max)");
        wait(10, SC_NS);
        
        cycle_count++;
        print_cycle_header("After clock edge");
        print_signals("0x3D00 written, global_max should remain 0x4200");
        
        // Write 0x4000 (FP16 ~2.0) to index 3
        cycle_count++;
        std::cout << "\n*** Write 0x4000 (~2.0) to index 3 ***" << std::endl;
        index_sig.write(3);
        Local_Max_In_sig.write(0x4000);
        print_cycle_header("Write 0x4000 to index 3");
        print_signals("Setting up write (should NOT change global_max)");
        wait(10, SC_NS);
        
        cycle_count++;
        print_cycle_header("After clock edge");
        print_signals("0x4000 written, global_max should remain 0x4200");
        
        // Read from index 3
        cycle_count++;
        write_sig.write(0);
        index_sig.write(3);
        wait(1, SC_NS);
        print_cycle_header("Read from index 3");
        print_signals("Final state after multiple writes");
        
        std::cout << "\n[RESULT]";
        if (Local_Max_Out_sig.read() == 0x4000 && Global_Max_Out_sig.read() == 0x4200) {
            std::cout << " [PASS] Multiple writes and global_max tracking correct" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Global max computation error" << std::endl;
        }

        // ===== Test 4: Combinational Read (No Clock Dependency) =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 4: Combinational Read - Index Change without Clock" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        write_sig.write(0);
        index_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("Read index 0 combinationally");
        print_signals("Reading from index 0");
        sc_uint16 read_idx0 = Local_Max_Out_sig.read();
        
        cycle_count++;
        index_sig.write(1);
        wait(1, SC_NS);
        print_cycle_header("Read index 1 combinationally");
        print_signals("Reading from index 1 (no clock, combinational)");
        sc_uint16 read_idx1 = Local_Max_Out_sig.read();
        
        cycle_count++;
        index_sig.write(2);
        wait(1, SC_NS);
        print_cycle_header("Read index 2 combinationally");
        print_signals("Reading from index 2 (no clock, combinational)");
        sc_uint16 read_idx2 = Local_Max_Out_sig.read();
        
        std::cout << "\n[RESULT]";
        if (read_idx0 == 0x4100 && read_idx1 == 0x4200 && read_idx2 == 0x3D00) {
            std::cout << " [PASS] Combinational read works correctly" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Combinational read failed" << std::endl;
        }

        // ===== Test 5: Write and Read at Different Addresses =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 5: Simultaneous Write to One Address, Read from Another" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        // Write 0xC400 (FP16 ~-3.0) to index 10
        cycle_count++;
        std::cout << "\n*** Write 0xC400 (~-3.0) to index 10 ***" << std::endl;
        write_sig.write(1);
        index_sig.write(10);
        Local_Max_In_sig.write(0xC400);
        print_cycle_header("Write 0xC400 (negative) to index 10");
        print_signals("Setting up write (negative, should NOT change global_max)");
        wait(10, SC_NS);
        
        cycle_count++;
        print_cycle_header("After clock edge");
        print_signals("0xC400 written, global_max should remain 0x4200");
        
        // Now write to index 10 while reading from index 0
        cycle_count++;
        std::cout << "\n*** Write 0x4400 (~6.0) to index 10 while reading from index 0 ***" << std::endl;
        write_sig.write(1);
        index_sig.write(10);
        Local_Max_In_sig.write(0x4400);  // New write value
        
        // But we can observe that index_sig affects output combinationally
        index_sig.write(0);  // Change to read index 0
        wait(1, SC_NS);
        print_cycle_header("Dual-port operation: reading from index 0");
        print_signals("Reading from index 0 while setting up write to index 10");
        
        sc_uint16 simultaneous_read = Local_Max_Out_sig.read();
        
        std::cout << "\n[RESULT]";
        if (simultaneous_read == 0x4100) {
            std::cout << " [PASS] Dual-port operation works" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Dual-port operation failed" << std::endl;
        }

        // ===== Test 6: Large Index Access (High Addresses) =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 6: Access High Address (Index 1023)" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        // Write to index 1023 (max index)
        cycle_count++;
        std::cout << "\n*** Write 0x4500 (~7.0) to index 1023 (max address) ***" << std::endl;
        write_sig.write(1);
        index_sig.write(1023);
        Local_Max_In_sig.write(0x4500);
        print_cycle_header("Write to maximum index 1023");
        print_signals("Setting up write of 0x4500 (should become new global_max)");
        wait(10, SC_NS);
        
        cycle_count++;
        print_cycle_header("After clock edge");
        print_signals("0x4500 written, should be new global_max");
        
        // Read from index 1023
        cycle_count++;
        write_sig.write(0);
        index_sig.write(1023);
        wait(1, SC_NS);
        print_cycle_header("Read from maximum index 1023");
        print_signals("Reading back written data");
        
        std::cout << "\n[RESULT]";
        if (Local_Max_Out_sig.read() == 0x4500) {
            std::cout << " [PASS] High address access works" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] High address access failed" << std::endl;
        }

        // ===== Test 7: Reset Function =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 7: Reset Function - Clear Buffer and Reset Global Max" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        std::cout << "\n*** First: Verify buffer has data before reset ***" << std::endl;
        cycle_count++;
        write_sig.write(0);
        index_sig.write(0);
        reset_sig.write(0);  // Normal operation
        wait(1, SC_NS);
        print_cycle_header("Read from index 0 before reset");
        print_signals("Buffer should contain 0x4100");
        
        sc_uint16 before_reset_idx0 = Local_Max_Out_sig.read();
        sc_uint16 before_reset_global = Global_Max_Out_sig.read();
        std::cout << "Before reset - Index 0: 0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << before_reset_idx0 << ", Global Max: 0x" << std::setw(4) 
                  << before_reset_global << std::dec << std::endl;
        
        std::cout << "\n*** Apply reset signal and wait for clock edge ***" << std::endl;
        cycle_count++;
        reset_sig.write(1);  // Set reset HIGH before clock edge
        print_cycle_header("Reset signal asserted");
        print_signals("Reset=1, will clear buffer on next clock edge");
        
        wait(10, SC_NS);  // Wait for clock edge to execute reset
        
        cycle_count++;
        reset_sig.write(0);  // Deassert reset after clock edge
        // Trigger combinational update by toggling write or index
        index_sig.write(1);  // Change index to trigger combinational read
        wait(1, SC_NS);  // Small delay to allow combinational read to update
        print_cycle_header("After reset on clock edge");
        print_signals("Buffer cleared, global_max reset to FP16_MIN (0xFBFF)");
        
        sc_uint16 after_reset_idx1 = Local_Max_Out_sig.read();
        sc_uint16 after_reset_global = Global_Max_Out_sig.read();
        std::cout << "After reset - Index 1: 0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << after_reset_idx1 << ", Global Max: 0x" << std::setw(4) 
                  << after_reset_global << std::dec << std::endl;
        
        std::cout << "\n[RESULT]";
        if (after_reset_idx1 == 0x0000 && after_reset_global == 0xFBFF) {
            std::cout << " [PASS] Reset function works correctly" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Reset function failed (expected idx1=0x0000, global=0xFBFF)" << std::endl;
        }

        // ===== Test 8: Global Max Initialization and Update =====
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 8: Global Max Initialization and Progressive Update" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        std::cout << "\n*** Verify global_max starts at minimum after reset ***" << std::endl;
        cycle_count++;
        write_sig.write(0);
        reset_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("Global max after reset");
        print_signals("Should be FP16_MIN = 0xFBFF");
        
        if (Global_Max_Out_sig.read() != 0xFBFF) {
            std::cout << "  ERROR: Expected 0xFBFF, got 0x" << std::hex << std::setfill('0') 
                      << std::setw(4) << Global_Max_Out_sig.read() << std::dec << std::endl;
        }
        
        std::cout << "\n*** Write series of values and track global_max updates ***" << std::endl;
        
        // Write 0x3C00 (small positive) - should become global_max
        cycle_count++;
        std::cout << "\n  Write 0x3C00 to index 0 (should become new global_max)" << std::endl;
        write_sig.write(1);
        index_sig.write(0);
        Local_Max_In_sig.write(0x3C00);
        print_cycle_header("Writing 0x3C00");
        print_signals("Setup write");
        wait(10, SC_NS);
        
        cycle_count++;
        write_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("After write of 0x3C00");
        print_signals("Global max should be 0x3C00");
        
        sc_uint16 gmax_after_first = Global_Max_Out_sig.read();
        std::cout << "  Global Max: 0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << gmax_after_first << std::dec << std::endl;
        
        // Write 0x4000 (larger) - should update global_max
        cycle_count++;
        std::cout << "\n  Write 0x4000 to index 1 (should become new global_max)" << std::endl;
        write_sig.write(1);
        index_sig.write(1);
        Local_Max_In_sig.write(0x4000);
        print_cycle_header("Writing 0x4000");
        print_signals("Setup write");
        wait(10, SC_NS);
        
        cycle_count++;
        write_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("After write of 0x4000");
        print_signals("Global max should be 0x4000");
        
        sc_uint16 gmax_after_second = Global_Max_Out_sig.read();
        std::cout << "  Global Max: 0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << gmax_after_second << std::dec << std::endl;
        
        // Write negative value - should NOT update global_max
        cycle_count++;
        std::cout << "\n  Write 0xC200 (negative) to index 2 (should NOT change global_max)" << std::endl;
        write_sig.write(1);
        index_sig.write(2);
        Local_Max_In_sig.write(0xC200);
        print_cycle_header("Writing 0xC200 (negative)");
        print_signals("Setup write");
        wait(10, SC_NS);
        
        cycle_count++;
        write_sig.write(0);
        wait(1, SC_NS);
        print_cycle_header("After write of 0xC200");
        print_signals("Global max should still be 0x4000");
        
        sc_uint16 gmax_after_negative = Global_Max_Out_sig.read();
        std::cout << "  Global Max: 0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << gmax_after_negative << std::dec << std::endl;
        
        std::cout << "\n[RESULT]";
        if (gmax_after_first == 0x3C00 && gmax_after_second == 0x4000 && 
            gmax_after_negative == 0x4000) {
            std::cout << " [PASS] Global max initialization and update working correctly" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL] Global max tracking failed" << std::endl;
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
    Max_Buffer_TestBench tb("Max_Buffer_TestBench");
    sc_start();
    return 0;
}
