#include <systemc>
#include <iostream>
#include <iomanip>

#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>
#include <set>
#include "../include/SOLE.h"
#include "../include/SOLE_MMIO.hpp"
#include "../Csim/Softmax.h"
#include "test_utils.h"


using namespace sc_core;
using namespace sc_dt;
using namespace std;
using namespace sole::mmio;

#define AXI_DEBUG 1

// ===== Constants for Testing =====
#define AXI_ADDR_WIDTH 32
#define AXI_DATA_WIDTH 64
#define AXI_STRB_WIDTH 8
#define TEST_ADDR_BASE 0x0000
#define TEST_DATA_SIZE 1024  // Increased to accommodate word_idx 0-524 (input at 100-124, output at 500-524)

void output_memory_to_log(std::ostream* log, const sc_dt::sc_uint<64> memory[],int start_word, size_t length) {
    if (log && log->good()) {
        (*log) << "\n[MEMORY DUMP]\n Address(word) | Data (Hex)\n";
        for (size_t i = 0; i < length; i++) {
            uint64_t data = memory[start_word + i].to_uint64();
            (*log) << "   " << dec << setfill(' ') << setw(5) << (start_word + i) 
            << "  :  0x " << hex << setfill('0') << setw(4) << (uint16_t)((data >> 48) & 0xFFFF) << " " 
            << setw(4) << (uint16_t)((data >> 32) & 0xFFFF) << " " 
            << setw(4) << (uint16_t)((data >> 16) & 0xFFFF) << " " 
            << setw(4) << (uint16_t)(data & 0xFFFF) << dec << "\n";
        }
    }
}

void output_SOLE_mmio_to_log(std::ostream* log, SOLE* dut) { 
    if (log && log->good()) {
        (*log) << "\n[SOLE MMIO DUMP]\n";
        (*log) << "Control Register: 0x" << hex << setfill('0') << setw(8) 
               << dut->reg_control.read().to_uint() << dec << setfill(' ') << "\n";
        (*log) << "Status Register: 0x" << hex << setfill('0') << setw(8) 
               << dut->reg_status.read().to_uint() << dec << setfill(' ') << "\n";
        (*log) << "Src Addr Base Low: 0x" << hex << setfill('0') << setw(8) 
               << dut->reg_src_addr_base_l.read().to_uint() << dec << setfill(' ') << "\n";
        (*log) << "Src Addr Base High: 0x" << hex << setfill('0') << setw(8) 
               << dut->reg_src_addr_base_h.read().to_uint() << dec << setfill(' ') << "\n";
        (*log) << "Src Addr Base (64-bit): 0x" << hex << setfill('0') << setw(16)
               << ((uint64_t)dut->reg_src_addr_base_h.read().to_uint() << 32 | dut->reg_src_addr_base_l.read().to_uint()) 
               << dec << setfill(' ') << "\n";
        (*log) << "Dst Addr Base Low: 0x" << hex << setfill('0') << setw(8) 
               << dut->reg_dst_addr_base_l.read().to_uint() << dec << setfill(' ') << "\n";
        (*log) << "Dst Addr Base High: 0x" << hex << setfill('0') << setw(8) 
               << dut->reg_dst_addr_base_h.read().to_uint() << dec << setfill(' ') << "\n";
        (*log) << "Dst Addr Base (64-bit): 0x" << hex << setfill('0') << setw(16)
               << ((uint64_t)dut->reg_dst_addr_base_h.read().to_uint() << 32 | dut->reg_dst_addr_base_l.read().to_uint()) 
               << dec << setfill(' ') << "\n";
        (*log) << "Data Length Low: 0x" << hex << setfill('0') << setw(8) 
               << dut->reg_length_l.read().to_uint() << dec << setfill(' ') << "\n";
        (*log) << "Data Length High: 0x" << hex << setfill('0') << setw(8) 
               << dut->reg_length_h.read().to_uint() << dec << setfill(' ') << "\n";
        (*log) << "Data Length (64-bit): 0x" << hex << setfill('0') << setw(16)
               << ((uint64_t)dut->reg_length_h.read().to_uint() << 32 | dut->reg_length_l.read().to_uint()) 
               << dec << setfill(' ') << "\n";
    }
}

// ===== Mock AXI4-Lite Slave Memory Model =====
/**
 * @class AxiSlaveMemory
 * @brief Simulates an AXI4-Lite slave device with internal memory storage
 * 
 * This module provides:
 * - Simple read/write memory with configurable base address
 * - AXI4-Lite slave protocol response (ready/valid handshaking)
 * - Automatic response generation for all AXI transactions
 */
