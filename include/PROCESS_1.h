#ifndef PROCESS_1_H
#define PROCESS_1_H

#include <systemc.h>
#include <iostream>
#include <sstream>
#include "MaxUnit.h"
#include "Log2Exp.h"
#include "Reduction.h"

using sc_uint4 = sc_dt::sc_uint<4>;
using sc_uint16 = sc_dt::sc_uint<16>;
using sc_uint32 = sc_dt::sc_uint<32>;
using sc_uint64 = sc_dt::sc_uint<64>;

// ===== Data structures for PROCESS_1 pipeline =====
namespace process1_pipeline {

/**
 * @brief Struct to hold 4 fp16 values (DataIn[4]) + validity flag
 */
struct Stage1_Data {
    sc_uint16 DataIn[4];  // 4 fp16 values
    bool data_valid;      // Validity flag: 1=data from successful read, 0=no valid data
};

/**
 * @brief Struct for Pipeline Stage 2: Max_Out and 4 fp16 inputs + validity flag
 */
struct Stage2_Data {
    sc_uint16 Max_Out;    // Maximum value from MaxUnit
    sc_uint16 DataIn[4];  // 4 fp16 input values
    bool data_valid;      // Validity flag: inherited from Stage1
};

/**
 * @brief Struct for Pipeline Stage 3: 5 fp16 values (4 subtraction results + 1 global max subtraction) + validity flag
 */
struct Stage3_Data {
    sc_uint16 diff[5];    // 5 fp16 subtraction results
                          // diff[0..3]: DataIn[i] - Max_Out
                          // diff[4]: Max_Out - Global_Max
    bool data_valid;      // Validity flag: inherited from Stage2
};

/**
 * @brief Struct for Pipeline Stage 4: 5 4-bit values from Log2Exp + validity flag
 */
struct Stage4_Data {
    sc_uint4 power[5];    // 5 4-bit outputs from Log2Exp modules
                          // power[0..3]: from DataIn[i] - Max_Out
                          // power[4]: from Max_Out - Global_Max
    bool data_valid;      // Validity flag: inherited from Stage3
};

/**
 * @brief Struct for Pipeline Stage 5: Packed power vector and right shift amount + validity flag
 */
struct Stage5_Data {
    sc_uint16 Power_of_Two_Vector;    ///< Packed 16-bit (4x 4-bit exponents)
    sc_uint4 Right_Shift_Num;         ///< 4-bit right shift amount
    bool data_valid;                  ///< Validity flag: inherited from Stage4
};

} // namespace process1_pipeline

// ===== Operator overloads for SystemC compatibility =====
namespace sc_core {
    // operator== for Stage1_Data
    inline bool operator==(const process1_pipeline::Stage1_Data& lhs, const process1_pipeline::Stage1_Data& rhs) {
        if (lhs.data_valid != rhs.data_valid) return false;
        for (int i = 0; i < 4; i++) {
            if (lhs.DataIn[i].to_uint() != rhs.DataIn[i].to_uint()) return false;
        }
        return true;
    }
    
    // operator<< for Stage1_Data
    inline std::ostream& operator<<(std::ostream& os, const process1_pipeline::Stage1_Data& data) {
        os << "Stage1_Data[valid=" << data.data_valid << ", data=(";
        for (int i = 0; i < 4; i++) {
            os << std::hex << data.DataIn[i].to_uint();
            if (i < 3) os << ",";
        }
        os << std::dec << ")]";
        return os;
    }
    
    // operator== for Stage2_Data
    inline bool operator==(const process1_pipeline::Stage2_Data& lhs, const process1_pipeline::Stage2_Data& rhs) {
        if (lhs.data_valid != rhs.data_valid) return false;
        if (lhs.Max_Out.to_uint() != rhs.Max_Out.to_uint()) return false;
        for (int i = 0; i < 4; i++) {
            if (lhs.DataIn[i].to_uint() != rhs.DataIn[i].to_uint()) return false;
        }
        return true;
    }
    
    // operator<< for Stage2_Data
    inline std::ostream& operator<<(std::ostream& os, const process1_pipeline::Stage2_Data& data) {
        os << "Stage2_Data[valid=" << data.data_valid << ", max=" << std::hex << data.Max_Out.to_uint() 
           << ", data=(";
        for (int i = 0; i < 4; i++) {
            os << data.DataIn[i].to_uint();
            if (i < 3) os << ",";
        }
        os << std::dec << ")]";
        return os;
    }
    
    // operator== for Stage3_Data
    inline bool operator==(const process1_pipeline::Stage3_Data& lhs, const process1_pipeline::Stage3_Data& rhs) {
        if (lhs.data_valid != rhs.data_valid) return false;
        for (int i = 0; i < 5; i++) {
            if (lhs.diff[i].to_uint() != rhs.diff[i].to_uint()) return false;
        }
        return true;
    }
    
