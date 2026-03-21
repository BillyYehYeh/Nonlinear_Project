#include "Softmax.h"
#include "utils.hpp"
#include <iomanip>

/**
 * @brief FIFO Control Logic Manager
 * 
 * **Functional Overview:**
 * Manages all FIFO-related control signals based on the current state:
 * 
 * **State 0 (IDLE):**
 * - All FIFO access disabled
 * - Clear signals inactive
 * 
 * **State 1 (PROCESS1):**
 * - Max_FIFO: Write-only (PROCESS_1 outputs data to be used by PROCESS_3)
 * - Output_FIFO: Write-only (Power_of_Two_Vector from PROCESS_1)
 * - Write enable is GATED with data_valid flag: only write when data is valid
 * - Detects FIFO full conditions → sets error flags
 * 
 * **State 2 (PROCESS2):**
 * - All FIFO access disabled (pipelined computation, no FIFO activity)
 * 
 * **State 3 (PROCESS3):**
 * - Max_FIFO: Read-only (PROCESS_3 consumes Local_Max data)
 * - Output_FIFO: Read-only (PROCESS_3 consumes Power_of_Two data)
 * 
 * **Error Condition:**
 * - Clear signals active when has_error flag is set
 * - Allows FIFO reset on error detection
 * 
 * **Data Validity Gating (NEW):**
 * - During PROCESS1, write_en is ANDed with process1_read_data_valid
 * - This ensures only valid data (from successful AXI reads) is written to FIFOs
 * - Invalid reads (timeout or missing data) result in write_en=0
 */
void Softmax::manage_fifo_control() {
    using namespace softmax::status;
    
    sc_uint2 current_state = State.read();
    bool stage1_valid = stage1_data_valid.read();
    bool stage5_valid = stage5_data_valid.read();
    
    // Manage read/write enables based on state and stage validity flags
    switch (current_state) {
        case STATE_IDLE:
            max_fifo_read_en.write(false);
            output_fifo_read_en.write(false);
            break;
            
        case STATE_PROCESS1: {
            max_fifo_read_en.write(false);
            output_fifo_read_en.write(false);
            break;
        }
            
        case STATE_PROCESS2:
            max_fifo_read_en.write(false);
            output_fifo_read_en.write(false);
            break;
            
        case STATE_PROCESS3: {
            // PROCESS_3: Read from both FIFOs (drain stage)
            // Only read when PROCESS_3 is NOT stalled
            // If stall=1, freeze FIFO reads to prevent data loss
            bool stall_active = stall_process3_output.read();
            bool Process3Stage1Valid = process3_stage1_valid.read();   // for Max_FIFO and Output_FIFO output data time Alignment
                       
            if (stall_active) {
                // STALL ACTIVE: Hold all FIFO operations (no reads)
                max_fifo_read_en.write(false);         
            } else {
                // NORMAL: PROCESS_3 proceeding, read from FIFOs
                max_fifo_read_en.write(true);           
            }

            std::cout << "Process3 Stage1 Valid: " << Process3Stage1Valid << std::endl;
            if (!stall_active && Process3Stage1Valid) {
                // STALL ACTIVE or stage1 is invalid disabled: Hold all FIFO operations (no reads)      
                output_fifo_read_en.write(true);    
            } else {
                // Not stalled and process3_stage1_valid is true, read from FIFOs      
                output_fifo_read_en.write(false);     
            }

            break;
        }
            
        default:
            // Default: disable all FIFO access
            max_fifo_read_en.write(false);
            output_fifo_read_en.write(false);
            break;
    }
    
    // FIFO clear signals: active when error is detected
    bool error_active = has_error.read();
    max_fifo_clear.write(error_active);
    output_fifo_clear.write(error_active);
}

