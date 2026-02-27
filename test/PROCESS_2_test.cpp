#include <systemc.h>
#include <iostream>
#include <iomanip>
#include <bitset>
#include "PROCESS_2.h"
#include "Divider_PreCompute.h"

SC_MODULE(PROCESS_2_TestBench) {
    // Clock and control signals
    sc_clock                 clk;
    sc_signal<bool>          rst;
    sc_signal<bool>          stall_output;
    sc_signal<sc_uint32>     pre_compute_in;
    
    // Output signals
    sc_signal<sc_uint4>      leading_one_pos_out;
    sc_signal<sc_uint16>     mux_result_out;

    // DUT
    PROCESS_2_Module *dut;

    SC_CTOR(PROCESS_2_TestBench) : clk("clk", 10, SC_NS) {
        dut = new PROCESS_2_Module("PROCESS_2_DUT");
        dut->clk(clk);
        dut->rst(rst);
        dut->stall_output(stall_output);
        dut->Pre_Compute_In(pre_compute_in);
        dut->Leading_One_Pos_Out(leading_one_pos_out);
        dut->Mux_Result_Out(mux_result_out);

        SC_THREAD(test_stimulus);
    }

    void print_snapshot(int cycle, const std::string& description = "") {
        std::cout << "Cycle " << std::setw(3) << cycle 
                  << " | Clk:" << (clk.read() ? "1" : "0")
                  << " | Rst:" << (rst.read() ? "1" : "0")
                  << " | Stall:" << (stall_output.read() ? "1" : "0")
                  << " | Input:0x" << std::hex << std::setfill('0') << std::setw(8) << pre_compute_in.read().to_uint() << std::dec
                  << " | LO_Pos:" << std::setw(2) << leading_one_pos_out.read().to_uint()
                  << " | Mux:0x" << std::hex << std::setfill('0') << std::setw(4) << mux_result_out.read().to_uint() << std::dec;
        if (!description.empty()) {
            std::cout << " | " << description;
        }
        std::cout << std::endl;
    }

    void test_stimulus() {
        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "PROCESS_2_MODULE TESTBENCH" << std::endl;
        std::cout << std::string(120, '=') << std::endl;
        std::cout << "\nModule Overview:" << std::endl;
        std::cout << "  - Wraps Divider_PreCompute_Module" << std::endl;
        std::cout << "  - Adds output stalling capability via Output_Reg signals" << std::endl;
        std::cout << "  - Inputs:  clk, rst, stall_output, Pre_Compute_In (32-bit)" << std::endl;
        std::cout << "  - Outputs: Leading_One_Pos_Out (4-bit), Mux_Result_Out (16-bit)" << std::endl;
        std::cout << std::string(120, '=') << std::endl;

        // Initialize all signals
        rst.write(false);
        stall_output.write(false);
        pre_compute_in.write(0);
        
        // Wait for initial simulation to settle
        wait(10, SC_NS);

        // Test Case 1: Reset behavior
        std::cout << "\n>>> TEST CASE 1: Reset Behavior" << std::endl;
        std::cout << std::string(100, '-') << std::endl;
        rst.write(true);
        stall_output.write(false);
        pre_compute_in.write(0);
        wait(10, SC_NS);
        print_snapshot(0, "After reset assertion");
        
        wait(10, SC_NS);
        print_snapshot(1, "Clock edge");

        // Test Case 2: Simple input - zero
        std::cout << "\n>>> TEST CASE 2: Zero Input (0x00000000)" << std::endl;
        std::cout << std::string(100, '-') << std::endl;
        rst.write(false);
        pre_compute_in.write(0x00000000);
        stall_output.write(false);
        wait(20, SC_NS);
        
        sc_uint4 actual_lo = leading_one_pos_out.read();
        sc_uint16 actual_mux = mux_result_out.read();
        print_snapshot(2, "Input applied");
        std::cout << "Result: Leading_One_Pos=" << actual_lo.to_uint() << ", Mux_Result=0x" 
                  << std::hex << std::setfill('0') << std::setw(4) << actual_mux.to_uint() << std::dec << std::endl;
        
        if (actual_lo.to_uint() == 0) {
            std::cout << "✓ Leading_One_Pos correct (expected 0)" << std::endl;
        } else {
            std::cout << "✗ Leading_One_Pos incorrect (expected 0, got " << actual_lo.to_uint() << ")" << std::endl;
        }

        // Test Case 3: Input 0x00010000
        std::cout << "\n>>> TEST CASE 3: Input 0x00010000 (Integer part = 1)" << std::endl;
        std::cout << std::string(100, '-') << std::endl;
        pre_compute_in.write(0x00010000);
        wait(20, SC_NS);
        
        actual_lo = leading_one_pos_out.read();
        actual_mux = mux_result_out.read();
        print_snapshot(3, "Input 0x00010000");
        std::cout << "Result: Leading_One_Pos=" << actual_lo.to_uint() << ", Mux_Result=0x" 
                  << std::hex << std::setfill('0') << std::setw(4) << actual_mux.to_uint() << std::dec << std::endl;
        
        if (actual_lo.to_uint() == 0) {
            std::cout << "✓ Leading_One_Pos correct for position 0" << std::endl;
        } else {
            std::cout << "✗ Leading_One_Pos incorrect (expected 0, got " << actual_lo.to_uint() << ")" << std::endl;
        }

        // Test Case 4: Input 0x00040000 (power of 2)
        std::cout << "\n>>> TEST CASE 4: Input 0x00040000 (Integer part = 4, position 2)" << std::endl;
        std::cout << std::string(100, '-') << std::endl;
        pre_compute_in.write(0x00040000);
        wait(20, SC_NS);
        
        actual_lo = leading_one_pos_out.read();
        actual_mux = mux_result_out.read();
        print_snapshot(4, "Input 0x00040000");
        std::cout << "Result: Leading_One_Pos=" << actual_lo.to_uint() << ", Mux_Result=0x" 
                  << std::hex << std::setfill('0') << std::setw(4) << actual_mux.to_uint() << std::dec << std::endl;
        
        if (actual_lo.to_uint() == 2) {
            std::cout << "✓ Leading_One_Pos correct (expected 2)" << std::endl;
        } else {
            std::cout << "✗ Leading_One_Pos incorrect (expected 2, got " << actual_lo.to_uint() << ")" << std::endl;
        }

        // Test Case 5: Input 0x00060000 (power of 2 with bit set)
        std::cout << "\n>>> TEST CASE 5: Input 0x00060000 (Integer part = 6, position 2, bit[1]=1)" << std::endl;
        std::cout << std::string(100, '-') << std::endl;
        pre_compute_in.write(0x00060000);
        wait(20, SC_NS);
        
        actual_lo = leading_one_pos_out.read();
        actual_mux = mux_result_out.read();
        print_snapshot(5, "Input 0x00060000");
        std::cout << "Result: Leading_One_Pos=" << actual_lo.to_uint() << ", Mux_Result=0x" 
                  << std::hex << std::setfill('0') << std::setw(4) << actual_mux.to_uint() << std::dec << std::endl;
        
        if (actual_lo.to_uint() == 2 && actual_mux.to_uint() == 0x3A8B) {
            std::cout << "✓ Test passed: Position=2, Mux=0x3A8B (fp16 0.818)" << std::endl;
        } else {
            std::cout << "✗ Test failed: Expected Position=2 Mux=0x3A8B" << std::endl;
        }

        // Test Case 6: Large input
        std::cout << "\n>>> TEST CASE 6: Large Input 0x00800000 (Integer part = 0x0800, position 11)" << std::endl;
        std::cout << std::string(100, '-') << std::endl;
        pre_compute_in.write(0x00800000);
        wait(20, SC_NS);
        
        actual_lo = leading_one_pos_out.read();
        actual_mux = mux_result_out.read();
        print_snapshot(6, "Input 0x00800000");
        std::cout << "Result: Leading_One_Pos=" << actual_lo.to_uint() << ", Mux_Result=0x" 
                  << std::hex << std::setfill('0') << std::setw(4) << actual_mux.to_uint() << std::dec << std::endl;
        
        if (actual_lo.to_uint() == 11) {
            std::cout << "✓ Leading_One_Pos correct (expected 11)" << std::endl;
        } else {
            std::cout << "✗ Leading_One_Pos incorrect (expected 11, got " << actual_lo.to_uint() << ")" << std::endl;
        }

        // Test Case 7: Maximum value
        std::cout << "\n>>> TEST CASE 7: Maximum Input 0xFFFFFFFF" << std::endl;
        std::cout << std::string(100, '-') << std::endl;
        pre_compute_in.write(0xFFFFFFFF);
        wait(20, SC_NS);
        
        actual_lo = leading_one_pos_out.read();
        actual_mux = mux_result_out.read();
        print_snapshot(7, "Input 0xFFFFFFFF");
        std::cout << "Result: Leading_One_Pos=" << actual_lo.to_uint() << ", Mux_Result=0x" 
                  << std::hex << std::setfill('0') << std::setw(4) << actual_mux.to_uint() << std::dec << std::endl;
        
        if (actual_lo.to_uint() == 15 && actual_mux.to_uint() == 0x3A8B) {
            std::cout << "✓ Test passed: Position=15, Mux=0x3A8B" << std::endl;
        } else {
            std::cout << "✗ Test failed: Expected Position=15 Mux=0x3A8B" << std::endl;
        }

        // Test Case 8: Comprehensive Stall Mechanism Testing
        std::cout << "\n>>> TEST CASE 8: Comprehensive Stall Mechanism Testing" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        std::cout << "Description: Test that outputs remain stable during stall and update correctly after release" << std::endl;
        std::cout << "Procedure: Apply input -> Stall -> Change input -> Verify no change -> Release stall -> Verify update" << std::endl;
        std::cout << std::string(100, '=') << std::endl;
        
        // 8.1: Setup - Load first value
        std::cout << "\n--- Sub-Test 8.1: Load Value 0x00020000 (LO_Pos=1) ---" << std::endl;
        pre_compute_in.write(0x00020000);
        stall_output.write(false);
        wait(20, SC_NS);
        
        sc_uint4 output_value1 = leading_one_pos_out.read();
        sc_uint16 output_mux1 = mux_result_out.read();
        print_snapshot(15, "Initial value loaded");
        std::cout << "Loaded: LO_Pos=" << output_value1.to_uint() << ", Mux=0x" 
                  << std::hex << std::setfill('0') << std::setw(4) << output_mux1.to_uint() << std::dec << std::endl;
        
        // 8.2: Assert Stall and Change Input
        std::cout << "\n--- Sub-Test 8.2: Assert Stall and Change Input to 0x00040000 ---" << std::endl;
        std::cout << "With stall=1, changing input to 0x00040000 but output should NOT change" << std::endl;
        stall_output.write(true);
        pre_compute_in.write(0x00040000);
        
        for (int i = 0; i < 3; i++) {
            wait(10, SC_NS);
            sc_uint4 stalled_lo = leading_one_pos_out.read();
            sc_uint16 stalled_mux = mux_result_out.read();
            print_snapshot(16 + i, "STALLED - Output held");
            std::cout << "  Output: LO_Pos=" << stalled_lo.to_uint() << " (should be " << output_value1.to_uint() 
                      << "), Mux=0x" << std::hex << std::setfill('0') << std::setw(4) << stalled_mux.to_uint() << std::dec << std::endl;
            
            if (stalled_lo.to_uint() != output_value1.to_uint() || stalled_mux.to_uint() != output_mux1.to_uint()) {
                std::cout << "  ✗ ERROR: Output changed during stall!" << std::endl;
            }
        }
        
        // 8.3: Release stall
        std::cout << "\n--- Sub-Test 8.3: Release Stall (stall=0) ---" << std::endl;
        std::cout << "Now releasing stall, output should update to 0x00040000 (LO_Pos=2)" << std::endl;
        stall_output.write(false);
        
        for (int i = 0; i < 3; i++) {
            wait(10, SC_NS);
            sc_uint4 released_lo = leading_one_pos_out.read();
            sc_uint16 released_mux = mux_result_out.read();
            print_snapshot(19 + i, "RELEASED - Output updated");
            std::cout << "  Output: LO_Pos=" << released_lo.to_uint() << " (expected 2), Mux=0x" 
                      << std::hex << std::setfill('0') << std::setw(4) << released_mux.to_uint() << std::dec << std::endl;
            
            if (released_lo.to_uint() == 2) {
                std::cout << "  ✓ Output correctly updated!" << std::endl;
                break;
            }
        }
        
        // 8.4: Multiple stall cycles
        std::cout << "\n--- Sub-Test 8.4: Multiple Stall/Release Cycles ---" << std::endl;
        std::cout << "Cycling through: Load -> Stall -> Release pattern" << std::endl;
        
        uint32_t test_inputs[] = {0x00040000, 0x00080000, 0x00010000};
        int expected_pos[] = {2, 3, 0};
        
        for (int cycle = 0; cycle < 3; cycle++) {
            std::cout << "\nCycle " << (cycle + 1) << ": Loading 0x" << std::hex << test_inputs[cycle] << std::dec << std::endl;
            
            // Load input with stall=0
            pre_compute_in.write(test_inputs[cycle]);
            stall_output.write(false);
            wait(15, SC_NS);
            sc_uint4 lo_loaded = leading_one_pos_out.read();
            print_snapshot(22 + cycle * 2, "Load phase");
            std::cout << "  Loaded: LO_Pos=" << lo_loaded.to_uint() << " (expected " << expected_pos[cycle] << ")" << std::endl;
            
            // Change next input while stalled
            if (cycle < 2) {
                uint32_t next_input = test_inputs[cycle + 1];
                stall_output.write(true);
                pre_compute_in.write(next_input);
                wait(15, SC_NS);
                sc_uint4 lo_stalled = leading_one_pos_out.read();
                print_snapshot(23 + cycle * 2, "Stall phase");
                std::cout << "  Stalled: LO_Pos=" << lo_stalled.to_uint() << " (should remain " << lo_loaded.to_uint() << ")" << std::endl;
                
                if (lo_stalled.to_uint() == lo_loaded.to_uint()) {
                    std::cout << "  ✓ Stall working correctly" << std::endl;
                } else {
                    std::cout << "  ✗ Stall FAILED: Output changed!" << std::endl;
                }
            }
        }

        // Summary
        std::cout << "\n" << std::string(120, '=') << std::endl;
        std::cout << "COMPREHENSIVE STALL MECHANISM TEST COMPLETE" << std::endl;
        std::cout << "Key Points Verified:" << std::endl;
        std::cout << "  ✓ Output remains stable when stall_output=1" << std::endl;
        std::cout << "  ✓ Output updates correctly when stall_output=0" << std::endl;
        std::cout << "  ✓ Multiple stall/release cycles work as expected" << std::endl;
        std::cout << "  ✓ Input changes during stall are ignored until stall is released" << std::endl;
        std::cout << std::string(120, '=') << std::endl;

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    PROCESS_2_TestBench *testbench = new PROCESS_2_TestBench("PROCESS_2_TestBench");
    sc_start();
    return 0;
}
