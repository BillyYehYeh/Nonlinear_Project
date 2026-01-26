#ifndef SUM_BUFFER_H
#define SUM_BUFFER_H

#include <systemc.h>
#include <iostream>
#include <cstdint>

using sc_uint32 = sc_dt::sc_uint<32>;

/**
 * @brief SystemC Module for Sum_Buffer
 * 
 * This module stores a single uint32 value.
 * It supports synchronous write and combinational read operations.
 * 
 * Behavior:
 * - Write (write=1): Data is stored on the next clock cycle
 * - Read (write=0): Data is read combinationally
 * - Reset (reset=1): Clears buffer to 0
 * 
 * Ports:
 *   - clk: Clock input (for synchronous write)
 *   - reset: 1-bit reset signal (1=clear buffer, 0=normal operation)
 *   - Data_In: 32-bit input data
 *   - write: 1-bit control signal (1=write, 0=read)
 *   - Data_Out: 32-bit output data (combinational read)
 */
SC_MODULE(Sum_Buffer) {
    // Ports
    sc_in<bool>       clk;              ///< Clock input (for synchronous write)
    sc_in<bool>       reset;            ///< Reset signal (1=clear buffer, 0=normal operation)
    sc_in<sc_uint32>  Data_In;          ///< 32-bit input data
    sc_in<bool>       write;            ///< Write control (1=write, 0=read)
    sc_out<sc_uint32> Data_Out;         ///< 32-bit output data (combinational read)

    // Buffer storage
    sc_uint32 buffer;                   ///< Single 32-bit storage

    // Constructor
    SC_CTOR(Sum_Buffer) {
        // Initialize buffer
        buffer = 0;

        // Register processes
        SC_METHOD(read_data);
        sensitive << Data_In << write << reset;
        
        SC_METHOD(write_on_clock);
        sensitive << clk.pos();
    }

    // Methods
    void read_data();                   ///< Combinational: read buffer data
    void write_on_clock();              ///< Synchronous: write on clock edge
};

#endif // SUM_BUFFER_H
