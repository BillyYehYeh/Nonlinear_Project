#include <iostream>
#include <systemc.h>
#include "Softmax.h"

using sc_dt::sc_uint;

/**
 * @brief Simple integration test for SOLE Softmax Module
 * 
 * This test verifies that the Softmax module can be instantiated
 * and linked with all its sub-modules without compilation errors.
 */

int sc_main(int argc, char* argv[]) {
    std::cout << "===== SOLE Softmax Module Compilation Test =====" << std::endl;
    std::cout << "Creating Softmax module instance..." << std::endl;
    
    // Create clock and reset signals
    sc_clock clk("clk", 10, SC_NS);
    sc_signal<bool> rst("rst");
    
    // Create Softmax module
    Softmax softmax_unit("softmax_unit");
    
    // Bind system signals (required)
    softmax_unit.clk(clk);
    softmax_unit.rst(rst);
    
    // Create and bind control signals using proper sc_signal declarations
    sc_signal<bool> start_sig("start_sig");
    sc_signal<sc_uint<64>> src_addr_sig("src_addr_sig");
    sc_signal<sc_uint<64>> dst_addr_sig("dst_addr_sig");
    sc_signal<sc_uint<32>> data_len_sig("data_len_sig");
    softmax_unit.start(start_sig);
    softmax_unit.src_addr_base(src_addr_sig);
    softmax_unit.dst_addr_base(dst_addr_sig);
    softmax_unit.data_length(data_len_sig);
    
    // Bind status output
    sc_signal<sc_uint<32>> status_sig("status_sig");
    softmax_unit.status(status_sig);
    
    // Bind AXI write address channel
    sc_signal<sc_uint<32>> axi_awaddr("axi_awaddr");
    sc_signal<bool> axi_awvalid("axi_awvalid");
    sc_signal<bool> axi_awready("axi_awready");
    softmax_unit.M_AXI_AWADDR(axi_awaddr);
    softmax_unit.M_AXI_AWVALID(axi_awvalid);
    softmax_unit.M_AXI_AWREADY(axi_awready);
    
    // Bind AXI write data channel
    sc_signal<sc_uint<64>> axi_wdata("axi_wdata");
    sc_signal<sc_uint<8>> axi_wstrb("axi_wstrb");
    sc_signal<bool> axi_wvalid("axi_wvalid");
    sc_signal<bool> axi_wready("axi_wready");
    softmax_unit.M_AXI_WDATA(axi_wdata);
    softmax_unit.M_AXI_WSTRB(axi_wstrb);
    softmax_unit.M_AXI_WVALID(axi_wvalid);
    softmax_unit.M_AXI_WREADY(axi_wready);
    
    // Bind AXI write response channel
    sc_signal<sc_uint<2>> axi_bresp("axi_bresp");
    sc_signal<bool> axi_bvalid("axi_bvalid");
    sc_signal<bool> axi_bready("axi_bready");
    softmax_unit.M_AXI_BRESP(axi_bresp);
    softmax_unit.M_AXI_BVALID(axi_bvalid);
    softmax_unit.M_AXI_BREADY(axi_bready);
    
    // Bind AXI read address channel
    sc_signal<sc_uint<32>> axi_araddr("axi_araddr");
    sc_signal<bool> axi_arvalid("axi_arvalid");
    sc_signal<bool> axi_arready("axi_arready");
    softmax_unit.M_AXI_ARADDR(axi_araddr);
    softmax_unit.M_AXI_ARVALID(axi_arvalid);
    softmax_unit.M_AXI_ARREADY(axi_arready);
    
    // Bind AXI read data channel
    sc_signal<sc_uint<64>> axi_rdata("axi_rdata");
    sc_signal<sc_uint<2>> axi_rresp("axi_rresp");
    sc_signal<bool> axi_rvalid("axi_rvalid");
    sc_signal<bool> axi_rready("axi_rready");
    softmax_unit.M_AXI_RDATA(axi_rdata);
    softmax_unit.M_AXI_RRESP(axi_rresp);
    softmax_unit.M_AXI_RVALID(axi_rvalid);
    softmax_unit.M_AXI_RREADY(axi_rready);
    
    std::cout << "✓ Softmax module instantiated successfully!" << std::endl;
    std::cout << "✓ All sub-modules (PROCESS_1, PROCESS_2, PROCESS_3) linked successfully!" << std::endl;
    std::cout << "✓ All AXI ports and control signals connected!" << std::endl;
    std::cout << "\n===== Compilation Test PASSED =====" << std::endl;
    
    return 0;
}
