#include "SOLE.h"

using namespace sole::mmio;

/**
 * @brief Processor MMIO Access Handler
 * 
 * **Functional Overview:**
 * Implements simple read/write interface for Processor to access SOLE MMIO registers.
 * Processor uses a 4-signal interface (Addr, Wdata, We, Rdata) for configuration.
 * 
 * **Interface Signals:**
 * - proc_addr[31:0]: Register address (Addr[7:0] = register offset)
 * - proc_wdata[31:0]: Data to write (used when proc_we=1)
 * - proc_we: Write enable (1=write operation, 0=read operation)
 * - proc_rdata[31:0]: Read data output (valid when proc_we=0)
 * 
 * **Processor Access Permissions (MMIO Register File):**
 * - REG_CONTROL (0x00): Write-only to Processor (Softmax/Norm read-only)
 *   * Bit[0]: START signal for initiating computation
 *   * Bit[31]: MODE selection (0=SoftMax, 1=Norm)
 * - REG_STATUS (0x04): Read-only to Processor (Softmax/Norm write-only)
 *   * Bit[0]: DONE - operation completed
 *   * Bit[1]: BUSY - operation in progress
 *   * Bit[2]: ERROR - error occurred
 * - REG_ADDR_BASE_L (0x08): Write-only to Processor (Softmax/Norm read-only)
 *   * Lower 32-bits of 64-bit base memory address
 * - REG_ADDR_BASE_H (0x0C): Write-only to Processor (Softmax/Norm read-only)
 *   * Upper 32-bits of 64-bit base memory address
 * - REG_LENGTH_L (0x10): Write-only to Processor (Softmax/Norm read-only)
 *   * Lower 32-bits of 64-bit data length
 * - REG_LENGTH_H (0x14): Write-only to Processor (Softmax/Norm read-only)
 *   * Upper 32-bits of 64-bit data length
 * 
 * **Operation Flow:**
 * 1. Processor writes configuration (Control=start, Addr/Length) via writes
 * 2. Compute engine reads configuration and performs autonomous memory access via AXI Master
 * 3. Engine updates Status register with completion/busy/error flags
 * 4. Processor reads Status register to monitor progress
 * 
 * **Combinational Read Logic:**
 * Register reads are performed combinationally based on current proc_addr and proc_we.
 */
void SOLE::mmio_access_process() {
    // Extract register offset from lower 8 bits of proc_addr
    sc_uint8 reg_offset = proc_addr.read() & ADDR_OFFSET_MASK;
    
    // Handle write operations (proc_we == 1)
    if (proc_we.read() == true) {
        sc_uint32 write_data = proc_wdata.read();
        
        switch (reg_offset) {
            case REG_CONTROL:
                // Write Control register: START bit [0], MODE bit [31]
                reg_control.write(write_data);
                break;
                
            case REG_STATUS:
                // Status is read-only from Processor perspective - ignore writes
                // (Status is updated by Softmax module directly)
                break;
                
            case REG_SRC_ADDR_BASE_L:
                // Write lower 32-bits of source base address
                reg_src_addr_base_l.write(write_data);
                break;
                
            case REG_SRC_ADDR_BASE_H:
                // Write upper 32-bits of source base address
                reg_src_addr_base_h.write(write_data);
                break;
                
            case REG_DST_ADDR_BASE_L:
                // Write lower 32-bits of destination base address
                reg_dst_addr_base_l.write(write_data);
                break;
                
            case REG_DST_ADDR_BASE_H:
                // Write upper 32-bits of destination base address
                reg_dst_addr_base_h.write(write_data);
                break;
                
            case REG_LENGTH_L:
                // Write lower 32-bits of length
                reg_length_l.write(write_data);
                break;
                
            case REG_LENGTH_H:
                // Write upper 32-bits of length
                reg_length_h.write(write_data);
                break;
                
            case REG_RESERVED:
                // Reserved register - ignore write
                break;
                
            default:
                // Invalid offset - no operation
                break;
        }
    }
    
    // Handle read operations (proc_we == 0)
    // Combinational read logic - output depends on current address
    sc_uint32 read_data = 0x0;
    
    switch (reg_offset) {
        case REG_STATUS:
            // Status register - read-only (from Softmax)
            read_data = reg_status.read();
            break;
        case REG_CONTROL:
        case REG_SRC_ADDR_BASE_L:
        case REG_SRC_ADDR_BASE_H:
        case REG_DST_ADDR_BASE_L:
        case REG_DST_ADDR_BASE_H:
        case REG_LENGTH_L:
        case REG_LENGTH_H:
        case REG_RESERVED:
        default:
            // All other registers are write-only - return 0x0 on read
            read_data = 0x0;
            break;
    }
    
    // Write read data to output port
    proc_rdata.write(read_data);
}


