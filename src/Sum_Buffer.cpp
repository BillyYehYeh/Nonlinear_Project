#include "Sum_Buffer.h"

/**
 * @brief Combinational Read Operation
 * 
 * Reads data from buffer and outputs it combinationally whenever inputs change.
 */
void Sum_Buffer::read_data() {
    // Output the buffer data
    Data_Out.write(buffer);
}

/**
 * @brief Synchronous Write Operation
 * 
 * On each positive clock edge:
 * - If reset=1: Clear buffer to 0
 * - Else if write=1: Store Data_In into buffer
 * - Else: No operation
 */
void Sum_Buffer::write_on_clock() {
    if (reset.read() == 1) {
        // Clear buffer
        buffer = 0;
    } else if (write.read() == 1) {
        // Write data to buffer
        sc_uint32 data = Data_In.read();
        buffer = data;
    }
}
