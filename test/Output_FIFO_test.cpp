#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include "Output_FIFO.h"
#include "../src/Output_FIFO.cpp"

SC_MODULE(Output_FIFO_TestBench) {
    sc_clock clk_sig{"clk_sig", 10, SC_NS};
    sc_signal<bool>   rst_sig;
    sc_signal<bool>   write_en_sig;
    sc_signal<bool>   read_en_sig;
    sc_signal<bool>   clear_sig;
    sc_signal<sc_dt::sc_uint<16>> data_in_sig;
    sc_signal<sc_dt::sc_uint<16>> data_out_sig;
    sc_signal<bool>   full_sig;
    sc_signal<bool>   empty_sig;

    Output_FIFO *dut;
    int cycle_count = 0;
    int write_count = 0;
    int read_count = 0;
    std::vector<sc_dt::sc_uint<16>> fifo_contents;

    SC_CTOR(Output_FIFO_TestBench) {
        dut = new Output_FIFO("Output_FIFO_DUT");
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
        std::cout << "\n" << std::string(120, '-') << std::endl;
        std::cout << "CYCLE #" << cycle_count << " | Time: " << sc_time_stamp() << std::endl;
        if (!description.empty()) {
            std::cout << "Description: " << description << std::endl;
        }
        std::cout << std::string(120, '-') << std::endl;
    }

    void print_fifo_status() {
        std::cout << "\n[FIFO INTERNAL STATUS]" << std::endl;
        std::cout << "  Total Writes Performed: " << write_count << std::endl;
        std::cout << "  Total Reads Performed: " << read_count << std::endl;
        std::cout << "  Current Occupancy: " << fifo_contents.size() << "/1024 entries" << std::endl;
        std::cout << "  Full Flag: " << (full_sig.read() ? "HIGH (FULL)" : "LOW (not full)") << std::endl;
        std::cout << "  Empty Flag: " << (empty_sig.read() ? "HIGH (EMPTY)" : "LOW (has data)") << std::endl;
    }

    void print_signals(const std::string& operation = "") {
        std::cout << std::left << std::setw(30) << "[CONTROL SIGNALS]" << std::endl;
        std::cout << "  Reset:               " << (rst_sig.read() ? "ACTIVE" : "inactive") << std::endl;
        std::cout << "  Write Enable:        " << (write_en_sig.read() ? "Active" : "inactive") << std::endl;
        std::cout << "  Read Enable:         " << (read_en_sig.read() ? "Active" : "inactive") << std::endl;
        std::cout << "  Clear:               " << (clear_sig.read() ? "Active" : "inactive") << std::endl;
        
        std::cout << std::left << std::setw(30) << "[DATA SIGNALS]" << std::endl;
        std::cout << "  Data In:             0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << (int)data_in_sig.read() << std::dec << std::setfill(' ') << std::endl;
        std::cout << "  Data Out:            0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << (int)data_out_sig.read() << std::dec << std::setfill(' ') << std::endl;
        
        std::cout << std::left << std::setw(30) << "[STATUS FLAGS]" << std::endl;
        std::cout << "  Full Flag:           " << (full_sig.read() ? "1 (FULL)" : "0 (not full)") << std::endl;
        std::cout << "  Empty Flag:          " << (empty_sig.read() ? "1 (EMPTY)" : "0 (has data)") << std::endl;
        
        if (!operation.empty()) {
            std::cout << "\n[OPERATION]: " << operation << std::endl;
        }
    }

    void test_stimulus() {
        int test_count = 0;
        int passed = 0;

        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "OUTPUT_FIFO TESTBENCH - Dual-Port SRAM Based FIFO (1024 entries, 16-bit uint16 data)" << std::endl;
        std::cout << "Architecture: Standalone SRAM instance with separate read/write pointers" << std::endl;
        std::cout << "Data Type: 16-bit Unsigned Integer | Latency: 1-cycle pipeline" << std::endl;
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
            std::cout << " \u2713 PASS - FIFO correctly initialized to empty state" << std::endl;
            passed++;
        } else {
            std::cout << " \u2717 FAIL - FIFO not in expected initial state" << std::endl;
        }

        // Test 2: Single Write
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 2: Write Single Value (0x1234)" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        write_en_sig.write(1);
        data_in_sig.write(0x1234);
        print_cycle_header("Setup write 0x1234");
        wait(10, SC_NS);
        
        cycle_count++;
        write_en_sig.write(0);
        wait(5, SC_NS);
        print_cycle_header("After write");
        
        std::cout << "\n[RESULT]";
        if (!empty_sig.read() && !full_sig.read()) {
            std::cout << " [PASS]" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL]" << std::endl;
        }

        // Test 3: Read Operation
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 3: Read Value (1-Cycle Latency)" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        read_en_sig.write(1);
        print_cycle_header("Enable read");
        wait(10, SC_NS);
        
        cycle_count++;
        read_en_sig.write(0);
        wait(5, SC_NS);
        print_cycle_header("Read data available");
        
        sc_dt::sc_uint<16> read_data = data_out_sig.read();
        std::cout << "Read Data: 0x" << std::hex << std::setfill('0') << std::setw(4) 
                  << (int)read_data << std::dec << std::endl;
        
        std::cout << "\n[RESULT]";
        if (read_data == 0x1234) {
            std::cout << " [PASS]" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL]" << std::endl;
        }

        // Test 4: Multiple Writes
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 4: Write Multiple Values" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        write_en_sig.write(1);
        data_in_sig.write(0xABCD);
        wait(10, SC_NS);
        
        cycle_count++;
        data_in_sig.write(0x5678);
        wait(10, SC_NS);
        
        cycle_count++;
        data_in_sig.write(0x9ABC);
        wait(10, SC_NS);
        
        cycle_count++;
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
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 5: Clear Operation" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        clear_sig.write(1);
        wait(10, SC_NS);
        
        cycle_count++;
        clear_sig.write(0);
        wait(5, SC_NS);
        
        std::cout << "\n[RESULT]";
        if (empty_sig.read()) {
            std::cout << " [PASS]" << std::endl;
            passed++;
        } else {
            std::cout << " [FAIL]" << std::endl;
        }

        // Test 6: Reset Operation
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST 6: Reset Operation" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        test_count++;
        
        cycle_count++;
        write_en_sig.write(1);
        data_in_sig.write(0x4444);
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
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "TEST SUMMARY: " << passed << "/" << test_count << " passed" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        
        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    Output_FIFO_TestBench tb("Output_FIFO_TestBench");
    sc_start();
    return 0;
}