SC_MODULE(AxiSlaveMemory) {
    // AXI4-Lite Slave Ports
    sc_in<bool>                               clk;
    sc_in<bool>                               rst;
    
    // Write Address Channel
    sc_in<sc_uint<AXI_ADDR_WIDTH>>            S_AXI_AWADDR;
    sc_in<bool>                               S_AXI_AWVALID;
    sc_out<bool>                              S_AXI_AWREADY;
    
    // Write Data Channel
    sc_in<sc_uint<AXI_DATA_WIDTH>>            S_AXI_WDATA;
    sc_in<sc_uint<AXI_STRB_WIDTH>>            S_AXI_WSTRB;
    sc_in<bool>                               S_AXI_WVALID;
    sc_out<bool>                              S_AXI_WREADY;
    
    // Write Response Channel
    sc_out<sc_uint<2>>                        S_AXI_BRESP;
    sc_out<bool>                              S_AXI_BVALID;
    sc_in<bool>                               S_AXI_BREADY;
    
    // Read Address Channel
    sc_in<sc_uint<AXI_ADDR_WIDTH>>            S_AXI_ARADDR;
    sc_in<bool>                               S_AXI_ARVALID;
    sc_out<bool>                              S_AXI_ARREADY;
    
    // Read Data Channel
    sc_out<sc_uint<AXI_DATA_WIDTH>>           S_AXI_RDATA;
    sc_out<sc_uint<2>>                        S_AXI_RRESP;
    sc_out<bool>                              S_AXI_RVALID;
    sc_in<bool>                               S_AXI_RREADY;
    
    // Internal memory storage (256 x 32-bit words)
    sc_uint<AXI_DATA_WIDTH> memory[TEST_DATA_SIZE];
    
    // Internal signals
    sc_signal<sc_uint<AXI_ADDR_WIDTH>>        write_addr;
    sc_signal<sc_uint<AXI_DATA_WIDTH>>        write_data;
    sc_signal<sc_uint<AXI_ADDR_WIDTH>>        read_addr;
    sc_signal<bool>                           write_pending;
    sc_signal<bool>                           read_pending;
    std::ostream*                             log_stream;
    
    SC_HAS_PROCESS(AxiSlaveMemory);
    
    AxiSlaveMemory(sc_module_name name) : sc_module(name) {
        // Initialize memory
        memset(memory, 0, sizeof(memory));
        log_stream = nullptr;
        
        // Register processes
        SC_METHOD(axi_write_process);
        sensitive << clk.pos() ;
        SC_METHOD(axi_read_process);
        sensitive << clk.pos() ;
    }

    void set_logger(std::ostream* os) {
        log_stream = os;
    }
    
    /**
     * @brief AXI Write Process
     * Handles AXI4-Lite write address, write data, and write response channels
     * 
     * Key Fix: 
     * - AXI address is in BYTES (8-byte aligned for 64-bit data)
     * - Convert byte address to 64-bit word index: word_idx = byte_addr >> 3
     * - Track address and data arrival independently with flags
     * - Execute write when BOTH address and data have arrived
     */
    void axi_write_process() {
        // Static state for write transaction tracking
        static sc_uint<32> write_addr_buf = 0;
        static sc_uint<64> write_data_buf = 0;
        static bool addr_received = false;
        static bool data_received = false;
        
        if (rst.read() == true) {  // If reset IS active (true), disable everything
            S_AXI_AWREADY.write(false);
            S_AXI_WREADY.write(false);
            S_AXI_BVALID.write(false);
            S_AXI_BRESP.write(0);
            addr_received = false;
            data_received = false;
            return;
        }
        
        // Normal operation: rst == false
        // Write Address Channel: Always ready to accept address
        S_AXI_AWREADY.write(true);
        
        // Write Data Channel: Always ready to accept data
        S_AXI_WREADY.write(true);
        
        // Capture address when handshake occurs
        bool aw_handshake = S_AXI_AWVALID.read() && S_AXI_AWREADY.read();
        bool w_handshake = S_AXI_WVALID.read() && S_AXI_WREADY.read();
        
        if (aw_handshake && !addr_received) {
            write_addr_buf = S_AXI_AWADDR.read();
            addr_received = true;
            if (log_stream && log_stream->good()) {
                uint32_t addr_val = (uint32_t)(write_addr_buf);
                if(AXI_DEBUG) (*log_stream) << "[AXI4_LITE][AW] byte_addr=0x" << std::hex << addr_val << std::dec << "\n";
            }
        }
        
        // Capture data when handshake occurs
        if (w_handshake && !data_received) {
            write_data_buf = S_AXI_WDATA.read();
            data_received = true;
            if (log_stream && log_stream->good()) {
                uint64_t data_val = write_data_buf.to_uint64();
                if(AXI_DEBUG) (*log_stream) << "[AXI4_LITE][W ] wdata=0x" << std::hex << data_val << std::dec << "\n";
            }
        }
        
        // Execute write transaction when BOTH address and data have been received
        if (addr_received && data_received) {
            // Convert BYTE ADDRESS to 64-bit WORD INDEX
            // Byte address >> 3 = word index (since each word is 8 bytes)
            uint32_t byte_addr = (uint32_t)(write_addr_buf);
            uint32_t word_idx = byte_addr >> 3;
            
            // Bounds check to prevent out-of-bounds writes
            if (word_idx >= TEST_DATA_SIZE) {
                if (log_stream && log_stream->good()) {
                    if(AXI_DEBUG) (*log_stream) << "[AXI4_LITE][WRITE_ERR] word_idx=" << word_idx
                                  << " exceeds TEST_DATA_SIZE=" << TEST_DATA_SIZE << "\n";
                } 
                S_AXI_BVALID.write(true);
                S_AXI_BRESP.write(2);  // SLVERR response
                if (S_AXI_BREADY.read()) {
                    addr_received = false;
                    data_received = false;
                }
                return;
            }
            
            sc_uint<64> data_to_write = write_data_buf;
            if (log_stream && log_stream->good()) {
                uint64_t data_val = data_to_write.to_uint64();
                if(AXI_DEBUG) (*log_stream) << "[AXI4_LITE][MEM_WRITE] word_idx=" << word_idx
                              << " byte_addr=0x" << std::hex << byte_addr
                              << " data=0x" << data_val << std::dec << "\n";
            }
            
            // Write 8 bytes of data to the specified word index
            memory[word_idx] = data_to_write;
            
            // Send write response
            S_AXI_BVALID.write(true);
            S_AXI_BRESP.write(0);  // OKAY response
            
            // Clear write transaction state when response is acknowledged
            if (S_AXI_BREADY.read()) {
                if (log_stream && log_stream->good()) {
                    if(AXI_DEBUG) (*log_stream) << "[AXI4_LITE][B ] bresp=" << S_AXI_BRESP.read() << "\n";
                }
                addr_received = false;
                data_received = false;
            }
        } else {
            S_AXI_BVALID.write(false);
        }
    }
    
    /**
     * @brief AXI Read Process
     * Handles AXI4-Lite read address and read data channels
     * 
     * Simplified behavior:
     * - Capture read address from S_AXI_ARADDR when valid
     * - On next clock cycle, return data immediately
     * - AXI address is in BYTES (8-byte aligned for 64-bit data)
     * - Convert byte address to 64-bit word index: word_idx = byte_addr >> 3
     */
    void axi_read_process() {
        static sc_uint<AXI_ADDR_WIDTH> addr_buf = 0;
        static bool has_addr = false;
        
        if (rst.read() == true) {  // If reset IS active (true), disable everything
            S_AXI_ARREADY.write(true);
            S_AXI_RVALID.write(false);
            S_AXI_RDATA.write(0);
            S_AXI_RRESP.write(0);
            has_addr = false;
            return;
        }
        
        // Normal operation: rst == false
        // Read Address Channel: Always ready to accept read address
        S_AXI_ARREADY.write(true);
        
        // Capture read address when read address handshake occurs
        if (S_AXI_ARVALID.read() && S_AXI_ARREADY.read()) {
            addr_buf = S_AXI_ARADDR.read();
            has_addr = true;

            if (log_stream && log_stream->good()) {
                uint32_t addr_val = (uint32_t)(addr_buf);
                if(AXI_DEBUG) {
                    (*log_stream) << sc_time_stamp() <<"[AXI4_LITE][AR] byte_addr=0x" << std::hex << addr_val
                                  << std::dec << " word_idx=" << (addr_val >> 3) << "\n";
                }
            }
        }
        
        // Read Data Channel: Return data from captured address
        if (has_addr) {
            uint32_t addr_val = (uint32_t)(addr_buf);
            uint32_t word_idx = addr_val >> 3;
            // Bounds check to prevent out-of-bounds reads
            if (word_idx >= TEST_DATA_SIZE) {
                throw std::runtime_error("Read address out of bounds in AxiSlaveMemory");
                if (log_stream && log_stream->good()) {
                    if(AXI_DEBUG) {
                        (*log_stream) << sc_time_stamp() << "[AXI4_LITE][READ_ERR] word_idx=" << word_idx
                                  << " exceeds TEST_DATA_SIZE=" << TEST_DATA_SIZE << "\n";
                    }
                }
                S_AXI_RDATA.write(0);
                S_AXI_RVALID.write(true);
                S_AXI_RRESP.write(2);  // SLVERR response for invalid address
                if (S_AXI_RREADY.read()) {
                    has_addr = false;
                }
            } else {
                // Read 8 bytes (64-bit word) from memory at word_idx
                sc_uint<64> data_to_read = memory[word_idx];
                S_AXI_RDATA.write(data_to_read);
                S_AXI_RVALID.write(true);
                S_AXI_RRESP.write(0);  // OKAY response
                if (log_stream && log_stream->good()) {
                    uint64_t rdata_val = data_to_read.to_uint64();
                    if(AXI_DEBUG) {
                        (*log_stream) << sc_time_stamp() << "[AXI4_LITE][R ] word_idx=" << word_idx
                                  << " rdata=0x" << std::hex << rdata_val
                                  << " rresp=" << S_AXI_RRESP.read() << std::dec << "\n";
                    }
                }
                
                // Clear address when response is accepted by master
                if (S_AXI_RREADY.read()) {
                    has_addr = false;
                }
            }
        } else {
            S_AXI_RVALID.write(false);
            S_AXI_RDATA.write(0);
        }
    }
};

