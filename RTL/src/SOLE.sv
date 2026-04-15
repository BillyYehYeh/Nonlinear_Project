module SOLE (
  input  logic                           clk,
  input  logic                           rst,
  input  logic [31:0]                    proc_addr,
  input  logic [31:0]                    proc_wdata,
  input  logic                           proc_we,
  output logic [31:0]                    proc_rdata,
  output logic                           interrupt,
  output logic [31:0]                    M_AXI_AWADDR,
  output logic                           M_AXI_AWVALID,
  input  logic                           M_AXI_AWREADY,
  output logic [63:0]                    M_AXI_WDATA,
  output logic [7:0]                     M_AXI_WSTRB,
  output logic                           M_AXI_WVALID,
  input  logic                           M_AXI_WREADY,
  input  logic [1:0]                     M_AXI_BRESP,
  input  logic                           M_AXI_BVALID,
  output logic                           M_AXI_BREADY,
  output logic [31:0]                    M_AXI_ARADDR,
  output logic                           M_AXI_ARVALID,
  input  logic                           M_AXI_ARREADY,
  input  logic [63:0]                    M_AXI_RDATA,
  input  logic [1:0]                     M_AXI_RRESP,
  input  logic                           M_AXI_RVALID,
  output logic                           M_AXI_RREADY
);
  // Local parameter definitions (from sole_pkg)
  localparam int AXI_ADDR_WIDTH = 32;
  localparam int AXI_DATA_WIDTH = 64;
  localparam int AXI_STRB_WIDTH = 8;
  
  localparam logic [7:0] REG_CONTROL         = 8'h00;
  localparam logic [7:0] REG_STATUS          = 8'h04;
  localparam logic [7:0] REG_SRC_ADDR_BASE_L = 8'h08;
  localparam logic [7:0] REG_SRC_ADDR_BASE_H = 8'h0C;
  localparam logic [7:0] REG_DST_ADDR_BASE_L = 8'h10;
  localparam logic [7:0] REG_DST_ADDR_BASE_H = 8'h14;
  localparam logic [7:0] REG_LENGTH_L        = 8'h18;
  localparam logic [7:0] REG_LENGTH_H        = 8'h1C;

  localparam int CTRL_START_BIT = 0;
  localparam int CTRL_MODE_BIT = 31;

  logic [31:0] reg_control;
  logic [31:0] reg_status;
  logic [31:0] reg_src_addr_base_l;
  logic [31:0] reg_src_addr_base_h;
  logic [31:0] reg_dst_addr_base_l;
  logic [31:0] reg_dst_addr_base_h;
  logic [31:0] reg_length_l;
  logic [31:0] reg_length_h;

  logic [31:0] softmax_status;
  logic softmax_start;
  logic [63:0] src_addr_base;
  logic [63:0] dst_addr_base;
  logic [63:0] data_length;

  assign src_addr_base = {reg_src_addr_base_h, reg_src_addr_base_l};
  assign dst_addr_base = {reg_dst_addr_base_h, reg_dst_addr_base_l};
  assign data_length = {reg_length_h, reg_length_l};
  assign softmax_start = reg_control[CTRL_START_BIT] && !reg_control[CTRL_MODE_BIT];

  Softmax u_softmax (
    .clk(clk),
    .rst(rst),
    .start(softmax_start),
    .src_addr_base(src_addr_base),
    .dst_addr_base(dst_addr_base),
    .data_length(data_length),
    .status_o(softmax_status),
    .M_AXI_AWADDR(M_AXI_AWADDR),
    .M_AXI_AWVALID(M_AXI_AWVALID),
    .M_AXI_AWREADY(M_AXI_AWREADY),
    .M_AXI_WDATA(M_AXI_WDATA),
    .M_AXI_WSTRB(M_AXI_WSTRB),
    .M_AXI_WVALID(M_AXI_WVALID),
    .M_AXI_WREADY(M_AXI_WREADY),
    .M_AXI_BRESP(M_AXI_BRESP),
    .M_AXI_BVALID(M_AXI_BVALID),
    .M_AXI_BREADY(M_AXI_BREADY),
    .M_AXI_ARADDR(M_AXI_ARADDR),
    .M_AXI_ARVALID(M_AXI_ARVALID),
    .M_AXI_ARREADY(M_AXI_ARREADY),
    .M_AXI_RDATA(M_AXI_RDATA),
    .M_AXI_RRESP(M_AXI_RRESP),
    .M_AXI_RVALID(M_AXI_RVALID),
    .M_AXI_RREADY(M_AXI_RREADY)
  );

  always_ff @(posedge clk) begin
    if (rst) begin
      reg_control <= 32'd0;
      reg_src_addr_base_l <= 32'd0;
      reg_src_addr_base_h <= 32'd0;
      reg_dst_addr_base_l <= 32'd0;
      reg_dst_addr_base_h <= 32'd0;
      reg_length_l <= 32'd0;
      reg_length_h <= 32'd0;
    end else if (proc_we) begin
      case (proc_addr[7:0])
        REG_CONTROL: reg_control <= proc_wdata;
        REG_SRC_ADDR_BASE_L: reg_src_addr_base_l <= proc_wdata;
        REG_SRC_ADDR_BASE_H: reg_src_addr_base_h <= proc_wdata;
        REG_DST_ADDR_BASE_L: reg_dst_addr_base_l <= proc_wdata;
        REG_DST_ADDR_BASE_H: reg_dst_addr_base_h <= proc_wdata;
        REG_LENGTH_L: reg_length_l <= proc_wdata;
        REG_LENGTH_H: reg_length_h <= proc_wdata;
        default: ;
      endcase
    end else if (softmax_start && (softmax_status[2:1] != 2'b00)) begin
      // Match SystemC SOLE pulse behavior: clear START once engine begins running.
      reg_control[CTRL_START_BIT] <= 1'b0;
    end
  end

  always @* begin
    reg_status = softmax_status;
    case (proc_addr[7:0])
      REG_STATUS: proc_rdata = reg_status;
      REG_CONTROL: proc_rdata = reg_control;
      REG_SRC_ADDR_BASE_L: proc_rdata = reg_src_addr_base_l;
      REG_SRC_ADDR_BASE_H: proc_rdata = reg_src_addr_base_h;
      REG_DST_ADDR_BASE_L: proc_rdata = reg_dst_addr_base_l;
      REG_DST_ADDR_BASE_H: proc_rdata = reg_dst_addr_base_h;
      REG_LENGTH_L: proc_rdata = reg_length_l;
      REG_LENGTH_H: proc_rdata = reg_length_h;
      default: proc_rdata = 32'd0;
    endcase

    interrupt = reg_status[0] | reg_status[3];
  end
endmodule