/**
 * @brief State Transition & Process Control Handler
 * 
 * **Functional Overview:**
 * Implements the core state machine transitions and process enable signals.
 * Handles the 4-state pipeline: IDLE → PROCESS1 → PROCESS2 → PROCESS3 → IDLE
 * Delegates data counter updates to data_counter_update() for cleaner code.
 * 
 * **State 0 (IDLE):**
 * - Waits for start signal from SOLE MMIO
 * - Validates data_length (must be > 0)
 * - Disables all process modules
 * - Clears error flags
 * 
 * **State 1 (PROCESS1):**
 * - Enables PROCESS_1 (reads from AXI + computes)
 * - Data counter incremented by data_counter_update() on AXI reads
 * - Transitions to PROCESS2 when all data fetched
 * 
 * **State 2 (PROCESS2):**
 * - Enables PROCESS_2 (autonomous computation)
 * - Counts cycles until processing complete
 * - Transitions to PROCESS3 when complete
 * 
 * **State 3 (PROCESS3):**
 * - Enables PROCESS_3 (writes to AXI + computes)
 * - Data counter incremented by data_counter_update() on AXI writes
 * - Transitions to IDLE when all data written
 * - Clears error flags on successful completion
 */
void Softmax::execute_state_transition() {
    using namespace softmax::status;
    
    sc_uint2 current_state = State.read();
    sc_uint64 total_length = data_length.read();
    bool start_signal = start.read();
    
    // DEBUG: Track state transitions
    /*static int debug_count = 0;
    if (debug_count++ < 500) {
        std::cerr << "[STATE_TRANS] state=" << (int)current_state.to_uint() 
                 << " start=" << start_signal << " length=" << total_length 
                 << " @" << sc_time_stamp() << std::endl;
    }*/
    
    // Static trackers for state transitions
    static bool process3_completion_detected = false;

    if(rst.read() == true) {
        // Reset all state and control signals
        State.write(STATE_IDLE);
        std::cerr << "============= STATE_IDLE(RESET) =============@" << sc_time_stamp() << std::endl;
        process_1_enable.write(false);
        process_2_enable.write(false);
        process_3_enable.write(false);
        error_code_sig.write(ERR_NONE);
        has_error.write(false);
        done.write(false);
        process3_completion_detected = false;
        return;  // Skip normal state machine logic during reset
    }
    
    // ===== Error Handling: Force return to IDLE if error detected =====
    if (has_error.read()) {
        // Immediately abort all processing and return to IDLE state
        State.write(STATE_IDLE);
        process_1_enable.write(false);
        process_2_enable.write(false);
        process_3_enable.write(false);
        done.write(true);
        process3_completion_detected = false;
        return;  // Skip normal state machine logic
        std::cerr << "============= STATE_IDLE(ERROR) =============@" << sc_time_stamp() << std::endl;
    }
    
    switch (current_state) {
        case STATE_IDLE:
            // ===== IDLE State: Wait for START =====
            process_1_enable.write(false);
            process_2_enable.write(false);
            process_3_enable.write(false);
            process3_completion_detected = false;
            error_code_sig.write(ERR_NONE);
            has_error.write(false);
            done.write(false);  // Clear interrupt signal
            
            if (start_signal) {
                // Validate data_length before starting
                if (total_length == 0) {
                    error_code_sig.write(ERR_DATA_LENGTH_ZERO);
                    has_error.write(true);
                    // Stay in IDLE, do not transition
                } else {
                    // Valid length, transition to PROCESS1
                    State.write(STATE_PROCESS1);
                    std::cerr << "============= STATE_PROCESS1 =============@" << sc_time_stamp() << std::endl;
                    process_1_enable.write(true);
                }
            }
            break;
            
        case STATE_PROCESS1: {
            // ===== PROCESS1 State: Read from Memory & Compute =====
            process_1_enable.write(true);
            process_2_enable.write(false);
            process_3_enable.write(false);
            
            // Transition to PROCESS2 when ALL conditions are met:
            // Note: Each AXI transfer = 4 elements (64-bit / 16-bit FP16)
            // 1. All valid data has been pushed to BOTH FIFOs (push_count_max * 4 >= total_length)
            // 2. All valid data has been pushed to Output FIFO (push_count_output * 4 >= total_length)
            // 3. ALL read data responses received from AXI (read_data_received * 4 >= total_length)
            //    + Wait additional 20 cycles for pipeline to complete
            sc_uint32 push_count_max = max_fifo_count.read();
            sc_uint32 push_count_output = output_fifo_count.read();
            sc_uint32 read_data_received = read_data_received_count_sig.read();
            bool p1_finish = (read_data_received * 4 >= total_length) && 
                             (push_count_max * 4 >= total_length) &&   
                             (push_count_output * 4 >= total_length);  
            if (p1_finish) {
                State.write(STATE_PROCESS2);
                std::cerr << "-----------Data Current Flow---------" << std::endl;
                std::cerr << "  read_data_received: " << read_data_received << std::endl;
                std::cerr << "  max_fifo_count: " << max_fifo_count.read() << std::endl;
                std::cerr << "  output_fifo_count: " << output_fifo_count.read() << std::endl;
                std::cerr << "============= STATE_PROCESS2 =============@" << sc_time_stamp() << std::endl;
                process_1_enable.write(false);
                process_2_enable.write(true);
            } 

            // Debug: Print buffer values and track PROCESS1 execution time
            std::cerr << sc_time_stamp() << "-----------[Buffer Data]---------" << std::endl;
            std::cerr << "Global_Max_Buffer: 0x" << std::hex << Global_Max_Buffer_Out.read() << std::endl;
            uint32_t sum_buffer_val = Sum_Buffer_Out.read();
            uint32_t int_part = (sum_buffer_val >> 16) & 0xFFFF;
            uint32_t frac_part = sum_buffer_val & 0xFFFF;
            double sum_buffer_dec = (double)int_part + (double)frac_part / 65536.0; // fixed point: 16.16
            std::cerr << "Sum_Buffer: 0x" << std::hex << std::setw(4) << std::setfill('0') << int_part
                      << "." << std::setw(4) << frac_part << std::dec << std::setfill(' ')
                      << " -> " << std::fixed << std::setprecision(6) << sum_buffer_dec << std::endl;

            break;
        }
            
        case STATE_PROCESS2: {
            // ===== PROCESS2 State: Main Computation =====
            process_1_enable.write(false);
            process_2_enable.write(true);
            process_3_enable.write(false);
            
           

            // Counter to track PROCESS2 execution time
            static sc_dt::sc_uint<32> process2_cycle_counter = 0;
            
            if (process_2_enable.read()) {
                process2_cycle_counter++;
                
                // Transition when computation complete
                // Assumed: PROCESS2 takes total 10 cycles
                if (process2_cycle_counter >= 10) {
                    State.write(STATE_PROCESS3);
                    

                    std::cerr << "============= STATE_PROCESS3 =============@" << sc_time_stamp() << std::endl;
                    process_2_enable.write(false);
                    process_3_enable.write(true);
                    process2_cycle_counter = 0;
                }
            }
            break;
        }
            
        case STATE_PROCESS3: {
            // ===== PROCESS3 State: Write to Memory & Compute =====
            process_1_enable.write(false);
            process_2_enable.write(false);
            process_3_enable.write(true);
            
            
            // Transition to IDLE when ALL write operations are complete:
            // - write_addr_sent_num * 4 >= data_length (all addresses sent)
            // - write_data_sent_num * 4 >= data_length (all data sent)
            // - write_response_received_num * 4 >= data_length (all responses received)
            sc_uint32 write_addr_count = write_addr_sent_num_sig.read();
            sc_uint32 write_data_count = write_data_sent_num_sig.read();
            sc_uint32 write_response_count = write_response_received_num_sig.read();
            bool p3_finish = (write_addr_count * 4 >= total_length) && 
                             (write_data_count * 4 >= total_length) && 
                             (write_response_count * 4 >= total_length);
            if (p3_finish) {
                // All write operations complete: transition to IDLE
                State.write(STATE_IDLE);
                std::cerr << "-----------Data Current Flow---------" << std::endl;
                std::cerr << "  write_addr_count: " << write_addr_count << std::endl;
                std::cerr << "  write_data_count: " << write_data_count << std::endl;
                std::cerr << "  write_response_count: " << write_response_count << std::endl;
                std::cerr << "============= STATE_IDLE =============@" << sc_time_stamp() << std::endl;
                process_3_enable.write(false);
                error_code_sig.write(ERR_NONE);  
                has_error.write(false);
                done.write(true);  
            }
            break;
        }
            
        default:
            // ===== Invalid State: Return to IDLE =====
            has_error.write(true);
            error_code_sig.write(ERR_INVALID_STATE);
            State.write(STATE_IDLE);
            std::cerr << "============= STATE_IDLE(ERROR) =============@" << sc_time_stamp() << std::endl;
            break;
    }
}

