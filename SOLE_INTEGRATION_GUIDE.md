# SOLE Integration Guide

## File Structure

```
CNN/
├── include/
│   ├── SOLE.h               ← SOLE module header
│   ├── Softmax.h            ← SoftMax engine (already present)
│   ├── axi4-lite.hpp        ← AXI4-Lite definitions
│   └── ... (other modules)
│
├── src/
│   ├── SOLE.cpp             ← SOLE implementation
│   └── ... (other source files)
│
└── SOLE_MODULE_DOCUMENTATION.md  ← This documentation
```

## Usage in SystemC Top Level

### 1. Include the Header
```cpp
#include "SOLE.h"
```

### 2. Instantiate SOLE Module
```cpp
int sc_main(int argc, char* argv[]) {
    // Create clock and reset signals
    sc_clock clk("clk", 10, SC_NS);        // 10 ns period
    sc_signal<bool> rst;
    
    // Create Processor MMIO signals (simplified 4-signal interface)
    sc_signal<sc_uint32> proc_addr;        // Register address
    sc_signal<sc_uint32> proc_wdata;       // Write data
    sc_signal<bool>      proc_we;          // Write enable (1=write, 0=read)
    sc_signal<sc_uint32> proc_rdata;       // Read data
    
    // Create AXI4-Lite Master signals (for Softmax to access memory)
    sc_signal<sc_uint32> m_axi_awaddr;
    sc_signal<bool>      m_axi_awvalid;
    sc_signal<bool>      m_axi_awready;
    // ... (create all other AXI4-Lite Master signals)
    
    // Instantiate SOLE module
    SOLE sole_instance("SOLE_INST");
    sole_instance.clk(clk);
    sole_instance.rst(rst);
    
    // Connect Processor MMIO interface (simple 4-signal)
    sole_instance.proc_addr(proc_addr);
    sole_instance.proc_wdata(proc_wdata);
    sole_instance.proc_we(proc_we);
    sole_instance.proc_rdata(proc_rdata);
    
    // Connect AXI4-Lite Master ports (for memory access)
    sole_instance.M_AXI_AWADDR(m_axi_awaddr);
    sole_instance.M_AXI_AWVALID(m_axi_awvalid);
    sole_instance.M_AXI_AWREADY(m_axi_awready);
    // ... (connect all other AXI Master ports)
    
    // Create memory model (AXI4-Lite Slave) if needed
    MemoryModel memory("MEMORY");
    memory.clk(clk);
    memory.M_AXI_AWADDR(m_axi_awaddr);
    memory.M_AXI_AWVALID(m_axi_awvalid);
    // ... (connect memory model)
    
    // Run simulation
    sc_start();
    
    return 0;
}
```

### 3. CMakeLists.txt Integration
```cmake
# Add SOLE module sources
set(SOLE_SOURCES
    src/SOLE.cpp
    src/Softmax.cpp      # Or wherever Softmax implementation is
)

# Add SOLE library
add_library(sole_lib ${SOLE_SOURCES})
target_include_directories(sole_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Link with SystemC
target_link_libraries(sole_lib PUBLIC ${SystemC_LIBRARIES})

# Add test executable (if needed)
add_executable(sole_test test/SOLE_test.cpp)
target_link_libraries(sole_test PRIVATE sole_lib ${SystemC_LIBRARIES})
```

## Key Port Groups

### System Ports
- `clk`: System clock input
- `rst`: Asynchronous reset input

### Processor MMIO Interface (Simplified 4-Signal)
- `proc_addr[31:0]`: Register address (Addr[7:0] = register offset)
- `proc_wdata[31:0]`: Write data to register (used when proc_we=1)
- `proc_we`: Write enable (1=write operation, 0=read operation)
- `proc_rdata[31:0]`: Read data from register (output, combinational during proc_we=0)

**Processor Interface Notes:**
- Register offset extracted from proc_addr[7:0] (lower 8 bits)
- Valid offsets: 0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18 (7 registers)
- Write operations are sequential (sampled on clock edge)
- Read operations are combinational (rdata updates immediately)
- Status register (0x04) is read-only for Processor
- Control, Address, Length registers are write-only for Processor

SOLE exposes an AXI4-Lite Master interface that is internally driven by the Softmax engine. These ports connect to system memory or memory controllers.

#### Master Write Address Channel (M_AXI_AW*)
- `M_AXI_AWADDR[31:0]`: Write address to memory
- `M_AXI_AWVALID`: Write address valid (output)
- `M_AXI_AWREADY`: Write address ready (input)

#### Master Write Data Channel (M_AXI_W*)
- `M_AXI_WDATA[63:0]`: Write data (64-bit wide, output)
- `M_AXI_WSTRB[7:0]`: Write strobes for 8 bytes (output)
- `M_AXI_WVALID`: Write data valid (output)
- `M_AXI_WREADY`: Write data ready (input)

