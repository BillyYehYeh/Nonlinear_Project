#ifndef Softmax_H
#define Softmax_H

#include <systemc.h>
#include <iostream>
#include "PROCESS_1.h"
#include "PROCESS_2.h"
#include "PROCESS_3.h"
#include "Max_FIFO.h"
#include "Output_FIFO.h"
#include "axi4-lite.hpp"
#include "Status.hpp"

using sc_uint2 = sc_dt::sc_uint<2>;
using sc_uint4 = sc_dt::sc_uint<4>;
using sc_uint16 = sc_dt::sc_uint<16>;
using sc_uint32 = sc_dt::sc_uint<32>;
using sc_uint64 = sc_dt::sc_uint<64>;
using sc_uint3 = sc_dt::sc_uint<3>;
using sc_uint8 = sc_dt::sc_uint<8>;

// Use AXI4-Lite parameters from axi4-lite.hpp
using namespace hybridacc::axi4lite;
using namespace softmax::status;
constexpr unsigned AXI_ADDR_WIDTH = AXI4L_DEFAULT_ADDR_WIDTH;   // 32
constexpr unsigned AXI_DATA_WIDTH = AXI4L_DEFAULT_DATA_WIDTH;   // 64
constexpr unsigned AXI_STRB_WIDTH = AXI4L_DEFAULT_STRB_WIDTH;   // 8

