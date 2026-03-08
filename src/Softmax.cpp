#include "Softmax.h"
#include "utils.h"

/**
 * @brief State Machine Process (Master Control)
 * 
 * **Functional Overview:**
 * Coordinates the overall state machine operation by:
 * 1. Delegating FIFO control to manage_fifo_control()
 * 2. Delegating state transitions to execute_state_transition()
 * 
 * Note: Address calculation now happens dynamically in axi_read_address_process()
 * and axi_write_request_process() based on AXI handshake counters.
 */
void Softmax::state_machine_process() {
    // Delegate FIFO control logic to specialized function
    manage_fifo_control();
    
    // Delegate state transition logic to specialized function
    execute_state_transition();
}

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
            // No FIFO activity in IDLE state
            max_fifo_write_en.write(false);
            max_fifo_read_en.write(false);
            output_fifo_write_en.write(false);
            output_fifo_read_en.write(false);
            break;
            
        case STATE_PROCESS1:
            // PROCESS_1: Write to both FIFOs (fill stage)
            // Max_FIFO write control depends on Stage1 data validity
            // Output_FIFO write control depends on Stage5 data validity
            max_fifo_write_en.write(true && stage1_valid);      // Max_FIFO: write only if Stage1 data valid
            max_fifo_read_en.write(false);
            output_fifo_write_en.write(true && stage5_valid);   // Output_FIFO: write only if Stage5 data valid
            output_fifo_read_en.write(false);
            
            // Check FIFO full conditions and set error flags
            if (max_fifo_full.read()) {
                has_error.write(true);
                error_code_sig.write(ERR_MAX_FIFO_FULL);
            }
            if (output_fifo_full.read()) {
                has_error.write(true);
                error_code_sig.write(ERR_OUTPUT_FIFO_FULL);
            }
            break;
            
        case STATE_PROCESS2:
            // PROCESS_2: FIFOs locked (no access)
            max_fifo_write_en.write(false);
            max_fifo_read_en.write(false);
            output_fifo_write_en.write(false);
            output_fifo_read_en.write(false);
            break;
            
        case STATE_PROCESS3: {
            // PROCESS_3: Read from both FIFOs (drain stage)
            // Only read when PROCESS_3 is NOT stalled
            // If stall=1, freeze FIFO reads to prevent data loss
            bool stall_active = stall_process3_output.read();
            
            if (stall_active) {
                // STALL ACTIVE: Hold all FIFO operations (no reads)
                max_fifo_write_en.write(false);
                max_fifo_read_en.write(false);       // Freeze FIFO reads
                output_fifo_write_en.write(false);
                output_fifo_read_en.write(false);    // Freeze FIFO reads
            } else {
                // NORMAL: PROCESS_3 proceeding, read from FIFOs
                max_fifo_write_en.write(false);
                max_fifo_read_en.write(true);        // Drain Max_FIFO for Local_Max input
                output_fifo_write_en.write(false);
                output_fifo_read_en.write(true);     // Drain Output_FIFO for Power_of_Two
            }
            break;
        }
            
        default:
            // Default: disable all FIFO access
            max_fifo_write_en.write(false);
            max_fifo_read_en.write(false);
            output_fifo_write_en.write(false);
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
    sc_uint32 total_length = data_length.read();
    bool start_signal = start.read();
    
    // Static trackers for state transitions
    static bool process1_completion_detected = false;
    static bool process3_completion_detected = false;
    
    // ===== Error Handling: Force return to IDLE if error detected =====
    if (has_error.read()) {
        // Immediately abort all processing and return to IDLE state
        State.write(STATE_IDLE);
        process_1_enable.write(false);
        process_2_enable.write(false);
        process_3_enable.write(false);
        process1_completion_detected = false;
        process3_completion_detected = false;
        return;  // Skip normal state machine logic
    }
    
    switch (current_state) {
        case STATE_IDLE:
            // ===== IDLE State: Wait for START =====
            process_1_enable.write(false);
            process_2_enable.write(false);
            process_3_enable.write(false);
            process1_completion_detected = false;
            process3_completion_detected = false;
            error_code_sig.write(ERR_NONE);
            has_error.write(false);
            
            if (start_signal) {
                // Validate data_length before starting
                if (total_length == 0) {
                    error_code_sig.write(ERR_DATA_LENGTH_ZERO);
                    has_error.write(true);
                    // Stay in IDLE, do not transition
                } else {
                    // Valid length, transition to PROCESS1
                    State.write(STATE_PROCESS1);
                    process_1_enable.write(true);
                }
            }
            break;
            
        case STATE_PROCESS1: {
            // ===== PROCESS1 State: Read from Memory & Compute =====
            process_1_enable.write(true);
            process_2_enable.write(false);
            process_3_enable.write(false);
            
            // Transition to PROCESS2 when:
            // - All valid data has been pushed to BOTH FIFOs
            // - fifo_push_count_max == data_length AND fifo_push_count_output == data_length
            sc_uint32 push_count_max = fifo_push_count_max.read();
            sc_uint32 push_count_output = fifo_push_count_output.read();
            
            if (push_count_max == total_length && push_count_output == total_length) {
                // All valid data pushed to both FIFOs, transition to PROCESS2
                if (!process1_completion_detected) {
                    State.write(STATE_PROCESS2);
                    process_1_enable.write(false);
                    process_2_enable.write(true);
                    process1_completion_detected = true;
                }
            } else {
                process1_completion_detected = false;
            }
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
            // - write_addr_sent_num == data_length (all addresses sent)
            // - write_data_sent_num == data_length (all data sent)
            // - write_response_received_num == data_length (all responses received)
            sc_uint32 write_addr_count = write_addr_sent_num_sig.read();
            sc_uint32 write_data_count = write_data_sent_num_sig.read();
            sc_uint32 write_response_count = write_response_received_num_sig.read();
            
            if (write_addr_count == total_length && 
                write_data_count == total_length && 
                write_response_count == total_length) {
                // All write operations complete: transition to IDLE
                if (!process3_completion_detected) {
                    State.write(STATE_IDLE);
                    process_3_enable.write(false);
                    process3_completion_detected = true;
                    error_code_sig.write(ERR_NONE);  // Clear errors on success
                    has_error.write(false);
                }
            } else {
                process3_completion_detected = false;
            }
            break;
        }
            
        default:
            // ===== Invalid State: Return to IDLE =====
            has_error.write(true);
            error_code_sig.write(ERR_INVALID_STATE);
            State.write(STATE_IDLE);
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
    status.write(status_value);
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
    static sc_uint32 local_fifo_push_max = 0;   // Count valid pushes to Max_FIFO
    static sc_uint32 local_fifo_push_output = 0;// Count valid pushes to Output_FIFO
    
    sc_uint2 current_state = State.read();
    bool stage1_valid = stage1_data_valid.read();  // Stage1 validity for Max_FIFO
    bool stage5_valid = stage5_data_valid.read();  // Stage5 validity for Output_FIFO
    bool max_fifo_write_en_base = max_fifo_write_en.read();
    bool output_fifo_write_en_base = output_fifo_write_en.read();
    
    if (rst.read()) {
        // Reset both buffers on reset signal
        global_max_reg = 0;
        sum_buffer_reg = 0;
        local_fifo_push_max = 0;
        local_fifo_push_output = 0;
        Global_Max_Buffer_Out.write(0);
        Sum_Buffer_Out.write(0);
        fifo_push_count_max.write(0);
        fifo_push_count_output.write(0);
    } 
    else if (current_state == STATE_PROCESS1) {
        // PROCESS_1 stage: Update buffers only when stage data is valid
        
        // Update Global_Max_Buffer only when Stage1 data is valid
        if (stage1_valid) {
            sc_uint16 incoming_max = Global_Max_Buffer_In.read();
            global_max_reg = fp16_max(global_max_reg, incoming_max);
            // Increment Max_FIFO push counter when Stage1 data is valid
            if (max_fifo_write_en_base) {
                local_fifo_push_max++;
            }
        }
        
        // Update Sum_Buffer only when Stage5 data is valid
        if (stage5_valid) {
            sc_uint32 incoming_sum = Sum_Buffer_In.read();
            sum_buffer_reg = incoming_sum;
            // Increment Output_FIFO push counter when Stage5 data is valid
            if (output_fifo_write_en_base) {
                local_fifo_push_output++;
            }
        }
        
        // Write updated values to outputs (combinational)
        Global_Max_Buffer_Out.write(global_max_reg);
        Sum_Buffer_Out.write(sum_buffer_reg);
        
        // Update push count signals for state transition checking
        fifo_push_count_max.write(local_fifo_push_max);
        fifo_push_count_output.write(local_fifo_push_output);
    } 
    else if (current_state == STATE_PROCESS2) {
        // Transitioning out of PROCESS1: keep outputs stable
        Global_Max_Buffer_Out.write(global_max_reg);
        Sum_Buffer_Out.write(sum_buffer_reg);
        
        // Update final push counts for state transition check
        fifo_push_count_max.write(local_fifo_push_max);
        fifo_push_count_output.write(local_fifo_push_output);
        
        // Reset counters for next run
        local_fifo_push_max = 0;
        local_fifo_push_output = 0;
    }
    else {
        // Other states: Maintain outputs (stall)
        Global_Max_Buffer_Out.write(global_max_reg);
        Sum_Buffer_Out.write(sum_buffer_reg);
    }
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
    sc_uint32 total_length = data_length.read();
    sc_uint64 src_base = src_addr_base.read();
    
    // Static counters for tracking read operations
    // Now used to calculate address dynamically: src_addr = src_base + read_addr_sent_num * 8
    static sc_uint32 read_addr_sent_num = 0;
    static sc_uint32 read_data_received_num = 0;
    static sc_uint32 read_data_timeout_counter = 0;  // Detect if no new data for 100 cycles
    static sc_uint32 last_read_data_count = 0;       // Previous cycle's received count for comparison
    
    const sc_uint32 READ_DATA_TIMEOUT_LIMIT = 100;   // Timeout threshold: 100 cycles without new data
    
    if (rst.read()) {
        axi_araddr_sig.write(0);
        axi_arvalid_sig.write(false);
        axi_rready_sig.write(false);
        read_addr_sent_num = 0;
        read_data_received_num = 0;
        read_data_timeout_counter = 0;
        last_read_data_count = 0;
    }
    else if (current_state == STATE_PROCESS1) {
        // During PROCESS1: manage read address handshake
        // Address calculation is now dynamic: use read_addr_sent_num as the index
        
        if (read_addr_sent_num < total_length) {
            // More data to read: present address and keep ARVALID high
            axi_arvalid_sig.write(true);
            
            // Dynamically calculate address: base + (read_addr_sent_num * 8 bytes per element)
            sc_uint64 next_src_addr = src_base + (sc_uint64)read_addr_sent_num * 8;
            axi_araddr_sig.write((sc_dt::sc_uint<32>)(next_src_addr & 0xFFFFFFFF));
            
            // Handshake Detection: ARVALID && ARREADY
            if (axi_arvalid_sig.read() && M_AXI_ARREADY.read()) {
                read_addr_sent_num++;
            }
        } else {
            // All reads sent, deassert ARVALID
            axi_arvalid_sig.write(false);
        }
        
        // Master ready to receive read data
        axi_rready_sig.write(true);
        
        // Data Reception Handshake: RVALID && RREADY
        // Signal to PROCESS_1: data validity flag
        bool read_data_valid = axi_rvalid_sig.read() && axi_rready_sig.read();
        process1_read_data_valid.write(read_data_valid);
        
        if (read_data_valid) {
            read_data_received_num++;
            read_data_timeout_counter = 0;  // Reset timeout counter when data arrives
        } else {
            // No data received this cycle, increment timeout counter
            read_data_timeout_counter++;
        }
        
        // ===== Timeout Detection: No new data for 100+ cycles =====
        if (read_data_timeout_counter >= READ_DATA_TIMEOUT_LIMIT) {
            // Timeout: AXI read data transmission appears stalled
            // Trigger error if we still haven't received all data yet
            if (read_data_received_num < total_length) {
                // Haven't received all data yet, but timeout occurred
                has_error.write(true);
                error_code_sig.write(ERR_AXI_READ_ERROR);
                read_data_timeout_counter = 0;  // Reset to prevent continuous error
            }
        }
        
        // Update last count for next cycle
        last_read_data_count = read_data_received_num;
    }
    else if (current_state == STATE_PROCESS2) {
        // Exiting PROCESS1: verify all data received
        axi_arvalid_sig.write(false);
        axi_rready_sig.write(false);
        
        // Check for missing data
        sc_uint32 inflight_data = read_addr_sent_num - read_data_received_num;
        if (inflight_data != 0) {
            has_error.write(true);
            error_code_sig.write(ERR_AXI_READ_ERROR);
        }
        
        // Reset counters for next run
        read_addr_sent_num = 0;
        read_data_received_num = 0;
        read_data_timeout_counter = 0;
        last_read_data_count = 0;
    }
    else {
        // Not in read states
        axi_arvalid_sig.write(false);
        axi_rready_sig.write(false);
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
    sc_uint32 total_length = data_length.read();
    sc_uint64 dst_base = dst_addr_base.read();
    bool stage3_valid = stage3_data_valid.read();  // Check PROCESS_3 Stage3 validity
    
    // Static counters for tracking write operations
    static sc_uint32 write_addr_sent_num = 0;
    static sc_uint32 write_data_sent_num = 0;
    static sc_uint32 write_response_received_num = 0;
    
    if (rst.read()) {
        axi_awaddr_sig.write(0);
        axi_awvalid_sig.write(false);
        axi_wdata_sig.write(0);
        axi_wstrb_sig.write(0xFF);
        axi_wvalid_sig.write(false);
        axi_bready_sig.write(false);
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
        
        bool awready = M_AXI_AWREADY.read();
        bool wready = M_AXI_WREADY.read();
        bool bvalid = axi_bvalid_sig.read();
        bool bready = axi_bready_sig.read();
        
        // Handshake tracking
        bool awvalid_current = axi_awvalid_sig.read();
        bool wvalid_current = axi_wvalid_sig.read();
        
        
        if (write_addr_sent_num < total_length && stage3_valid) {
            // More data to write AND Stage3 has valid data: present write request
            axi_awvalid_sig.write(true);
            axi_wvalid_sig.write(true);
            axi_bready_sig.write(true);
            
            // Dynamically calculate write address: base + (write_addr_sent_num * 8 bytes per element)
            // Use write_addr_sent_num as the index (not write_data_sent_num)
            sc_uint64 next_dst_addr = dst_base + (sc_uint64)write_addr_sent_num * 8;
            axi_awaddr_sig.write((sc_dt::sc_uint<32>)(next_dst_addr & 0xFFFFFFFF));
            
            // Write data comes from PROCESS_3 output (axi_wdata_sig already routed from PROCESS_3)
            axi_wstrb_sig.write(0xFF);  // All 8 bytes enabled
            
            // ===== Handshake Detection =====
            
            // Write Address Handshake: AWVALID && AWREADY
            bool aw_handshake_success = awvalid_current && awready;
            if (aw_handshake_success) {
                write_addr_sent_num++;
                write_addr_sent_num_sig.write(write_addr_sent_num);
            }
            
            // Write Data Handshake: WVALID && WREADY
            bool w_handshake_success = wvalid_current && wready;
            if (w_handshake_success) {
                write_data_sent_num++;
                write_data_sent_num_sig.write(write_data_sent_num);
            }
            
            // Write Response Handshake: BVALID && BREADY
            if (bvalid && axi_bready_sig.read()) {
                write_response_received_num++;
                write_response_received_num_sig.write(write_response_received_num);
            }
            
            // ===== Stall Detection =====
            // If valid signals are high but slave not ready (handshake incomplete),
            // stall PROCESS_3 to prevent data loss
            bool handshake_attempted = awvalid_current || wvalid_current;
            bool handshake_complete = aw_handshake_success && w_handshake_success;
            
            if (handshake_attempted && !handshake_complete) {
                // Handshake attempted but incomplete: Stall PROCESS_3
                stall_process3_output.write(true);
            } else {
                // Handshake successful or not yet attempted: Allow PROCESS_3 to progress
                stall_process3_output.write(false);
            }
        } 
        else if (write_addr_sent_num < total_length && !stage3_valid) {
            // Data to write but Stage3 currently invalid: Wait for valid data
            axi_awvalid_sig.write(false);
            axi_wvalid_sig.write(false);
            axi_bready_sig.write(true);  // Still ready for responses
            stall_process3_output.write(false);
            
            // Continue receiving responses for previously-sent writes
            if (bvalid && bready) {
                write_response_received_num++;
                write_response_received_num_sig.write(write_response_received_num);
            }
        }
        else {
            // All writes sent, deassert valid signals but keep ready for responses
            axi_awvalid_sig.write(false);
            axi_wvalid_sig.write(false);
            axi_bready_sig.write(true);
            stall_process3_output.write(false);
            
            // Continue receiving responses for already-sent writes
            if (bvalid && bready) {
                write_response_received_num++;
                write_response_received_num_sig.write(write_response_received_num);
            }
        }
    }
    else if (current_state == STATE_IDLE && write_addr_sent_num > 0) {
        // Exiting PROCESS3: verify all writes completed
        axi_awvalid_sig.write(false);
        axi_wvalid_sig.write(false);
        axi_bready_sig.write(false);
        stall_process3_output.write(false);
        
        // Check for write mismatches
        if (write_addr_sent_num != write_data_sent_num) {
            has_error.write(true);
            error_code_sig.write(ERR_AXI_WRITE_ERROR);
        }
        if (write_addr_sent_num != write_response_received_num) {
            has_error.write(true);
            error_code_sig.write(ERR_AXI_WRITE_ERROR);
        }
        
        // Reset counters for next run
        write_addr_sent_num = 0;
        write_data_sent_num = 0;
        write_response_received_num = 0;
    }
    else {
        // Not in write states
        axi_awvalid_sig.write(false);
        axi_wvalid_sig.write(false);
        axi_bready_sig.write(false);
        stall_process3_output.write(false);
    }
}

/**
 * @brief AXI Response Handling Process
 * 
 * **Functional Overview:**
 * Monitors AXI response signals and manages ready signals.
 * - Reads response on R channel: RDATA, RVALID, RRESP
 * - Write response on B channel: BRESP, BVALID
 * - Updates internal error detection based on RRESP and BRESP
 * 
 * **Response Codes:**
 * - 0x0 (OKAY): Successful
 * - 0x2 (SLVERR): Slave error
 * - 0x3 (DECERR): Decode error
 */
void Softmax::axi_response_process() {
    using namespace softmax::status;
    
    // Read response channel monitoring
    if (axi_rvalid_sig.read()) {
        // Check for read errors
        if (axi_rresp_sig.read() != 0x0) {  // Not OKAY
            has_error.write(true);
            error_code_sig.write(ERR_AXI_READ_ERROR);
        }
    }
    
    // Write response channel monitoring
    if (axi_bvalid_sig.read()) {
        // Check for write errors
        if (axi_bresp_sig.read() != 0x0) {  // Not OKAY
            has_error.write(true);
            error_code_sig.write(ERR_AXI_WRITE_ERROR);
        }
    }
}

/**
 * @brief AXI Port Connection Process (Combinational)
 * 
 * **Functional Overview:**
 * Provides continuous bidirectional connection between internal AXI signals
 * and the actual M_AXI Master ports. This allows the state machine and
 * request processes to work with clean internal signals, while maintaining
 * proper AXI protocol on the external ports.
 * 
 * Acts as a simple combinational "wire" that keeps signals in sync in real-time.
 */
void Softmax::axi_port_connection_process() {
    // ===== Write Address Channel =====
    // Drive the output: internal signal → M_AXI output port
    // Master drives: Address and Valid
    // Read the input: M_AXI input port → internal signal
    // Monitor: Ready from slave
    
    M_AXI_AWADDR.write(axi_awaddr_sig.read());    // Write address
    M_AXI_AWVALID.write(axi_awvalid_sig.read());  // Write address valid
    axi_awready_sig.write(M_AXI_AWREADY.read());  // Read slave ready
    
    // ===== Write Data Channel =====
    // Master drives: Data and Strobes and Valid
    // Monitor: Ready from slave
    
    M_AXI_WDATA.write(axi_wdata_sig.read());      // Write data (64-bit)
    M_AXI_WSTRB.write(axi_wstrb_sig.read());      // Write strobes (8-bit)
    M_AXI_WVALID.write(axi_wvalid_sig.read());    // Write data valid
    axi_wready_sig.write(M_AXI_WREADY.read());    // Read slave ready
    
    // ===== Write Response Channel =====
    // Monitor: Response and Valid from slave
    // Master drives: Ready to slave
    
    axi_bresp_sig.write(M_AXI_BRESP.read());      // Read write response
    axi_bvalid_sig.write(M_AXI_BVALID.read());    // Read response valid
    M_AXI_BREADY.write(axi_bready_sig.read());    // Drive ready to slave
    
    // ===== Read Address Channel =====
    // Master drives: Address and Valid
    // Monitor: Ready from slave
    
    M_AXI_ARADDR.write(axi_araddr_sig.read());    // Read address
    M_AXI_ARVALID.write(axi_arvalid_sig.read());  // Read address valid
    axi_arready_sig.write(M_AXI_ARREADY.read());  // Read slave ready
    
    // ===== Read Data Channel =====
    // Monitor: Data and Response and Valid from slave
    // Master drives: Ready to slave
    
    axi_rdata_sig.write(M_AXI_RDATA.read());      // Read data (64-bit)
    axi_rresp_sig.write(M_AXI_RRESP.read());      // Read response
    axi_rvalid_sig.write(M_AXI_RVALID.read());    // Read data valid
    M_AXI_RREADY.write(axi_rready_sig.read());    // Drive ready to slave
}
