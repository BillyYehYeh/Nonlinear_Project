module SRAM #(
  parameter int ADDR_BITS = 12,
  parameter int DATA_BITS = 16
) (
  input  logic                  clk,
  input  logic                  rst,
  input  logic                  we,
  input  logic                  re,
  input  logic [ADDR_BITS-1:0]  waddr,
  input  logic [ADDR_BITS-1:0]  raddr,
  input  logic [DATA_BITS-1:0]  wdata,
  output logic [DATA_BITS-1:0]  rdata
);
  localparam int DEPTH = (1 << ADDR_BITS);
  logic [DATA_BITS-1:0] mem [0:DEPTH-1];
  logic [DATA_BITS-1:0] rdata_reg;
  integer i;

  always_ff @(posedge clk) begin
    if (rst) begin
      for (i = 0; i < DEPTH; i++) begin
        mem[i] <= '0;
      end
      rdata_reg <= '0;
    end else begin
      if (we) mem[waddr] <= wdata;
      if (re) rdata_reg <= mem[raddr];
    end
  end

  assign rdata = rdata_reg;
endmodule
