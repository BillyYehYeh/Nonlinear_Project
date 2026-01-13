#ifndef LOG2EXP_H
#define LOG2EXP_H

#include <systemc.h>

SC_MODULE(Log2Exp) {
    // Ports
    sc_in<sc_bv<16>> fp16_in;      // 16-bit floating point input (sign 1 | exponent 5 | mantissa 10)
    sc_out<sc_bv<4>> result_out;   // 4-bit output

    // Internal signals
    //sc_signal<sc_bv<5>> mantissa_extract;  // Extracted mantissa (implicit bit + 4 MSB)
    //sc_signal<sc_bv<10>> sum_result;       // Sum of three shifted mantissas
    //sc_signal<sc_bv<4>> shift_result;      // 4-bit result after shifting
    //sc_signal<sc_bv<5>> exponent;          // Extracted exponent
    //sc_signal<int> actual_exponent;        // Actual exponent (E - 15)
    //sc_signal<int> shifted_value_sig;      // Shifted value after exponent adjustment
    //sc_signal<int> shift_res_int_sig;      // 4-bit integer before conversion

    // Constructor
    SC_CTOR(Log2Exp) {
        SC_METHOD(process);
        sensitive << fp16_in;
    }

    // Process function
    void process();
};

#endif // LOG2EXP_H
