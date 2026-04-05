`include "sole_pkg.sv"
module Max_FIFO (
  input  logic                        clk,
  input  logic                        rst,
  input  logic [15:0]                 data_in,
  input  logic                        write_en,
  output logic [15:0]                 data_out,
  input  logic                        read_ready,
  output logic                        read_valid,
  input  logic                        clear,
  output logic                        full,
  output logic                        empty,
  output logic [sole_pkg::FIFO_ADDR_BITS-1:0] count
);
  logic [sole_pkg::FIFO_ADDR_BITS-1:0] wptr;
  logic [sole_pkg::FIFO_ADDR_BITS-1:0] rptr;
  logic [15:0] mem_rdata;
  logic re_q;

  SRAM #(.ADDR_BITS(sole_pkg::FIFO_ADDR_BITS), .DATA_BITS(16)) u_sram (
    .clk(clk), .rst(rst),
    .we(write_en && !full), .re(read_ready && !empty),
    .waddr(wptr), .raddr(rptr),
    .wdata(data_in), .rdata(mem_rdata)
  );

  always_ff @(posedge clk) begin
    if (rst || clear) begin
      wptr <= '0;
      rptr <= '0;
      re_q <= 1'b0;
    end else begin
      re_q <= read_ready && !empty;
      if (write_en && !full) wptr <= wptr + 1'b1;
      if (read_ready && !empty) rptr <= rptr + 1'b1;
    end
  end

  always_comb begin
    count = wptr - rptr;
    empty = (count == 0);
    full = (&count);
    data_out = mem_rdata;
    read_valid = re_q;
  end
endmodule
