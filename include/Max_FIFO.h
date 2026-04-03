#ifndef MAX_FIFO_H
#define MAX_FIFO_H

#include <systemc.h>
#include <iostream>
#include <cstdint>
#include "SRAM.h"

using sc_uint16 = sc_dt::sc_uint<16>;

#ifndef DATA_LENGTH_MAX
#define DATA_LENGTH_MAX 4096
#endif

constexpr unsigned max_fifo_ceil_log2(unsigned n, unsigned bits = 0, unsigned value = 1) {
    return (value >= n) ? bits : max_fifo_ceil_log2(n, bits + 1, value << 1);
}

constexpr unsigned MAX_FIFO_ADDR_BITS = max_fifo_ceil_log2(DATA_LENGTH_MAX);
using max_fifo_addr_t = sc_dt::sc_uint<MAX_FIFO_ADDR_BITS>;

/**
 * @brief SystemC Module for Max_FIFO
 * 
 * FIFO built using dual-port SRAM for FP16 values.
 * 
 * Ports:
 *   - clk: Clock input
 *   - rst: Reset signal (1=reset entire SRAM and pointers, 0=normal)
 *   - data_in: 16-bit FP16 input
 *   - write_en: Write enable signal
 *   - read_ready: Read ready signal
 *   - clear: Clear signal (resets read_addr to equal write_addr)
 *   - data_out: 16-bit FP16 output (1-cycle latency from SRAM)
 *   - full: Full flag output
 *   - empty: Empty flag output
 *   - count: Number of elements currently in FIFO
 */
SC_MODULE(Max_FIFO) {
    // Ports
    sc_in<bool>       clk;              ///< Clock input
    sc_in<bool>       rst;              ///< Reset signal
    
    sc_in<sc_uint16>  data_in;          ///< 16-bit FP16 input
    sc_in<bool>       write_en;         ///< Write enable
    
    sc_out<sc_uint16> data_out;         ///< 16-bit FP16 output
    sc_in<bool>       read_ready;       ///< Read ready signal
    sc_out<bool>      read_valid;       ///< Read data valid signal
    
    sc_in<bool>       clear;            ///< Clear signal (read_addr = write_addr)
    sc_out<bool>      full;             ///< Full flag
    sc_out<bool>      empty;            ///< Empty flag
    sc_out<max_fifo_addr_t> count;            ///< Number of elements currently in FIFO


    // SRAM instance (MAX_FIFO_ADDR_BITS address, 16-bit data)
    SRAM<MAX_FIFO_ADDR_BITS, 16> *sram;

    // FIFO pointers
    sc_signal<max_fifo_addr_t> write_addr_sig; ///< Write pointer
    sc_signal<max_fifo_addr_t> read_addr_sig;  ///< Read pointer
    sc_signal<sc_uint16> sram_rdata_sig; ///< SRAM read data
    // Skid buffer after SRAM output (captures returned read data on stall)
    sc_signal<sc_uint16> skid_reg_sig;
    sc_signal<bool> skid_valid_sig;
    // Delayed read issue flag: true when SRAM return data is valid this cycle.
    sc_signal<bool> sram_output_data_valid;
    // Gated SRAM control signals (prevent writes when full, reads when empty)
    sc_signal<bool> we_sig;
    sc_signal<bool> re_sig;
    
    // Constructor
    SC_HAS_PROCESS(Max_FIFO);
    Max_FIFO(sc_core::sc_module_name name) : sc_core::sc_module(name) {
        // Create SRAM instance
        sram = new SRAM<MAX_FIFO_ADDR_BITS, 16>("Max_FIFO_SRAM");
        
        // Connect SRAM ports
        sram->clk(clk);
        sram->rst(rst);
        sram->we(we_sig);
        sram->re(re_sig);
        sram->waddr(write_addr_sig);
        sram->raddr(read_addr_sig);
        sram->wdata(data_in);
        sram->rdata(sram_rdata_sig);

        // Register processes
        SC_METHOD(update_flags);
        sensitive << write_addr_sig << read_addr_sig;
        
        SC_METHOD(pointer_update);
        sensitive << clk.pos();

        SC_METHOD(count_update);
        sensitive << clk.pos();
        
        // Gate write/read enables combinationally so SRAM never writes when full
        // and never reads when empty.
        SC_METHOD(gate_signals);
        sensitive << write_en << read_ready << full << empty
              << skid_valid_sig << sram_output_data_valid;

        SC_METHOD(output_pipeline_update);
        sensitive << clk.pos();
        
        SC_METHOD(read_output);
        sensitive << sram_rdata_sig << sram_output_data_valid << skid_reg_sig << skid_valid_sig;

        SC_METHOD(read_data_valid_flag);
        sensitive << sram_output_data_valid << skid_valid_sig;

        //SC_METHOD(Print_Push_Count);
        //sensitive << clk.pos();
        SC_METHOD(Print_FIFO_SRAM);
        sensitive << clk.pos();
    }

    // Methods
    void update_flags();                ///< Updates full and empty flags
    void pointer_update();              ///< Updates read/write pointers on clock
    void count_update();                ///< Updates interface-level occupancy count on clock
    void output_pipeline_update();      ///< Captures returned SRAM data into skid register during stalls
    void read_output();                 ///< Propagates SRAM read data to output
    void gate_signals();                 ///< Gate write/read enables against full/empty
    void read_data_valid_flag();        ///< Output-valid flag for direct SRAM return or skid data
    //void Print_Push_Count();            ///< Debug: Print push count and data
    void Print_FIFO_SRAM();             ///< Debug: Print SRAM state
};

#endif // MAX_FIFO_H
