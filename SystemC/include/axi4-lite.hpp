#pragma once

#include <systemc>
#include <cstdint>

namespace hybridacc {
namespace axi4lite {

constexpr uint32_t AXI_RESP_OKAY = 0x0;
constexpr uint32_t AXI_RESP_EXOKAY = 0x1;
constexpr uint32_t AXI_RESP_SLVERR = 0x2;
constexpr uint32_t AXI_RESP_DECERR = 0x3;

constexpr unsigned AXI4L_DEFAULT_ADDR_WIDTH = 32;
constexpr unsigned AXI4L_DEFAULT_DATA_WIDTH = 64;
constexpr unsigned AXI4L_DEFAULT_STRB_WIDTH = AXI4L_DEFAULT_DATA_WIDTH / 8;

constexpr uint32_t AXI4L_REQ_QUEUE_DEPTH = 16;
constexpr uint32_t AXI4L_RESP_QUEUE_DEPTH = 16;

} // namespace axi4lite
} // namespace hybridacc