// ===== SOLE TestBench =====
/**
 * @class SOLE_TestBench
 * @brief SystemC testbench for SOLE module verification
 * 
 * Tests:
 * 1. MMIO register write/read operations
 * 2. Basic SOLE control flow (start, done, busy signals)
 * 3. AXI4-Lite master interface handshaking
 * 4. Integration with Softmax engine
 */
SC_MODULE(SOLE_TestBench) {
    // Clock and Reset
    sc_signal<bool>                           clk;
    sc_signal<bool>                           rst;
    
    // MMIO Processor Interface Signals
    sc_signal<sc_uint<32>>                    proc_addr;
    sc_signal<sc_uint<32>>                    proc_wdata;
    sc_signal<bool>                           proc_we;
    sc_signal<sc_uint<32>>                    proc_rdata;
    
    // AXI4-Lite Master Interface Signals
    // Write Address Channel
    sc_signal<sc_uint<32>>                    M_AXI_AWADDR;
    sc_signal<bool>                           M_AXI_AWVALID;
    sc_signal<bool>                           M_AXI_AWREADY;
    
    // Write Data Channel
    sc_signal<sc_uint<64>>                    M_AXI_WDATA;
    sc_signal<sc_uint<8>>                     M_AXI_WSTRB;
    sc_signal<bool>                           M_AXI_WVALID;
    sc_signal<bool>                           M_AXI_WREADY;
    
    // Write Response Channel
    sc_signal<sc_uint<2>>                     M_AXI_BRESP;
    sc_signal<bool>                           M_AXI_BVALID;
    sc_signal<bool>                           M_AXI_BREADY;
    
    // Read Address Channel
    sc_signal<sc_uint<32>>                    M_AXI_ARADDR;
    sc_signal<bool>                           M_AXI_ARVALID;
    sc_signal<bool>                           M_AXI_ARREADY;
    
    // Read Data Channel
    sc_signal<sc_uint<64>>                    M_AXI_RDATA;
    sc_signal<sc_uint<2>>                     M_AXI_RRESP;
    sc_signal<bool>                           M_AXI_RVALID;
    sc_signal<bool>                           M_AXI_RREADY;
    
    // Module instances
    SOLE                                      *dut;
    AxiSlaveMemory                            *axi_slave;
    
    // Test statistics
    int test_total;
    int test_passed;
    int test_failed;
    
    SC_HAS_PROCESS(SOLE_TestBench);
    
    SOLE_TestBench(sc_module_name name) : sc_module(name), 
                                          test_total(0),
                                          test_passed(0), 
                                          test_failed(0) {
        // Instantiate DUT
        dut = new SOLE("SOLE_DUT");
        dut->clk(clk);
        dut->rst(rst);
        
        // MMIO Processor Interface
        dut->proc_addr(proc_addr);
        dut->proc_wdata(proc_wdata);
        dut->proc_we(proc_we);
        dut->proc_rdata(proc_rdata);
        
        // AXI4-Lite Master Interface
        dut->M_AXI_AWADDR(M_AXI_AWADDR);
        dut->M_AXI_AWVALID(M_AXI_AWVALID);
        dut->M_AXI_AWREADY(M_AXI_AWREADY);
        dut->M_AXI_WDATA(M_AXI_WDATA);
        dut->M_AXI_WSTRB(M_AXI_WSTRB);
        dut->M_AXI_WVALID(M_AXI_WVALID);
        dut->M_AXI_WREADY(M_AXI_WREADY);
        dut->M_AXI_BRESP(M_AXI_BRESP);
        dut->M_AXI_BVALID(M_AXI_BVALID);
        dut->M_AXI_BREADY(M_AXI_BREADY);
        dut->M_AXI_ARADDR(M_AXI_ARADDR);
        dut->M_AXI_ARVALID(M_AXI_ARVALID);
        dut->M_AXI_ARREADY(M_AXI_ARREADY);
        dut->M_AXI_RDATA(M_AXI_RDATA);
        dut->M_AXI_RRESP(M_AXI_RRESP);
        dut->M_AXI_RVALID(M_AXI_RVALID);
        dut->M_AXI_RREADY(M_AXI_RREADY);
        
        // Instantiate AXI Slave Memory
        axi_slave = new AxiSlaveMemory("AXI_SLAVE");
        axi_slave->clk(clk);
        axi_slave->rst(rst);
        
        // Connect AXI Master to Slave
        axi_slave->S_AXI_AWADDR(M_AXI_AWADDR);
        axi_slave->S_AXI_AWVALID(M_AXI_AWVALID);
        axi_slave->S_AXI_AWREADY(M_AXI_AWREADY);
        axi_slave->S_AXI_WDATA(M_AXI_WDATA);
        axi_slave->S_AXI_WSTRB(M_AXI_WSTRB);
        axi_slave->S_AXI_WVALID(M_AXI_WVALID);
        axi_slave->S_AXI_WREADY(M_AXI_WREADY);
        axi_slave->S_AXI_BRESP(M_AXI_BRESP);
        axi_slave->S_AXI_BVALID(M_AXI_BVALID);
        axi_slave->S_AXI_BREADY(M_AXI_BREADY);
        axi_slave->S_AXI_ARADDR(M_AXI_ARADDR);
        axi_slave->S_AXI_ARVALID(M_AXI_ARVALID);
        axi_slave->S_AXI_ARREADY(M_AXI_ARREADY);
        axi_slave->S_AXI_RDATA(M_AXI_RDATA);
        axi_slave->S_AXI_RRESP(M_AXI_RRESP);
        axi_slave->S_AXI_RVALID(M_AXI_RVALID);
        axi_slave->S_AXI_RREADY(M_AXI_RREADY);
        
        // Register test threads
        SC_THREAD(clock_generator);
        SC_THREAD(test_stimulus);
    }
    
    // ===== Helper Functions for MMIO Operations =====
    
    /**
     * @brief MMIO Register Write
     * @param address Register offset to write
     * @param data Data value to write
     */
    void mmio_write(unsigned int address, unsigned int data) {
        // Align to clock edge and perform write
        wait(clk.posedge_event());  // Wait for positive clock edge
        proc_addr.write(address & 0xFF);
        proc_wdata.write(data);
        proc_we.write(true);
        wait(clk.posedge_event());  // Wait for next positive edge (SC_METHOD will execute)
        proc_we.write(false);
        wait(clk.posedge_event());  // Give SC_METHOD time to process
    }
    
    /**
     * @brief MMIO Register Read
     * @param address Register offset to read
     * @return Register value
     */
    unsigned int mmio_read(unsigned int address) {
        wait(clk.posedge_event());
        proc_addr.write(address & 0xFF);
        proc_we.write(false);
        wait(clk.posedge_event());  // Wait for SC_METHOD to execute
        unsigned int data = proc_rdata.read();
        return data;
    }
    
    /**
     * @brief Verify Test Condition
     * @param condition Test assertion
     * @param test_name Description of test
     */
    void verify_test(bool condition, const string& test_name) {
        test_total++;
        if (condition) {
            cout << "[PASS] " << test_name << endl;
            test_passed++;
        } else {
            cout << "[FAIL] " << test_name << endl;
            test_failed++;
        }
    }

    const char* status_state_name(uint32_t status) {
        uint32_t state = (status >> 1) & 0x3;
        switch (state) {
            case 0: return "IDLE";
            case 1: return "PROCESS1";
            case 2: return "PROCESS2";
            case 3: return "PROCESS3";
            default: return "UNKNOWN";
        }
    }
    
    /**
     * @brief Clock Generator - Runs in separate thread
     */
    void clock_generator() {
        while (true) {
            clk.write(false);
            // 0.5 ns high/low => 1 ns period
            wait(0.5, SC_NS);
            clk.write(true);
            wait(0.5, SC_NS);
        }
    }
    
    // ===== Main Test Stimulus =====
    void test_stimulus() {
        const int INPUT_START_WORD = 100;
        const int OUTPUT_START_WORD = 500;

        ofstream test_log("test.log");
        if (!test_log.is_open()) {
            cerr << "[ERROR] Failed to create test.log" << endl;
            sc_stop();
            return;
        }

        axi_slave->set_logger(&test_log);
        test_log << "===== SOLE TEST LOG =====\n";
        test_log.flush();

        // Read input test vectors from file (one value per line)
        vector<float> input_values;
        ifstream data_file("SOLE_test_Data.txt");
        if (!data_file.is_open()) {
            // Try relative path from build directory
            data_file.open("../test/SOLE_test_Data.txt");
        }
        if (!data_file.is_open()) {
            cerr << "[ERROR] Failed to open SOLE_test_Data.txt" << endl;
            sc_stop();
            return;
        }
        string line;
        while (std::getline(data_file, line)) {
            // skip empty lines and trim whitespace
            std::stringstream ss(line);
            float v;
            if (!(ss >> v)) continue;
            input_values.push_back(v);
        }
        data_file.close();

        int NUM_DATA = (int)input_values.size();
        if (NUM_DATA == 0) {
            cerr << "[ERROR] No input data found in SOLE_test_Data.txt" << endl;
            sc_stop();
            return;
        }
        const int NUM_64BIT_WORDS = (NUM_DATA + 3) / 4;

        float* hw_input = new float[NUM_DATA];
        float* hw_output = new float[NUM_DATA];
        float* sw_output = new float[NUM_DATA];

         // Reset sequence
        rst.write(true);
        // Scaled down from 100 ns to 10 ns to match 1 ns clock period
        wait(10, SC_NS);
        rst.write(false);
        wait(10, SC_NS);

        // (1) memory存入input data結果(產生測資)
        test_log << "\n[1] Memory Input Data Write\n";
        test_log << "Index | InputFloat | InputFP16Hex | MemWordIdx | ElemInWord\n";
        for (int i = 0; i < NUM_64BIT_WORDS; i++) {
            sc_uint<64> packed = 0;
            for (int j = 0; j < 4 && (i * 4 + j) < NUM_DATA; j++) {
                int idx = i * 4 + j;
                // Use test input values read from SOLE_test_Data.txt
                float val = hw_input[idx] = input_values[idx];
                uint16_t fp16_val = float_to_fp16(val);
                packed |= ((sc_uint<64>)fp16_val << (j * 16));

                test_log << setw(4) << idx << "  " << setw(10)
                         << fixed << setprecision(6) << val << "       "
                         << " 0x" << hex << setfill('0') << setw(4) << fp16_val << dec << setfill(' ') << "       "
                         << (INPUT_START_WORD + i) << "           "
                         << j << "\n";
            }
            axi_slave->memory[INPUT_START_WORD + i] = packed;
        }
        output_memory_to_log(&test_log, axi_slave->memory, INPUT_START_WORD, NUM_64BIT_WORDS);

       

        // (2) testbench設定SOLE MMIO過程
        test_log << "\n[2] Testbench MMIO Configuration\n";
        test_log << "TimeNs,Reg,ValueHex,Note\n";

        mmio_write(REG_SRC_ADDR_BASE_L, INPUT_START_WORD * 8);
        test_log << sc_time_stamp() << ",REG_SRC_ADDR_BASE_L,0x"
                 << hex << (INPUT_START_WORD * 8) << dec << ",source base byte address\n";
        mmio_write(REG_SRC_ADDR_BASE_H, 0);
        test_log << sc_time_stamp() << ",REG_SRC_ADDR_BASE_H,0x0,source high\n";
        mmio_write(REG_DST_ADDR_BASE_L, OUTPUT_START_WORD * 8);
        test_log << sc_time_stamp() << ",REG_DST_ADDR_BASE_L,0x"
                 << hex << (OUTPUT_START_WORD * 8) << dec << ",destination base byte address\n";
        mmio_write(REG_DST_ADDR_BASE_H, 0);
        test_log << sc_time_stamp() << ",REG_DST_ADDR_BASE_H,0x0,destination high\n";
        mmio_write(REG_LENGTH_L, NUM_DATA);
        test_log << sc_time_stamp() << ",REG_LENGTH_L,0x"
                 << hex << NUM_DATA << dec << ",number of FP16 elements\n";
        mmio_write(REG_LENGTH_H, 0);
        test_log << sc_time_stamp() << ",REG_LENGTH_H,0x0,length high\n";
        // ===== Start the softmax module =====
        mmio_write(REG_CONTROL, 0x00000001);
        test_log << sc_time_stamp() << ",REG_CONTROL,0x1,mode=softmax start=1\n";
        
        // After starting the softmax, set the start bit back to zero (important!!)
        mmio_write(REG_CONTROL, 0x00000000);

        output_SOLE_mmio_to_log(&test_log, dut);
        test_log.flush();
        
        // (3) SOLE透過AXI4_Lite讀取input data並寫回結果的過程
        test_log << "\n[3] Softmax Start Running\n";
        test_log << "TimeNs,Status,State,Done,ErrorCode\n";

        // 等待四个关键事件按顺序发生
        test_log << "\n[Stage 1: Waiting for PROCESS1 state]\n";
        test_log.flush();
        while (true) {
            wait(clk.posedge_event());
            uint32_t st = mmio_read(REG_STATUS);
            uint32_t cur_state = (st >> 1) & 0x3;
            if (cur_state == 1) {
                test_log << "time: " << sc_time_stamp() <<" Event: Entered PROCESS1\n";
                test_log.flush();
                break;
            }
        }
        
        test_log << "\n[Stage 2: Waiting for PROCESS2 state]\n";
        test_log.flush();
        while (true) {
            wait(clk.posedge_event());
            uint32_t st = mmio_read(REG_STATUS);
            uint32_t cur_state = (st >> 1) & 0x3;
            if (cur_state == 2) {
                test_log << "time: " << sc_time_stamp() <<" Event: Entered PROCESS2\n";
                test_log.flush();
                break;
            }
        }
        
        test_log << "\n[Stage 3: Waiting for PROCESS3 state]\n";
        test_log.flush();
        while (true) {
            wait(clk.posedge_event());
            uint32_t st = mmio_read(REG_STATUS);
            uint32_t cur_state = (st >> 1) & 0x3;
            if (cur_state == 3) {
                test_log << "time: " << sc_time_stamp() <<" Event: Entered PROCESS3\n";
                test_log.flush();
                break;
            }
        }

        test_log << "\n[Stage 4: Waiting for softmax_done]\n";
        test_log.flush();
        while (dut->softmax_done != 1) {
            wait(clk.posedge_event());
        }
        test_log << "time: " << sc_time_stamp() <<" Event: softmax_done asserted\n";;
        test_log.flush();
        
        // (4) Softmax運算結束，確認memory中的資料
        test_log << "\n[5] Output Stored Back to Memory via AXI4_Lite";
        output_memory_to_log(&test_log, axi_slave->memory, OUTPUT_START_WORD, NUM_64BIT_WORDS);

        // (5) Softmax計算結果和軟體數值模擬(golden data)比較表
        SOLE_softmax(sw_output, hw_input, NUM_DATA);
        test_log << "\n[4] Softmax Compute Results\n";
        test_log << "Index |  Input  |  HW_Output  |  SW_Output  |  AbsError\n";

        float sum_hw_output = 0.0f;
        float sum_sw_output = 0.0f;
        float max_abs_error = 0.0f;

        for (int i = 0; i < NUM_DATA; i++) {
            int mem_word_idx = OUTPUT_START_WORD + (i / 4);
            int elem_idx = i % 4;
            sc_uint<64> packed_data = axi_slave->memory[mem_word_idx];
            uint16_t fp16_val = (packed_data >> (elem_idx * 16)) & 0xFFFF;
            hw_output[i] = fp16_to_float(fp16_val);

            float abs_error = fabsf(hw_output[i] - sw_output[i]);
            sum_hw_output += hw_output[i];
            sum_sw_output += sw_output[i];
            if (abs_error > max_abs_error) {
                max_abs_error = abs_error;
            }

            test_log << setfill(' ') << setw(3) << i << "    "
                     << setw(7) << fixed << setprecision(6) << hw_input[i] << "  "
                     << setw(12) << fixed << setprecision(9) << hw_output[i] << "  "
                     << setw(12) << fixed << setprecision(9) << sw_output[i] << "   "
                     << setw(12) << scientific << setprecision(6) << abs_error << "\n";
        }
        // --- Additional analysis: cosine similarity and top-5 values ---
        // Cosine similarity: dot(hw, sw) / (||hw|| * ||sw||)
        double dot = 0.0;
        double norm_hw = 0.0;
        double norm_sw = 0.0;
        for (int i = 0; i < NUM_DATA; ++i) {
            dot += (double)hw_output[i] * (double)sw_output[i];
            norm_hw += (double)hw_output[i] * (double)hw_output[i];
            norm_sw += (double)sw_output[i] * (double)sw_output[i];
        }
        double cosine = 0.0;
        if (norm_hw > 0.0 && norm_sw > 0.0) cosine = dot / (sqrt(norm_hw) * sqrt(norm_sw));

        // Top-5 values and indices for HW and SW outputs
        vector<pair<float,int>> hw_pairs; hw_pairs.reserve(NUM_DATA);
        vector<pair<float,int>> sw_pairs; sw_pairs.reserve(NUM_DATA);
        for (int i = 0; i < NUM_DATA; ++i) {
            hw_pairs.emplace_back(hw_output[i], i);
            sw_pairs.emplace_back(sw_output[i], i);
        }
        auto cmp = [](const pair<float,int>& a, const pair<float,int>& b){ return a.first > b.first; };
        sort(hw_pairs.begin(), hw_pairs.end(), cmp);
        sort(sw_pairs.begin(), sw_pairs.end(), cmp);

        test_log << "\n================== FINAL REPORT ================= " ;
        test_log << "\n[ANALYSIS] Cosine Similarity (HW vs SW) = " << fixed << setprecision(9) << cosine << "\n";

        test_log << "\n[ANALYSIS] Top 5 large inputs (value @ index):\n";
        vector<pair<float,int>> input_pairs; input_pairs.reserve(NUM_DATA);
        for (int i = 0; i < NUM_DATA; ++i) {
            input_pairs.emplace_back(hw_input[i], i);
        }
        sort(input_pairs.begin(), input_pairs.end(), cmp);
        for (int k = 0; k < 5 && k < (int)input_pairs.size(); ++k) {
            test_log << "  " << k << ": " << fixed << setprecision(6) << input_pairs[k].first
                     << " @ " << input_pairs[k].second << "\n";
        }

        test_log << "\n[ANALYSIS] Top 5 large HW outputs (value @ index):\n";
        for (int k = 0; k < 5 && k < (int)hw_pairs.size(); ++k) {
            test_log << "  " << k << ": " << fixed << setprecision(9) << hw_pairs[k].first
                     << " @ " << hw_pairs[k].second << "\n";
        }

        test_log << "\n[ANALYSIS] Top 5 large SW outputs (value @ index):\n";
        for (int k = 0; k < 5 && k < (int)sw_pairs.size(); ++k) {
            test_log << "  " << k << ": " << fixed << setprecision(9) << sw_pairs[k].first
                     << " @ " << sw_pairs[k].second << "\n";
        }

        test_log << "\n[ANALYSIS] Sum & MaxAbsError\n"
                 << "HW_Sum=" << fixed << setprecision(9) << sum_hw_output << "\n"
                 << "SW_Sum=" << fixed << setprecision(9) << sum_sw_output << "\n"
                 << "MaxAbsError=" << scientific << setprecision(9) << max_abs_error << "\n";

       

        test_log << "\n============ END OF SOLE TEST LOG ============\n";
        test_log.flush();
        test_log.close();

        delete[] hw_input;
        delete[] hw_output;
        delete[] sw_output;
        sc_stop();
    }
};

// ===== Main Entry Point =====
int sc_main(int argc, char* argv[]) {
    // Create testbench instance
    SOLE_TestBench testbench("SOLE_TestBench");
    
    // Run simulation until testbench calls sc_stop()
    sc_start();
    
    cout << "\n[SIMULATION COMPLETE]" << endl;
    
    return 0;
}