/**
 * @brief Status Update Process
 * 
 * **Functional Overview:**
 * Computes the 32-bit Status register output based on current state,
 * error conditions, and error codes. This status is sent back to SOLE MMIO
 * for Processor to read.
 * 
 * **Status Register Format (32-bit):**
 * [31:8]     Reserved
 * [7:4]      error_code (4-bit error code)
 * [3]        error (1-bit error flag)
 * [2:1]      state (2-bit state value)
 * [0]        Reserved
 */
void Softmax::status_update_process() {
    using namespace softmax::status;
    
    // Get current state and error info
    sc_uint2 current_state = State.read();
    uint8_t error_code = error_code_sig.read();
    bool has_err = has_error.read();
    
    // Build status register value
    uint32_t status_value = 0;
    
    // Set state bits [2:1]
    status_value |= ((uint32_t)(current_state & 0x3) << 1);
    
    // Set error flag bit [3]
    if (has_err) {
        status_value |= (1 << 3);
    }
    
    // Set error code bits [7:4]
    status_value |= ((uint32_t)(error_code & 0xF) << 4);
    
    // Write status to output
    status_o.write(status_value);
}

/**
 * @brief Manage PROCESS_2 Output Stall Control Signal
 * 
 * Controls when PROCESS_2 module is allowed to propagate output data downstream.
 * The stall_process2_output_Signal prevents data from being written to pipeline
 * registers when it should be held (frozen).
 */
