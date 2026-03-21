#ifndef SOLE_H
#define SOLE_H

#include <systemc.h>
#include <iostream>
#include <cstdint>
#include "axi4-lite.hpp"
#include "SOLE_MMIO.hpp"
#include "Softmax.h"

using namespace hybridacc::axi4lite;
using namespace sole::mmio;

using sc_uint2 = sc_dt::sc_uint<2>;
using sc_uint4 = sc_dt::sc_uint<4>;
using sc_uint8 = sc_dt::sc_uint<8>;
using sc_uint32 = sc_dt::sc_uint<32>;
using sc_uint64 = sc_dt::sc_uint<64>;

/**
 * @class SOLE (Softmax/Normalization Non-Linear Engine)
 * @brief Hardware Accelerator for SoftMax and Normalization operations
 * 
 * **Functional Overview:**
 * - Simple Processor interface (4-signal MMIO access): Addr, Wdata, We, Rdata
 *   - Processor writes configuration: Control, Addr_Base, Length registers (write-only)
 *   - Processor reads status (read-only)
 * - Compute engines (Softmax/Norm) actively read config regs and write status
 * - AXI4-Lite Master interface for autonomous memory access from compute engines
 * - Demux logic to route operations to SoftMax or Norm engines
 * - SoftMax engine implementation (Norm engine placeholder)
 */
