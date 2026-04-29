#ifndef LOG2EXP_H
#define LOG2EXP_H

#include <systemc.h>

using sc_uint4 = sc_dt::sc_uint<4>;
using sc_uint16 = sc_dt::sc_uint<16>;

SC_MODULE(Log2Exp) {
    // Ports
    sc_in<sc_uint16> fp16_in;      // 16-bit floating point input (sign 1 | exponent 5 | mantissa 10)
    sc_out<sc_uint4> result_out;   // 4-bit output

    // Constructor
    SC_CTOR(Log2Exp) {
        SC_METHOD(process);
        sensitive << fp16_in;
    }

    // Process function
    void process();
};

#endif // LOG2EXP_H