void Softmax::manage_process2_stall() {
    using namespace softmax::status;
    
    sc_uint2 current_state = State.read();
    
    if (current_state == STATE_PROCESS2) {
        // STATE_PROCESS2: Allow PROCESS_2 output to propagate (no stall)
        stall_process2_output_Signal.write(false);
    } else {
        // All other states: Stall PROCESS_2 output (freeze pipeline registers)
        stall_process2_output_Signal.write(true);
    }
}

void Softmax::Buffer_Update() {
    using namespace softmax::status;
    
    // Global_Max_Buffer and Sum_Buffer logic with stage-based validity tracking
    // Also count valid data pushes to FIFOs based on stage validity flags
    
    static sc_uint16 global_max_reg = 0;        // Register to store FP16 max value
    static sc_uint32 sum_buffer_reg = 0;        // Register to store sum value
    
    sc_uint2 current_state = State.read();
    bool stage1_valid = stage1_data_valid.read();  // Stage1 validity for Max_FIFO
    bool stage5_valid = stage5_data_valid.read();  // Stage5 validity for Output_FIFO

    if (rst.read()) {
        // Reset both buffers on reset signal
        global_max_reg = 0;
        sum_buffer_reg = 0;
        Global_Max_Buffer_Out.write(0);
        Sum_Buffer_Out.write(0);
    } 
    else if (current_state == STATE_PROCESS1) {
        // PROCESS_1 stage: Update buffers only when stage data is valid
        // Note: Each FIFO push is for 4 elements (one 64-bit transfer)
        
        // Update Global_Max_Buffer only when Stage1 data is valid
        if (stage1_valid) {
            sc_uint16 incoming_max = Global_Max_Buffer_In.read();  
            global_max_reg = fp16_max(global_max_reg, incoming_max);
            
        }
        
        // Update Sum_Buffer only when Stage5 data is valid
        if (stage5_valid) {
            sc_uint32 incoming_sum = Sum_Buffer_In.read();
            sum_buffer_reg = incoming_sum;

        }
        
        // Write updated values to outputs (combinational)
        Global_Max_Buffer_Out.write(global_max_reg);
        Sum_Buffer_Out.write(sum_buffer_reg);
        
    } 
    else if (current_state == STATE_PROCESS2) {
        // Transitioning out of PROCESS1: keep outputs stable
        Global_Max_Buffer_Out.write(global_max_reg);
        Sum_Buffer_Out.write(sum_buffer_reg);
    }
    else {
        // Other states: Maintain outputs (stall)
        Global_Max_Buffer_Out.write(global_max_reg);
        Sum_Buffer_Out.write(sum_buffer_reg);
    }

    /*std::cerr << "Buffer_Update: stage1_valid=" << stage1_valid 
                      << " Global_Max_Buffer_In=0x" << std::hex << Global_Max_Buffer_In.read() 
                      << " Global_Max_Buffer_Out=0x" << Global_Max_Buffer_Out.read() 
                      << " @" << sc_time_stamp() << std::dec << std::endl;*/
}