#### Master Write Response Channel (M_AXI_B*)
- `M_AXI_BRESP[1:0]`: Write response from memory (input)
- `M_AXI_BVALID`: Write response valid (input)
- `M_AXI_BREADY`: Write response ready (output)

#### Master Read Address Channel (M_AXI_AR*)
- `M_AXI_ARADDR[31:0]`: Read address from memory
- `M_AXI_ARVALID`: Read address valid (output)
- `M_AXI_ARREADY`: Read address ready (input)

#### Master Read Data Channel (M_AXI_R*)
- `M_AXI_RDATA[63:0]`: Read data from memory (64-bit wide, input)
- `M_AXI_RRESP[1:0]`: Read response from memory (input)
- `M_AXI_RVALID`: Read data valid (input)
- `M_AXI_RREADY`: Read data ready (output)

**Important Master Interface Notes:**
- Master interface is **controlled by Softmax engine**, not directly by software
- Master is active only when Mode=0 (Softmax mode) in Control register
- Data width is 64-bit for efficient memory transfers
- Address range: Full 32-bit addressable space (0x00000000-0xFFFFFFFF)
- Typical usage: Softmax accesses input/output data in system SRAM or external memory

## Register Offsets Reference

| Purpose | Register Name | Offset | Access | Notes |
|---------|---------------|--------|--------|-------|
| Control | Control | 0x00 | W | Bit[0]=START, Bit[31]=MODE |
| Status | Status | 0x04 | R | Bit[0]=DONE, Bit[1]=BUSY |
| Address (Low) | Addr_Base_L | 0x08 | W | Lower 32-bits of 64-bit address |
| Address (High) | Addr_Base_H | 0x0C | W | Upper 32-bits of 64-bit address |
| Length (Low) | Length_L | 0x10 | W | Lower 32-bits of 64-bit length |
| Length (High) | Length_H | 0x14 | W | Upper 32-bits of 64-bit length |
| Reserved | Reserved | 0x18 | - | For future use |

## Quick Test Example

```cpp
// Pseudo-code for testing SOLE with SoftMax using simple MMIO interface

// Helper functions for MMIO access
void mmio_write(sc_signal<sc_uint32>& proc_addr,
                sc_signal<sc_uint32>& proc_wdata,
                sc_signal<bool>& proc_we,
                uint32_t addr, uint32_t data) {
    proc_addr.write(addr);
    proc_wdata.write(data);
    proc_we.write(true);  // Write mode
    wait(1, SC_NS);       // 1 clock cycle for write to complete
}

uint32_t mmio_read(sc_signal<sc_uint32>& proc_addr,
                   sc_signal<bool>& proc_we,
                   sc_signal<sc_uint32>& proc_rdata,
                   uint32_t addr) {
    proc_addr.write(addr);
    proc_we.write(false);  // Read mode
    wait(1, SC_NS);        // 1 clock cycle for read
    return proc_rdata.read();
}

void test_sole_softmax() {
    // 1. Write base address (64-bit split into two 32-bit registers)
    mmio_write(proc_addr, proc_wdata, proc_we, 0x08, 0x12340000);  // Addr_Base_L
    mmio_write(proc_addr, proc_wdata, proc_we, 0x0C, 0x00000000);  // Addr_Base_H
    
    // 2. Write length (64-bit split into two 32-bit registers)
    mmio_write(proc_addr, proc_wdata, proc_we, 0x10, 0x00001000);  // Length_L (4KiB)
    mmio_write(proc_addr, proc_wdata, proc_we, 0x14, 0x00000000);  // Length_H
    
    // 3. Start SoftMax operation
    //    Control[0] = START bit (write 1 to trigger)
    //    Control[31] = MODE (0 for SoftMax, 1 for Norm)
    mmio_write(proc_addr, proc_wdata, proc_we, 0x00, 0x00000001);  // START=1, MODE=0
    
    // 4. Poll status register for completion
    uint32_t status;
    int timeout = 1000;
    do {
        status = mmio_read(proc_addr, proc_we, proc_rdata, 0x04);  // Read Status
        wait(1, SC_US);  // Wait 1 microsecond between polls
    } while (((status & 0x1) == 0) && (--timeout > 0));
    
    // 5. Check result
    if (status & 0x1) {
        printf("✓ SoftMax completed (Status=0x%x)\n", status);
    } else if (status & 0x4) {
        printf("✗ Error occurred (Status=0x%x)\n", status);
    } else if (status & 0x2) {
        printf("⏳ Still busy but timeout (Status=0x%x)\n", status);
    } else {
        printf("✗ Timeout - no response from engine\n");
    }
}
```

