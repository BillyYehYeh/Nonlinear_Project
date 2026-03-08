# AXI4-Lite Master Ports Configuration

## Overview
The Softmax module now includes AXI4-Lite Master ports for system-level integration with AXI4-Lite Slaves (e.g., memory controllers, registers).

## Configuration Parameters
```cpp
#define AXI_ADDR_WIDTH  32      // 32-bit addressing
#define AXI_DATA_WIDTH  32      // 32-bit data bus
#define AXI_STRB_WIDTH  4       // 4 byte enable signals
```

## Port List

### System Ports
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `clk` | input | 1 | System clock |
| `rst` | input | 1 | Active-high reset |

### Write Address Channel (M_AXI_AW*)
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `M_AXI_AWADDR` | output | 32 | Write address |
| `M_AXI_AWPROT` | output | 3 | Write protection type (0=secure/non-priv/data) |
| `M_AXI_AWVALID` | output | 1 | Write address valid |
| `M_AXI_AWREADY` | input | 1 | Write address ready (slave) |

### Write Data Channel (M_AXI_W*)
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `M_AXI_WDATA` | output | 32 | Write data |
| `M_AXI_WSTRB` | output | 4 | Write strobes (byte enables) |
| `M_AXI_WVALID` | output | 1 | Write data valid |
| `M_AXI_WREADY` | input | 1 | Write data ready (slave) |

### Write Response Channel (M_AXI_B*)
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `M_AXI_BRESP` | input | 2 | Write response (00=OK, 10=SLVERR, 11=DECERR) |
| `M_AXI_BVALID` | input | 1 | Write response valid (slave) |
| `M_AXI_BREADY` | output | 1 | Write response ready |

### Read Address Channel (M_AXI_AR*)
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `M_AXI_ARADDR` | output | 32 | Read address |
| `M_AXI_ARPROT` | output | 3 | Read protection type (0=secure/non-priv/data) |
| `M_AXI_ARVALID` | output | 1 | Read address valid |
| `M_AXI_ARREADY` | input | 1 | Read address ready (slave) |

### Read Data Channel (M_AXI_R*)
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `M_AXI_RDATA` | input | 32 | Read data (slave) |
| `M_AXI_RRESP` | input | 2 | Read response (00=OK, 10=SLVERR, 11=DECERR) |
| `M_AXI_RVALID` | input | 1 | Read data valid (slave) |
| `M_AXI_RREADY` | output | 1 | Read data ready |

## Internal Signals (Private)

All external ports are connected to internal control signals with `_sig` suffix:
```cpp
sc_signal<sc_dt::sc_uint<AXI_ADDR_WIDTH>>  axi_awaddr_sig;
sc_signal<sc_uint3>                        axi_awprot_sig;
sc_signal<bool>                            axi_awvalid_sig;
... (and so on for all channels)
```

## Write Transaction Example

```
Cycle 0: AWADDR=0x1000, AWVALID=1, WDATA=0xDEADBEEF, WSTRB=0xF, WVALID=1
Cycle 1: AWREADY=1, WREADY=1 (slave accepts both)
Cycle 2: AWVALID=0, WVALID=0 (master deasserts)
Cycle 3: BRESP=00, BVALID=1 (slave responds OK)
Cycle 4: BREADY=1 (master acknowledges response)
```

## Read Transaction Example

```
Cycle 0: ARADDR=0x2000, ARVALID=1
Cycle 1: ARREADY=1 (slave accepts address)
Cycle 2: ARVALID=0 (master deasserts)
Cycle 3: RDATA=0xCAFEBABE, RRESP=00, RVALID=1
Cycle 4: RREADY=1 (master acknowledges data)
```

## Default Values Initialized

```cpp
axi_awaddr_sig = 0x00000000
axi_awprot_sig = 0x0        // Secure, non-privileged, data access
axi_wdata_sig  = 0x00000000
axi_wstrb_sig  = 0xF        // All bytes enabled
axi_*valid_sig = false      // No valid transactions by default
axi_*ready_sig = false      // No ready until configured
```

## Usage Notes

1. **Master Responsibility:**
   - Drive `AWVALID`, `WVALID` for write transactions
   - Drive `ARVALID` for read transactions
   - Monitor `*READY` signals from slave and respect handshake protocol
   - Handle `BRESP` and `RRESP` for transaction status

2. **Handshake Rules:**
   - `VALID` signal can only be deasserted after `READY` is sampled high
   - `READY` can be asserted independent of `VALID`
   - Both can be asserted simultaneously (same cycle transfer)

3. **Byte Enables (`WSTRB`):**
   - Bit[0] = Byte 0 enable (bits 7:0)
   - Bit[1] = Byte 1 enable (bits 15:8)
   - Bit[2] = Byte 2 enable (bits 23:16)
   - Bit[3] = Byte 3 enable (bits 31:24)

## Integration Steps

1. Connect the module ports to your AXI4-Lite infrastructure slave
2. Manage the internal control signals (`axi_*_sig`) via state machine or controller
3. Implement read/write transaction logic as needed
4. Respect AXI4-Lite protocol timing and handshake rules

---
**Status:** ✓ All AXI4-Lite Master ports configured and wired