SC_MODULE(SOLE) {
    // ===== System Ports =====
    sc_in<bool>                 clk;            ///< System clock
    sc_in<bool>                 rst;            ///< Asynchronous reset
    
    // ===== Processor MMIO Interface (Simplified 4-signal) =====
    sc_in<sc_dt::sc_uint<32>>   proc_addr;      ///< Processor MMIO address (Addr[7:0] = reg offset)
    sc_in<sc_dt::sc_uint<32>>   proc_wdata;     ///< Processor write data
    sc_in<bool>                 proc_we;        ///< Processor write enable (1=write, 0=read)
    sc_out<sc_dt::sc_uint<32>>  proc_rdata;     ///< Processor read data
    
    // ===== AXI4-Lite Master Ports (Write Address Channel) ======
    sc_out<sc_dt::sc_uint<AXI_ADDR_WIDTH>>  M_AXI_AWADDR;   ///< Master write address
    sc_out<bool>                            M_AXI_AWVALID;  ///< Master write address valid
    sc_in<bool>                             M_AXI_AWREADY;  ///< Master write address ready
    
    // ===== AXI4-Lite Master Ports (Write Data Channel) =====
    sc_out<sc_dt::sc_uint<AXI_DATA_WIDTH>> M_AXI_WDATA;     ///< Master write data (64-bit)
    sc_out<sc_dt::sc_uint<AXI_STRB_WIDTH>> M_AXI_WSTRB;     ///< Master write strobes (8-bit)
    sc_out<bool>                           M_AXI_WVALID;    ///< Master write data valid
    sc_in<bool>                            M_AXI_WREADY;    ///< Master write data ready
    
    // ===== AXI4-Lite Master Ports (Write Response Channel) =====
    sc_in<sc_uint2>                        M_AXI_BRESP;     ///< Master write response
    sc_in<bool>                            M_AXI_BVALID;    ///< Master write response valid
    sc_out<bool>                           M_AXI_BREADY;    ///< Master write response ready
    
    // ===== AXI4-Lite Master Ports (Read Address Channel) =====
    sc_out<sc_dt::sc_uint<AXI_ADDR_WIDTH>>  M_AXI_ARADDR;   ///< Master read address
    sc_out<bool>                            M_AXI_ARVALID;  ///< Master read address valid
    sc_in<bool>                             M_AXI_ARREADY;  ///< Master read address ready
    
    // ===== AXI4-Lite Master Ports (Read Data Channel) =====
    sc_in<sc_dt::sc_uint<AXI_DATA_WIDTH>>  M_AXI_RDATA;     ///< Master read data (64-bit)
    sc_in<sc_uint2>                        M_AXI_RRESP;     ///< Master read response
    sc_in<bool>                            M_AXI_RVALID;    ///< Master read data valid
    sc_out<bool>                           M_AXI_RREADY;    ///< Master read data ready

    // ===== Internal Signals & Registers =====
    
    // MMIO Register File
    sc_signal<sc_uint32>        reg_control;           ///< Control register (0x00)
    sc_signal<sc_uint32>        reg_status;            ///< Status register (0x04) - read from Softmax
    sc_signal<sc_uint32>        reg_src_addr_base_l;   ///< Source address base low 32-bit (0x08)
    sc_signal<sc_uint32>        reg_src_addr_base_h;   ///< Source address base high 32-bit (0x0C)
    sc_signal<sc_uint32>        reg_dst_addr_base_l;   ///< Destination address base low 32-bit (0x10)
    sc_signal<sc_uint32>        reg_dst_addr_base_h;   ///< Destination address base high 32-bit (0x14)
    sc_signal<sc_uint32>        reg_length_l;          ///< Data length low 32-bit (0x18)
    sc_signal<sc_uint32>        reg_length_h;          ///< Data length high 32-bit (0x1C)
    
    // Status signals
    sc_signal<bool>             softmax_done;      ///< SoftMax engine done signal
    sc_signal<bool>             softmax_busy;      ///< SoftMax engine busy signal
    sc_signal<sc_uint32>        softmax_status;    ///< SoftMax status output (connected to reg_status)
    sc_signal<bool>             norm_done;         ///< Norm engine done signal (placeholder)
    sc_signal<bool>             norm_busy;         ///< Norm engine busy signal (placeholder)
    
    // Demux control signals
    sc_signal<bool>             mode;              ///< Mode selection: 0=SoftMax, 1=Norm
    sc_signal<bool>             start;             ///< Start signal from control register
    sc_signal<sc_uint64>        src_addr_base;     ///< 64-bit source address from registers
    sc_signal<sc_uint64>        dst_addr_base;     ///< 64-bit destination address from registers
    sc_signal<sc_uint64>        data_length;       ///< 64-bit data length from registers
    
    // Softmax control signals
    sc_signal<bool>             softmax_enable;    ///< Enable signal for Softmax
    
    // AXI4-Lite Master signals from Softmax (internal routing)
    sc_signal<sc_dt::sc_uint<AXI_ADDR_WIDTH>> softmax_awaddr;    ///< Softmax write address
    sc_signal<bool>                           softmax_awvalid;   ///< Softmax write address valid
    sc_signal<bool>                           softmax_awready;   ///< Softmax write address ready
    
    sc_signal<sc_dt::sc_uint<AXI_DATA_WIDTH>> softmax_wdata;     ///< Softmax write data
    sc_signal<sc_dt::sc_uint<AXI_STRB_WIDTH>> softmax_wstrb;     ///< Softmax write strobes
    sc_signal<bool>                           softmax_wvalid;    ///< Softmax write data valid
    sc_signal<bool>                           softmax_wready;    ///< Softmax write data ready
    
    sc_signal<sc_uint2>                       softmax_bresp;     ///< Softmax write response
    sc_signal<bool>                           softmax_bvalid;    ///< Softmax write response valid
    sc_signal<bool>                           softmax_bready;    ///< Softmax write response ready
    
    sc_signal<sc_dt::sc_uint<AXI_ADDR_WIDTH>> softmax_araddr;    ///< Softmax read address
    sc_signal<bool>                           softmax_arvalid;   ///< Softmax read address valid
    sc_signal<bool>                           softmax_arready;   ///< Softmax read address ready
    
    sc_signal<sc_dt::sc_uint<AXI_DATA_WIDTH>> softmax_rdata;     ///< Softmax read data
    sc_signal<sc_uint2>                       softmax_rresp;     ///< Softmax read response
    sc_signal<bool>                           softmax_rvalid;    ///< Softmax read data valid
    sc_signal<bool>                           softmax_rready;    ///< Softmax read data ready
    
    // Softmax engine instance
    Softmax                     *softmax_unit;
    
    // Norm engine instance (placeholder - not instantiated yet)
    // Norm_Module              *norm_unit;
    
    // AXI4-Lite Slave state machine
    sc_signal<sc_uint2>         axi_read_state;    ///< Read state machine (AR, R channels)
    sc_signal<sc_uint2>         axi_write_state;   ///< Write state machine (AW, W, B channels)
    
    // AXI4 response codes (from axi4-lite.hpp)
    // AXI_RESP_OKAY   = 0x0
    // AXI_RESP_SLVERR = 0x2
    // AXI_RESP_DECERR = 0x3

    // ===== Methods =====
    
    /**
     * @brief Processor MMIO Access Handler
     * Implements simple read/write interface for Processor to access MMIO registers
     * - Write (proc_we=1): Updates Control, Addr_Base_L/H, Length_L/H registers
     * - Read (proc_we=0): Returns Status or other readable registers
     */
    void mmio_access_process();
    
    /**
     * @brief Demux Logic
     * Routes control signals to SoftMax or Norm engine based on Mode bit
     * Softmax/Norm engines read config registers and write status register
     * Multiplexes AXI4-Lite Master signals from compute engines to MMIO outputs
     */
    void demux_logic();
    
    // ===== Constructor =====
    SC_HAS_PROCESS(SOLE);
    SOLE(sc_core::sc_module_name name) : sc_core::sc_module(name), 
        clk("clk"), rst("rst"), proc_addr("proc_addr"), proc_wdata("proc_wdata"), proc_we("proc_we"), proc_rdata("proc_rdata"),
        M_AXI_AWADDR("M_AXI_AWADDR"), M_AXI_AWVALID("M_AXI_AWVALID"), M_AXI_AWREADY("M_AXI_AWREADY"),
        M_AXI_WDATA("M_AXI_WDATA"), M_AXI_WSTRB("M_AXI_WSTRB"), M_AXI_WVALID("M_AXI_WVALID"), M_AXI_WREADY("M_AXI_WREADY"),
        M_AXI_BRESP("M_AXI_BRESP"), M_AXI_BVALID("M_AXI_BVALID"), M_AXI_BREADY("M_AXI_BREADY"),
        M_AXI_ARADDR("M_AXI_ARADDR"), M_AXI_ARVALID("M_AXI_ARVALID"), M_AXI_ARREADY("M_AXI_ARREADY"),
        M_AXI_RDATA("M_AXI_RDATA"), M_AXI_RRESP("M_AXI_RRESP"), M_AXI_RVALID("M_AXI_RVALID"), M_AXI_RREADY("M_AXI_RREADY")
    {
 
        std::cout << "Constructing SOLE module..." << std::endl;
        // Initialize MMIO registers
        /*reg_control.write(0);
        reg_status.write(0);
        reg_src_addr_base_l.write(0);
        reg_src_addr_base_h.write(0);
        reg_dst_addr_base_l.write(0);
        reg_dst_addr_base_h.write(0);
        reg_length_l.write(0);
        reg_length_h.write(0);
        
        // Initialize status signals
        softmax_done.write(false);
        softmax_busy.write(false);
        norm_done.write(false);
        norm_busy.write(false);
        
        // Initialize demux control signals
        mode.write(false);
        start.write(false);
        
        // Initialize AXI state machines
        axi_read_state.write(0);
        axi_write_state.write(0);*/
        
        // ===== Instantiate Softmax Engine =====
        softmax_unit = new Softmax("softmax_unit");
        softmax_unit->clk(clk);
        softmax_unit->rst(rst);
        softmax_unit->done(softmax_done);
        
        // Connect control signals from SOLE MMIO
        softmax_unit->start(start);
        softmax_unit->src_addr_base(src_addr_base);
        softmax_unit->dst_addr_base(dst_addr_base);
        softmax_unit->data_length(data_length);
        
        // Connect status feedback to SOLE regfile
        softmax_unit->status_o(softmax_status);
        
        // Connect Softmax AXI4-Lite Master interface (Write Address Channel)
        softmax_unit->M_AXI_AWADDR(softmax_awaddr);
        softmax_unit->M_AXI_AWVALID(softmax_awvalid);
        softmax_unit->M_AXI_AWREADY(softmax_awready);
        
        // Connect Softmax AXI4-Lite Master interface (Write Data Channel)
        softmax_unit->M_AXI_WDATA(softmax_wdata);
        softmax_unit->M_AXI_WSTRB(softmax_wstrb);
        softmax_unit->M_AXI_WVALID(softmax_wvalid);
        softmax_unit->M_AXI_WREADY(softmax_wready);
        
        // Connect Softmax AXI4-Lite Master interface (Write Response Channel)
        softmax_unit->M_AXI_BRESP(softmax_bresp);
        softmax_unit->M_AXI_BVALID(softmax_bvalid);
        softmax_unit->M_AXI_BREADY(softmax_bready);
        
        // Connect Softmax AXI4-Lite Master interface (Read Address Channel)
        softmax_unit->M_AXI_ARADDR(softmax_araddr);
        softmax_unit->M_AXI_ARVALID(softmax_arvalid);
        softmax_unit->M_AXI_ARREADY(softmax_arready);
        
        // Connect Softmax AXI4-Lite Master interface (Read Data Channel)
        softmax_unit->M_AXI_RDATA(softmax_rdata);
        softmax_unit->M_AXI_RRESP(softmax_rresp);
        softmax_unit->M_AXI_RVALID(softmax_rvalid);
        softmax_unit->M_AXI_RREADY(softmax_rready);
        
        // ===== Register SC_METHOD processes =====
        
        // Processor MMIO Access Process (combinational for reads, sequential for writes)
        SC_METHOD(mmio_access_process);
        sensitive << clk.pos() << proc_we << proc_addr << proc_wdata
                  << softmax_busy << softmax_done << norm_busy << norm_done << softmax_status;
        
        // Demux Logic (combinational)
        SC_METHOD(demux_logic);
        sensitive << reg_control << reg_src_addr_base_l << reg_src_addr_base_h 
                  << reg_dst_addr_base_l << reg_dst_addr_base_h
                  << reg_length_l << reg_length_h << softmax_done << norm_done
                  << softmax_awaddr << softmax_awvalid << M_AXI_AWREADY
                  << softmax_wdata << softmax_wstrb << softmax_wvalid << M_AXI_WREADY
                  << M_AXI_BRESP << M_AXI_BVALID << softmax_bready
                  << softmax_araddr << softmax_arvalid << M_AXI_ARREADY
                  << M_AXI_RDATA << M_AXI_RRESP << M_AXI_RVALID << softmax_rready;

        std::cout << "SOLE Module Construction Complete: " << name << std::endl;
    }
};

#endif // SOLE_H