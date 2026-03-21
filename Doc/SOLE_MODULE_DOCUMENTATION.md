# SOLE (Softmax/Normalization Linear Engine) Module Documentation

## Overview

**SOLE** is a hardware accelerator module that supports SoftMax and Normalization operations. It provides:
- **Simplified Processor Interface** (4-signal MMIO) for configuration and status monitoring
  - Processor writes configuration: Control, Address, Length registers (write-only from Processor perspective)
  - Processor reads status (read-only from Processor perspective)
- **MMIO Register File** for configuration and status management with dual-access semantics
- **Demux Logic** to route operations to specialized compute engines
- **SoftMax Engine** fully integrated with AXI4-Lite Master for memory access
- **Norm Engine** (preliminary placeholder for future implementation)
- **AXI4-Lite Master Interface** for autonomous memory access by compute engines

## Module Architecture

```
┌─────────────────────────────────────────────────────────────┐
│              Host Processor (MMIO Master)                   │
│     [proc_addr, proc_wdata, proc_we, proc_rdata]           │
└────────────────┬─────────────────────────────────────────────┘
                 │ (4-Signal MMIO Interface)
                 ▼
┌─────────────────────────────────────────────────────────────┐
│                     SOLE Module                              │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────────┐         ┌──────────────────┐          │
│  │ MMIO Access      │         │   MMIO RegFile   │          │
│  │ Handler          │◄────────┤  (Dual-Access)   │          │
│  │ (Proc Interface) │         │  (32-bit x 7)    │          │
│  └──────────────────┘         └────────┬─────────┘          │
│           △                            │                    │
│           │                            ▼                    │
│           │                 ┌──────────────────┐            │
│           │                 │   Demux Logic    │            │
│           │                 │  (Mode Select)   │            │
│           │                 │  (Signal Route)  │            │
│           │                 └────┬────────┬────┘            │
│           │                      │        │                 │
│           │       ┌──────────────┘        └──────┐          │
│           │       │                              │          │
│    Proc Can       │                              │          │
│    Manipulate     │                              │          │
│    Config Only    │                              │          │
│           │   ┌───▼──────┐          ┌────────────▼───┐     │
│           │   │ SoftMax   │          │  Norm Engine   │     │
│           │   │  Engine   │          │  (Placeholder) │     │
│           │   │  (Master) │          │                │     │
│           │   └─────┬─────┘          └────────────────┘     │
│           │         │ Reads Config                          │
│           │         │ Writes Status                         │
│           │         │ AXI Master Access                     │
│           │         │                                      │
│           │    ┌────▼─────────┐                            │
│           │    │ AXI4-Lite    │                            │
│           │    │ Master Port  │                            │
│           │    │(AR/AW/R/W/B) │                            │
│           │    └────┬─────────┘                            │
│           │         │                                      │
└───────────┼─────────┼──────────────────────────────────────┘
            │         │
            │  (4-Signal)     (AXI4-Lite Master)
            │         │
   Processor│         ▼
   Reads   │    ┌──────────────────┐
   Status   │    │  System Memory   │
            │    │    / SRAM        │
            └──→ │(AXI Master Slave)│
                 └──────────────────┘
```
                  │   (Slave)        │
                  └──────────────────┘
