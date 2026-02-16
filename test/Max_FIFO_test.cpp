#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include "Max_FIFO.h"
#include "../src/Max_FIFO.cpp"

SC_MODULE(Max_FIFO_TestBench) {
    sc_clock clk_sig{"clk_sig", 10, SC_NS};
    sc_signal<bool>   rst_sig;
    sc_signal<bool>   write_en_sig;
    sc_signal<bool>   read_en_sig;
    sc_signal<bool>   clear_sig;
    sc_signal<sc_dt::sc_uint<16>> data_in_sig;
    sc_signal<sc_dt::sc_uint<16>> data_out_sig;
    sc_signal<bool>   full_sig;
    sc_signal<bool>   empty_sig;

    Max_FIFO *dut;
    int cycle_count = 0;
    int write_count = 0;
    int read_count = 0;
    std::vector<sc_dt::sc_uint<16>> fifo_contents;

    SC_CTOR(Max_FIFO_TestBench) {
        dut = new Max_FIFO("Max_FIFO_DUT");
        dut->clk(clk_sig);
        dut->rst(rst_sig);
        dut->write_en(write_en_sig);
        dut->read_en(read_en_sig);
        dut->clear(clear_sig);
        dut->data_in(data_in_sig);
        dut->data_out(data_out_sig);
        dut->full(full_sig);
        dut->empty(empty_sig);

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

    void print_fifo_status() {
        std::cout << "\n[FIFO STATUS]" << std::endl;
        std::cout << "  Total Writes: " << write_count << " | Total Reads: " << read_count << std::endl;
        std::cout << "  Occupancy: " << fifo_contents.size() << "/1024" << std::endl;
        std::cout << "  Full: " << (full_sig.read() ? "YES" : "NO") << " | Empty: " << (empty_sig.read() ? "YES" : "NO") << std::endl;
    }

    void print_signals(const std::string& operation = "") {
        std::cout << std::left << std::setw(25) << "[CONTROL SIGNALS]" << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Reset:" 
                  << (rst_sig.read() ? "ACTIVE" : "inactive") << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Write Enable:" 
                  << (write_en_sig.read() ? "Active" : "inactive") << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Read Enable:" 
                  << (read_en_sig.read() ? "Active" : "inactive") << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Clear:" 
                  << (clear_sig.read() ? "Active" : "inactive") << std::endl;
        
        std::cout << std::left << std::setw(25) << "[DATA SIGNALS]" << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Data In:" 
                  << "0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << (int)data_in_sig.read() << std::dec << std::setfill(' ') << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Data Out:" 
                  << "0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << (int)data_out_sig.read() << std::dec << std::setfill(' ') << std::endl;
        
        std::cout << std::left << std::setw(25) << "[STATUS FLAGS]" << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Full Flag:" 
                  << (full_sig.read() ? "1 (FULL)" : "0 (not full)") << std::endl;
        std::cout << "  " << std::left << std::setw(18) << "Empty Flag:" 
                  << (empty_sig.read() ? "1 (EMPTY)" : "0 (has data)") << std::endl;
        
        if (!operation.empty()) {
            std::cout << "\n[OPERATION]: " << operation << std::endl;
        }
    }

    void test_stimulus() {
        int test_count = 0;
        int passed = 0;

        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "MAX_FIFO TESTBENCH - Dual-Port SRAM Based FIFO (1024 entries, 16-bit FP16 data)" << std::endl;
        std::cout << "Architecture: Standalone SRAM instance with separate read/write pointers" << std::endl;
        std::cout << "Data Type: FP16 (Half-precision floating point) | Latency: 1-cycle pipeline" << std::endl;
        std::cout << std::string(120, '=') << std::endl;

        // Test 1: Initial State
        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "TEST 1: Initial State Verification - FIFO Should Be Empty" << std::endl;
        std::cout << std::string(120, '=') << std::endl;
        test_count++;
        cycle_count++;
        
        rst_sig.write(0);
        write_en_sig.write(0);
        read_en_sig.write(0);
        clear_sig.write(0);
        
        wait(15, SC_NS);
        print_cycle_header("Verify initial FIFO state");
        print_signals("Checking empty flag and no data");
        print_fifo_status();
        
        std::cout << "\n[VERIFICATION]";
        if (empty_sig.read() && !full_sig.read()) {
            std::cout << " ✓ PASS - FIFO correctly initialized to empty state" << std::endl;
            passed++;
        } else {
            std::cout << " ✗ FAIL - FIFO not in expected initial state" << std::endl;
        }

        // Test 2: Single Write
        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "TEST 2: Write Single Value - Testing Data Entry" << std::endl;
        std::cout << "Value: 0x4100 (FP16 ~2.0)" << std::endl;
        std::cout << std::string(120, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        write_en_sig.write(1);
        data_in_sig.write(0x4100);
        print_cycle_header("Enable write and apply data");
        print_signals("Writing FP16 value 0x4100 (~2.0) to FIFO");
        fifo_contents.push_back(0x4100);
        write_count++;
        print_fifo_status();
        
        wait(10, SC_NS);
        
        cycle_count++;
        write_en_sig.write(0);
        wait(5, SC_NS);
        print_cycle_header("Write pulse completed");
        print_signals("Check FIFO state after write");
        print_fifo_status();
        
        std::cout << "\n[VERIFICATION]";
        if (!empty_sig.read() && !full_sig.read()) {
            std::cout << " ✓ PASS - Data accepted into FIFO" << std::endl;
            passed++;
        } else {
            std::cout << " ✗ FAIL - Write operation failed" << std::endl;
        }

        // Test 3: Read Operation with Latency
        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "TEST 3: Read Operation - Testing 1-Cycle Pipeline Latency" << std::endl;
        std::cout << "SRAM has one cycle pipeline delay for read data" << std::endl;
        std::cout << std::string(120, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        read_en_sig.write(1);
        print_cycle_header("Enable read (1-cycle latency begins)");
        print_signals("Initiating read from FIFO (data appears next cycle)");
        
        wait(10, SC_NS);
        
        cycle_count++;
        read_en_sig.write(0);
        wait(5, SC_NS);
        print_cycle_header("Read data available after pipeline delay");
        print_signals("Data is now present on output port");
        
        sc_dt::sc_uint<16> read_data = data_out_sig.read();
        std::cout << "\n[DATA READ] Value: 0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << (int)read_data << std::dec << std::setfill(' ') << " (FP16 ~2.0)" << std::endl;
        read_count++;
        print_fifo_status();
        
        std::cout << "\n[VERIFICATION]";
        if (read_data == 0x4100) {
            std::cout << " ✓ PASS - Correct data read (0x4100 matches written value)" << std::endl;
            passed++;
        } else {
            std::cout << " ✗ FAIL - Read data mismatch" << std::endl;
        }

        // Test 4: Multiple Writes
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 4: Write Multiple Values" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        write_en_sig.write(1);
        for (int i = 0; i < 3; i++) {
            data_in_sig.write(0x4200 + (i << 8));
            wait(10, SC_NS);
        }
        write_en_sig.write(0);
        wait(5, SC_NS);
        
        std::cout << "\n[RESULT]";
        if (!empty_sig.read()) {
            std::cout << " [PASS]" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL]" << std::endl;
        }

        // Test 5: Clear Operation
        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "TEST 5: Clear Operation - Testing FIFO Reset Functionality" << std::endl;
        std::cout << "Clear synchronously resets read pointer to write pointer (empties FIFO)" << std::endl;
        std::cout << std::string(120, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        clear_sig.write(1);
        print_cycle_header("Assert clear signal");
        print_signals("Clearing all buffered data (synchronous reset)");
        print_fifo_status();
        
        wait(10, SC_NS);
        
        cycle_count++;
        clear_sig.write(0);
        wait(5, SC_NS);
        print_cycle_header("Clear signal deasserted");
        print_signals("FIFO should be empty now");
        fifo_contents.clear();
        print_fifo_status();
        
        std::cout << "\n[VERIFICATION]";
        if (empty_sig.read()) {
            std::cout << " ✓ PASS - Clear operation successful, FIFO now empty" << std::endl;
            passed++;
        } else {
            std::cout << " ✗ FAIL - Clear operation failed" << std::endl;
        }

        // Test 6: Reset Operation
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 6: Reset Operation" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        write_en_sig.write(1);
        data_in_sig.write(0x5000);
        wait(10, SC_NS);
        
        cycle_count++;
        write_en_sig.write(0);
        wait(5, SC_NS);
        
        cycle_count++;
        rst_sig.write(1);
        wait(10, SC_NS);
        
        cycle_count++;
        rst_sig.write(0);
        wait(5, SC_NS);
        
        std::cout << "\n[RESULT]";
        if (empty_sig.read() && !full_sig.read()) {
            std::cout << " [PASS]" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL]" << std::endl;
        }

        // Test Summary
        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << std::string(120, '=') << std::endl;
        std::cout << "Tests Passed:            " << passed << "/" << test_count << std::endl;
        std::cout << "Total Write Operations:  " << write_count << std::endl;
        std::cout << "Total Read Operations:   " << read_count << std::endl;
        std::cout << "\nFIFO Specifications:" << std::endl;
        std::cout << "  Capacity:              1024 entries" << std::endl;
        std::cout << "  Data Width:            16 bits (FP16 format)" << std::endl;
        std::cout << "  Memory Type:           Dual-Port SRAM with separate read/write pointers" << std::endl;
        std::cout << "  Read Latency:          1 cycle (pipelined)" << std::endl;
        std::cout << "  Write Latency:         0 cycles (combinatorial)" << std::endl;
        std::cout << std::string(120, '=') << std::endl;
        
        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    Max_FIFO_TestBench tb("Max_FIFO_TestBench");
    sc_start();
    return 0;
}
