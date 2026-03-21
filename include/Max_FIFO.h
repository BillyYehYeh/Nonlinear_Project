#ifndef MAX_FIFO_H
#define MAX_FIFO_H

#include <systemc.h>
#include <iostream>
#include <cstdint>
#include "SRAM.h"

using sc_uint16 = sc_dt::sc_uint<16>;
using sc_uint10 = sc_dt::sc_uint<10>;

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
 *   - read_en: Read enable signal
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
    sc_in<bool>       read_en;          ///< Read enable
    sc_in<bool>       clear;            ///< Clear signal (read_addr = write_addr)
    sc_out<sc_uint16> data_out;         ///< 16-bit FP16 output
    sc_out<bool>      full;             ///< Full flag
    sc_out<bool>      empty;            ///< Empty flag
    sc_out<sc_uint10> count;            ///< Number of elements currently in FIFO


    // SRAM instance (10-bit address = 1024 entries, 16-bit data)
    SRAM<10, 16> *sram;

    // FIFO pointers
    sc_signal<sc_uint10> write_addr_sig; ///< Write pointer
    sc_signal<sc_uint10> read_addr_sig;  ///< Read pointer
    sc_signal<sc_uint16> sram_rdata_sig; ///< SRAM read data
    // Gated SRAM control signals (prevent writes when full, reads when empty)
    sc_signal<bool> we_sig;
    sc_signal<bool> re_sig;
    
    // Constructor
    SC_HAS_PROCESS(Max_FIFO);
    Max_FIFO(sc_core::sc_module_name name) : sc_core::sc_module(name) {
        // Create SRAM instance
        sram = new SRAM<10, 16>("Max_FIFO_SRAM");
        
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
        
        // Gate write/read enables combinationally so SRAM never writes when full
        // and never reads when empty.
        SC_METHOD(gate_signals);
        sensitive << write_en << read_en << full << empty;
        
        SC_METHOD(read_output);
        sensitive << sram_rdata_sig;

        //SC_METHOD(Print_Push_Count);
        //sensitive << clk.pos();
        
        //SC_METHOD(Print_FIFO_SRAM);
        //sensitive << clk.pos();
    }

    // Methods
    void update_flags();                ///< Updates full and empty flags
    void pointer_update();              ///< Updates read/write pointers on clock
    void read_output();                 ///< Propagates SRAM read data to output
    void gate_signals();                 ///< Gate write/read enables against full/empty
    //void Print_Push_Count();            ///< Debug: Print push count and data
    //void Print_FIFO_SRAM();             ///< Debug: Print SRAM state
};

#endif // MAX_FIFO_H
