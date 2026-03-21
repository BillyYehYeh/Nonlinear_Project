#include "Max_FIFO.h"
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <string>

/**
 * @brief Update full and empty flags based on pointers
 * 
 * Empty: read_addr == write_addr
 * Full: (write_addr + 1) % 1024 == read_addr
 */
void Max_FIFO::update_flags() {
    sc_uint10 w_ptr = write_addr_sig.read();
    sc_uint10 r_ptr = read_addr_sig.read();
    
    // FIFO is empty when pointers are equal
    empty.write(w_ptr == r_ptr);
    
    // FIFO is full when next write would overwrite read pointer
    bool is_full = (((w_ptr + 1) & 0x3FF) == r_ptr);
    full.write(is_full);
    
    // Occupancy: difference between write and read pointers (modulo 1024)
    sc_uint10 occupancy = w_ptr - r_ptr;
    count.write(occupancy);
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
void Max_FIFO::pointer_update() {
    if (rst.read()) {
        // Reset both pointers to 0
        write_addr_sig.write(0);
        read_addr_sig.write(0);
    } else if (clear.read()) {
        // Clear FIFO: set read pointer equal to write pointer
        read_addr_sig.write(write_addr_sig.read());
    } else {
        // Normal operation
        // Only advance pointers when not full/empty respectively
        if (write_en.read() && !full.read()) {
            sc_uint10 w_ptr = write_addr_sig.read();
            write_addr_sig.write((w_ptr + 1) & 0x3FF);  // Modulo 1024           
        }

        if (read_en.read() && !empty.read()) {
            sc_uint10 r_ptr = read_addr_sig.read();
            read_addr_sig.write((r_ptr + 1) & 0x3FF);   // Modulo 1024
        }
    }
}

void Max_FIFO::gate_signals() {
    // Combinationally gate the external enables so SRAM never receives
    // a write when FIFO is full or a read when FIFO is empty.
    bool we = write_en.read() && !full.read();
    bool re = read_en.read() && !empty.read();
    we_sig.write(we);
    re_sig.write(re);
}

/**
 * @brief Output read data from SRAM
 * 
 * Propagates SRAM read data to output port.
 * Note: Data has 1-cycle latency due to SRAM pipeline register.
 */
void Max_FIFO::read_output() {
    data_out.write(sram_rdata_sig.read());
}
/*
void Max_FIFO::Print_Push_Count() {
    std::cerr << "\033[32m" << "[MAX_FIFO] Push Count: " << count.read()
              << " Incoming Push Data: " << "0x" << std::hex << std::setw(4) << std::setfill('0')
              << static_cast<uint16_t>(data_in.read()) << std::dec << std::setfill(' ') << " write_en: " << write_en.read() 
              << "\033[0m" << "  @ " << sc_time_stamp() << std::endl;
}
*/
/*void Max_FIFO::Print_FIFO_SRAM() {
    sc_uint10 w_ptr = write_addr_sig.read();
    sc_uint10 r_ptr = read_addr_sig.read();
    std::cerr << sc_time_stamp() << " " << "\033[35m" << "[MAX_FIFO] SRAM State (push order): " << std::endl;

    // Compute occupancy (number of stored elements)
    sc_uint10 occ = w_ptr - r_ptr;
    bool anyPrinted = false;
    if (occ != 0) {
        // Print entries from read pointer up to (but not including) write pointer
        // Group output: five printed data entries per line.
        unsigned printed_in_line = 0;
        const std::size_t column_width = 26; // fixed width per entry including markers
        for (unsigned k = 0; k < static_cast<unsigned>(occ); ++k) {
            unsigned idx = (static_cast<unsigned>(r_ptr.to_uint()) + k) & 0x3FFu;
            sc_uint16 data = sram->debug_peek(static_cast<int>(idx));

            anyPrinted = true;

            // Build the entry string, append markers, then pad to fixed width
            std::ostringstream oss;
            oss << "  [" << std::setw(3) << idx << "] 0x"
                << std::hex << std::setw(4) << std::setfill('0') << static_cast<uint16_t>(data)
                << std::dec << std::setfill(' ');

            // Markers
            if (idx == r_ptr.to_uint()) oss << " <-R";
            if (idx == ((w_ptr.to_uint() + 1023) & 0x3FF)) oss << " <-last";

            std::string entry = oss.str();
            if (entry.size() < column_width) entry.append(column_width - entry.size(), ' ');

            std::cerr << entry;

            printed_in_line++;
            if (printed_in_line >= 5) {
                std::cerr << std::endl;
                printed_in_line = 0;
            }
        }

        // If the last line wasn't terminated (fewer than 5 items), end it now
        if (printed_in_line != 0) std::cerr << std::endl;
    }

    if (!anyPrinted) {
        std::cerr << "  <no valid data>" << std::endl;
    }
    std::cerr << "data_out=0x" << std::hex << std::setw(4) << std::setfill('0') << static_cast<uint16_t>(data_out.read()) 
              << " count=" << std::dec << count.read() << std::endl;
    std::cerr << "\033[0m";
}*/