/**
 * @brief AXI Read Address Generation Process (Proper Handshake)
 * 
 * **Functional Overview:**
 * Implements proper AXI4-Lite read address handshake:
 * - ARVALID/ARREADY handshake: only advance when both high
 * - RVALID/RREADY handshake: only advance when both high
 * - Tracks read addresses sent vs data received (inflight)
 * - Detects missing data (addr sent but data not received)
 * 
 * **Tracking:**
 * - read_addr_sent_num: increments when ARVALID && ARREADY
 * - read_data_received_num: increments when RVALID && RREADY
 * - inflight_data = read_addr_sent_num - read_data_received_num
 * - At end of PROCESS1: inflight_data must be 0
 * 
 * **Operation:**
 */
void Softmax::axi_read_address_process() {
    using namespace softmax::status;
    
    sc_uint2 current_state = State.read();
    sc_uint64 total_length = data_length.read();
    sc_uint64 src_base = src_addr_base.read();
    
    static sc_uint32 read_addr_sent_num = 0;
    static sc_uint32 read_data_received_num = 0;
    static sc_uint32 read_data_timeout_counter = 0;
   
    const sc_uint32 READ_DATA_TIMEOUT_LIMIT = 100;
      
    if (rst.read()) {
        axi_araddr_sig.write(0);
        axi_arvalid_sig.write(false);
        axi_rready_sig.write(false);
        read_addr_sent_num = 0;
        read_data_received_num = 0;
        read_data_received_count_sig.write(0);
        read_data_timeout_counter = 0;

    }
    else if (current_state == STATE_PROCESS1) {
        
        //====================READ ADDR========================
        // READ_ADDR Handshake occurred:
        bool READ_ADDR_handshake = M_AXI_ARVALID.read() && M_AXI_ARREADY.read();
        if (READ_ADDR_handshake) { 
            read_addr_sent_num++;
            std::cerr << "[ADDR_READ_HANDSHAKE] read_addr_sent_num now " << read_addr_sent_num 
                    << " @" << sc_time_stamp() << std::endl;
        }

        if (read_addr_sent_num * 4 < total_length) {
            // DIRECT WRITE to M_AXI ports to avoid delta-cycle timing issues
            sc_uint64 next_src_addr = src_base + (sc_uint64)read_addr_sent_num * 8;
            M_AXI_ARADDR.write((sc_dt::sc_uint<32>)(next_src_addr & 0xFFFFFFFF));
            M_AXI_ARVALID.write(true);  // ← DIRECT WRITE TO PORT, not to internal signal
            std::cerr << "[ARADDR]" << (sc_dt::sc_uint<32>)(next_src_addr & 0xFFFFFFFF)
                            << " @" << sc_time_stamp() << std::endl;

        } else {
            // All reads sent, deassert ARVALID
            M_AXI_ARVALID.write(false);  
        }
        
        //====================READ DATA========================
        // READ_DATA Handshake occurred:
        bool READ_DATA_handshake = M_AXI_RVALID.read() && M_AXI_RREADY.read();
        if (READ_DATA_handshake) { 
            read_data_received_num++;
            sc_uint64 data = M_AXI_RDATA.read();
            std::cerr << "[READ_DATA_HANDSHAKE] Num: " << std::dec << read_data_received_num 
                    << " READ DATA : 0x" 
                    << std::hex << std::setfill('0')
                    << std::setw(4) << data.range(63, 48) << " "
                    << std::setw(4) << data.range(47, 32) << " "
                    << std::setw(4) << data.range(31, 16) << " "
                    << std::setw(4) << data.range(15, 0)
                    << " @" << sc_time_stamp() << std::dec << std::endl;
        }
        
        
        read_data_received_count_sig.write(read_data_received_num);  // Update signal for state machine
        
        // Set M_AXI_RREADY signal
        if (read_data_received_num * 4 < total_length) {
            M_AXI_RREADY.write(true);
        } else {
            M_AXI_RREADY.write(false);      // All Read Data received
        }
        
        // ===== Timeout Detection: No new data for 100+ cycles =====
        /*if (READ_DATA_handshake) {
            read_data_timeout_counter = 0;  // Reset timeout counter when data arrives
        } else {
            read_data_timeout_counter++;    // No data received this cycle
        }
        
        if (read_data_timeout_counter >= READ_DATA_TIMEOUT_LIMIT) {
            // Just track timeout locally - execute_state_transition will check this
            if (read_data_received_num * 4 < total_length) {
                read_data_timeout_counter = 0;  // Reset to allow recovery
            }
        }*/

    }
    else {
        // Not in read states
        axi_arvalid_sig.write(false);
        axi_rready_sig.write(false);
        read_addr_sent_num = 0;
        read_data_received_num = 0;
        read_data_timeout_counter = 0;
    }
}

