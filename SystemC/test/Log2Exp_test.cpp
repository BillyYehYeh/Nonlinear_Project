#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <bitset>
#include <cmath>
#include "Log2Exp.h"
#include "test_utils.h"


SC_MODULE(Log2Exp_TestBench) {
    sc_signal<sc_dt::sc_uint<16>> fp16_in_sig;
    sc_signal<sc_dt::sc_uint<4>>  result_out_sig;

    Log2Exp *dut;
    
    // Test tracking
    int total_tests = 0;
    int passed_tests = 0;
    int failed_tests = 0;

    SC_CTOR(Log2Exp_TestBench) {
        dut = new Log2Exp("Log2Exp_DUT");
        dut->fp16_in(fp16_in_sig);
        dut->result_out(result_out_sig);

        SC_THREAD(test_stimulus);
    }

    void print_fp16_info_and_internal_calculation(uint16_t bits) {
        // FP16 bit extraction
        int sign = (bits >> 15) & 0x1;
        int exp_bits = (bits >> 10) & 0x1F;
        int mant_bits = bits & 0x3FF;
        float float_val = fp16_to_float(bits);
        
        std::cout << "\n========== INPUT FP16 ANALYSIS ==========" << std::endl;
        std::cout << "  Hex Value:        0x" << std::hex << std::setw(4) << std::setfill('0') << bits << std::dec << std::endl;
        std::cout << "  Binary:           " << std::bitset<16>(bits) << std::endl;
        std::cout << "  Sign Bit:         " << sign << std::endl;
        std::cout << "  Exponent (5bit):  " << std::bitset<5>(exp_bits) << " (decimal: " << exp_bits << ")" << std::endl;
        std::cout << "  Mantissa (10bit): " << std::bitset<10>(mant_bits) << " (decimal: " << mant_bits << ")" << std::endl;
        std::cout << "  Float Value:      " << std::scientific << std::setprecision(6) << float_val << std::defaultfloat << std::endl;
        
        // Internal calculation steps (replicating Log2Exp.cpp logic)
        std::cout << "\n========== INTERNAL CALCULATION STEPS ==========" << std::endl;
        
        int exponent_val = exp_bits;
        int actual_exp_val = exponent_val - 15;
        
        std::cout << "Step 1: Extract Components" << std::endl;
        std::cout << "  exponent_val = " << exponent_val << std::endl;
        std::cout << "  actual_exp_val = exponent_val - 15 = " << actual_exp_val << std::endl;
        
        // Construct mantissa with implicit 1
        uint32_t mant_val = ((1U << 14) | (mant_bits << 4)) & 0x3FFFF;
        uint32_t op1 = mant_val;
        uint32_t op2 = (mant_val >> 1);
        uint32_t op3 = (mant_val >> 4);
        
        std::cout << "\nStep 2: Construct Mantissa with Implicit 1" << std::endl;
        std::cout << "  mant_val (18bit): " << std::bitset<18>(mant_val) << " (0x" << std::hex << mant_val << std::dec << ")" << std::endl;
        std::cout << "  op1 = mant_val:   " << std::bitset<18>(op1) << " (dec: " << op1 << ")" << std::endl;
        std::cout << "  op2 = mant>>1:    " << std::bitset<18>(op2) << " (dec: " << op2 << ")" << std::endl;
        std::cout << "  op3 = mant>>4:    " << std::bitset<18>(op3) << " (dec: " << op3 << ")" << std::endl;
        
        // Calculate sum
        uint32_t sum = op1 + op2 - op3;
        std::cout << "\nStep 3: Calculate Sum = op1 + op2 - op3" << std::endl;
        std::cout << "  sum (18bit):      " << std::bitset<18>(sum) << " (0x" << std::hex << sum << std::dec << ", dec: " << sum << ")" << std::endl;
        
        // Apply exponent shift
        uint32_t shifted_value;
        std::cout << "\nStep 4: Apply Exponent Shift" << std::endl;
        std::cout << "  actual_exp_val = " << actual_exp_val << std::endl;
        
        if (actual_exp_val <= 0) {
            shifted_value = (sum >> (-actual_exp_val));
            std::cout << "  Shift RIGHT by " << (-actual_exp_val) << " positions (actual_exp <= 0)" << std::endl;
        } else {
            shifted_value = (sum << actual_exp_val);
            std::cout << "  Shift LEFT by " << actual_exp_val << " positions (actual_exp > 0)" << std::endl;
        }
        std::cout << "  shifted_value:    " << std::bitset<18>(shifted_value) << " (0x" << std::hex << shifted_value << std::dec << ", dec: " << shifted_value << ")" << std::endl;
        
        // Extract result bits [17:14]
        uint32_t shift_res = (shifted_value >> 14) & 0xF;
        std::cout << "\nStep 5: Extract Result Bits [17:14]" << std::endl;
        std::cout << "  shifted_value[17:14]: " << std::bitset<4>(shift_res) << " (dec: " << shift_res << ")" << std::endl;
        
        // Multiplexer logic
        uint32_t final_output;
        std::cout << "\nStep 6: Apply Multiplexer Logic (Exponent Range Check)" << std::endl;
        std::cout << "  Condition Check:" << std::endl;
        std::cout << "  - exponent_val <= 13? " << (exponent_val <= 13 ? "YES" : "NO") << std::endl;
        std::cout << "  - exponent_val >= 19? " << (exponent_val >= 19 ? "YES" : "NO") << std::endl;
        
        if (exponent_val == 18) {
            int bit15 = (sum >> 15) & 0x1;
            std::cout << "  - exponent_val == 18 AND sum[15]=1? " << (exponent_val == 18 && bit15 == 1 ? "YES" : "NO") << std::endl;
        }
        
        if (exponent_val <= 13) {
            final_output = 0x0;
            std::cout << "  => Output: 0x0 (exponent too small)" << std::endl;
        } else if (exponent_val >= 19) {
            final_output = 0xF;
            std::cout << "  => Output: 0xF (exponent too large)" << std::endl;
        } else if (exponent_val == 18 && ((sum >> 15) & 0x1) == 1) {
            final_output = 0xF;
            std::cout << "  => Output: 0xF (boundary condition: exp=18, bit15=1)" << std::endl;
        } else {
            final_output = shift_res;
            std::cout << "  => Output: 0x" << std::hex << final_output << std::dec << " (normal case: shift_res)" << std::endl;
        }
        
        std::cout << "========== END CALCULATION ==========" << std::endl;
    }

    bool run_test(const std::string& test_name, uint16_t input_val) {
        total_tests++;
        
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << "[TEST " << total_tests << "] " << test_name << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        
        // Print input and internal calculation details
        print_fp16_info_and_internal_calculation(input_val);
        
        // Run the DUT
        fp16_in_sig.write(sc_dt::sc_uint<16>(input_val));
        wait(1, SC_NS);
        
        sc_dt::sc_uint<4> result = result_out_sig.read();
        int result_int = result.to_uint();

        float input_float = fp16_to_float(input_val);
        
        // Theoretical model: output = clamp(floor(-1.4375 * input), 0, 15)
        float theoretical_value = -1.4375f * input_float;
        int theoretical_x = static_cast<int>(std::floor(theoretical_value));
        // Clamp to [0, 15] (4-bit range)
        if (theoretical_x < 0) theoretical_x = 0;
        if (theoretical_x > 15) theoretical_x = 15;

        std::cout << "\n========== THEORETICAL MODEL CALCULATION ==========" << std::endl;
        std::cout << "Theoretical Model: output = clamp(floor(-1.4375 * input), 0, 15)" << std::endl;
        
        std::cout << "  Input Value:                " << std::scientific << std::setprecision(6) << input_float << std::defaultfloat << std::endl;
        std::cout << "  Calculation: -1.4375 * input = " << std::fixed << std::setprecision(6) << theoretical_value << std::endl;
        std::cout << "  After floor():              " << std::fixed << std::setprecision(0) << std::floor(theoretical_value) << std::endl;
        std::cout << "  After clamp [0, 15]:        " << theoretical_x << std::endl;

        std::cout << "\n========== MODULE OUTPUT ==========" << std::endl;
        std::cout << "  Output (4-bit binary): " << std::bitset<4>(result_int) << std::endl;
        std::cout << "  Output (decimal):      " << result_int << std::endl;
        std::cout << "  Output (hex):          0x" << std::hex << result_int << std::dec << std::endl;

        std::cout << "\n========== VERIFICATION ==========" << std::endl;
        bool test_passed = true;
        
        if (theoretical_x >= 0) {
            // Strict check if expected value is specified
            if (result_int == theoretical_x) {
                std::cout << "  [PASS] Output matches expected value: " << theoretical_x << std::endl;
                passed_tests++;
            } else {
                std::cout << "  [FAIL] Expected output: " << theoretical_x << ", Actual output: " << result_int << std::endl;
                std::cout << "         Error: " << std::abs(result_int - theoretical_x) << std::endl;
                test_passed = false;
                failed_tests++;
            }
        } else {
            // Check if module output matches theoretical value
            if (result_int == theoretical_x) {
                std::cout << "  [PASS] Output " << result_int << " matches theoretical value " << theoretical_x << std::endl;
                passed_tests++;
            } else {
                std::cout << "  [FAIL] Expected output: " << theoretical_x << ", Actual output: " << result_int << std::endl;
                std::cout << "         Error: " << std::abs(result_int - theoretical_x) << std::endl;
                test_passed = false;
                failed_tests++;
            }
        }
        
        return test_passed;
    }

    void test_stimulus() {
        std::cout << std::string(100, '=') << std::endl;
        std::cout << "\n" << std::string(30, ' ') << "LOG2EXP MODULE - VERIFICATION TEST SUITE" << std::endl;
        std::cout << "\n  Theoretical Model:  input = -(2^(-x))  =>  output = integer_part(x)" << std::endl;
        std::cout << "  Input Format:       16-bit FP16 negative number" << std::endl;
        std::cout << "  Output Format:      4-bit unsigned integer" << std::endl;
        std::cout << std::string(100, '=') << std::endl;

        // Test cases: input = -(2^(-x))
        // x = 0: input = -1.0 (FP16: 0xBC00)
        // x = 1: input = -0.5 (FP16: 0xB800)
        // x = 2: input = -0.25 (FP16: 0xB400)
        // x = 3: input = -0.125 (FP16: 0xB000)
        // x = 4: input = -0.0625 (FP16: 0xAC00)
        // x = 5: input = -0.03125 (FP16: 0xA800)
        
        run_test("Test 1: x=0, input=-(2^0)=-1.0", 0xBC00);
        run_test("Test 2: x=1, input=-(2^(-1))=-0.5", 0xB800);
        run_test("Test 3: x=2, input=-(2^(-2))=-0.25", 0xB400);
        run_test("Test 4: x=3, input=-(2^(-3))=-0.125", 0xB000);
        run_test("Test 5: x=4, input=-(2^(-4))=-0.0625", 0xAC00);
        run_test("Test 6: x=5, input=-(2^(-5))=-0.03125", 0xA800);
        
        // Boundary and special cases
        run_test("Test 7: x=2.5, input≈-0.177 (FP16: 0xB17F)", 0xB17F);
        run_test("Test 8: x=3.5, input≈-0.0884 (FP16: 0xAD00)", 0xAD00);
        
        // Additional test cases
        run_test("Test 9: Negative test (-0.99)", 0xbbeb);
        run_test("Test 10: Negative test (-0.7)", 0xb999);
        run_test("Test 11: Negative test (-0.69)", 0xb985);
        run_test("Test 12: Negative test (-40)", 0xD100);
        
        // Print summary
        print_test_summary();
        sc_stop();
    }
    
    void print_test_summary() {
        std::cout << "\n" << std::string(100, '=') << std::endl;
        std::cout << std::string(35, ' ') << "TEST SUMMARY REPORT" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        std::cout << "  Total Tests:  " << total_tests << std::endl;
        std::cout << "  Passed:       " << passed_tests << " (" << (total_tests > 0 ? (passed_tests * 100) / total_tests : 0) << "%)" << std::endl;
        std::cout << "  Failed:       " << failed_tests << " (" << (total_tests > 0 ? (failed_tests * 100) / total_tests : 0) << "%)" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        
        if (failed_tests == 0) {
            std::cout << std::string(35, ' ') << "*** ALL TESTS PASSED ***" << std::endl;
        } else {
            std::cout << std::string(35, ' ') << "*** SOME TESTS FAILED ***" << std::endl;
        }
        std::cout << std::string(100, '=') << std::endl;
    }
};

int sc_main(int argc, char* argv[]) {
    Log2Exp_TestBench tb("Log2Exp_TestBench");
    sc_start();
    return 0;
}