SC_MODULE(Softmax) {
    // ===== System Ports =====
    sc_in<bool>              clk;                  ///< Clock signal 
    sc_in<bool>              rst;                  ///< Reset signal
    
    // ===== Control Signals from SOLE MMIO =====
    sc_in<bool>              start;                ///< Start signal from MMIO Control register
    sc_in<sc_uint64>         src_addr_base;        ///< Source base address (read from memory)
    sc_in<sc_uint64>         dst_addr_base;        ///< Destination base address (write to memory)
    sc_in<sc_uint32>         data_length;          ///< Number of FP16 elements to process
    
    // ===== Status Output to SOLE MMIO =====
    sc_out<sc_uint32>        status;               ///< Status register (state, error, error_code) 
    
    // ===== AXI4-Lite Master Ports (Write Address Channel) =====
    sc_out<sc_dt::sc_uint<AXI_ADDR_WIDTH>>  M_AXI_AWADDR;   ///< Write address
    sc_out<bool>                            M_AXI_AWVALID;  ///< Write address valid
    sc_in<bool>                             M_AXI_AWREADY;  ///< Write address ready
    
    // ===== AXI4-Lite Master Ports (Write Data Channel) =====
    sc_out<sc_dt::sc_uint<AXI_DATA_WIDTH>> M_AXI_WDATA;     ///< Write data (64-bit)
    sc_out<sc_dt::sc_uint<AXI_STRB_WIDTH>> M_AXI_WSTRB;     ///< Write strobes (8-bit, byte enables)
    sc_out<bool>                           M_AXI_WVALID;    ///< Write data valid
    sc_in<bool>                            M_AXI_WREADY;    ///< Write data ready
    
    // ===== AXI4-Lite Master Ports (Write Response Channel) =====
    sc_in<sc_uint2>                        M_AXI_BRESP;     ///< Write response (2-bit)
    sc_in<bool>                            M_AXI_BVALID;    ///< Write response valid
    sc_out<bool>                           M_AXI_BREADY;    ///< Write response ready
    
    // ===== AXI4-Lite Master Ports (Read Address Channel) =====
    sc_out<sc_dt::sc_uint<AXI_ADDR_WIDTH>>  M_AXI_ARADDR;   ///< Read address
    sc_out<bool>                            M_AXI_ARVALID;  ///< Read address valid
    sc_in<bool>                             M_AXI_ARREADY;  ///< Read address ready
    
    // ===== AXI4-Lite Master Ports (Read Data Channel) =====
    sc_in<sc_dt::sc_uint<AXI_DATA_WIDTH>>  M_AXI_RDATA;     ///< Read data (64-bit)
    sc_in<sc_uint2>                        M_AXI_RRESP;     ///< Read response (2-bit)
    sc_in<bool>                            M_AXI_RVALID;    ///< Read data valid
    sc_out<bool>                           M_AXI_RREADY;    ///< Read data ready        

    // ===== Internal Signals & Registers =====
    PROCESS_1_Module   *Process_1_unit; 
    PROCESS_2_Module   *Process_2_unit; 
    PROCESS_3_Module   *Process_3_unit; 
    Max_FIFO           *Max_FIFO_unit;
    Output_FIFO        *Output_FIFO_unit; 

    /** State Machine - Controls which process is active */
    sc_signal<sc_uint2>     State;          ///< '0':idle   '1':Process1 running  '2':Process2 running  '3':Process3 running
    
    /** Enable signals for each process module */
    sc_signal<bool>         process_1_enable;      ///< Enable PROCESS_1 (read from memory + compute)
    sc_signal<bool>         process_2_enable;      ///< Enable PROCESS_2 (main computation)
    sc_signal<bool>         process_3_enable;      ///< Enable PROCESS_3 (write to memory + compute)
    
    /** Data counter and address generation */
    sc_signal<sc_uint32>    data_counter;          ///< Counts processed elements (0 to data_length-1)
    sc_signal<sc_uint64>    current_src_addr;      ///< Current source address = src_addr_base + data_counter*8
    sc_signal<sc_uint64>    current_dst_addr;      ///< Current destination address = dst_addr_base + data_counter*8
    
    /** Status tracking */
    sc_signal<uint8_t>      error_code_sig;        ///< Current error code
    sc_signal<bool>         has_error;             ///< Error flag

    /** Global Max Buffer & Sum Buffer  */
    sc_signal<sc_uint16>    Global_Max_Buffer_Out;      ///< Read output (combinational, all states)
    sc_signal<sc_uint16>    Global_Max_Buffer_In;       ///< Update input (write-only, state=1)

    sc_signal<sc_uint32>    Sum_Buffer_Out;             ///< Read output (combinational, all states)
    sc_signal<sc_uint32>    Sum_Buffer_In;              ///< Update input (write-only, state=1)
    
    /** FIFO Control Signals */
    sc_signal<bool>         max_fifo_write_en;          ///< Max FIFO write enable (1 when state=1)
    sc_signal<bool>         max_fifo_read_en;           ///< Max FIFO read enable (1 when state=3)
    sc_signal<bool>         max_fifo_clear;             ///< Max FIFO clear (1 when error)
    sc_signal<bool>         max_fifo_full;              ///< Max FIFO full flag
    sc_signal<bool>         max_fifo_empty;             ///< Max FIFO empty flag
    
    sc_signal<bool>         output_fifo_write_en;       ///< Output FIFO write enable (1 when state=1)
    sc_signal<bool>         output_fifo_read_en;        ///< Output FIFO read enable (1 when state=3)
    sc_signal<bool>         output_fifo_clear;          ///< Output FIFO clear (1 when error)
    sc_signal<bool>         output_fifo_full;           ///< Output FIFO full flag
    sc_signal<bool>         output_fifo_empty;          ///< Output FIFO empty flag

    /** Data signal routing between Modules  */
    sc_signal<sc_uint16>    Global_Max_In_Signal;
    sc_signal<sc_uint32>    Sum_Buffer_In_Signal;
    sc_signal<sc_uint16>    Power_of_Two_Vector_Signal;
    sc_signal<sc_uint32>    Sum_Buffer_Update_Signal;
    sc_signal<sc_uint16>    Local_Max_Output_Signal;
    sc_signal<sc_uint32>    Pre_Compute_In_Signal;
    sc_signal<sc_uint4>     Leading_One_Pos_Out_Signal;
    sc_signal<sc_uint16>    Mux_Result_Out_Signal;

    sc_signal<sc_uint16>    Local_Max_Signal;
    sc_signal<sc_uint16>    Global_Max_Signal;
    sc_signal<sc_uint16>    Output_Buffer_In_Signal;

    /**Control signal */
    sc_signal<bool>         stall_process2_output_Signal;
    
    /** PROCESS1 Data Validity Flags (from pipeline stages) */
    sc_signal<bool>         process1_read_data_valid;   ///< Read data valid from AXI (input to PROCESS1)
    sc_signal<bool>         stage1_data_valid;          ///< Stage1 data valid flag (output from PROCESS1)
    sc_signal<bool>         stage5_data_valid;          ///< Stage5 data valid flag (output from PROCESS1)
    sc_signal<sc_uint32>    fifo_push_count_max;        ///< Number of valid data pushed to Max_FIFO
    sc_signal<sc_uint32>    fifo_push_count_output;     ///< Number of valid data pushed to Output_FIFO
    
    /** PROCESS3 Data Validity Flag and Write Control */
    sc_signal<bool>         stage3_data_valid;          ///< Stage3 data valid flag (output from PROCESS3, for AXI write control)
    sc_signal<bool>         write_handshake_success;    ///< TRUE when both AWVALID&&AWREADY and WVALID&&WREADY occur
    sc_signal<bool>         stall_process3_output;      ///< Stall signal for PROCESS_3 (when write handshake fails)
    
    /** Write completion tracking signals */
    sc_signal<sc_uint32>    write_addr_sent_num_sig;    ///< Number of write addresses sent (for state transition checking)
    sc_signal<sc_uint32>    write_data_sent_num_sig;    ///< Number of write data items sent (for state transition checking)
    sc_signal<sc_uint32>    write_response_received_num_sig;  ///< Number of write responses received (for state transition checking)
    
    // ===== AXI4-Lite Internal Signals (Master) =====
    
    /** Write Address Channel Control Signals */
    sc_signal<sc_dt::sc_uint<AXI_ADDR_WIDTH>> axi_awaddr_sig;
    sc_signal<bool>                           axi_awvalid_sig;
    sc_signal<bool>                           axi_awready_sig;
    
    /** Write Data Channel Control Signals */
    sc_signal<sc_dt::sc_uint<AXI_DATA_WIDTH>> axi_wdata_sig;     ///< 64-bit write data
    sc_signal<sc_dt::sc_uint<AXI_STRB_WIDTH>> axi_wstrb_sig;     ///< 8-bit write strobes
    sc_signal<bool>                           axi_wvalid_sig;
    sc_signal<bool>                           axi_wready_sig;
    
    /** Write Response Channel Control Signals */
    sc_signal<sc_uint2>                       axi_bresp_sig;
    sc_signal<bool>                           axi_bvalid_sig;
    sc_signal<bool>                           axi_bready_sig;
    
    /** Read Address Channel Control Signals */
    sc_signal<sc_dt::sc_uint<AXI_ADDR_WIDTH>> axi_araddr_sig;
    sc_signal<bool>                           axi_arvalid_sig;
    sc_signal<bool>                           axi_arready_sig;
    
    /** Read Data Channel Control Signals */
    sc_signal<sc_dt::sc_uint<AXI_DATA_WIDTH>> axi_rdata_sig;     ///< 64-bit read data
    sc_signal<sc_uint2>                       axi_rresp_sig;
    sc_signal<bool>                           axi_rvalid_sig;
    sc_signal<bool>                           axi_rready_sig;
    
    // Note: Removed axi_read_request_valid and axi_write_request_valid
    // These were causing extra clock delays. Now using combinational logic based on state
    // to enable pipelined reads/writes (one data per clock cycle)


    // ===== Constructor =====
    SC_HAS_PROCESS(Softmax);
    Softmax(sc_core::sc_module_name name) : sc_core::sc_module(name) {

        // Initialize state machine
        State.write(STATE_IDLE);
        data_counter.write(0);
        error_code_sig.write(ERR_NONE);
        has_error.write(false);
        
        // Initialize status output
        status.write(0);
        
        // Initialize new process1 data validity and FIFO push counters
        process1_read_data_valid.write(false);
        stage1_data_valid.write(false);
        stage5_data_valid.write(false);
        fifo_push_count_max.write(0);
        fifo_push_count_output.write(0);
        
        // Initialize process3 data validity and write control signals
        stage3_data_valid.write(false);
        write_handshake_success.write(false);
        stall_process3_output.write(false);
        write_addr_sent_num_sig.write(0);
        write_data_sent_num_sig.write(0);
        write_response_received_num_sig.write(0);
        
        // ===== Initialize AXI4-Lite Control Signals =====
        axi_awaddr_sig.write(0);
        axi_awvalid_sig.write(false);
        axi_wdata_sig.write(0);
        axi_wstrb_sig.write(0xFF);      // All 8 bytes enabled (64-bit data)
        axi_wvalid_sig.write(false);
        axi_bready_sig.write(false);
        axi_araddr_sig.write(0);
        axi_arvalid_sig.write(false);
        axi_rready_sig.write(false);
        


        // ===== Construct PROCESS_1_Module=====
        Process_1_unit = new PROCESS_1_Module("Process_1_unit");
        Process_1_unit->clk(clk);
        Process_1_unit->rst(rst);
        Process_1_unit->DataIn_64bits(axi_rdata_sig);
        Process_1_unit->enable(process_1_enable);
        Process_1_unit->data_valid(process1_read_data_valid);  // Connect data validity from AXI
        Process_1_unit->Global_Max(Global_Max_Buffer_Out);
        Process_1_unit->Sum_Buffer_In(Sum_Buffer_In_Signal);
        Process_1_unit->Power_of_Two_Vector(Power_of_Two_Vector_Signal);
        Process_1_unit->Sum_Buffer_Update(Sum_Buffer_Update_Signal);
        Process_1_unit->Local_Max_Output(Local_Max_Output_Signal);
        Process_1_unit->stage1_valid(stage1_data_valid);  // Output: Stage1 validity flag
        Process_1_unit->stage5_valid(stage5_data_valid);  // Output: Stage5 validity flag

        // ===== Construct PROCESS_2_Module=====
        Process_2_unit = new PROCESS_2_Module("Process_2_unit");
        Process_2_unit->clk(clk);
        Process_2_unit->rst(rst);
        Process_2_unit->enable(process_2_enable);  // Add enable signal
        Process_2_unit->stall_output(stall_process2_output_Signal);
        Process_2_unit->Pre_Compute_In(Pre_Compute_In_Signal);
        Process_2_unit->Leading_One_Pos_Out(Leading_One_Pos_Out_Signal);
        Process_2_unit->Mux_Result_Out(Mux_Result_Out_Signal);

        // ===== Construct PROCESS_3_Module=====
        Process_3_unit = new PROCESS_3_Module("Process_3_unit");
        Process_3_unit->clk(clk);
        Process_3_unit->rst(rst);
        Process_3_unit->enable(process_3_enable);
        Process_3_unit->stall(stall_process3_output);
        Process_3_unit->data_valid(process_3_enable);      // Input: data valid when PROCESS_3 is enabled (new input data arrives)
        Process_3_unit->Local_Max(Local_Max_Signal);
        Process_3_unit->Global_Max(Global_Max_Buffer_Out);
        Process_3_unit->ks_In(Leading_One_Pos_Out_Signal);
        Process_3_unit->Mux_Result_In(Mux_Result_Out_Signal);
        Process_3_unit->Output_Buffer_In(Output_Buffer_In_Signal);
        Process_3_unit->Output_Vector(axi_wdata_sig);
        Process_3_unit->stage3_valid(stage3_data_valid);   // Output: stage3 valid flag for AXI write control

        // ===== Construct Max_FIFO=====
        Max_FIFO_unit = new Max_FIFO("Max_FIFO_unit");
        Max_FIFO_unit->clk(clk);
        Max_FIFO_unit->rst(rst);
        Max_FIFO_unit->data_in(Local_Max_Output_Signal);
        Max_FIFO_unit->write_en(max_fifo_write_en);
        Max_FIFO_unit->read_en(max_fifo_read_en);
        Max_FIFO_unit->clear(max_fifo_clear);
        Max_FIFO_unit->data_out(Local_Max_Signal);
        Max_FIFO_unit->full(max_fifo_full);
        Max_FIFO_unit->empty(max_fifo_empty);

        // ===== Construct Output_FIFO=====
        Output_FIFO_unit = new Output_FIFO("Output_FIFO_unit");
        Output_FIFO_unit->clk(clk);
        Output_FIFO_unit->rst(rst);
        Output_FIFO_unit->data_in(Power_of_Two_Vector_Signal);
        Output_FIFO_unit->write_en(output_fifo_write_en);
        Output_FIFO_unit->read_en(output_fifo_read_en);
        Output_FIFO_unit->clear(output_fifo_clear);
        Output_FIFO_unit->data_out(Output_Buffer_In_Signal);
        Output_FIFO_unit->full(output_fifo_full);
        Output_FIFO_unit->empty(output_fifo_empty);

        // Buffer update 
        SC_METHOD(Buffer_Update);
        sensitive << clk.pos();
        
        // ===== AXI Read Address Request Process (clocked) =====
        SC_METHOD(axi_read_address_process);
        sensitive << clk.pos();
        
        // ===== AXI Write Address & Data Request Process (clocked) =====
        SC_METHOD(axi_write_request_process);
        sensitive << clk.pos();
        
        // ===== AXI Response Handling Process (combinational) =====
        SC_METHOD(axi_response_process);
        sensitive << M_AXI_RVALID << M_AXI_BVALID << M_AXI_ARREADY << M_AXI_AWREADY;
        
        // ===== AXI Port Connection Process (combinational) =====
        SC_METHOD(axi_port_connection_process);
        sensitive << axi_awaddr_sig << axi_awvalid_sig << axi_wdata_sig << axi_wstrb_sig 
                  << axi_wvalid_sig << axi_bready_sig << axi_araddr_sig << axi_arvalid_sig 
                  << axi_rready_sig << M_AXI_RDATA << M_AXI_RRESP << M_AXI_RVALID 
                  << M_AXI_BRESP << M_AXI_BVALID << M_AXI_ARREADY << M_AXI_AWREADY << M_AXI_WREADY;
        
        // ===== Status Update Process (combinational) =====
        SC_METHOD(status_update_process);
        sensitive << State << error_code_sig << has_error;
        
        // ===== PROCESS_2 Output Stall Control (combinational) =====
        SC_METHOD(manage_process2_stall);
        sensitive << State;
        
        // ===== State machine process (thread - clocked) =====
        SC_THREAD(state_machine_process);
        sensitive << clk.pos();
    
    }
    
    // ===== Methods =====
    
    /**
     * @brief State Machine Process (Master Control)
     * Coordinates state transitions and address generation
     * Calls manage_fifo_control() and execute_state_transition() for separation of concerns
     */
    void state_machine_process();
    
    /**
     * @brief FIFO Control Logic Manager
     * Handles all FIFO control signals based on current state:
     * - max_fifo_write_en, max_fifo_read_en, max_fifo_clear
     * - output_fifo_write_en, output_fifo_read_en, output_fifo_clear
     * - Detects FIFO full conditions and sets error flags
     */
    void manage_fifo_control();
    
    /**
     * @brief Update Current Address Calculation
     * Computes current_src_addr and current_dst_addr based on data_counter
     * Centralized function to avoid code duplication across multiple processes
     * 
     * Calculation:
     * - current_src_addr = src_addr_base + data_counter * 8
     * - current_dst_addr = dst_addr_base + data_counter * 8
     */
    void update_current_addr();
    
    /**
     * @brief Data Counter Update Logic
     * Manages data_counter increments and resets based on current state and AXI signals.
     * Separated from state machine for better modularity:
     * - IDLE/transitions: reset counter to 0
     * - PROCESS1: increment on successful AXI reads (RVALID && RREADY)
     * - PROCESS3: increment on successful AXI writes (WVALID && WREADY)
     */
    void data_counter_update();
    
    /**
     * @brief State Transition & Process Control
     * Executes state machine transitions (IDLE → PROCESS1 → PROCESS2 → PROCESS3 → IDLE)
     * Manages enable signals (process_1_enable, process_2_enable, process_3_enable)
     * Delegates data counter management to data_counter_update()
     * Detects errors (data_length=0, invalid state)
     */
    void execute_state_transition();
    
    /**
     * @brief Status Update Process
     * Updates the status output register with current state, error flag, and error code
     */
    void status_update_process();
    
    /**
     * @brief Manage PROCESS_2 Output Stall Signal
     * Controls stall_process2_output_Signal based on current state:
     * - When State == STATE_PROCESS2: stall_process2_output_Signal = 0 (allow output)
     * - Otherwise: stall_process2_output_Signal = 1 (stall output)
     */
    void manage_process2_stall();
    
    /**
     * @brief Buffer Update Process
     * Manages Global_Max and Sum buffers with FP16 comparison and accumulation
     */
    void Buffer_Update();
    
    /**
     * @brief AXI Read Address Generation Process
     * Generates AXI read address requests during PROCESS1 state
     * Connects current_src_addr to M_AXI_ARADDR
     */
    void axi_read_address_process();
    
    /**
     * @brief AXI Write Request Process
     * Generates AXI write address and data requests during PROCESS3 state
     * Connects current_dst_addr to M_AXI_AWADDR and axi_wdata_sig to M_AXI_WDATA
     */
    void axi_write_request_process();
    
    /**
     * @brief AXI Response Handling Process
     * Monitors AXI response signals and updates ready signals
     */
    void axi_response_process();
    
    /**
     * @brief AXI Port Connection Process (Combinational)
     * Continuously connects internal AXI signals to the actual M_AXI output ports
     * and reads from M_AXI input ports to internal signals
     */
    void axi_port_connection_process();
};

#endif // Softmax_H