/**
 * @brief Demux Logic (Multiplexer Control)
 * 
 * **Functional Overview:**
 * Routes compute engine control and AXI4-Lite Master signals based on mode selection.
 * Acts as the core arbitration point between SoftMax and Norm engines.
 * 
 * **Mode Selection:**
 * - Mode[31] = 0: SoftMax Engine (currently implemented)
 *   * Enables Softmax operation
 *   * Routes Softmax's AXI Master signals to SOLE's Master ports
 *   * Reports Softmax status (done, busy)
 * 
 * - Mode[31] = 1: Norm Engine (placeholder for future implementation)
 *   * Clears all Master signals
 *   * Ready for Norm engine instantiation
 * 
 * **Address & Length Handling:**
 * Constructs 64-bit values from dual 32-bit register pairs:
 * - Base Address: {REG_ADDR_BASE_H, REG_ADDR_BASE_L}
 * - Data Length: {REG_LENGTH_H, REG_LENGTH_L}
 * 
 * **AXI Master Signal Routing:**
 * When Mode=0 (SoftMax):
 * - Mirrors Softmax write address channel → M_AXI_AW*
 * - Mirrors Softmax write data channel → M_AXI_W*
 * - Mirrors Softmax read address channel → M_AXI_AR*
 * - Captures AXI responses → internal softmax_*ready signals
 * 
 * **Sensitivity:**
 * Triggers on changes to:
 * - Control register (mode/start bits)
 * - Address and length registers
 * - Softmax status signals
 * - All Softmax Master channel signals
 * - All SOLE Master response signals
 */