**Key Points:**
- **Writes** are sequential: Processor sets proc_we=1, then waits for at least 1 clock edge
- **Reads** are combinational: Processor sets proc_we=0 and proc_rdata updates immediately
- **Status Register** bit meanings:
  - Bit[0] DONE: 1 = operation completed
  - Bit[1] BUSY: 1 = operation in progress  
  - Bit[2] ERROR: 1 = error detected
- **Independent Operation**: Once START is set, Softmax engine operates autonomously on memory via AXI Master interface

## Master Interface Operation

When SOLE is configured in SoftMax mode (Control[31]=0), the embedded Softmax engine uses the Master interface to:

1. **Read input data** from system memory via M_AXI_AR*/R* channels
2. **Perform computations** internally
3. **Write output results** back to memory via M_AXI_AW*/W*/B* channels

The Master interface operates **independently and asynchronously** from MMIO interface commands. Software workflow:
1. Configure via MMIO (write Addr_Base and Length)
2. Assert START bit
3. Monitor DONE flag via Status register
4. All memory access is handled autonomously by Softmax engine

Actual memory transactions are managed entirely by the Softmax engine without Processor involvement.

## Signal Flow Diagram

```
Host Processor
      │
      ├─→ MMIO Write (Control=0x1)  [via proc_addr/proc_wdata/proc_we]
      │        │
      │        ▼
      │   [SOLE Module MMIO Handler]
      │        │
      │        ├─→ Update Control/Addr/Length Registers
      │        │
      │        └─→ Demux (Mode[31]=0 → SoftMax)
      │        │        │
      │        │        ▼
      │        │   [SoftMax Engine]
      │        │        │
      │        │        ├─→ Read Control/Addr/Length registers (autonomous)
      │        │        │
      │        │        ├─→ [AXI4-Lite Master] Fetch input data from memory
      │        │        │        │
      │        │        │        ▼
      │        │        │   System Memory / SRAM
      │        │        │
      │        │        ├─→ Process SoftMax computation (internal)
      │        │        │
      │        │        ├─→ [AXI4-Lite Master] Write results back to memory
      │        │        │
      │        │        └─→ Write Status register (DONE=1)
      │        │
      │        └─ Status updated in register file
      │
      └←─ MMIO Read (Status)  [via proc_addr/proc_rdata] ✓

**Dual-Access Architecture:**
- Processor uses simple MMIO interface (4 signals) - blocking on writes, combinational on reads
- Softmax engine uses autonomous MMIO access (reads config) + AXI Master (memory operations)
- No handshaking: Processor tells engine what to do, then polls status independently
```

## Implementation Status

| Component | Status | Notes |
|-----------|--------|-------|
| SOLE Module Header (SOLE.h) | ✓ Complete | Simple MMIO interface + AXI4-Lite Master |
| SOLE Implementation (SOLE.cpp) | ✓ Complete | Simplified MMIO access handler, Demux, Status update |
| MMIO Configuration (SOLE_MMIO.hpp) | ✓ Complete | Namespaced register constants in sole::mmio |
| Processor MMIO Interface | ✓ Complete | 4-signal interface (Addr, Wdata, We, Rdata) |
| Dual-Access Register File | ✓ Complete | Control/Addr/Length write-only to Proc, read-only to engine; Status opposite |
| AXI4-Lite Master Interface | ✓ Complete | Full 5-channel Master for Softmax → Memory |
| SoftMax Integration | ✓ Ready | Instantiated, Master signals routed via Demux |
| SoftMax Master Routing | ✓ Complete | All 24 Master ports properly multiplexed |
| Norm Engine | ⚠ Placeholder | Structure ready, implementation pending |
| Documentation | ✓ Complete | Full module documentation with dual-access details |

## Connected Signals in SOLE

```
┌─────────────────────────────────────────────────────┐
│                 SOLE Internal                        │
├─────────────────────────────────────────────────────┤
│                                                      │
│  AXI Handler → reg_control                          │
│                    │                                 │
│                    ▼                                 │
│             Demux Logic                             │
│                    │                                 │
│        ┌───────────┴──────────┐                     │
│        │ (Mode=0)             │ (Mode=1)            │
│        ▼                      ▼                      │
│   SoftMax Unit            Norm Unit                 │
│   (connected)             (placeholder)             │
│        │                                             │
│        └────────┬─────────────┘                     │
│                 ▼                                    │
│         Status Aggregation                         │
│                 │                                    │
│                 ▼                                    │
│           reg_status                                │
│                 │                                    │
│                 ▼                                    │
│         AXI Response Handler                        │
│                 │                                    │
│                 ▼                                    │
│           S_AXI_RDATA                              │
│                                                      │
└─────────────────────────────────────────────────────┘
```

---

**Integration Guide Version**: 1.0  
**Last Updated**: February 28, 2026  
**Module**: SOLE (Softmax/Normalization Linear Engine)
