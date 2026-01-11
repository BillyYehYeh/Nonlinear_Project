#include <systemc.h>
#include "Log2Exp.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstdint>
#include <sstream>

// Helper function to convert FP16 bits to float
float fp16_to_float(uint16_t bits) {
    uint32_t s = (bits >> 15) & 0x1;
    uint32_t e = (bits >> 10) & 0x1F;
    uint32_t m = bits & 0x3FF;
    
    // Handle special cases
    if (e == 0) {
        if (m == 0) return s ? -0.0f : 0.0f;
        // Subnormal number
        return (s ? -1.0f : 1.0f) * std::pow(2.0f, -14.0f) * (m / 1024.0f);
    }
    if (e == 31) {
        if (m == 0) return s ? -INFINITY : INFINITY;
        return NAN;
    }
    
    // Normal number: value = (-1)^s * 2^(e-15) * (1 + m/1024)
    float mantissa = 1.0f + m / 1024.0f;
    int exponent = e - 15;
    return (s ? -1.0f : 1.0f) * std::pow(2.0f, (float)exponent) * mantissa;
}

// Test bench for interactive testing
SC_MODULE(Log2Exp_Interactive_TB) {
    Log2Exp dut;
    sc_signal<sc_bv<16>> fp16_in;
    sc_signal<sc_bv<4>> result_out;

    SC_CTOR(Log2Exp_Interactive_TB) : dut("Log2Exp_DUT") {
        dut.fp16_in(fp16_in);
        dut.result_out(result_out);

        SC_THREAD(test_process);
    }

    void test_process() {
        std::cout << "\n========== Log2Exp Interactive Test ==========" << std::endl;
        std::cout << "Enter FP16 values in hexadecimal (0x0000 to 0xFFFF) to test the module" << std::endl;
        std::cout << "Type 'q' or 'quit' to exit, 'help' for instructions" << std::endl;
        std::cout << "==========================================\n" << std::endl;

        std::string input;
        while (true) {
            std::cout << "Enter FP16 hex value (or 'quit'/'help'): ";
            std::getline(std::cin, input);

            // Handle quit command
            if (input == "q" || input == "quit" || input == "exit") {
                std::cout << "\nExiting test...\n" << std::endl;
                break;
            }

            // Handle help command
            if (input == "help" || input == "h") {
                print_help();
                continue;
            }

            // Parse hexadecimal input
            uint16_t fp16_bits = 0;
            try {
                // Handle both 0x and non-0x prefixed inputs
                if (input.substr(0, 2) == "0x" || input.substr(0, 2) == "0X") {
                    fp16_bits = static_cast<uint16_t>(std::stoul(input, nullptr, 16));
                } else {
                    fp16_bits = static_cast<uint16_t>(std::stoul(input, nullptr, 16));
                }
            } catch (const std::exception& e) {
                std::cout << "Invalid input! Please enter a hexadecimal value (e.g., 0xc3ff or c3ff)" << std::endl;
                continue;
            }

            // Run the test with this input
            run_single_test(fp16_bits);
            wait(2, SC_NS);  // Wait for module to process
        }

        sc_stop();
    }

    void run_single_test(uint16_t fp16_bits) {
        fp16_in.write(sc_bv<16>(fp16_bits));
        wait(1, SC_NS);

        // Extract FP16 components
        int sign = (fp16_bits >> 15) & 0x1;
        int exp_bits = (fp16_bits >> 10) & 0x1F;
        int mant_bits = fp16_bits & 0x3FF;

        // Convert to float
        float fp16_value = fp16_to_float(fp16_bits);

        // Get module output
        sc_bv<4> module_output = result_out.read();
        int output_unsigned = module_output.to_uint();

        // Get internal signals from module
        sc_bv<5> mantissa_ext = dut.mantissa_extract.read();
        sc_bv<10> sum_res = dut.sum_result.read();
        sc_bv<5> exp_sig = dut.exponent.read();

        // Print results
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "TEST RESULT FOR INPUT: 0x" << std::hex << std::setw(4) << std::setfill('0') << fp16_bits << std::dec << std::endl;
        std::cout << std::string(80, '=') << std::endl;

        // Input breakdown
        std::cout << "\nINPUT BREAKDOWN:" << std::endl;
        std::cout << "  Sign: " << sign << std::endl;
        std::cout << "  Exponent (5 bits): " << exp_bits << " (0x" << std::hex << exp_bits << std::dec << ")" << std::endl;
        std::cout << "  Mantissa (10 bits): 0x" << std::hex << std::setw(3) << std::setfill('0') << mant_bits << std::dec << std::endl;
        std::cout << "  FP16 Value: " << std::fixed << std::setprecision(6) << fp16_value << std::endl;

        // Module calculation process
        std::cout << "\nMODULE INTERNAL CALCULATIONS:" << std::endl;
        std::cout << std::setw(40) << "Step" 
                  << " | " << std::setw(20) << "Value" 
                  << " | " << std::setw(20) << "Hex" << std::endl;
        std::cout << std::string(85, '-') << std::endl;

        std::cout << std::setw(40) << "Exponent field (E)"
                  << " | " << std::setw(20) << exp_bits
                  << " | " << "0x" << std::hex << exp_bits << std::dec << std::endl;

        std::cout << std::setw(40) << "Mantissa (implicit 1 + 4 MSB)"
                  << " | " << std::setw(20) << mantissa_ext.to_uint()
                  << " | " << "0x" << std::hex << mantissa_ext.to_uint() << std::dec << std::endl;

        // Calculate operations
        int mant_val = mantissa_ext.to_uint();
        int op1 = mant_val;
        int op2 = (mant_val << 1) & 0x3FF;
        int op3 = (mant_val << 4) & 0x3FF;
        int sum_calc = (op1 + op2 + op3) & 0x3FF;

        std::cout << std::setw(40) << "Op1 (original mantissa)"
                  << " | " << std::setw(20) << op1
                  << " | " << "0x" << std::hex << op1 << std::dec << std::endl;

        std::cout << std::setw(40) << "Op2 (mantissa << 1)"
                  << " | " << std::setw(20) << op2
                  << " | " << "0x" << std::hex << op2 << std::dec << std::endl;

        std::cout << std::setw(40) << "Op3 (mantissa << 4)"
                  << " | " << std::setw(20) << op3
                  << " | " << "0x" << std::hex << op3 << std::dec << std::endl;

        std::cout << std::setw(40) << "Sum (10 bits masked)"
                  << " | " << std::setw(20) << sum_res.to_uint()
                  << " | " << "0x" << std::hex << sum_res.to_uint() << std::dec << std::endl;

        // Exponent shift calculation
        int actual_exp = exp_bits - 15;
        std::cout << std::setw(40) << "Actual exponent (E-15)"
                  << " | " << std::setw(20) << actual_exp
                  << " | " << "0x" << std::hex << (actual_exp & 0xFF) << std::dec << std::endl;

        // Shift operation
        int shifted_val;
        std::string shift_direction;
        if (actual_exp <= 0) {
            int shift_amt = -actual_exp;
            shifted_val = (sum_res.to_uint() << shift_amt);
            shift_direction = "LEFT by " + std::to_string(shift_amt);
        } else {
            shifted_val = (sum_res.to_uint() >> actual_exp);
            shift_direction = "RIGHT by " + std::to_string(actual_exp);
        }

        std::cout << std::setw(40) << ("Shift " + shift_direction)
                  << " | " << std::setw(20) << shifted_val
                  << " | " << "0x" << std::hex << shifted_val << std::dec << std::endl;

        // 4-bit integer extraction
        int shift_res_int = (shifted_val >> 4) & 0xF;
        std::cout << std::setw(40) << "4-bit integer (>>4 & 0xF)"
                  << " | " << std::setw(20) << shift_res_int
                  << " | " << "0x" << std::hex << shift_res_int << std::dec << std::endl;

        std::cout << std::string(85, '-') << std::endl;

        // Final output
        std::cout << "\nFINAL OUTPUT:" << std::endl;
        std::cout << std::setw(40) << "Module Result (4 bits)"
                  << " | " << std::setw(20) << output_unsigned
                  << " | " << "0x" << std::hex << output_unsigned << std::dec << std::endl;

        // Mux logic explanation
        std::cout << "\nMUX LOGIC APPLIED:" << std::endl;
        if (exp_bits <= 9) {
            std::cout << "  Condition: exp_bits (" << exp_bits << ") <= 9 → Output = 0x0" << std::endl;
        } else if (exp_bits >= 15) {
            std::cout << "  Condition: exp_bits (" << exp_bits << ") >= 15 → Output = 0xF" << std::endl;
        } else {
            std::cout << "  Condition: 10 <= exp_bits (" << exp_bits << ") <= 14 → Output = shift_result = 0x" 
                      << std::hex << shift_res_int << std::dec << std::endl;
        }

        std::cout << "\n" << std::string(80, '=') << std::endl;
    }

    void print_help() {
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "HELP - Log2Exp Interactive Test" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        std::cout << "\nInput Format:" << std::endl;
        std::cout << "  - Enter FP16 values as 16-bit hexadecimal" << std::endl;
        std::cout << "  - Prefix with '0x' is optional" << std::endl;
        std::cout << "  - Examples: 0xc3ff, c3ff, 0xa000, a000" << std::endl;

        std::cout << "\nFP16 Format:" << std::endl;
        std::cout << "  Bit 15: Sign (0=positive, 1=negative)" << std::endl;
        std::cout << "  Bits 14-10: Exponent (5 bits, bias=15)" << std::endl;
        std::cout << "  Bits 9-0: Mantissa (10 bits)" << std::endl;

        std::cout << "\nExample Inputs:" << std::endl;
        std::cout << "  0x0000 - Zero" << std::endl;
        std::cout << "  0x8000 - Negative zero" << std::endl;
        std::cout << "  0xbc00 - -1.0" << std::endl;
        std::cout << "  0xc3ff - -3.998047" << std::endl;

        std::cout << "\nCommands:" << std::endl;
        std::cout << "  help, h - Show this help message" << std::endl;
        std::cout << "  quit, q, exit - Exit the program" << std::endl;

        std::cout << std::string(80, '=') << "\n" << std::endl;
    }
};

int sc_main(int argc, char* argv[]) {
    Log2Exp_Interactive_TB tb("Log2Exp_Interactive_Testbench");
    sc_start();
    return 0;
}
