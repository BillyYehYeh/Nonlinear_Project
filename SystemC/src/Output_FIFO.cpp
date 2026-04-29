#include "Output_FIFO.h"
#include <iomanip>
#include <cstdint>
#include <sstream>
#include <string>

static constexpr unsigned FIFO_ADDR_MASK = (1u << OUTPUT_FIFO_ADDR_BITS) - 1u;

/**
 * @brief Update full and empty flags based on pointers
 * 
 * Empty: read_addr == write_addr
 * Full: (write_addr + 1) % 1024 == read_addr
 */
void Output_FIFO::update_flags() {
    output_fifo_addr_t w_ptr = write_addr_sig.read();
    output_fifo_addr_t r_ptr = read_addr_sig.read();
    
    // FIFO is empty when pointers are equal
    empty.write(w_ptr == r_ptr);
    
    // FIFO is full when next write would overwrite read pointer
    bool is_full = (((w_ptr + 1) & FIFO_ADDR_MASK) == r_ptr);
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
        // Only advance pointers when not full/empty respectively
        if (we_sig.read()) {
            output_fifo_addr_t w_ptr = write_addr_sig.read();
            write_addr_sig.write((w_ptr + 1) & FIFO_ADDR_MASK);  // Modulo FIFO depth
        }

        if (re_sig.read()) {
            output_fifo_addr_t r_ptr = read_addr_sig.read();
            read_addr_sig.write((r_ptr + 1) & FIFO_ADDR_MASK);   // Modulo FIFO depth
        }
    }
}

void Output_FIFO::count_update() {
    if (rst.read() || clear.read()) {
        count.write(0);
        return;
    }

    output_fifo_addr_t count_next = count.read();
    const bool accepted_write = we_sig.read();
    const bool accepted_read_handshake = read_ready.read() && read_valid.read();

    if (accepted_write && !accepted_read_handshake) {
        count_next = count_next + 1;
    } else if (!accepted_write && accepted_read_handshake) {
        count_next = count_next - 1;
    }

    count.write(count_next);
}

/**
 * @brief Output read data from SRAM
 * 
 * Propagates SRAM read data to output port.
 * Note: Data has 1-cycle latency due to SRAM pipeline register.
 */
void Output_FIFO::read_output() {
    if (skid_valid_sig.read()) {
        data_out.write(skid_reg_sig.read());
    } else {
        data_out.write(sram_rdata_sig.read());
    }
}

void Output_FIFO::gate_signals() {
    // Keep write protection against full and continuously prefetch reads
    // while there is buffered capacity (skid + in-flight return < 2).
    // This keeps output data ready even when read_ready toggles.
    unsigned buffered = (skid_valid_sig.read() ? 1u : 0u)
                      + (sram_output_data_valid.read() ? 1u : 0u);

    bool we = write_en.read() && !full.read();
    bool re = false;
    if (!empty.read()) {
        if (read_ready.read()) {
            re = (buffered < 2u);
        } else {
            // When downstream is not ready, keep only one unconsumed head item
            // in the output path to avoid overwriting skid data.
            re = !skid_valid_sig.read() && !sram_output_data_valid.read();
        }
    }
    we_sig.write(we);
    re_sig.write(re);
}

void Output_FIFO::output_pipeline_update() {
    if (rst.read() || clear.read()) {
        skid_reg_sig.write(0);
        skid_valid_sig.write(false);
        sram_output_data_valid.write(false);
        return;
    }

    sc_uint16 skid_reg_next = skid_reg_sig.read();
    bool skid_valid_next = skid_valid_sig.read();

    const bool consume_skid = read_ready.read() && skid_valid_next;
    if (consume_skid) {
        skid_valid_next = false;
    }

    const bool new_data_valid = sram_output_data_valid.read();
    if (new_data_valid) {
        const sc_uint16 new_data = sram_rdata_sig.read();
        // Data can bypass directly only when there is no skid data and the
        // downstream consumes this cycle; otherwise retain it in skid.
        const bool bypass_consumed = read_ready.read() && !skid_valid_sig.read();
        if (!bypass_consumed) {
            skid_valid_next = true;
            skid_reg_next = new_data;
        }
    }

    skid_reg_sig.write(skid_reg_next);
    skid_valid_sig.write(skid_valid_next);
    sram_output_data_valid.write(re_sig.read());
}

void Output_FIFO::read_data_valid_flag() {
    read_valid.write(sram_output_data_valid.read() || skid_valid_sig.read());
}
/*
void Output_FIFO::Print_FIFO_SRAM() {
    sc_uint10 w_ptr = write_addr_sig.read();
    sc_uint10 r_ptr = read_addr_sig.read();
    std::cerr << sc_time_stamp() << " " << "\033[35m" << "[OUTPUT_FIFO] SRAM State (push order): " << std::endl;

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

            // Print all entries including zeros
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
              << " count=" << std::dec << count.read()
              << " skid_valid=" << skid_valid_sig.read()
              << " skid_reg=0x" << std::hex << std::setw(4) << std::setfill('0') << static_cast<uint16_t>(skid_reg_sig.read())
              << std::dec << std::setfill(' ') << std::endl;
    std::cerr << "\033[0m";
}*/
