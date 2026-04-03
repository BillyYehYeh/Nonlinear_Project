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
    bool reset = rst.read();
    bool error = has_error.read();
    sc_uint2 current_state = state.read();

    // ============ FIFO Write Control ============
    bool max_fifo_write = process1_stage1_valid.read();
    bool output_fifo_write = process1_stage5_valid.read();
    bool fifo_write_disable = reset || error || (current_state != STATE_PROCESS1);
    if(fifo_write_disable) {
        max_fifo_write_en.write(false);
        output_fifo_write_en.write(false);
    } else {
        max_fifo_write_en.write(max_fifo_write);      
        output_fifo_write_en.write(output_fifo_write);   
    }

    // ============ FIFO Read Control ============
    bool fifo_read_disable = reset || error || (current_state != STATE_PROCESS3);
    if(fifo_read_disable) {
        max_fifo_read_en.write(false);
        output_fifo_read_en.write(false);
    } else {
        bool max_fifo_pop = !process3_stall.read();  
        bool output_fifo_pop = (!process3_stall.read()) && process3_stage2_valid.read();  

        max_fifo_read_en.write(max_fifo_pop);           
        output_fifo_read_en.write(output_fifo_pop);      
    }

    // ============ FIFO Clear Control ============
    max_fifo_clear.write(reset || error);
    output_fifo_clear.write(reset || error);
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
    
    sc_uint2 current_state = state.read();
    bool p1_finish = process1_finish_flag.read();
    bool p3_finish = process3_finish_flag.read();
    bool start_signal = start.read();
     
    if(rst.read() == true) {
        state.write(STATE_IDLE);
        process_1_enable.write(false);
        process_2_enable.write(false);
        process_3_enable.write(false);
        return;  
    }
    
    // ===== Error Handling: Force return to IDLE if error detected =====
    if (has_error.read()) {
        state.write(STATE_IDLE);
        process_1_enable.write(false);
        process_2_enable.write(false);
        process_3_enable.write(false);
        return; 
    }
    
    switch (current_state) {
        case STATE_IDLE:
            // ===== IDLE State: Wait for START =====
            process_1_enable.write(false);
            process_2_enable.write(false);
            process_3_enable.write(false);

            if (start_signal) {
                state.write(STATE_PROCESS1);
                process_1_enable.write(true);
            }
            break;
            
        case STATE_PROCESS1: {
            // ===== PROCESS1 State: Read from Memory & Compute =====
            process_1_enable.write(true);
            process_2_enable.write(false);
            process_3_enable.write(false);
            if (p1_finish) {
                state.write(STATE_PROCESS2);
                process_1_enable.write(false);
                process_2_enable.write(true);
            } 
            break;
        }
            
        case STATE_PROCESS2: {
            process_1_enable.write(false);
            process_2_enable.write(true);
            process_3_enable.write(false);
            
            // Counter to track PROCESS2 execution time
            static sc_dt::sc_uint<32> process2_cycle_counter = 0;
            if (process_2_enable.read()) {
                process2_cycle_counter++;
                if (process2_cycle_counter >= 10) {     // Assumed: PROCESS2 takes total 10 cycles
                    state.write(STATE_PROCESS3);
                    process_2_enable.write(false);
                    process_3_enable.write(true);
                    process2_cycle_counter = 0;
                }
            }
            break;
        }
            
        case STATE_PROCESS3: {
            process_1_enable.write(false);
            process_2_enable.write(false);
            process_3_enable.write(true);
            if (p3_finish) {
                // All write operations complete: transition to IDLE
                state.write(STATE_IDLE);
                process_3_enable.write(false);
            }
            break;
        }

        default:
            // ===== Invalid State: Return to IDLE =====
            state.write(STATE_IDLE);
            break;
    }
}

// Transition to PROCESS2 when ALL conditions are met:
// Note: Each AXI transfer = 4 elements (64-bit / 16-bit FP16)
// 1. All valid data has been pushed to BOTH FIFOs (push_count_max * 4 >= total_length)
// 2. All valid data has been pushed to Output FIFO (push_count_output * 4 >= total_length)
// 3. ALL read data responses received from AXI (read_data_received * 4 >= total_length) 

