#include "Output_Buffer.h"

/**
 * @brief Combinational Read Operation
 * 
 * Reads data from buffer at the specified index and outputs it combinationally.
 */
void Output_Buffer::read_data() {
    // Combinational read from buffer at index address
    sc_uint10 addr = index.read();
    Data_Out.write(buffer[addr]);
}

/**
 * @brief Synchronous Write Operation
 * 
 * On each positive clock edge:
 * - If reset=1: Clear all buffer entries to 0
 * - Else if write=1: Store Data_In at buffer[index]
 * - Else: No operation
 */
void Output_Buffer::write_on_clock() {
    if (reset.read() == 1) {
        // Clear entire buffer
        for (int i = 0; i < 1024; i++) {
            buffer[i] = 0;
        }
    } else if (write.read() == 1) {
        sc_uint10 addr = index.read();
        sc_uint16 data = Data_In.read();
        buffer[addr] = data;
    }
}