    // operator<< for Stage3_Data
    inline std::ostream& operator<<(std::ostream& os, const process1_pipeline::Stage3_Data& data) {
        os << "Stage3_Data[valid=" << data.data_valid << ", diff=(";
        for (int i = 0; i < 5; i++) {
            os << std::hex << data.diff[i].to_uint();
            if (i < 4) os << ",";
        }
        os << std::dec << ")]";
        return os;
    }
    
    // operator== for Stage4_Data
    inline bool operator==(const process1_pipeline::Stage4_Data& lhs, const process1_pipeline::Stage4_Data& rhs) {
        if (lhs.data_valid != rhs.data_valid) return false;
        for (int i = 0; i < 5; i++) {
            if (lhs.power[i].to_uint() != rhs.power[i].to_uint()) return false;
        }
        return true;
    }
    
    // operator<< for Stage4_Data
    inline std::ostream& operator<<(std::ostream& os, const process1_pipeline::Stage4_Data& data) {
        os << "Stage4_Data[valid=" << data.data_valid << ", power=(";
        for (int i = 0; i < 5; i++) {
            os << (int)data.power[i].to_uint();
            if (i < 4) os << ",";
        }
        os << ")]";
        return os;
    }
    
    // operator== for Stage5_Data
    inline bool operator==(const process1_pipeline::Stage5_Data& lhs, const process1_pipeline::Stage5_Data& rhs) {
        if (lhs.data_valid != rhs.data_valid) return false;
        if (lhs.Power_of_Two_Vector.to_uint() != rhs.Power_of_Two_Vector.to_uint()) return false;
        if (lhs.Right_Shift_Num.to_uint() != rhs.Right_Shift_Num.to_uint()) return false;
        return true;
    }
    
    // operator<< for Stage5_Data
    inline std::ostream& operator<<(std::ostream& os, const process1_pipeline::Stage5_Data& data) {
        os << "Stage5_Data[valid=" << data.data_valid << ", pow_vector=" << std::hex 
           << data.Power_of_Two_Vector.to_uint() << ", shift=" << (int)data.Right_Shift_Num.to_uint() 
           << std::dec << "]";
        return os;
    }
}

/**
 * @brief PROCESS_1 Module - Exponential Processing Pipeline
 * 
 * A 5-stage pipelined module that:
 * 1. Splits 64-bit input into 4 fp16 values and finds maximum
 * 2. Computes differences: DataIn[i] - Max_Out and Max_Out - Global_Max
 * 3. Applies Log2Exp transformation to get 4-bit exponents
 * 4. Packs exponents and performs right-shift and accumulation
 * 
 * Latency: 5 clock cycles
 */