void SOLE::demux_logic() {
    // Extract Mode and Start signals from Control register
    sc_uint32 ctrl = reg_control.read();
    mode.write((ctrl >> CTRL_MODE_BIT) & 0x1);
    start.write((ctrl >> CTRL_START_BIT) & 0x1);
    
    // Construct 64-bit source address from high and low 32-bits
    sc_uint64 src_addr = ((sc_uint64)reg_src_addr_base_h.read() << 32) | 
                         (sc_uint64)reg_src_addr_base_l.read();
    src_addr_base.write(src_addr);
    
    // Construct 64-bit destination address from high and low 32-bits
    sc_uint64 dst_addr = ((sc_uint64)reg_dst_addr_base_h.read() << 32) | 
                         (sc_uint64)reg_dst_addr_base_l.read();
    dst_addr_base.write(dst_addr);
    
    // Construct 64-bit length from high and low 32-bits
    sc_uint64 length = ((sc_uint64)reg_length_h.read() << 32) | 
                       (sc_uint64)reg_length_l.read();
    data_length.write(length);
    
    // Route signals to appropriate engine
    if (mode.read() == 0) {
        // Route to SoftMax Engine
        softmax_enable.write(start.read());
        softmax_busy.write(start.read());
        
        // Route Softmax AXI Master signals to SOLE Master output ports
        M_AXI_AWADDR.write(softmax_awaddr.read());
        M_AXI_AWVALID.write(softmax_awvalid.read());
        softmax_awready.write(M_AXI_AWREADY.read());
        
        M_AXI_WDATA.write(softmax_wdata.read());
        M_AXI_WSTRB.write(softmax_wstrb.read());
        M_AXI_WVALID.write(softmax_wvalid.read());
        softmax_wready.write(M_AXI_WREADY.read());
        
        softmax_bresp.write(M_AXI_BRESP.read());
        softmax_bvalid.write(M_AXI_BVALID.read());
        M_AXI_BREADY.write(softmax_bready.read());
        
        M_AXI_ARADDR.write(softmax_araddr.read());
        M_AXI_ARVALID.write(softmax_arvalid.read());
        softmax_arready.write(M_AXI_ARREADY.read());
        
        softmax_rdata.write(M_AXI_RDATA.read());
        softmax_rresp.write(M_AXI_RRESP.read());
        softmax_rvalid.write(M_AXI_RVALID.read());
        M_AXI_RREADY.write(softmax_rready.read());
        
    } else {
        // Route to Norm Engine (placeholder - not yet implemented)
        // Clear Master signals when Norm is selected
        M_AXI_AWADDR.write(0);
        M_AXI_AWVALID.write(false);
        M_AXI_WDATA.write(0);
        M_AXI_WSTRB.write(0);
        M_AXI_WVALID.write(false);
        M_AXI_BREADY.write(false);
        M_AXI_ARADDR.write(0);
        M_AXI_ARVALID.write(false);
        M_AXI_RREADY.write(false);
    }
}

/**
 * @brief Status Update & Aggregation Logic
 * 
 * **Functional Overview:**
 * Monitors compute engine status signals and updates the Status register.
 * Implements control flow for operation completion and auto-clear of start bit.
 * 
 * **Status Register Bit Mapping:**
 * Reads status from selected engine (mode-dependent):
 * - Bit[0] (DONE): Set if engine reports completion
 * - Bit[1] (BUSY): Set if engine reports in-progress operation
 * - Bit[2] (ERROR): Set if engine reports error condition
 * 
 * **Engine-Specific Aggregation:**
 * 
 * **SoftMax Mode (Mode=0):**
 *   Status = {softmax_error, softmax_busy, softmax_done}
 * 
 * **Norm Mode (Mode=1):**
 *   Status = {norm_error, norm_busy, norm_done}
 *   (Placeholder - awaiting Norm engine implementation)
 * 
 * **Auto-Clear Behavior:**
 * When START bit is asserted and selected engine becomes BUSY:
 * - Automatically clears CTRL_START_BIT to prevent re-triggering
 * - Creates single-shot pulse behavior for operation trigger
 * 
 * **Sensitivity:**
 * Triggers on changes to engine status signals:
 * - softmax_busy, softmax_done, softmax_error
 * - norm_busy, norm_done, norm_error (future)
 */
void SOLE::status_update() {
    sc_uint32 status = 0x0;
    sc_uint32 ctrl = reg_control.read();
    
    // Aggregate status based on Mode selection
    if (mode.read() == 0) {
        // SoftMax engine status
        if (softmax_done.read()) {
            status |= (1 << STAT_DONE_BIT);
        }
        if (softmax_busy.read()) {
            status |= (1 << STAT_BUSY_BIT);
        }
    } else {
        // Norm engine status (placeholder)
        if (norm_done.read()) {
            status |= (1 << STAT_DONE_BIT);
        }
        if (norm_busy.read()) {
            status |= (1 << STAT_BUSY_BIT);
        }
    }
    
    // Update Status register
    reg_status.write(status);
    
    // Clear Start bit after operation begins (pulse behavior)
    if (start.read() && (softmax_busy.read() || norm_busy.read())) {
        sc_uint32 new_ctrl = ctrl & ~(1 << CTRL_START_BIT);
        reg_control.write(new_ctrl);
    }
}