```

## External Interfaces

### System Signals
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `clk` | Input | 1 | System clock |
| `rst` | Input | 1 | Asynchronous reset |

### Processor MMIO Interface (Simplified 4-Signal)

Processor communicates with SOLE using a simple 4-signal interface for MMIO register access:

| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `proc_addr` | Input | 32 | MMIO register address (Addr[7:0] = register offset) |
| `proc_wdata` | Input | 32 | Data to write (used when proc_we=1) |
| `proc_we` | Input | 1 | Write enable (1=write operation, 0=read operation) |
| `proc_rdata` | Output | 32 | Read data output (combinational, valid when proc_we=0) |

**Processor Interface Characteristics:**
- **Reads** (proc_we=0): Combinational access to Status and configuration registers
  - Status register provides real-time engine status
  - Control/Address/Length registers return current values (for debugging)
- **Writes** (proc_we=1): Sequential updates to Control, Address, Length registers
  - Updates occur on clock edges
  - Status register is read-only (writes ignored)
- **Address Decoding**: Lower 8 bits (proc_addr[7:0]) specify register offset
  - Valid offsets: 0x00-0x18 (7 registers in 256-byte space)
  - Out-of-range addresses: Return 0x0 on read, ignored on write

### Master Interface Ports (AXI4-Lite M_AXI)

SOLE exposes an AXI4-Lite Master interface for memory access operations controlled by the embedded Softmax engine.

#### Write Address Channel (M_AXI_AW*)
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `M_AXI_AWADDR` | Output | 32 | Write address to system memory |
| `M_AXI_AWVALID` | Output | 1 | Write address valid |
| `M_AXI_AWREADY` | Input | 1 | Write address ready |

#### Write Data Channel (M_AXI_W*)
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `M_AXI_WDATA` | Output | 64 | Write data (64-bit wide) |
| `M_AXI_WSTRB` | Output | 8 | Write strobes (byte enables, 8 bytes) |
| `M_AXI_WVALID` | Output | 1 | Write data valid |
| `M_AXI_WREADY` | Input | 1 | Write data ready |

#### Write Response Channel (M_AXI_B*)
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `M_AXI_BRESP` | Input | 2 | Write response (0=OK, 3=DECERR) |
| `M_AXI_BVALID` | Input | 1 | Write response valid |
| `M_AXI_BREADY` | Output | 1 | Write response ready |

#### Read Address Channel (M_AXI_AR*)
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `M_AXI_ARADDR` | Output | 32 | Read address from system memory |
| `M_AXI_ARVALID` | Output | 1 | Read address valid |
| `M_AXI_ARREADY` | Input | 1 | Read address ready |

#### Read Data Channel (M_AXI_R*)
| Signal | Direction | Width | Description |
|--------|-----------|-------|-------------|
| `M_AXI_RDATA` | Input | 64 | Read data (64-bit wide) |
| `M_AXI_RRESP` | Input | 2 | Read response (0=OK, 3=DECERR) |
| `M_AXI_RVALID` | Input | 1 | Read data valid |
| `M_AXI_RREADY` | Output | 1 | Read data ready |

**Master Interface Notes:**
- Master interface is internally driven by the Softmax engine via Demux logic
- Write data width is 64-bit per axi4-lite.hpp specification
- Master address range: Full 32-bit (0x00000000 - 0xFFFFFFFF)
- Controlled via Mode bit in Control Register (Bit[31]=0 for Softmax mode)
- When Mode=1 (Normalization), Master outputs are disabled (all valid signals low)

## MMIO Register Map

All register accesses use the lower 8 bits of address as offset (proc_addr[7:0]).

### Register Definitions with Dual-Access Semantics

| Offset | Name | From Processor | From Compute Engine | Width | Description |
|--------|------|---|---|-------|-------------|
| 0x00 | Control | **Write-only** | Read-only | 32 | Control register - Start & Mode selection |
| 0x04 | Status | **Read-only** | Write-only | 32 | Status register - Done, Busy, Error flags |
| 0x08 | Addr_Base_L | **Write-only** | Read-only | 32 | Base address low 32-bit |
| 0x0C | Addr_Base_H | **Write-only** | Read-only | 32 | Base address high 32-bit (64-bit addressing) |
| 0x10 | Length_L | **Write-only** | Read-only | 32 | Data length low 32-bit |
| 0x14 | Length_H | **Write-only** | Read-only | 32 | Data length high 32-bit |
| 0x18 | Reserved | - | - | 32 | Reserved for future use |

**Access Permission Model:**

This register file implements a **dual-access architecture**:
1. **Processor (via MMIO)** configures operations:
   - Writes Control, Addr_Base_L/H, Length_L/H (configuration registers)
   - Reads Status (to monitor operation progress)
   
2. **Compute Engines (Softmax/Norm)** execute operations autonomously:
   - Read configuration registers (Control, Address, Length) to understand what to do
   - Write to Status register to report progress and completion
   - Perform actual memory operations via AXI4-Lite Master independently

This design allows **concurrent operation**:
- While Processor is checking status (reading Status register)
- Softmax is simultaneously reading config and accessing memory
- No handshaking needed after START is set

### Control Register (0x00, Write-only to Processor, Read-only to Engine)
```
Bit[0]    : START - Start/enable operation
            Processor writes 1 to trigger computation
            Auto-clears or manually cleared by compute engine
Bit[30:1] : Reserved
Bit[31]   : MODE - Operation mode selection
            0 = SoftMax Engine (reads input, computes softmax, stores output)
            1 = Normalization (not yet implemented)
```

### Status Register (0x04, Read-only to Processor, Write-only to Engine)
```
Bit[0]    : DONE  - Operation completed (1 = done, set by engine)
Bit[1]    : BUSY  - Operation in progress (1 = busy, set by engine)
Bit[2]    : ERROR - Error occurred (1 = error, set by engine on failure)
Bit[31:3] : Reserved
Bit[31:3] : Reserved
```

### Address Base Registers
- **Addr_Base_L (0x08)**: Lower 32 bits of 64-bit base address
- **Addr_Base_H (0x0C)**: Upper 32 bits of 64-bit base address
- **Full Address**: {Addr_Base_H, Addr_Base_L}

### Length Registers
- **Length_L (0x10)**: Lower 32 bits of 64-bit data length
- **Length_H (0x14)**: Upper 32 bits of 64-bit data length
- **Full Length**: {Length_H, Length_L}

## Operation Flow

### Step 1: Configure Base Address
```c
// Write base address low word
write_register(SOLE_BASE + 0x08, 0x12345678);

