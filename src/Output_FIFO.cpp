#include "Output_FIFO.h"

/**
 * @brief Update full and empty flags based on pointers
 * 
 * Empty: read_addr == write_addr
 * Full: (write_addr + 1) % 1024 == read_addr
 */
void Output_FIFO::update_flags() {
    sc_uint10 w_ptr = write_addr_sig.read();
    sc_uint10 r_ptr = read_addr_sig.read();
    
    // FIFO is empty when pointers are equal
    empty.write(w_ptr == r_ptr);
    
    // FIFO is full when next write would overwrite read pointer
    bool is_full = (((w_ptr + 1) & 0x3FF) == r_ptr);
    full.write(is_full);
}

/**
 * @brief Update read and write pointers on clock edge
 * 
 * On each positive clock edge:
 * - If rst=1: Reset both pointers to 0
 * - If clear=1: Set read_addr = write_addr (flush FIFO)
 * - Otherwise:
 *   - If write_en=1: Increment write_addr (modulo 1024)
 *   - If read_en=1: Increment read_addr (modulo 1024)
 */
void Output_FIFO::pointer_update() {
    if (rst.read()) {
        // Reset both pointers to 0
        write_addr_sig.write(0);
        read_addr_sig.write(0);
    } else if (clear.read()) {
        // Clear FIFO: set read pointer equal to write pointer
        read_addr_sig.write(write_addr_sig.read());
    } else {
        // Normal operation
        if (write_en.read()) {
            sc_uint10 w_ptr = write_addr_sig.read();
            write_addr_sig.write((w_ptr + 1) & 0x3FF);  // Modulo 1024
        }
        
        if (read_en.read()) {
            sc_uint10 r_ptr = read_addr_sig.read();
            read_addr_sig.write((r_ptr + 1) & 0x3FF);   // Modulo 1024
        }
    }
}

/**
 * @brief Output read data from SRAM
 * 
 * Propagates SRAM read data to output port.
 * Note: Data has 1-cycle latency due to SRAM pipeline register.
 */
void Output_FIFO::read_output() {
    data_out.write(sram_rdata_sig.read());
}
