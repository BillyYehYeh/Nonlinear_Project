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
        ERR_NONE               = 0x0,   ///< No error
        ERR_AXI_READ_ERROR     = 0x1,   ///< AXI4-Lite read response error
        ERR_AXI_WRITE_ERROR    = 0x2,   ///< AXI4-Lite write response error
        ERR_MAX_FIFO_FULL      = 0x3,   ///< Max FIFO overflow (PROCESS_1 stalled)
        ERR_OUTPUT_FIFO_FULL   = 0x4,   ///< Output FIFO overflow (PROCESS_3 stalled)
        ERR_DATA_LENGTH_ZERO   = 0x5,   ///< Data length is zero or invalid
        ERR_ADDR_MISMATCH      = 0x6,   ///< Address alignment or range error
        ERR_TIMEOUT            = 0x7,   ///< Operation timeout
        ERR_INVALID_STATE      = 0xF    ///< Invalid state transition
    };

    /**
     * @struct StatusReg_t
     * @brief Status Register Format (32-bit)
     * 
     * Bit layout:
     *  [31:8]     Reserved
     *  [7:4]      error_code (4-bit error code)
     *  [3]        error (1-bit error flag)
     *  [2:1]      state (2-bit state value)
     *  [0]        RESERVED
     */
    struct StatusReg_t {
        uint32_t value;

        /**
         * @brief Extract state from status register
         * @return Current state (0-3)
         */
        inline uint8_t get_state() const {
            return (value >> 1) & 0x3;
        }

        /**
         * @brief Set state in status register
         * @param state New state value (0-3)
         */
        inline void set_state(uint8_t state) {
            value = (value & 0xFFFFFFF9) | ((state & 0x3) << 1);
        }

        /**
         * @brief Get error flag
         * @return 1 if error occurred, 0 otherwise
         */
        inline bool get_error() const {
            return (value >> 3) & 0x1;
        }

        /**
         * @brief Set error flag
         * @param err 1 to set error, 0 to clear
         */
        inline void set_error(bool err) {
            if (err) {
                value |= (1 << 3);
            } else {
                value &= ~(1 << 3);
            }
        }

        /**
         * @brief Get error code
         * @return 4-bit error code
         */
        inline uint8_t get_error_code() const {
            return (value >> 4) & 0xF;
        }

        /**
         * @brief Set error code
         * @param code 4-bit error code value
         */
        inline void set_error_code(uint8_t code) {
            value = (value & 0xFFFFFF0F) | ((code & 0xF) << 4);
        }

        /**
         * @brief Check if operation is in progress
         * @return true if state is not idle
         */
        inline bool is_busy() const {
            return get_state() != STATE_IDLE;
        }

        /**
         * @brief Check if operation is complete
         * @return true if state is idle and no error
         */
        inline bool is_done() const {
            return get_state() == STATE_IDLE && !get_error();
        }

        /**
         * @brief Clear status to idle state with no error
         */
        inline void clear() {
            value = 0;
        }

        /**
         * @brief Create status with given state and error code
         * @param state State value (0-3)
         * @param error_code Error code (0-15), 0 means no error
         * @return Status register value
         */
        static inline uint32_t make_status(uint8_t state, uint8_t error_code) {
            uint32_t result = 0;
            result |= ((state & 0x3) << 1);
            if (error_code != ERR_NONE) {
                result |= (1 << 3);  // Set error flag
            }
            result |= ((error_code & 0xF) << 4);
            return result;
        }
    };

    // Helper constants
    constexpr uint32_t STATUS_STATE_MASK      = 0x6;      ///< Mask for state bits [2:1]
    constexpr uint32_t STATUS_ERROR_FLAG_MASK = 0x8;      ///< Mask for error flag [3]
    constexpr uint32_t STATUS_ERROR_CODE_MASK = 0xF0;     ///< Mask for error code bits [7:4]

} // namespace softmax::status

#endif // STATUS_HPP