/**
 * @brief AXI Write Request Process (Dynamic Data Validity with Handshake Stalling)
 * 
 * **Functional Overview:**
 * Manages AXI write address and data requests with dynamic control based on PROCESS_3 output validity.
 * - Only initiates writes when PROCESS_3 Stage3 output is valid (stage3_valid=1)
 * - Tracks write handshakes: AWVALID&&AWREADY and WVALID&&WREADY
 * - Detects handshake failures (when slave not ready) and stalls PROCESS_3 pipeline
 * - Allows AXI slave time to process data before next write cycle
 * 
 * **Write Protocol Flow:**
 * Clock N: STATE_PROCESS3 entered, wait for stage3_valid signal
 * Clock M (when stage3_valid=1): Initiate AXI write with AWVALID=1, WVALID=1
 *   - If M_AXI_AWREADY && M_AXI_WREADY: Write accepted, increment counters
 *   - If M_AXI_AWREADY=0 OR M_AXI_WREADY=0: Write stalled, stall PROCESS_3 output
 * 
 * **Stalling Logic:**
 * When write handshake is incomplete (not both AWREADY and WREADY):
 * - Set stall_process3_output=1 to freeze PROCESS_3 pipeline
 * - This prevents new data from propagating while AXI slave is busy
 * - Once slave is ready and handshake succeeds, clear stall signal
 * 
 * **Address Calculation:**
 * write_address = dst_base + (write_addr_sent_num * 8)
 * Uses write_addr_sent_num (not write_data_sent_num) as the index
 */
