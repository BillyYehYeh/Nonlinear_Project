#ifndef OUTPUT_BUFFER_H
#define OUTPUT_BUFFER_H

#include <systemc.h>
#include <iostream>
#include <cstdint>
#include <vector>

using sc_uint16 = sc_dt::sc_uint<16>;
using sc_uint10 = sc_dt::sc_uint<10>;

/**
 * @brief SystemC Module for Output_Buffer
 * 
 * This module is a 1024-entry uint16 buffer.
 * It supports synchronous write and combinational read operations.
 * 
 * Behavior:
 * - Write (write=1): Data is stored on the next clock cycle at address specified by index
 * - Read (write=0): Data is read combinationally from the address specified by index
 * - Reset (reset=1): Clears all buffer entries to 0
 * 
 * Ports:
 *   - clk: Clock input (for synchronous write)
 *   - reset: 1-bit reset signal (1=clear buffer, 0=normal operation)
 *   - Data_In: 16-bit uint16 input
 *   - index: 10-bit address for buffer access
 *   - write: 1-bit control signal (1=write, 0=read)
 *   - Data_Out: 16-bit uint16 output (current address data, combinational read)
 */
SC_MODULE(Output_Buffer) {
    // Ports
    sc_in<bool>       clk;              ///< Clock input (for synchronous write)
    sc_in<bool>       reset;            ///< Reset signal (1=clear buffer, 0=normal operation)
    sc_in<sc_uint16>  Data_In;          ///< 16-bit uint16 input
    sc_in<sc_uint10>  index;            ///< 10-bit address (0-1023)
    sc_in<bool>       write;            ///< Write control (1=write, 0=read)
    sc_out<sc_uint16> Data_Out;         ///< 16-bit uint16 output (current address, combinational)

    // Buffer storage
    std::vector<sc_uint16> buffer;      ///< 1024-entry uint16 buffer

    // Constructor
    SC_CTOR(Output_Buffer) {
        // Initialize buffer with 1024 entries (all zeros initially)
        buffer.resize(1024, 0);

        // Register processes
        SC_METHOD(read_data);
        sensitive << Data_In << index << write << reset;
        
        SC_METHOD(write_on_clock);
        sensitive << clk.pos();
    }

    // Methods
    void read_data();                   ///< Combinational: read at index
    void write_on_clock();              ///< Synchronous: write on clock edge
};

#endif // OUTPUT_BUFFER_H