// Transition to IDLE when ALL write operations are complete:
// - write_addr_sent_num * 4 >= data_length (all addresses sent)
// - write_data_sent_num * 4 >= data_length (all data sent)
// - write_response_received_num * 4 >= data_length (all responses received)
void Softmax::state_transition_flag() {

    sc_uint64 total_length = data_length.read();
    sc_uint2 state_now = state.read();

    // for process1 finish condition
    sc_uint32 push_count_max = max_fifo_count.read();
    sc_uint32 push_count_output = output_fifo_count.read();
    sc_uint32 read_data_received = read_data_received_count_sig.read();

    // for process3 finish condition
    sc_uint32 write_addr_count = write_addr_sent_num_sig.read();
    sc_uint32 write_data_count = write_data_sent_num_sig.read();
    sc_uint32 write_response_count = write_response_received_num_sig.read();
    
    // Determine finish conditions for each process
    bool p1_finish = (read_data_received * 4 >= total_length) && 
                     (push_count_max * 4 >= total_length) &&   
                     (push_count_output * 4 >= total_length) && 
                     state_now == STATE_PROCESS1;  

    bool p3_finish = (write_addr_count * 4 >= total_length) && 
                     (write_data_count * 4 >= total_length) && 
                     (write_response_count * 4 >= total_length) &&
                     state_now == STATE_PROCESS3;

    process1_finish_flag.write(p1_finish);
    process3_finish_flag.write(p3_finish);
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
 * [0]        done (1-bit done flag, pulses HIGH for one clock when PROCESS3 completes)
 */
void Softmax::status_update_process() {
    using namespace softmax::status;
    
    // Get current state and error info
    sc_uint2 current_state = state.read();
    uint8_t error_code = error_code_sig.read();
    bool has_err = has_error.read();
    bool done_signal = done_pulse.read();
    
    // Build status register value
    uint32_t status_value = 0;
    
    // Set done bit [0]
    if (done_signal) {
        status_value |= (1 << 0);
    }
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
    
    sc_uint2 current_state = state.read();
    if (current_state == STATE_PROCESS2) {
        stall_process2_output_Signal.write(false);
    } else {
        stall_process2_output_Signal.write(true);
    }
}

void Softmax::Buffer_Update() {
    using namespace softmax::status;
    
    // Global_Max_Buffer and Sum_Buffer logic with stage-based validity tracking
    // Also count valid data pushes to FIFOs based on stage validity flags
    
    static sc_uint16 global_max_reg = 0;        // Register to store FP16 max value
    static sc_uint32 sum_buffer_reg = 0;        // Register to store sum value
    
    sc_uint2 current_state = state.read();
    bool stage1_valid = process1_stage1_valid.read();   // Stage1 validity for Global_Max_Buffer
    bool stage5_valid = process1_stage5_valid.read();   // Stage5 validity for Sum_Buffer

    if (rst.read()) {
        // Reset both buffers on reset signal
        global_max_reg = 0;
        sum_buffer_reg = 0;
        Global_Max_Buffer_Out.write(0);
        Sum_Buffer_Out.write(0);
    } 
    else if (current_state == STATE_PROCESS1) {  
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
    
    sc_uint2 current_state = state.read();
    sc_uint64 total_length = data_length.read();
    sc_uint64 src_base = src_addr_base.read();
    
    static sc_uint32 read_addr_sent_num = 0;
    static sc_uint32 read_data_received_num = 0;
    static sc_uint32 read_data_timeout_counter = 0;
   
    const sc_uint32 READ_DATA_TIMEOUT_LIMIT = 100;
      
    if (rst.read()) {
        read_addr_sent_num = 0;
        read_data_received_num = 0;
        read_data_received_count_sig.write(0);
        read_addr_sent_num_sig.write(0);
        read_data_timeout_counter = 0;

    }
    else if (current_state == STATE_PROCESS1) {
        
        //====================READ ADDR========================
        // READ_ADDR Handshake occurred:
        bool READ_ADDR_handshake = M_AXI_ARVALID.read() && M_AXI_ARREADY.read();
        if (READ_ADDR_handshake) { 
            read_addr_sent_num++;
            read_addr_sent_num_sig.write(read_addr_sent_num);
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
    }
    else {
        // Not in read states
        read_addr_sent_num = 0;
        read_data_received_num = 0;
        read_addr_sent_num_sig.write(0);
        read_data_received_count_sig.write(0);
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
 * **Address Calculation:**
 * write_address = dst_base + (write_addr_sent_num * 8)
 * Uses write_addr_sent_num (not write_data_sent_num) as the index
 */
void Softmax::axi_write_request_process() {
    using namespace softmax::status;
    
    sc_uint2 current_state = state.read();
    sc_uint64 total_length = data_length.read();
    sc_uint64 dst_base = dst_addr_base.read();
    bool stage4_valid = process3_stage4_valid.read();  // Check PROCESS_3 Stage4 validity (aligned with M_AXI_WDATA)

    // Static counters for tracking write operations
    static sc_uint32 write_addr_sent_num = 0;
    static sc_uint32 write_data_sent_num = 0;
    static sc_uint32 write_response_received_num = 0;
    static bool prev_stage3_valid = false;
    static bool prev_stall = false;
    
    if (rst.read()) {
        M_AXI_AWADDR.write(0);
        M_AXI_AWVALID.write(false);
        M_AXI_WSTRB.write(0);
        M_AXI_WVALID.write(false);
        M_AXI_BREADY.write(false);
        write_addr_sent_num_sig.write(0);
        write_data_sent_num_sig.write(0);
        write_response_received_num_sig.write(0);
        write_addr_sent_num = 0;
        write_data_sent_num = 0;
        write_response_received_num = 0;
        prev_stage3_valid = false;
        prev_stall = false;
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
        
        if (write_addr_sent_num * 4 < total_length && stage4_valid) {
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
        std::cerr << "\033[35m" << "M_AXI_WVALID: " << M_AXI_WVALID.read() << " M_AXI_WREADY: " << M_AXI_WREADY.read() << "\033[0m" << " @" << sc_time_stamp()<< std::endl;
        if (WRITE_DATA_handshake) { 
            write_data_sent_num++;
            write_data_sent_num_sig.write(write_data_sent_num);

            sc_uint64 data = M_AXI_WDATA.read();
            std::cerr << "\033[34m" << "[WRITE_DATA_HANDSHAKE] " << "\033[34m" <<"Num: " << std::dec << write_data_sent_num 
                    << "\033[34m" << " WRITE DATA : 0x" 
                    << std::hex << std::setfill('0')
                    << std::setw(4) << data.range(63, 48) << " "
                    << std::setw(4) << data.range(47, 32) << " "
                    << std::setw(4) << data.range(31, 16) << " "
                    << std::setw(4) << data.range(15, 0)
                    << "\033[0m" << " @" << sc_time_stamp() << std::dec << std::endl;
        }
        /*sc_uint64 data_1 = M_AXI_WDATA.read();
        std::cerr << "[WRITE_DATA] " << "\033[34m" << "DATA : 0x"   
                    << std::hex 
                    << data_1.range(63, 48) << " "
                    << data_1.range(47, 32) << " "
                    << data_1.range(31, 16) << " "
                    << data_1.range(15, 0)
                    << "\033[0m" << " @" << sc_time_stamp() << std::dec << std::endl;*/

        if (write_data_sent_num * 4 < total_length && stage4_valid) {
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
            std::cerr << "\033[1;34m" << "[WRITE_RESP_HANDSHAKE] write_response_received_num now " << write_response_received_num 
                    << "\033[0m" << " @" << sc_time_stamp() << std::endl;
        }

    }
    else {
        // Not in write states
        M_AXI_AWADDR.write(0);
        M_AXI_AWVALID.write(false);
        M_AXI_WSTRB.write(0);
        M_AXI_WVALID.write(false);
        M_AXI_BREADY.write(false);
        write_addr_sent_num_sig.write(0);
        write_data_sent_num_sig.write(0);
        write_response_received_num_sig.write(0);
        write_addr_sent_num = 0;
        write_data_sent_num = 0;
        write_response_received_num = 0;
        prev_stage3_valid = false;
        prev_stall = false;
    }
}

 /* **Stalling Logic:**
 * When write handshake is incomplete (not both AWREADY and WREADY):
 * - Set process3_stall=1 to freeze PROCESS_3 pipeline
 * - This prevents new data from propagating while AXI slave is busy
 * - Once slave is ready and handshake succeeds, clear stall signal
 */
void Softmax::stall_process3_control() {
    sc_uint2 state_now = state.read();
    // Data has arrived at stage4 (aligned with M_AXI_WDATA), but AXI slave is not ready to accept data
    bool STALL_PROCESS = process3_stage4_valid.read() && !M_AXI_WREADY.read();
    if (rst.read()) {
        process3_stall.write(false);
    } else if (state_now == STATE_PROCESS3) {
        process3_stall.write(STALL_PROCESS);
    } else {
        process3_stall.write(false);
    }
}


void Softmax::validity_signal_update() {
    process1_read_data_valid.write( M_AXI_RVALID.read() && M_AXI_RREADY.read());
    process3_read_data_valid.write(max_fifo_read_en.read());

}

void Softmax::dubug_print() {
    bool max_fifo_pop = !process3_stall.read();  
    bool output_fifo_pop = (!process3_stall.read()) && process3_stage2_valid.read();

    bool axi_write_handshake = (M_AXI_WVALID.read() && M_AXI_WREADY.read());
    std::cerr << "\033[32m" << "[axi_write_handshake]: " << axi_write_handshake 
              << "[process3_stall]: " << process3_stall.read() << "\033[0m" << " @" << sc_time_stamp() << std::endl;
    std::cerr << "\033[32m" << "[max_fifo_read_en]: " << max_fifo_read_en.read() 
              << "[output_fifo_read_en]: " << output_fifo_read_en.read() << "\033[0m" << " @" << sc_time_stamp() << std::endl;
}
 

/**
 * @brief Done Pulse Handler
 * 
 * **Functional Overview:**
 * Generates a one-cycle pulse for the done signal when PROCESS3 completes.
 * Detects the rising edge of p3_finish (transition from false to true) and
 * pulses done_pulse HIGH for exactly one clock cycle.
 * 
 * **Operation:**
 * Clock 0: p3_finish=0, p3_finish_prev=0 → done_pulse=0
 * Clock 1: p3_finish=1, p3_finish_prev=0 → Rising edge detected → done_pulse=1, p3_finish_prev=1
 * Clock 2: p3_finish=1, p3_finish_prev=1 → No rising edge → done_pulse=0, p3_finish_prev=1
 * 
 * This ensures only ONE clock cycle with done_pulse=1
 */
void Softmax::done_pulse_handler() {
    sc_uint32 waddr_count = write_addr_sent_num_sig.read();
    sc_uint32 wdata_count = write_data_sent_num_sig.read();
    sc_uint32 wresp_count = write_response_received_num_sig.read();
    sc_uint64 length = data_length.read();
    
    // Check if all write operations are complete
    bool p3_finish = (waddr_count * 4 >= length) && 
                     (wdata_count * 4 >= length) && 
                     (wresp_count * 4 >= length);
    
    // Read previous cycle's p3_finish state
    bool p3_finish_prev = done_pulse_prev.read();
    
    if (rst.read()) {
        done_pulse.write(false);
        done_pulse_prev.write(false);
    }
    else if (p3_finish && !p3_finish_prev) {
        done_pulse.write(true);
    }
    else {
        done_pulse.write(false);
    }
    
    // Update prev state for next cycle
    done_pulse_prev.write(p3_finish);
}

void Softmax::error_detection_process() {
    using namespace softmax::status;
    
    sc_uint2 current_state = state.read();
    sc_uint64 total_length = data_length.read();
    bool start_signal = start.read();
    
    // Static counters for timeout detection (maintained across cycles)
    static sc_uint32 read_timeout_counter = 0;
    static sc_uint32 write_timeout_counter = 0;
    
    // Check for error conditions
    bool error_detected = false;
    uint8_t error_code = ERR_NONE;
    
    // ===== Check for Invalid State =====
    // Ensure current_state is within valid range (0-3)
    if (current_state > 3) {
        error_detected = true;
        error_code = ERR_INVALID_STATE;
    }
    
    // ===== Check for Data Length Error =====
    // Data length is invalid at start: zero or exceeds configured DATA_LENGTH_MAX.
    if (!error_detected && current_state == STATE_IDLE && start_signal &&
        (total_length == 0 || total_length > DATA_LENGTH_MAX)) {
        error_detected = true;
        error_code = ERR_DATA_LENGTH_INVALID;
    }
    
    // ===== Check for Max FIFO Overflow =====
    if (!error_detected && max_fifo_full.read() && process1_stage1_valid.read()) {
        error_detected = true;
        error_code = ERR_MAX_FIFO_OVERFLOW;
    }
    
    // ===== Check for Output FIFO Overflow =====
    if (!error_detected && output_fifo_full.read() && process1_stage5_valid.read()) {
        error_detected = true;
        error_code = ERR_OUTPUT_FIFO_OVERFLOW;
    }
    
    // ===== Check for AXI Read Error =====
    if (!error_detected && current_state == STATE_PROCESS1) {
        // Check AXI read response error
        // RRESP: 2'b00 = OKAY, 2'b01 = EXOKAY, 2'b10 = SLVERR, 2'b11 = DECERR
        bool read_error_occurred = M_AXI_RVALID.read() && M_AXI_RREADY.read() && 
                                   (M_AXI_RRESP.read() != 0x0);
        if (read_error_occurred) {
            error_detected = true;
            error_code = ERR_AXI_READ_ERROR;
        }
    }
    
    // ===== Check for AXI Read Data Missing =====
    if (!error_detected && process1_finish_flag.read()) {
        sc_uint32 addr_sent = read_addr_sent_num_sig.read();
        sc_uint32 data_received = read_data_received_count_sig.read();
        
        // If number of addresses sent does not match number of data received, some data is missing
        if (addr_sent != data_received) {
            error_detected = true;
            error_code = ERR_AXI_READ_DATA_MISSING;
        }
        // Debugging: print the number of addresses sent and data received at the end of PROCESS1
        std::cerr << "-----------[Data Read Details]---------" << std::endl;
        std::cerr << "   Read Data Received: " << data_received << std::endl;
        std::cerr << "   Read Addr Sent: " << addr_sent << std::endl;
        std::cerr << "   Max FIFO Count: " << max_fifo_count.read() << std::endl;
        std::cerr << "   Output FIFO Count: " << output_fifo_count.read() << std::endl;
    }
    
    // ===== Check for AXI Write Error =====
    if (!error_detected && current_state == STATE_PROCESS3) {
        // Check AXI write response error
        // BRESP: 2'b00 = OKAY, 2'b01 = EXOKAY, 2'b10 = SLVERR, 2'b11 = DECERR
        bool write_error_occurred = M_AXI_BVALID.read() && M_AXI_BREADY.read() && 
                                    (M_AXI_BRESP.read() != 0x0);
        if (write_error_occurred) {
            error_detected = true;
            error_code = ERR_AXI_WRITE_ERROR;
        }
    }
    
    // ===== Check for AXI Write Response Mismatch =====
    if (!error_detected && process3_finish_flag.read()) {
        sc_uint32 addr_sent = write_addr_sent_num_sig.read();
        sc_uint32 data_sent = write_data_sent_num_sig.read();
        sc_uint32 resp_received = write_response_received_num_sig.read();

        // If the number of addresses sent, data sent, and responses received do not match, there is a mismatch
        bool error_condition = (addr_sent != data_sent) || (addr_sent != resp_received) || (data_sent != resp_received);
        
        if (error_condition) {
            error_detected = true;
            error_code = ERR_AXI_WRITE_RESPONSE_MISMATCH;
        }
        std::cerr << "-----------[Data Write Details]---------" << std::endl;
        std::cerr << "   Write Addr Count: " << addr_sent << std::endl;
        std::cerr << "   Write Data Count: " << data_sent << std::endl;
        std::cerr << "   Write Response Count: " << resp_received << std::endl;
        
    }
    
    // ===== Check for AXI Read Timeout =====
    // Track READ_DATA handshake (M_AXI_RVALID && M_AXI_RREADY) inactivity
    if (!error_detected && current_state == STATE_PROCESS1) {
        bool read_data_handshake = M_AXI_RVALID.read() && M_AXI_RREADY.read();
        bool read_addr_handshake = M_AXI_ARVALID.read() && M_AXI_ARREADY.read();
        bool any_read_handshake = read_data_handshake || read_addr_handshake;
        sc_uint32 pack_received = read_data_received_count_sig.read();
        sc_uint64 data_received = (sc_uint64)pack_received * 4;
        
        if (any_read_handshake) {
            // Handshake occurred: reset timeout counter
            read_timeout_counter = 0;
        } else if (data_received < total_length) {
            // No handshake and transfer incomplete: increment timeout counter
            read_timeout_counter++;
            
            // Timeout detected if counter >= AXI_TIMEOUT_THRESHOLD
            if (read_timeout_counter >= AXI_TIMEOUT_THRESHOLD) {
                error_detected = true;
                error_code = ERR_AXI_READ_TIMEOUT;
            }
        }
    }
    // When not in PROCESS1, reset read timeout counter
    else if (current_state != STATE_PROCESS1) {
        read_timeout_counter = 0;
    }
    
    // ===== Check for AXI Write Timeout =====
    // Track any write handshake inactivity (AWVALID&&AWREADY OR WVALID&&WREADY OR BVALID&&BREADY)
    if (!error_detected && current_state == STATE_PROCESS3) {
        bool write_addr_handshake = M_AXI_AWVALID.read() && M_AXI_AWREADY.read();
        bool write_data_handshake = M_AXI_WVALID.read() && M_AXI_WREADY.read();
        bool write_resp_handshake = M_AXI_BVALID.read() && M_AXI_BREADY.read();
        bool any_write_handshake = write_addr_handshake || write_data_handshake || write_resp_handshake;
        sc_uint32 addr_pack_sent = write_addr_sent_num_sig.read();
        sc_uint32 data_pack_sent = write_data_sent_num_sig.read();
        sc_uint32 resp_received = write_response_received_num_sig.read();
        sc_uint64 addr_sent = (sc_uint64)addr_pack_sent * 4;
        sc_uint64 data_sent = (sc_uint64)data_pack_sent * 4;
        sc_uint64 resp_recv = (sc_uint64)resp_received * 4;
        
        // Check if transfer incomplete
        bool transfer_incomplete = (addr_sent < total_length) || (data_sent < total_length) || (resp_recv < total_length);

        //std::cerr << "write_timeout_counter: " << write_timeout_counter << " @" << sc_time_stamp() << std::endl;
        if (any_write_handshake) {
            // Any write handshake occurred: reset timeout counter
            write_timeout_counter = 0;
        } else if (transfer_incomplete && !any_write_handshake &&
                   (M_AXI_AWVALID.read() || M_AXI_WVALID.read() || M_AXI_BREADY.read())) {
            // No handshake while AWVALID/WVALID/BVALID is asserted: increment timeout counter.
            write_timeout_counter++;
        } else {
            // Do not count idle pipeline bubbles as AXI timeout.
            write_timeout_counter = 0;
        }
            
        // Timeout detected if counter >= AXI_TIMEOUT_THRESHOLD
        if (write_timeout_counter >= AXI_TIMEOUT_THRESHOLD) {
            error_detected = true;
            error_code = ERR_AXI_WRITE_TIMEOUT;
        }
    }
    // When not in PROCESS3, reset write timeout counter
    else if (current_state != STATE_PROCESS3) {
        write_timeout_counter = 0;
    }
    
    // ===== Update error signals =====
    has_error.write(error_detected);
    error_code_sig.write(error_code);
}
