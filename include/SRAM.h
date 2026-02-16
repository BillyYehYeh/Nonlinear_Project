#ifndef SRAM_H
#define SRAM_H

#include <systemc.h>
#include <vector>

template<int ADDR_BITS, int DATA_BITS>
class SRAM : public sc_module {
public:
    // Template constants
    static constexpr int BYTE_SIZE = 1 << ADDR_BITS;  // 2^ADDR_BITS
    
    // Ports
    sc_in<bool> clk;
    sc_in<bool> rst;
    sc_in<bool> we;                                    // Write enable
    sc_in<bool> re;                                    // Read enable
    sc_in<sc_dt::sc_uint<ADDR_BITS>> waddr;           // Write address
    sc_in<sc_dt::sc_uint<ADDR_BITS>> raddr;           // Read address
    sc_in<sc_dt::sc_uint<DATA_BITS>> wdata;           // Write data
    sc_out<sc_dt::sc_uint<DATA_BITS>> rdata;          // Read data
    
    // Constructor
    SC_HAS_PROCESS(SRAM);
    SRAM(sc_module_name name) : sc_module(name) {
        // Initialize memory
        mem.resize(BYTE_SIZE);
        reset_memory();
        
        // Register the memory process
        SC_METHOD(memory_process);
        sensitive << clk.pos();
        dont_initialize();
    }
    
    // Destructor
    ~SRAM() {}
    
private:
    // Memory storage
    std::vector<sc_dt::sc_uint<DATA_BITS>> mem;
    
    // Output register for 1-cycle pipeline
    sc_dt::sc_uint<DATA_BITS> rdata_reg;
    
    // FSM process
    void memory_process() {
        if (rst.read()) {
            reset_memory();
            rdata.write(0);
        } else {
            // Write operation
            if (we.read()) {
                int waddr_val = waddr.read().to_int();
                if (waddr_val < BYTE_SIZE) {
                    mem[waddr_val] = wdata.read();
                }
            }
            
            // Read operation with 1-cycle latency
            if (re.read()) {
                int raddr_val = raddr.read().to_int();
                if (raddr_val < BYTE_SIZE) {
                    rdata_reg = mem[raddr_val];
                } else {
                    rdata_reg = 0;
                }
            }
            
            // Output the read data from register (1-cycle pipeline)
            rdata.write(rdata_reg);
        }
    }
    
    // Helper functions
    void reset_memory() {
        for (int i = 0; i < BYTE_SIZE; i++) {
            mem[i] = 0;
        }
    }
};

#endif // SRAM_H