SC_MODULE(PROCESS_1_Module) {
    // ===== Input Ports =====
    sc_in<bool>              clk;               ///< Clock input
    sc_in<bool>              rst;               ///< Reset input
    sc_in<bool>              enable;            ///< Enable signal (1=active, 0=stall)
    sc_in<sc_uint64>         DataIn_64bits;     ///< 64-bit input (4x fp16)
    sc_in<sc_uint16>         Global_Max;        ///< Global maximum (uint16)
    sc_in<sc_uint32>         Sum_Buffer_In;     ///< 32-bit accumulator input
    sc_in<bool>              data_valid;        ///< Data validity flag (from AXI read)
    
    // ===== Output Ports =====
    sc_out<sc_uint16>        Power_of_Two_Vector;  ///< 16-bit packed 4-bit exponents
    sc_out<sc_uint32>        Sum_Buffer_Update;    ///< 32-bit accumulator output
    sc_out<sc_uint16>        Local_Max_Output;     ///< 16-bit Local output to Max Buffer
    sc_out<bool>             stage1_valid;         ///< Stage1 data valid flag (for Max_FIFO control)
    sc_out<bool>             stage5_valid;         ///< Stage5 data valid flag (for Output_FIFO control)

    // ===== Internal Signals =====
    
    // Input parsing signals
    sc_signal<sc_uint16>     DataIn_unpacked[4];   ///< 4 unpacked fp16 values from input
    
    // MaxUnit instantiation
    MaxUnit                  *max_unit;
    sc_signal<sc_uint16>     Max_Out_comb;         ///< Direct output from MaxUnit
    
    // Reduction Module instantiation
    Reduction_Module         *reduction_unit;      ///< Exponential sum reduction module
    sc_signal<sc_uint32>     Reduction_Output;     ///< Output from Reduction module (32-bit sum)
    sc_signal<sc_uint16>   Reduction_In;         ///< Input to Reduction module (packed 4x 4-bit)

    // Log2Exp module instantiation (Between Stage 3 and 4)
    Log2Exp                  *log2exp_units[5];    ///< 5 Log2Exp module instances
    sc_signal<sc_uint16>   log2exp_in[5];        ///< 5 16-bit inputs to Log2Exp modules
    sc_signal<sc_uint4>    log2exp_out[5];       ///< 5 4-bit outputs from Log2Exp modules
    
    // Pipeline Stage 1: Next and Register (Stage1_Data: DataIn[4])
    sc_signal<process1_pipeline::Stage1_Data>   Stage1_Next;          ///< Stage 1 combinational output
    sc_signal<process1_pipeline::Stage1_Data>   Stage1_Reg;           ///< Stage 1 registered output
    
    // Pipeline Stage 2: Next and Register (Stage2_Data: DataIn[4] + MaxUnit Output)
    sc_signal<process1_pipeline::Stage2_Data>   Stage2_Next;          ///< Stage 2 combinational output
    sc_signal<process1_pipeline::Stage2_Data>   Stage2_Reg;           ///< Stage 2 registered output
    
    // Pipeline Stage 3: Next and Register (Stage3_Data: 5 fp16 differences)
    sc_signal<process1_pipeline::Stage3_Data>   Stage3_Next;          ///< Stage 3 combinational output
    sc_signal<process1_pipeline::Stage3_Data>   Stage3_Reg;           ///< Stage 3 registered output
    
    // Pipeline Stage 4: Next and Register (Stage4_Data: 5 4-bit values)
    sc_signal<process1_pipeline::Stage4_Data>   Stage4_Next;          ///< Stage 4 combinational output
    sc_signal<process1_pipeline::Stage4_Data>   Stage4_Reg;           ///< Stage 4 registered output
    
    // Pipeline Stage 5: Next and Register (Stage5_Data: power vector and right shift)
    sc_signal<process1_pipeline::Stage5_Data>   Stage5_Next;          ///< Stage 5 combinational output
    sc_signal<process1_pipeline::Stage5_Data>   Stage5_Reg;           ///< Stage 5 registered output
    

    
    // ===== Methods =====
    void Stage1_Comb();                    ///< Combinational: Generate Stage1_Next
    void Stage2_Comb();                    ///< Combinational: Generate Stage2_Next (differences)
    void Stage3_Comb();                    ///< Combinational: Route to Log2Exp, Generate Stage3_Next
    void Stage4_Comb();                    ///< Combinational: Pack power vector, Generate Stage4_Next
    void Stage5_Comb();                    ///< Combinational: Generate Stage5_Next
    void Pipeline_Update();                ///< Sequential: Update all pipeline registers on clk.pos()
    void Output_Comb();                    ///< Combinational: Generate final outputs
    void Print_Stage_Regs();               ///< Sequential: Print all stage registers on clk.pos()
    
    // ===== Constructor =====
    SC_HAS_PROCESS(PROCESS_1_Module);
    PROCESS_1_Module(sc_core::sc_module_name name) : sc_core::sc_module(name) {
        // Instantiate MaxUnit
        max_unit = new MaxUnit("max_unit");
        max_unit->clk(clk);
        max_unit->rst(rst);
        max_unit->A(DataIn_unpacked[0]);
        max_unit->B(DataIn_unpacked[1]);
        max_unit->C(DataIn_unpacked[2]);
        max_unit->D(DataIn_unpacked[3]);
        max_unit->Max_Out(Max_Out_comb);
        
        // Instantiate Reduction Module
        reduction_unit = new Reduction_Module("reduction_unit");
        reduction_unit->clk(clk);
        reduction_unit->rst(rst);
        reduction_unit->Input_Vector(Reduction_In);
        reduction_unit->Output_Sum(Reduction_Output);
        
        // Instantiate 5 Log2Exp modules
        for (int i = 0; i < 5; i++) {
            std::stringstream ss;
            ss << "log2exp_" << i;
            log2exp_units[i] = new Log2Exp(ss.str().c_str());
            log2exp_units[i]->fp16_in(log2exp_in[i]);
            log2exp_units[i]->result_out(log2exp_out[i]);
        }
        
        SC_METHOD(Stage1_Comb);
        sensitive << DataIn_64bits << data_valid;
        
        SC_METHOD(Stage2_Comb);
        sensitive << Stage1_Reg << Max_Out_comb;
        
        SC_METHOD(Stage3_Comb);
        sensitive << Stage2_Reg << Global_Max;
        
        SC_METHOD(Stage4_Comb);
        sensitive << Stage3_Reg;
        for (int i = 0; i < 5; i++) {
            sensitive << log2exp_out[i];
        }
        
        SC_METHOD(Stage5_Comb);
        sensitive << Stage4_Reg;
        
        SC_METHOD(Pipeline_Update);
        sensitive << clk.pos();
        
        SC_METHOD(Output_Comb);
        sensitive << Stage5_Reg << Reduction_Output << Sum_Buffer_In;
        
        SC_METHOD(Print_Stage_Regs);
        sensitive << clk.pos();
    }
};

#endif // PROCESS_1_H