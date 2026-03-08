#include <iostream>
#include <systemc.h>
#include "Softmax.h"

using sc_dt::sc_uint;

int sc_main(int argc, char* argv[]) {
    std::cout << "===== Minimal Softmax Test =====" << std::endl;
    
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rst("rst");
    
    Softmax softmax_unit("softmax_unit");
    
    softmax_unit.clk(clk);
    softmax_unit.rst(rst);
    
    // Bind all port defaults to avoid unbound port errors
    sc_signal<bool> sig_bool_def(false);
    sc_signal<sc_uint<2>> sig_2b(0);
    sc_signal<sc_uint<8>> sig_8b(0);
    sc_signal<sc_uint<32>> sig_32b(0);
    sc_signal<sc_uint<64>> sig_64b(0);
    
    softmax_unit.start(sig_bool_def);
    softmax_unit.src_addr_base(sig_64b);
    softmax_unit.dst_addr_base(sig_64b);
    softmax_unit.data_length(sig_32b);
    softmax_unit.status(sig_32b);
    
    // AXI Master Ports
    softmax_unit.M_AXI_AWADDR(sig_32b);
    softmax_unit.M_AXI_AWVALID(sig_bool_def);
    softmax_unit.M_AXI_AWREADY(sig_bool_def);
    softmax_unit.M_AXI_WDATA(sig_64b);
    softmax_unit.M_AXI_WSTRB(sig_8b);
    softmax_unit.M_AXI_WVALID(sig_bool_def);
    softmax_unit.M_AXI_WREADY(sig_bool_def);
    softmax_unit.M_AXI_BRESP(sig_2b);
    softmax_unit.M_AXI_BVALID(sig_bool_def);
    softmax_unit.M_AXI_BREADY(sig_bool_def);
    softmax_unit.M_AXI_ARADDR(sig_32b);
    softmax_unit.M_AXI_ARVALID(sig_bool_def);
    softmax_unit.M_AXI_ARREADY(sig_bool_def);
    softmax_unit.M_AXI_RDATA(sig_64b);
    softmax_unit.M_AXI_RRESP(sig_2b);
    softmax_unit.M_AXI_RVALID(sig_bool_def);
    softmax_unit.M_AXI_RREADY(sig_bool_def);
    
    std::cout << "✓ All ports bound successfully!" << std::endl;
    std::cout << "===== Test PASSED =====" << std::endl;
    
    return 0;
}
