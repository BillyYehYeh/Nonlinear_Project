#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <bitset>
#include <cmath>
#include "Log2Exp.h"

#define FP16_INPUT 0x8001  

// Helper function to convert FP16 bits to float
float fp16_to_float(uint16_t bits) {
    int sign = (bits >> 15) & 0x1;
    int exp_bits = (bits >> 10) & 0x1F;
    int mant_bits = bits & 0x3FF;
    
    if (exp_bits == 0 && mant_bits == 0) {
        return 0.0f;
    }
    
    float mantissa = 1.0f;
    if (exp_bits != 0) {
        mantissa = 1.0f + mant_bits / 1024.0f;
    } else {
        mantissa = mant_bits / 1024.0f;
    }
    
    int exponent = exp_bits - 15;
    return (sign ? -1.0f : 1.0f) * std::pow(2.0f, (float)exponent) * mantissa;
}

SC_MODULE(Log2Exp_TestBench) {
    sc_signal<sc_bv<16>> fp16_in_sig;
    sc_signal<sc_bv<4>>  result_out_sig;

    Log2Exp *dut;

    SC_CTOR(Log2Exp_TestBench) {
        dut = new Log2Exp("Log2Exp_DUT");
        dut->fp16_in(fp16_in_sig);
        dut->result_out(result_out_sig);

        SC_THREAD(test_stimulus);
    }

    void print_fp16_info(uint16_t bits) {
        int sign = (bits >> 15) & 0x1;
        int exp_bits = (bits >> 10) & 0x1F;
        int mant_bits = bits & 0x3FF;
        
        std::cout << "  Sign: " << sign << ", Exponent: " << exp_bits << ", Mantissa: " << mant_bits << std::endl;
        std::cout << "  Float value: " << fp16_to_float(bits) << std::endl;
    }

    void run_test(uint16_t input_val) {
        std::cout << "\n" << std::string(80, '-') << std::endl;
        std::cout << "Input (hex): 0x" << std::hex << std::setw(4) << std::setfill('0') << input_val << std::dec << std::endl;
        std::cout << "Input (binary): " << std::bitset<16>(input_val) << std::endl;
        
        print_fp16_info(input_val);
        
        fp16_in_sig.write(sc_bv<16>(input_val));
        wait(1, SC_NS);
        
        sc_bv<4> result = result_out_sig.read();
        uint16_t result_int = result.to_uint();

        float x = fp16_to_float(FP16_INPUT);
        float Exp_TRUE = std::exp(x); 
        double Exp_APPROXIMATE = std::exp2(x * (1.4375));
        double Exp_APPROXIMATE_OUTPUT = std::exp2(-result_int);

        std::cout << "-------------MODULE OUTPUT--------------" << std::endl;
        std::cout << " Output (4-bit):   " << std::bitset<4>(result_int) << std::endl;
        std::cout << " Output (decimal):   " << result_int << std::endl;
        std::cout << " Ideal Output(x * -1.4375): " << x * (-1.4375) << std::endl;

        std::cout << "-------------E^x Value--------------" << std::endl;
        std::cout << " Exp_TRUE: " << Exp_TRUE << std::endl;
        std::cout << " Exp_APPROXIMATE: " << Exp_APPROXIMATE << std::endl;
        std::cout << " Exp_APPROXIMATE_OUTPUT: " << Exp_APPROXIMATE_OUTPUT << std::endl;
    
    }

    void test_stimulus() {
        std::cout << std::string(80, '=') << std::endl;
        std::cout << "\nModule: Converts FP16 input to 4-bit logarithmic output" << std::endl;
        std::cout << "Input: 16-bit FP16 value (fp16_in)" << std::endl;
        std::cout << "Output: 4-bit result (result_out)" << std::endl;

        //uint16_t input = 0x0400;  
        run_test(FP16_INPUT);
        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    Log2Exp_TestBench tb("Log2Exp_TestBench");
    sc_start();
    return 0;
}