// Write base address high word
write_register(SOLE_BASE + 0x0C, 0x00000000);
```

### Step 2: Configure Data Length
```c
// Write length low word
write_register(SOLE_BASE + 0x10, 0x00001000);  // 4096 elements

// Write length high word
write_register(SOLE_BASE + 0x14, 0x00000000);
```

### Step 3: Select Mode and Start
```c
// SoftMax operation (Mode=0, START=1)
write_register(SOLE_BASE + 0x00, 0x00000001);

// OR for Normalization (Mode=1, START=1)
// write_register(SOLE_BASE + 0x00, 0x80000001);
```

### Step 4: Poll Status
```c
uint32_t status;
do {
    status = read_register(SOLE_BASE + 0x04);
} while ((status & 0x1) == 0);  // Wait for DONE bit

if (status & 0x4) {
    printf("Error occurred\n");
} else {
    printf("Operation completed successfully\n");
}
```

## Internal Implementation Details

### AXI4-Lite Slave Handler (`axi_read_process`, `axi_write_process`)
- **Read Handler**: Accepts read addresses, decodes register offset, returns corresponding register value
- **Write Handler**: Accepts write addresses and data, updates MMIO registers with write strobes validation
- **Always Ready**: Both handlers always assert ready signals for simplified implementation

### Demux Logic (`demux_logic`)
- **Mode Selection**: Extracts Mode bit[31] from Control register
- **Routing**: Routes address, length, and start signals to SoftMax (Mode=0) or Norm (Mode=1)
- **32 to 64-bit Conversion**: Combines low/high register pairs into 64-bit values for addresses and lengths

### Status Update (`status_update`)
- **Status Aggregation**: Combines status signals from selected compute engine
- **Auto-Clear**: Automatically clears START bit after operation begins (pulse behavior)
- **Done Detection**: Sets DONE bit when operation completes
- **Error Handling**: Sets ERROR bit if any error condition occurs

## Integration with Softmax Module

The SOLE module instantiates one `Softmax` module internally. Wire connections:
```cpp
softmax_unit = new Softmax("softmax_unit");
softmax_unit->clk(clk);
softmax_unit->rst(rst);

// Softmax AXI4-Lite Master ports connect to system AXI fabric
// (actual connection depends on SoC interconnect)

// Signal routing:
// - Address and length from MMIO RegFile → Softmax configuration
// - Start signal from Demux → Softmax operation trigger
// - Done/Busy signals from Softmax → Status register
```

## Design Notes

- **Address Width**: Full 64-bit support via two 32-bit registers (Addr_Base_L/H)
- **Data Length**: Full 64-bit support via two 32-bit registers (Length_L/H)
- **Register Mask**: Only lower 8 bits of address used (allowing 256-byte address space per instance)
- **Response Codes**: 
  - `AXI_RESP_OKAY (0x0)`: Successful transaction
  - `AXI_RESP_DECERR (0x3)`: Decode error (invalid register address)
- **Norm Engine**: Placeholder structure provided for future implementation
- **Synchronization**: All state machines are synchronous to `clk` edge

## Example: SoftMax Operation

```c
// Assuming SOLE instance at 0x80000000

// 1. Write configuration
write32(0x80000008, 0x00100000);  // Base address low
write32(0x8000000C, 0x00000000);  // Base address high
write32(0x80000010, 0x00001000);  // Length low (4K)
write32(0x80000014, 0x00000000);  // Length high

// 2. Start SoftMax (Mode=0, START=1)
write32(0x80000000, 0x00000001);

// 3. Poll completion
while ((read32(0x80000004) & 0x1) == 0) {
    ; // Wait for DONE
}

printf("SoftMax operation complete\n");
```

## Testing Considerations

1. **Register Access**: Verify read/write operations for all registers
2. **Mode Selection**: Test both SoftMax (Mode=0) and Norm (Mode=1) paths
3. **Start Pulse**: Verify START bit auto-clears after operation begins
4. **Status Flags**: Validate DONE, BUSY, and ERROR flag behavior
5. **Address Encoding**: Test 64-bit address/length combinations
6. **Error Handling**: Test invalid register addresses (expect DECERR responses)

---

**Module Status**: ✓ Complete (SoftMax integrated, Norm placeholder ready)

**Last Updated**: February 28, 2026
