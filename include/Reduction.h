#ifndef REDUCTION_H
#define REDUCTION_H

#include <systemc.h>
#include <iostream>

using sc_uint4 = sc_dt::sc_uint<4>;
using sc_uint32 = sc_dt::sc_uint<32>;

/**
 * @brief Exponential computation: 2^X where X is 4-bit input
 * 
 * Calculates 2 to the power of X, where X is a 4-bit unsigned integer.
 * This function computes 2^X and returns a 32-bit unsigned result.
 * 
 * Input range: 0 to 15 (4-bit)
 * Output range: 1 to 32768 (32-bit)
 * 
 * @param X 4-bit unsigned integer exponent
 * @return sc_uint32 Result of 2^X (32-bit)
 */
sc_uint32 power_of_two(sc_uint4 X);

/**
 * @brief SystemC Module for 2^X Computation
 * 
 * Combinational logic module that computes 2 to the power of a 4-bit input.
 * 
 * Ports:
 *   - X_in: 4-bit unsigned integer input
 *   - Y_out: 32-bit unsigned integer output (2^X_in)
 */
SC_MODULE(Reduction_Module) {
    // Ports
    sc_in<sc_uint4>   X_in;    ///< 4-bit input exponent
    sc_out<sc_uint32> Y_out;   ///< 32-bit output (2^X_in)

    // Constructor
    SC_CTOR(Reduction_Module) {
        SC_METHOD(compute_power);
        sensitive << X_in;
    }

    // Methods
    void compute_power();  ///< Combinational logic to compute 2^X
};

#endif // REDUCTION_H
