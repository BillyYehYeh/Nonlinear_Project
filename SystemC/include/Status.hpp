#ifndef STATUS_HPP
#define STATUS_HPP

#include <cstdint>

/**
 * @namespace softmax::status
 * @brief Status and error code definitions for Softmax module
 * 
 * Defines the Status register format and error codes for runtime monitoring
 */
namespace softmax::status {

    /**
     * @enum State_t
     * @brief State machine values
     */
    enum State_t : uint8_t {
        STATE_IDLE     = 0,   ///< Idle state - waiting for START signal
        STATE_PROCESS1 = 1,   ///< State 1 - PROCESS_1 is running (reading from memory)
        STATE_PROCESS2 = 2,   ///< State 2 - PROCESS_2 is running (computation)
        STATE_PROCESS3 = 3    ///< State 3 - PROCESS_3 is running (writing to memory)
    };

    /**
     * @enum ErrorCode_t
     * @brief Error codes indicating failure reasons
     */
    enum ErrorCode_t : uint8_t {
        ERR_NONE                        = 0x0,   ///< No error
        ERR_AXI_READ_ERROR              = 0x1,   ///< AXI4-Lite read response error
        ERR_AXI_READ_TIMEOUT            = 0x2,   ///< AXI4-Lite read timeout
        ERR_AXI_READ_DATA_MISSING       = 0x3,   ///< AXI4-Lite read data missing
        ERR_AXI_WRITE_ERROR             = 0x4,   ///< AXI4-Lite write response error
        ERR_AXI_WRITE_TIMEOUT           = 0x5,   ///< AXI4-Lite write timeout
        ERR_AXI_WRITE_RESPONSE_MISMATCH = 0x6,   ///< AXI4-Lite write response mismatch
        ERR_MAX_FIFO_OVERFLOW           = 0x7,   ///< Max FIFO overflow
        ERR_OUTPUT_FIFO_OVERFLOW        = 0x8,   ///< Output FIFO overflow
        ERR_DATA_LENGTH_INVALID         = 0x9,   ///< Data length is zero or invalid
        ERR_INVALID_STATE               = 0xA    ///< Invalid state transition
    };

} // namespace softmax::status

#endif // STATUS_HPP
