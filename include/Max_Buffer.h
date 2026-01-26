#ifndef MAX_BUFFER_H
#define MAX_BUFFER_H

#include <systemc.h>
#include <iostream>
#include <cstdint>
#include <vector>

using sc_uint16 = sc_dt::sc_uint<16>;
using sc_uint10 = sc_dt::sc_uint<10>;
using sc_uint1 = sc_dt::sc_uint<1>;

/**
 * @brief SystemC Module for Max_Buffer
 * 
 * This module is a dual-port FP16 buffer that stores 1024 FP16 numbers.
 * It supports synchronous write and combinational read operations.
 * 
 * Behavior:
 * - Write (write=1): Data is stored on the next clock cycle at address specified by index
 * - Read (write=0): Data is read combinationally from the address specified by index
 * - Local_Max_Out: Output of current read operation (combinational)
 * - Global_Max_Out: Maximum value across entire buffer (combinational)
 * 
 * Ports:
 *   - clk: Clock input (for synchronous write)
 *   - reset: 1-bit reset signal (1=reset buffer and global_max, 0=normal operation)
 *   - Local_Max_In: 16-bit FP16 input
 *   - write: 1-bit control signal (1=write, 0=read)
 *   - index: 10-bit address for buffer access
 *   - Local_Max_Out: 16-bit FP16 output (current address data, combinational read)
 *   - Global_Max_Out: 16-bit FP16 output (maximum across buffer, combinational)
 */
SC_MODULE(Max_Buffer) {
    // Ports
    sc_in<bool>       clk;              ///< Clock input (for synchronous write)
    sc_in<bool>       reset;            ///< Reset signal (1=clear buffer, 0=normal operation)
    sc_in<sc_uint16>  Local_Max_In;     ///< 16-bit FP16 input
    sc_in<sc_uint1>   write;            ///< Write control (1=write, 0=read)
    sc_in<sc_uint10>  index;            ///< 10-bit address (0-1023)
    sc_out<sc_uint16> Local_Max_Out;    ///< 16-bit FP16 output (current address, combinational)
    sc_out<sc_uint16> Global_Max_Out;   ///< 16-bit FP16 output (global max, combinational)

    // Buffer storage
    std::vector<sc_uint16> buffer;      ///< 1024-entry FP16 buffer
    sc_uint16 global_max;               ///< Cached global maximum value
    static const sc_uint16 FP16_MIN;    ///< Minimum FP16 value (-65504)

    // Constructor
    SC_CTOR(Max_Buffer) {
        // Initialize buffer with 1024 entries (all zeros initially)
        buffer.resize(1024, 0);
        global_max = FP16_MIN;  // Initialize global max to minimum FP16 value

        // Register processes
        SC_METHOD(compute_read_and_global_max);
        sensitive << Local_Max_In << index << write;
        
        SC_METHOD(write_on_clock);
        sensitive << clk.pos();
    }

    // Methods
    void compute_read_and_global_max();  ///< Combinational: read at index and output global max
    void write_on_clock();               ///< Synchronous: write on clock edge and update global_max
};
// FP16 minimum value: sign=1, exp=11110, mantissa=1111111111 = 0xFBFF (-65504)
const sc_uint16 Max_Buffer::FP16_MIN = 0xFBFF;

#endif // MAX_BUFFER_H