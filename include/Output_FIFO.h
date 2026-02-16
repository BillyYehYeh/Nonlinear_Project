#ifndef OUTPUT_FIFO_H
#define OUTPUT_FIFO_H

#include <systemc.h>
#include <iostream>
#include <cstdint>
#include "SRAM.h"

using sc_uint16 = sc_dt::sc_uint<16>;
using sc_uint10 = sc_dt::sc_uint<10>;

/**
 * @brief SystemC Module for Output_FIFO
 * 
 * FIFO built using dual-port SRAM for uint16 values.
 * 
 * Ports:
 *   - clk: Clock input
 *   - rst: Reset signal (1=reset entire SRAM and pointers, 0=normal)
 *   - data_in: 16-bit uint16 input
 *   - write_en: Write enable signal
 *   - read_en: Read enable signal
 *   - clear: Clear signal (resets read_addr to equal write_addr)
 *   - data_out: 16-bit uint16 output (1-cycle latency from SRAM)
 *   - full: Full flag output
 *   - empty: Empty flag output
 */
SC_MODULE(Output_FIFO) {
    // Ports
    sc_in<bool>       clk;              ///< Clock input
    sc_in<bool>       rst;              ///< Reset signal
    sc_in<sc_uint16>  data_in;          ///< 16-bit uint16 input
    sc_in<bool>       write_en;         ///< Write enable
    sc_in<bool>       read_en;          ///< Read enable
    sc_in<bool>       clear;            ///< Clear signal (read_addr = write_addr)
    sc_out<sc_uint16> data_out;         ///< 16-bit uint16 output
    sc_out<bool>      full;             ///< Full flag
    sc_out<bool>      empty;            ///< Empty flag

    // SRAM instance (10-bit address = 1024 entries, 16-bit data)
    SRAM<10, 16> *sram;

    // FIFO pointers
    sc_signal<sc_uint10> write_addr_sig; ///< Write pointer
    sc_signal<sc_uint10> read_addr_sig;  ///< Read pointer
    sc_signal<sc_uint16> sram_rdata_sig; ///< SRAM read data
    
    // Constructor
    SC_HAS_PROCESS(Output_FIFO);
    Output_FIFO(sc_core::sc_module_name name) : sc_core::sc_module(name) {
        // Create SRAM instance
        sram = new SRAM<10, 16>("Output_FIFO_SRAM");
        
        // Connect SRAM ports
        sram->clk(clk);
        sram->rst(rst);
        sram->we(write_en);
        sram->re(read_en);
        sram->waddr(write_addr_sig);
        sram->raddr(read_addr_sig);
        sram->wdata(data_in);
        sram->rdata(sram_rdata_sig);

        // Register processes
        SC_METHOD(update_flags);
        sensitive << write_addr_sig << read_addr_sig;
        
        SC_METHOD(pointer_update);
        sensitive << clk.pos();
        
        SC_METHOD(read_output);
        sensitive << sram_rdata_sig;
    }

    // Methods
    void update_flags();                ///< Updates full and empty flags
    void pointer_update();              ///< Updates read/write pointers on clock
    void read_output();                 ///< Propagates SRAM read data to output
};

#endif // OUTPUT_FIFO_H
