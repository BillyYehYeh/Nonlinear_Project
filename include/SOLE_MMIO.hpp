#ifndef SOLE_MMIO_HPP
#define SOLE_MMIO_HPP

#include <cstdint>

/**
 * @namespace sole::mmio
 * @brief SOLE MMIO Register definitions and control bit fields
 * 
 * This namespace contains all MMIO register map definitions for the SOLE
 * (Softmax/Normalization Linear Engine) accelerator module.
 */
namespace sole {
namespace mmio {

// ===== MMIO Register Map =====
// All registers are 32-bit aligned at the specified offsets
// Address filtering uses 8-bit LSB (Addr[7:0]) for register offset
// 
// **Processor (Host) Access Permissions:**
// - Control, Src_Addr_Base, Dst_Addr_Base, Length: Write-only for Processor
// - Status: Read-only for Processor
//
// **Compute Engine (Softmax/Norm) Access Permissions:**
// - Control, Src_Addr_Base, Dst_Addr_Base, Length: Read-only for Engine
// - Status: Write-only for Engine

/**
 * @brief Control Register Offset
 * Access: Write-only from Processor, Read-only from Compute Engine
 * Width: 32 bits
 * Description: Controls operation start and mode selection
 */
constexpr uint32_t REG_CONTROL        = 0x00;

/**
 * @brief Status Register Offset
 * Access: Read-only from Processor, Write-only from Compute Engine
 * Width: 32 bits
 * Description: Reports operation status (state, error, error_code)
 */
constexpr uint32_t REG_STATUS         = 0x04;

/**
 * @brief Source Address Base Low Offset
 * Access: Write-only from Processor, Read-only from Compute Engine
 * Width: 32 bits
 * Description: Lower 32-bits of 64-bit source address for reading input data
 */
constexpr uint32_t REG_SRC_ADDR_BASE_L = 0x08;

/**
 * @brief Source Address Base High Offset
 * Access: Write-only from Processor, Read-only from Compute Engine
 * Width: 32 bits
 * Description: Upper 32-bits of 64-bit source address (supports full 64-bit addressing)
 */
constexpr uint32_t REG_SRC_ADDR_BASE_H = 0x0C;

/**
 * @brief Destination Address Base Low Offset
 * Access: Write-only from Processor, Read-only from Compute Engine
 * Width: 32 bits
 * Description: Lower 32-bits of 64-bit destination address for writing output data
 */
constexpr uint32_t REG_DST_ADDR_BASE_L = 0x10;

/**
 * @brief Destination Address Base High Offset
 * Access: Write-only from Processor, Read-only from Compute Engine
 * Width: 32 bits
 * Description: Upper 32-bits of 64-bit destination address
 */
constexpr uint32_t REG_DST_ADDR_BASE_H = 0x14;

/**
 * @brief Data Length Low Offset
 * Access: Write-only from Processor, Read-only from Compute Engine
 * Width: 32 bits
 * Description: Lower 32-bits of 64-bit data length (number of FP16 elements to process)
 */
constexpr uint32_t REG_LENGTH_L       = 0x18;

/**
 * @brief Data Length High Offset
 * Access: Write-only from Processor, Read-only from Compute Engine
 * Width: 32 bits
 * Description: Upper 32-bits of 64-bit data length
 */
constexpr uint32_t REG_LENGTH_H       = 0x1C;

/**
 * @brief Reserved Register Offset
 * Access: None (reserved)
 * Width: 32 bits
 * Description: Reserved for future use
 */
constexpr uint32_t REG_RESERVED       = 0x20;

// ===== Control Register Bit Fields =====
// Format: Register offset + bit position

/**
 * @brief START Signal Bit Position
 * Bit Position: [0]
 * Description: Write 1 to start/enable operation (pulse behavior)
 * Auto-clears after operation begins
 */
constexpr uint32_t CTRL_START_BIT  = 0;

/**
 * @brief MODE Selection Bit Position
 * Bit Position: [31]
 * Description: Selects compute engine mode
 *   0 = SoftMax operation
 *   1 = Normalization operation
 */
constexpr uint32_t CTRL_MODE_BIT   = 31;

// ===== Status Register Bit Fields =====
// Format: Register offset + bit position

/**
 * @brief DONE Flag Bit Position
 * Bit Position: [0]
 * Description: Indicates operation completion
 *   0 = Operation in progress or not started
 *   1 = Operation completed
 */
constexpr uint32_t STAT_DONE_BIT   = 0;

/**
 * @brief BUSY Flag Bit Position
 * Bit Position: [1]
 * Description: Indicates operation in progress
 *   0 = Idle/ready for new operation
 *   1 = Operation in progress
 */
constexpr uint32_t STAT_BUSY_BIT   = 1;

/**
 * @brief ERROR Flag Bit Position
 * Bit Position: [2]
 * Description: Indicates error condition
 *   0 = No error
 *   1 = Error occurred during operation
 */
constexpr uint32_t STAT_ERROR_BIT  = 2;

// ===== Address Offset Mask =====
// Used to extract register offset from full addresses

/**
 * @brief MMIO Address Offset Mask
 * Mask: 0xFF (8-bit)
 * Description: Extracts lower 8 bits of address as register offset
 * This allows each SOLE instance to occupy a 256-byte address space
 */
constexpr uint32_t ADDR_OFFSET_MASK = 0xFF;

} // namespace mmio
} // namespace sole

#endif // SOLE_MMIO_HPP
