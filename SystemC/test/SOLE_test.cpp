#include <systemc>
#include <iostream>
#include <iomanip>

#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <vector>
#include <deque>
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
#define STATE_MONITOR_DEBUG 1

#define timeout_watchdog_enable 1
#define MAX_TIMEOUT_CYCLES 10000  

#define AXI_READ_ARREADY_DELAY 0   // Cycles after ARVALID become high before ARREADY goes high
#define AXI_READ_RVALID_DELAY  0   // Cycles after Read Address Handshake success before RVALID goes high
#define AXI_WRITE_WREADY_DELAY 0   // Cycles after WVALID become high before WREADY goes high
#define error_recovery_test 0      // 1: inject error then restart, 0: run simple one-pass test

// ===== Constants for Testing =====
#define AXI_ADDR_WIDTH 32
#define AXI_DATA_WIDTH 64
#define AXI_STRB_WIDTH 8
#define TEST_ADDR_BASE 0x0000
#define TEST_DATA_SIZE 2048  // Covers input/output regions up to 4096 FP16 elements (max output word index 1523)

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
        static std::deque<sc_uint<AXI_ADDR_WIDTH>> write_addr_queue;
        static sc_uint<32> last_write_addr = 0;
        static bool has_last_write_addr = false;
        static bool wready_pulse_enable = false;
        static int wready_pulse_cnt = -1;
        
        if (rst.read() == true) {  // If reset IS active (true), disable everything
            S_AXI_AWREADY.write(false);
            S_AXI_WREADY.write(false);
            S_AXI_BVALID.write(false);
            S_AXI_BRESP.write(0);
            write_addr_queue.clear();
            last_write_addr = 0;
            has_last_write_addr = false;
            wready_pulse_enable = false;
            wready_pulse_cnt = -1;
            return;
        }
        
        // Normal operation: rst == false
        // Keep AWREADY high so address channel can run ahead; queued addresses
        // are paired with W beats in order.
        bool awready = true;
        S_AXI_AWREADY.write(awready);
        
        // WREADY pulse model:
        // after first WVALID is seen, wait N cycles then pulse WREADY high for 1 cycle,
        // then repeat (wait N cycles, pulse 1 cycle).
        if (!wready_pulse_enable && S_AXI_WVALID.read()) {
            wready_pulse_enable = true;
            wready_pulse_cnt = AXI_WRITE_WREADY_DELAY;
        }

        bool wready = false;
        if (wready_pulse_enable) {
            if (AXI_WRITE_WREADY_DELAY <= 0) {
                wready = true;
            } else if (wready_pulse_cnt <= 0) {
                wready = true;
                wready_pulse_cnt = AXI_WRITE_WREADY_DELAY;
            } else {
                wready_pulse_cnt--;
            }
        }
        S_AXI_WREADY.write(wready);
        
        // Handshake must be detected using READY/VALID values visible on ports this cycle.
        // Using local "awready/wready" can create a one-cycle mismatch with the master.
        bool aw_handshake = S_AXI_AWVALID.read() && S_AXI_AWREADY.read();
        bool w_handshake = S_AXI_WVALID.read() && S_AXI_WREADY.read();
        
        if (aw_handshake) {
            write_addr_buf = S_AXI_AWADDR.read();
            write_addr_queue.push_back(write_addr_buf);
            if (log_stream && log_stream->good()) {
                uint32_t addr_val = (uint32_t)(write_addr_buf);
                if(AXI_DEBUG) (*log_stream) << "[AXI4_LITE][AW] byte_addr=0x" << std::hex << addr_val << std::dec << "\n";
            }
        }
        
        // In this testbench model, every W handshake gets an immediate B response pulse.
        if (w_handshake) {
            sc_uint<64> write_data_buf = S_AXI_WDATA.read();
            if (log_stream && log_stream->good()) {
                uint64_t data_val = write_data_buf.to_uint64();
                if(AXI_DEBUG) (*log_stream) << "[AXI4_LITE][W ] wdata=0x" << std::hex << data_val << std::dec << "\n";
            }

            if (!write_addr_queue.empty()) {
                write_addr_buf = write_addr_queue.front();
                write_addr_queue.pop_front();
            } else {
                write_addr_buf = has_last_write_addr ? (last_write_addr + 8) : 0;
                if (log_stream && log_stream->good()) {
                    if(AXI_DEBUG) (*log_stream) << "[AXI4_LITE][AW_FALLBACK] inferred byte_addr=0x"
                                  << std::hex << (uint32_t)write_addr_buf << std::dec << "\n";
                }
            }
            last_write_addr = write_addr_buf;
            has_last_write_addr = true;

            uint32_t byte_addr = (uint32_t)(write_addr_buf);
            uint32_t word_idx = byte_addr >> 3;

            if (word_idx >= TEST_DATA_SIZE) {
                if (log_stream && log_stream->good()) {
                    if(AXI_DEBUG) (*log_stream) << "[AXI4_LITE][WRITE_ERR] word_idx=" << word_idx
                                  << " exceeds TEST_DATA_SIZE=" << TEST_DATA_SIZE << "\n";
                }
                S_AXI_BRESP.write(2);  // SLVERR
            } else {
                if (log_stream && log_stream->good()) {
                    uint64_t data_val = write_data_buf.to_uint64();
                    if(AXI_DEBUG) (*log_stream) << "[AXI4_LITE][MEM_WRITE] word_idx=" << word_idx
                                  << " byte_addr=0x" << std::hex << byte_addr
                                  << " data=0x" << data_val << std::dec << "\n";
                }
                memory[word_idx] = write_data_buf;
                S_AXI_BRESP.write(0);  // OKAY
            }

            S_AXI_BVALID.write(true);
            if (log_stream && log_stream->good()) {
                if(AXI_DEBUG) (*log_stream) << "[AXI4_LITE][B ] bresp=" << S_AXI_BRESP.read() << "\n";
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
        static int arready_delay_cnt = -1;
        static int read_resp_start_delay_cnt = -1;
        static std::deque<sc_uint<AXI_ADDR_WIDTH>> read_addr_queue;
        
        if (rst.read() == true) {  // If reset IS active (true), disable everything
            S_AXI_ARREADY.write(false);
            S_AXI_RVALID.write(false);
            S_AXI_RDATA.write(0);
            S_AXI_RRESP.write(0);
            has_addr = false;
            arready_delay_cnt = -1;
            read_resp_start_delay_cnt = -1;
            read_addr_queue.clear();
            return;
        }
        
        // Normal operation: rst == false
        // For delay=0 keep old behavior: ARREADY always high.
        bool arready = false;
        if (AXI_READ_ARREADY_DELAY <= 0) {
            arready = true;
            arready_delay_cnt = -1;
        } else {
            // Keep countdown stable after first request to avoid starvation.
            if (arready_delay_cnt < 0 && S_AXI_ARVALID.read()) {
                arready_delay_cnt = AXI_READ_ARREADY_DELAY;
            }
            if (arready_delay_cnt == 0) {
                arready = true;
            } else if (arready_delay_cnt > 0) {
                arready_delay_cnt--;
            }
        }
        S_AXI_ARREADY.write(arready);
        
        // Capture read address when read address handshake occurs
        if (S_AXI_ARVALID.read() && arready) {
            sc_uint<AXI_ADDR_WIDTH> ar_addr = S_AXI_ARADDR.read();
            read_addr_queue.push_back(ar_addr);
            // Rearm delay for next transaction when AR delay is enabled.
            if (AXI_READ_ARREADY_DELAY > 0) {
                arready_delay_cnt = AXI_READ_ARREADY_DELAY;
            }

            if (log_stream && log_stream->good()) {
                uint32_t addr_val = (uint32_t)(ar_addr);
                if(AXI_DEBUG) {
                    (*log_stream) << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns[AXI4_LITE][AR] byte_addr=0x" << std::hex << addr_val
                                  << std::dec << " word_idx=" << (addr_val >> 3) << "\n";
                    log_stream->flush();
                }
            }
        }

        // Start next read response when idle and there is pending AR request.
        // AXI_READ_RVALID_DELAY is modeled as response start latency per response.
        if (!has_addr && !read_addr_queue.empty()) {
            if (AXI_READ_RVALID_DELAY <= 0) {
                addr_buf = read_addr_queue.front();
                read_addr_queue.pop_front();
                has_addr = true;
                read_resp_start_delay_cnt = -1;
            } else {
                if (read_resp_start_delay_cnt < 0) {
                    read_resp_start_delay_cnt = AXI_READ_RVALID_DELAY;
                }
                if (read_resp_start_delay_cnt > 0) {
                    read_resp_start_delay_cnt--;
                } else {
                    addr_buf = read_addr_queue.front();
                    read_addr_queue.pop_front();
                    has_addr = true;
                    read_resp_start_delay_cnt = -1;
                }
            }
        }
        
        // Read Data Channel: Return data from captured address
        if (has_addr) {
            uint32_t addr_val = (uint32_t)(addr_buf);
            uint32_t word_idx = addr_val >> 3;
            bool rvalid_now = true;

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
                S_AXI_RVALID.write(rvalid_now);
                S_AXI_RRESP.write(2);  // SLVERR response for invalid address
                if (rvalid_now && S_AXI_RREADY.read()) {
                    has_addr = false;
                }
            } else {
                // Read 8 bytes (64-bit word) from memory at word_idx
                sc_uint<64> data_to_read = memory[word_idx];
                S_AXI_RDATA.write(data_to_read);
                S_AXI_RVALID.write(rvalid_now);
                S_AXI_RRESP.write(0);  // OKAY response
                if (rvalid_now && log_stream && log_stream->good()) {
                    uint64_t rdata_val = data_to_read.to_uint64();
                    if(AXI_DEBUG) {
                        (*log_stream) << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns[AXI4_LITE][R ] word_idx=" << word_idx
                                  << " rdata=0x" << std::hex << rdata_val
                                  << " rresp=" << S_AXI_RRESP.read() << std::dec << "\n";
                        log_stream->flush();
                    }
                }
                
                // Clear address when response is accepted by master
                if (rvalid_now && S_AXI_RREADY.read()) {
                    has_addr = false;
                }
            }
        } else {
            S_AXI_RVALID.write(false);
            S_AXI_RDATA.write(0);
            S_AXI_RRESP.write(0);
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
    sc_signal<bool>                           interrupt;
    
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
    
    // Status monitoring (for change detection)
    sc_uint32 last_status;      ///< Track previous status value to detect changes
    bool enable_status_monitor; ///< Flag to enable/disable real-time status monitoring
    std::ofstream test_log_monitoring;  ///< Log file for continuous status monitor
    
    // Timing tracking
    sc_time start_time;         ///< Time when start bit was set
    sc_time done_time;          ///< Time when done bit was detected
    sc_time execution_time;     ///< Total execution time
    
    SC_HAS_PROCESS(SOLE_TestBench);
    
    SOLE_TestBench(sc_module_name name) : sc_module(name), 
                                          test_total(0),
                                          test_passed(0), 
                                          test_failed(0),
                                          last_status(0xFFFFFFFF),
                                          enable_status_monitor(false),
                                          start_time(0, SC_NS),
                                          done_time(0, SC_NS),
                                          execution_time(0, SC_NS) {  // Initialize to invalid value to force first print
        // Instantiate DUT
        dut = new SOLE("SOLE_DUT");
        dut->clk(clk);
        dut->rst(rst);
        
        // MMIO Processor Interface
        dut->proc_addr(proc_addr);
        dut->proc_wdata(proc_wdata);
        dut->proc_we(proc_we);
        dut->proc_rdata(proc_rdata);
        dut->interrupt(interrupt);
        
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
        SC_THREAD(continuous_status_monitor);
        SC_THREAD(interrupt_monitor);
        SC_THREAD(timeout_watchdog);
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
     * @brief Monitor and Print Status Register (Only on change)
     * Reads the status register and prints it in purple if the value has changed
     */
    void monitor_status() {
        uint32_t current_status = mmio_read(sole::mmio::REG_STATUS);
        
        // Only print if status changed
        if (current_status != last_status) {
            bool done_bit = current_status & 0x1;
            uint32_t state = (current_status >> 1) & 0x3;
            bool error_flag = (current_status >> 3) & 0x1;
            uint8_t error_code = (current_status >> 4) & 0xF;
            
            std::cerr << "\033[35m"  // Purple color
                      << "[STATUS_MMIO] 0x" << std::hex << std::setw(8) << std::setfill('0') << current_status
                      << " | done=" << std::dec << (int)done_bit
                      << " state=" << status_state_name(current_status)
                      << " error=" << (int)error_flag
                      << " error_code=0x" << std::hex << (int)error_code
                      << "\033[0m"  // Reset color
                      << " @" << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns" << std::endl;
            
            last_status = current_status;
        }
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
    
    /**
     * @brief Continuous Status Monitor - Runs every clock cycle
     * This thread monitors the SOLE status register continuously, catching all state transitions
     * even if the main test_stimulus thread is busy with other operations
     */
    void continuous_status_monitor() {
        // Wait for enable signal from test_stimulus
        while (!enable_status_monitor) {
            wait(1, SC_NS);  // Keep responsive without consuming watchdog budget
        }
        
        test_log_monitoring << "\n[CONTINUOUS MONITOR STARTED]\n";
        test_log_monitoring.flush();
        
        while (enable_status_monitor) {
            wait(clk.posedge_event());  // Trigger on every clock edge
            
            // Directly read the raw status register (not through MMIO to avoid protocol overhead)
            uint32_t current_status = dut->reg_status.read().to_uint();
            
            // Only log if status changed
            if (current_status != last_status) {
                bool done_bit = current_status & 0x1;
                uint32_t state = (current_status >> 1) & 0x3;
                bool error_flag = (current_status >> 3) & 0x1;
                uint8_t error_code = (current_status >> 4) & 0xF;
                
                // Log to both stderr (console) and file
                std::string state_name;
                switch(state) {
                    case 0: state_name = "IDLE"; break;
                    case 1: state_name = "PROCESS1"; break;
                    case 2: state_name = "PROCESS2"; break;
                    case 3: state_name = "PROCESS3"; break;
                    default: state_name = "UNKNOWN"; break;
                }
                
                std::cerr << "\033[36m"  // Cyan color for continuous monitor
                          << "[STATUS_HW] 0x" << std::hex << std::setw(8) << std::setfill('0') << current_status
                          << " | done=" << std::dec << (int)done_bit
                          << " | state=" << state_name
                          << " | error=" << (int)error_flag
                          << " | error_code=0x" << std::hex << (int)error_code
                          << "\033[0m"  // Reset color
                          << " @" << std::dec << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns" << std::endl;
                
                test_log_monitoring << "[STATUS_HW] 0x" << std::hex << std::setw(8) << std::setfill('0') << current_status
                                    << " | done=" << std::dec << (int)done_bit
                                    << " | state=" << state_name
                                    << " | error=" << (int)error_flag
                                    << " | error_code=0x" << std::hex << (int)error_code
                                    << " @" << std::dec << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns\n";
                test_log_monitoring.flush();
                
                last_status = current_status;
            }
        }
        
        test_log_monitoring << "\n[CONTINUOUS MONITOR STOPPED]\n";
        test_log_monitoring.flush();
    }

    void interrupt_monitor() {
        bool last_interrupt = false;

        // Start monitoring after reset sequence to avoid startup X/initial noise.
        wait(rst.posedge_event());
        wait(rst.negedge_event());

        while (true) {
            wait(clk.posedge_event());

            bool irq = interrupt.read();
            if (irq && !last_interrupt) {
                uint32_t status = dut->reg_status.read().to_uint();
                bool done_bit = ((status >> STAT_DONE_BIT) & 0x1) != 0;
                bool error_bit = ((status >> STAT_ERROR_BIT) & 0x1) != 0;

                std::cerr << "\033[1;33m"
                          << "[INTERRUPT] interrupt=1"
                          << " | done=" << (int)done_bit
                          << " | error=" << (int)error_bit
                          << " | status=0x" << std::hex << std::setw(8) << std::setfill('0') << status
                          << "\033[0m"
                          << " @" << std::dec << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns"
                          << std::endl;

                if (test_log_monitoring.is_open()) {
                    test_log_monitoring << "[INTERRUPT] interrupt=1"
                                        << " | done=" << (int)done_bit
                                        << " | error=" << (int)error_bit
                                        << " | status=0x" << std::hex << std::setw(8) << std::setfill('0') << status
                                        << " @" << std::dec << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns\n";
                    test_log_monitoring.flush();
                }
            }

            last_interrupt = irq;
        }
    }

    /**
     * @brief Independent timeout watchdog thread
     * Starts at simulation begin and stops simulation when timeout is reached.
     */
    void timeout_watchdog() {
        if (!timeout_watchdog_enable) {
            return;
        }

        const int timeout_ns = MAX_TIMEOUT_CYCLES;
        wait(timeout_ns, SC_NS);

        std::cerr << "\033[31m" << "[TIMEOUT] Watchdog reached " << timeout_ns
                  << " ns @ " << (long long)(sc_time_stamp() / sc_time(1, SC_NS))
                  << " ns, stopping simulation" << "\033[0m" << std::endl;
        sc_stop();
    }
    
    // ===== Main Test Stimulus =====
    void test_stimulus() {
        const int INPUT_START_WORD = 100;
        const int OUTPUT_START_WORD = 500;

        ofstream test_log("../test/SOLE_test_Result.log");
        if (!test_log.is_open()) {
            cerr << "[ERROR] Failed to create test.log" << endl;
            sc_stop();
            return;
        }

        axi_slave->set_logger(&test_log);
        test_log << "===== SOLE TEST LOG =====\n";
        test_log.flush();
        
        // Open continuous monitor log file
        test_log_monitoring.open("../test/SOLE_test_Monitor.log");
        if (!test_log_monitoring.is_open()) {
            cerr << "[ERROR] Failed to create monitor log file" << endl;
        }
        
        // Enable continuous status monitoring EARLY (before reset)
        if(STATE_MONITOR_DEBUG) {
            enable_status_monitor = true;
        }
        
        // Give monitor thread time to start running
        wait(10, SC_NS);
        
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
#if error_recovery_test
        test_log << "\n[2] Error Injection + Recovery Start Test\n";
#else
        test_log << "\n[2] Testbench MMIO Configuration\n";
#endif
        test_log << "TimeNs,Reg,ValueHex,Note\n";

#if error_recovery_test
        // 2a) Inject an MMIO configuration error: start with zero length.
        mmio_write(REG_LENGTH_L, 0);
        mmio_write(REG_LENGTH_H, 0);
        mmio_write(REG_CONTROL, 0x00000001);
        test_log << (long long)(sc_time_stamp() / sc_time(1, SC_NS))
                 << " ns,REG_CONTROL,0x1,inject error with length=0\n";

        bool injected_error_seen = false;
        bool injected_interrupt_seen = false;
        for (int i = 0; i < 64; ++i) {
            wait(clk.posedge_event());
            uint32_t st = dut->reg_status.read().to_uint();
            bool err = ((st >> STAT_ERROR_BIT) & 0x1) != 0;
            if (err) {
                injected_error_seen = true;
                injected_interrupt_seen = interrupt.read();
                test_log << "time: " << (long long)(sc_time_stamp() / sc_time(1, SC_NS))
                         << " ns Event: Injected error detected, status=0x"
                         << hex << setfill('0') << setw(8) << st << dec
                         << " interrupt=" << (int)injected_interrupt_seen << "\n";
                break;
            }
        }
        verify_test(injected_error_seen, "Injected MMIO error is detected");
        verify_test(injected_interrupt_seen, "Interrupt is asserted on injected error");

        // 2b) Clear START and verify error bit can clear before restart.
        mmio_write(REG_CONTROL, 0x00000000);
        bool error_cleared = false;
        for (int i = 0; i < 64; ++i) {
            wait(clk.posedge_event());
            uint32_t st = dut->reg_status.read().to_uint();
            bool err = ((st >> STAT_ERROR_BIT) & 0x1) != 0;
            if (!err) {
                error_cleared = true;
                break;
            }
        }
        verify_test(error_cleared, "Error bit clears after deasserting START");

        // 2c) Reconfigure valid settings and start again.
        test_log << "\n[2-RECOVERY] Reconfigure valid MMIO and restart\n";
#endif

        mmio_write(REG_SRC_ADDR_BASE_L, INPUT_START_WORD * 8);
        test_log << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns,REG_SRC_ADDR_BASE_L,0x"
                 << hex << (INPUT_START_WORD * 8) << dec << ",source base byte address\n";
        mmio_write(REG_SRC_ADDR_BASE_H, 0);
        test_log << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns,REG_SRC_ADDR_BASE_H,0x0,source high\n";
        mmio_write(REG_DST_ADDR_BASE_L, OUTPUT_START_WORD * 8);
        test_log << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns,REG_DST_ADDR_BASE_L,0x"
                 << hex << (OUTPUT_START_WORD * 8) << dec << ",destination base byte address\n";
        mmio_write(REG_DST_ADDR_BASE_H, 0);
        test_log << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns,REG_DST_ADDR_BASE_H,0x0,destination high\n";
        mmio_write(REG_LENGTH_L, NUM_DATA);
        test_log << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns,REG_LENGTH_L,0x"
                 << hex << NUM_DATA << dec << ",number of FP16 elements\n";
        mmio_write(REG_LENGTH_H, 0);
        test_log << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns,REG_LENGTH_H,0x0,length high\n";
        // ===== Start the softmax module =====
        start_time = sc_time_stamp();  // Record start time BEFORE sending start command
        mmio_write(REG_CONTROL, 0x00000001);
        test_log << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns,REG_CONTROL,0x1,mode=softmax start=1\n";
        
        // After starting the softmax, set the start bit back to zero (important!!)
        mmio_write(REG_CONTROL, 0x00000000);

        output_SOLE_mmio_to_log(&test_log, dut);
        test_log.flush();
        
        // (3) SOLE透過AXI4_Lite讀取input data並寫回結果的過程
        test_log << "\n[3] Softmax Start Running\n";
        test_log << "TimeNs,Status,State,Done,ErrorCode\n";
        
        // Print initial status
        //monitor_status();

        // 等待四个关键事件按顺序发生
        test_log << "\n[Stage 1: Waiting for PROCESS1 state]\n";
        test_log.flush();
        while (true) {
            wait(clk.posedge_event());
            uint32_t st = mmio_read(REG_STATUS);
            uint32_t cur_state = (st >> 1) & 0x3;
            //monitor_status();  // Print status changes
            if (cur_state == 1) {
                test_log << "time: " << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns Event: Entered PROCESS1\n";
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
            //monitor_status();  // Print status changes
            if (cur_state == 2) {
                test_log << "time: " << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns Event: Entered PROCESS2\n";
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
            //monitor_status();  // Print status changes
            if (cur_state == 3) {
                test_log << "time: " << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns Event: Entered PROCESS3\n";
                test_log.flush();
                break;
            }
        }

        test_log << "\n[Stage 4: Waiting for softmax_done]\n";
        test_log.flush();
        
        // Wait for DONE bit (bit 0) in status register to pulse HIGH
        // The done signal pulses HIGH for exactly one clock cycle when PROCESS3 completes
        // IMPORTANT: mmio_read() consumes 2 clock cycles, so we need tight polling to catch the 1-cycle pulse
        
        bool done_detected = false;
        bool completion_detected = false;
        bool idle_completion_detected = false;
        int timeout_cycles = 0;
        uint32_t last_status_checked = 0xFFFFFFFF;  // Track last checked status to avoid duplicate prints
        
        // Stage 4a: Pre-polling phase - single MMIO read to establish baseline
        uint32_t baseline_status = mmio_read(REG_STATUS);
        last_status_checked = baseline_status;
        //monitor_status();
        test_log << "Stage 4a baseline status: 0x" << hex << baseline_status << dec << " @" << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns\n";
        test_log.flush();
        
        // Stage 4b: Tight polling - wait cycle-by-cycle without heavy MMIO reads
        // This allows us to catch the 1-cycle done pulse
        test_log << "Stage 4b: Entering tight polling loop (direct HW register read)...\n";
        test_log.flush();
        
        while (!completion_detected && timeout_cycles < MAX_TIMEOUT_CYCLES) {
            // Wait one cycle
            wait(clk.posedge_event());
            timeout_cycles++;
            
            // Read status directly from hardware register (like monitoring thread does)
            // This is much faster than MMIO read and allows catching single-cycle pulses
            uint32_t status_val = dut->reg_status.read().to_uint();
            bool done_bit = (status_val >> 0) & 0x1;
            uint32_t cur_state = (status_val >> 1) & 0x3;
            
            // Print only if status changed (to track state transitions)
            if (status_val != last_status_checked) {
                test_log << "time: " << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns"
                        << " | Status: 0x" << hex << setfill('0') << setw(8) << status_val << dec
                        << " | done=" << (int)done_bit 
                        << " | state=" << status_state_name(status_val)
                        << "\n";
                test_log.flush();
                last_status_checked = status_val;
            }
            
            if (done_bit) {
                done_detected = true;
                completion_detected = true;
                done_time = sc_time_stamp();  // Record done time
                execution_time = done_time - start_time;  // Calculate execution time
                test_log << "time: " << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns Event: DONE pulse detected!\n";
                test_log << "[TIMING] Start time: " << (long long)(start_time / sc_time(1, SC_NS)) << " ns\n";
                test_log << "[TIMING] Done time: " << (long long)(done_time / sc_time(1, SC_NS)) << " ns\n";
                test_log << "[TIMING] Execution time: " << (long long)(execution_time / sc_time(1, SC_NS)) << " ns\n";
                test_log.flush();
            } else if (cur_state == 0 && timeout_cycles > 1) {
                // Fallback completion criteria: done pulse may be too narrow to sample in this thread.
                // If state has cleanly returned to IDLE after PROCESS3, treat as completion.
                idle_completion_detected = true;
                completion_detected = true;
                done_time = sc_time_stamp();
                execution_time = done_time - start_time;
                test_log << "time: " << (long long)(sc_time_stamp() / sc_time(1, SC_NS))
                         << " ns Event: Completion inferred by STATE=IDLE (done pulse may have been missed)\n";
                test_log << "[TIMING] Start time: " << (long long)(start_time / sc_time(1, SC_NS)) << " ns\n";
                test_log << "[TIMING] Completion time: " << (long long)(done_time / sc_time(1, SC_NS)) << " ns\n";
                test_log << "[TIMING] Execution time: " << (long long)(execution_time / sc_time(1, SC_NS)) << " ns\n";
                test_log.flush();
            }
        }

        if (!completion_detected) {
            test_log << "ERROR: Timeout waiting for done signal after " << timeout_cycles << " cycles\n";
        } else if (idle_completion_detected) {
            test_log << "time: " << (long long)(sc_time_stamp() / sc_time(1, SC_NS))
                     << " ns Completion successfully captured by IDLE fallback\n";
        } else {
            test_log << "time: " << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns DONE pulse successfully captured\n";
        }
        test_log.flush();
        
        // Wait one more cycle for done to deassert (it's a one-cycle pulse)
        wait(clk.posedge_event());
        uint32_t final_status = mmio_read(REG_STATUS);
        test_log << "time: " << (long long)(sc_time_stamp() / sc_time(1, SC_NS)) << " ns After done deassert: Status = 0x" 
                << hex << setfill('0') << setw(8) << final_status << dec
                << " | done=" << ((final_status >> 0) & 0x1) << "\n";
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
    #if error_recovery_test
        verify_test(cosine > 0.99, "Recovery restart run cosine similarity > 0.99");
    #else
        verify_test(cosine > 0.99, "Simple run cosine similarity > 0.99");
    #endif

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
        test_log << "\n[EXECUTION TIME] SOLE Execution Time: " << (long long)(execution_time / sc_time(1, SC_NS)) << " ns\n";        
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
        
        // Disable continuous monitoring and close monitor log
        enable_status_monitor = false;
        wait(2, SC_NS);  // Give monitor thread time to stop
        if (test_log_monitoring.is_open()) {
            test_log_monitoring.close();
        }

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