void Softmax::axi_write_request_process() {
    using namespace softmax::status;
    
    sc_uint2 current_state = State.read();
    sc_uint64 total_length = data_length.read();
    sc_uint64 dst_base = dst_addr_base.read();
    bool stage3_valid = stage3_data_valid.read();  // Check PROCESS_3 Stage3 validity
    
    // Static counters for tracking write operations
    static sc_uint32 write_addr_sent_num = 0;
    static sc_uint32 write_data_sent_num = 0;
    static sc_uint32 write_response_received_num = 0;
    
    if (rst.read()) {
        M_AXI_AWADDR.write(0);
        M_AXI_AWVALID.write(false);
        M_AXI_WSTRB.write(0);
        M_AXI_WVALID.write(false);
        M_AXI_BREADY.write(false);
        stall_process3_output.write(false);
        write_addr_sent_num_sig.write(0);
        write_data_sent_num_sig.write(0);
        write_response_received_num_sig.write(0);
        write_addr_sent_num = 0;
        write_data_sent_num = 0;
        write_response_received_num = 0;
    }
    else if (current_state == STATE_PROCESS3) {
        // During PROCESS3: manage write handshakes based on data validity
        //====================WRITE ADDR========================
        // WRITE_ADDR Handshake occurred:
        bool WRITE_ADDR_handshake = M_AXI_AWVALID.read() && M_AXI_AWREADY.read();
        if (WRITE_ADDR_handshake) { 
            write_addr_sent_num++;
            write_addr_sent_num_sig.write(write_addr_sent_num);
            std::cerr << "[WRITE_ADDR_HANDSHAKE] write_addr_sent_num now " << write_addr_sent_num 
                    << " @" << sc_time_stamp() << std::endl;
        }
        
        if (write_addr_sent_num * 4 < total_length && stage3_valid) {
            sc_uint64 next_dst_addr = dst_base + (sc_uint64)write_addr_sent_num * 8;
            M_AXI_AWADDR.write((sc_dt::sc_uint<32>)(next_dst_addr & 0xFFFFFFFF));
            M_AXI_AWVALID.write(true);  
            std::cerr << "[AWADDR]" << (sc_dt::sc_uint<32>)(next_dst_addr & 0xFFFFFFFF)
                            << " @" << sc_time_stamp() << std::endl;

        } else {
            // All writes sent, deassert AWVALID
            M_AXI_AWVALID.write(false);  
        }

        //====================WRITE DATA========================
        M_AXI_WSTRB.write(0xFF);    // Set write strb to enabled all 8 bytes when writing

        bool WRITE_DATA_handshake = M_AXI_WVALID.read() && M_AXI_WREADY.read();
        if (WRITE_DATA_handshake) { 
            write_data_sent_num++;
            write_data_sent_num_sig.write(write_data_sent_num);

            sc_uint64 data = M_AXI_WDATA.read();
            std::cerr << "[WRITE_DATA_HANDSHAKE] Num: " << std::dec << write_data_sent_num 
                    << " READ DATA : 0x" 
                    << std::hex << std::setfill('0')
                    << std::setw(4) << data.range(63, 48) << " "
                    << std::setw(4) << data.range(47, 32) << " "
                    << std::setw(4) << data.range(31, 16) << " "
                    << std::setw(4) << data.range(15, 0)
                    << " @" << sc_time_stamp() << std::dec << std::endl;
        }
        sc_uint64 data_1 = M_AXI_WDATA.read();
        std::cerr << "[WRITE_DATA] " << "\033[34m" << "DATA : 0x"   
                    << std::hex << std::setfill('0')
                    << std::setw(4) << data_1.range(63, 48) << " "
                    << std::setw(4) << data_1.range(47, 32) << " "
                    << std::setw(4) << data_1.range(31, 16) << " "
                    << std::setw(4) << data_1.range(15, 0)
                    << "\033[0m" << " @" << sc_time_stamp() << std::dec << std::endl;

        if (write_data_sent_num * 4 < total_length && stage3_valid) {
            M_AXI_WVALID.write(true);  
        } else {
            M_AXI_WVALID.write(false);  
        }

        //====================WRITE RESPONSE========================
        M_AXI_BREADY.write(true);
  
        bool WRITE_RESP_handshake = M_AXI_BVALID.read() && M_AXI_BREADY.read();
        if (WRITE_RESP_handshake) { 
            write_response_received_num++;
            write_response_received_num_sig.write(write_response_received_num);
            std::cerr << "[WRITE_RESP_HANDSHAKE] write_response_received_num now " << write_response_received_num 
                    << " @" << sc_time_stamp() << std::endl;
        }

        //====================stall_process3========================
        // Stall PROCESS3 only when a VALID is asserted but READY is not asserted yet.
        // This prevents stalling when there is no active transaction.
        bool STALL_PROCESS = (M_AXI_AWVALID.read() && !M_AXI_AWREADY.read()) ||
                             (M_AXI_WVALID.read()  && !M_AXI_WREADY.read());
        stall_process3_output.write(STALL_PROCESS);
    }
    else {
        // Not in write states
        M_AXI_AWADDR.write(0);
        M_AXI_AWVALID.write(false);
        M_AXI_WSTRB.write(0);
        M_AXI_WVALID.write(false);
        M_AXI_BREADY.write(false);
        stall_process3_output.write(false);
        write_addr_sent_num_sig.write(0);
        write_data_sent_num_sig.write(0);
        write_response_received_num_sig.write(0);
        write_addr_sent_num = 0;
        write_data_sent_num = 0;
        write_response_received_num = 0;
    }
}

void Softmax::validity_signal_update() {
    process1_read_data_valid.write( M_AXI_RVALID.read() && M_AXI_RREADY.read());
    process3_read_data_valid.write( process_3_enable.read() && !max_fifo_empty.read() && max_fifo_read_en.read() );
}

void Softmax::process3_read_data_valid_delay1() {
    process3_read_data_valid_delay.write( process3_read_data_valid.read() );
}
// delay the valid signal for one cycle to align with the data output from Max_FIFO and Output_FIFO, which are read in Process3  

/*
void Softmax::print_Global_Max_Buffer_In() {
    sc_uint16 max_in = Global_Max_Buffer_In.read();
    std::cerr << "Global_Max_Buffer_In: 0x" << std::hex << std::setw(4) << std::setfill('0') 
              << static_cast<uint16_t>(max_in) << std::dec << " @" << sc_time_stamp() << std::endl;
}*/