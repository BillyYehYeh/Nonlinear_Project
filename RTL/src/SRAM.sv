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
  localparam int MACRO_DEPTH = 128;
  localparam int MACRO_WIDTH = 64;
  localparam int WORDS_PER_MACRO_ROW = MACRO_WIDTH / DATA_BITS; // 4 when DATA_BITS=16
  localparam int NUM_ROWS = DEPTH / WORDS_PER_MACRO_ROW;
  localparam int NUM_MACROS = NUM_ROWS / MACRO_DEPTH;

  logic [DATA_BITS-1:0] rdata_reg;
  logic [DEPTH-1:0]     valid_bits;

  // Row shadow keeps SystemC-equivalent dual-operation behavior in simulation.
  logic [MACRO_WIDTH-1:0] row_shadow [0:NUM_ROWS-1];

  logic [6:0]  macro_a    [0:NUM_MACROS-1];
  logic [63:0] macro_d    [0:NUM_MACROS-1];
  logic [63:0] macro_bweb [0:NUM_MACROS-1];
  logic        macro_ceb  [0:NUM_MACROS-1];
  logic        macro_web  [0:NUM_MACROS-1];
  logic [63:0] macro_q    [0:NUM_MACROS-1];

  int unsigned w_lane;
  int unsigned w_row;
  int unsigned w_bank;
  int unsigned w_macro_addr;
  int unsigned r_lane;
  int unsigned r_row;
  int unsigned r_bank;
  int unsigned r_macro_addr;
  int unsigned b;
  int unsigned bit_base;
  int unsigned i;

`ifndef SYNTHESIS
  initial begin
    if ((MACRO_WIDTH % DATA_BITS) != 0) begin
      $error("SRAM parameter error: DATA_BITS=%0d must divide macro width %0d", DATA_BITS, MACRO_WIDTH);
      $fatal(1);
    end
    if ((DEPTH % WORDS_PER_MACRO_ROW) != 0) begin
      $error("SRAM parameter error: DEPTH=%0d must be divisible by WORDS_PER_MACRO_ROW=%0d", DEPTH, WORDS_PER_MACRO_ROW);
      $fatal(1);
    end
    if ((NUM_ROWS % MACRO_DEPTH) != 0) begin
      $error("SRAM parameter error: NUM_ROWS=%0d must be divisible by MACRO_DEPTH=%0d", NUM_ROWS, MACRO_DEPTH);
      $fatal(1);
    end
  end
`endif

  always_comb begin
    w_lane = 0;
    w_row = 0;
    w_bank = 0;
    w_macro_addr = 0;
    r_lane = 0;
    r_row = 0;
    r_bank = 0;
    r_macro_addr = 0;

    if (we) begin
      w_lane = waddr % WORDS_PER_MACRO_ROW;
      w_row = waddr / WORDS_PER_MACRO_ROW;
      w_bank = w_row / MACRO_DEPTH;
      w_macro_addr = w_row % MACRO_DEPTH;
    end

    if (re) begin
      r_lane = raddr % WORDS_PER_MACRO_ROW;
      r_row = raddr / WORDS_PER_MACRO_ROW;
      r_bank = r_row / MACRO_DEPTH;
      r_macro_addr = r_row % MACRO_DEPTH;
    end
  end

  always_comb begin
    for (b = 0; b < NUM_MACROS; b++) begin
      macro_a[b] = '0;
      macro_d[b] = '0;
      macro_bweb[b] = '1;
      macro_ceb[b] = 1'b1;
      macro_web[b] = 1'b1;
    end

    if (re) begin
      macro_ceb[r_bank] = 1'b0;
      macro_web[r_bank] = 1'b1;
      macro_a[r_bank] = r_macro_addr[6:0];
    end

    if (we) begin
      bit_base = w_lane * DATA_BITS;

      // Single-port macro: if read/write hit same bank in one cycle,
      // prioritize write command to keep macro image coherent.
      macro_ceb[w_bank] = 1'b0;
      macro_web[w_bank] = 1'b0;
      macro_a[w_bank] = w_macro_addr[6:0];
      macro_d[w_bank][bit_base +: DATA_BITS] = wdata;
      macro_bweb[w_bank][bit_base +: DATA_BITS] = '0;
    end
  end

  always_ff @(posedge clk) begin
    logic [MACRO_WIDTH-1:0] row_tmp;
    if (rst) begin
      rdata_reg <= '0;
      valid_bits <= '0;
      for (i = 0; i < NUM_ROWS; i++) begin
        row_shadow[i] <= '0;
      end
    end else begin
      if (we) begin
        row_tmp = row_shadow[w_row];
        row_tmp[(w_lane * DATA_BITS) +: DATA_BITS] = wdata;
        row_shadow[w_row] <= row_tmp;
        valid_bits[waddr] <= 1'b1;
      end

      if (re) begin
        // Match SystemC SRAM.h behavior: write updates memory first, then read.
        if (we && (waddr == raddr)) begin
          rdata_reg <= wdata;
        end else if (valid_bits[raddr]) begin
          rdata_reg <= row_shadow[r_row][(r_lane * DATA_BITS) +: DATA_BITS];
        end else begin
          rdata_reg <= '0;
        end
      end
    end
  end

  generate
    genvar gi;
    for (gi = 0; gi < NUM_MACROS; gi++) begin : gen_sram_macro
      TS1N16ADFPCLLLVTA128X64M4SWSHOD u_sram (
        .SLP(1'b0),
        .DSLP(1'b0),
        .SD(1'b0),
        .PUDELAY(),
        .CLK(clk),
        .CEB(macro_ceb[gi]),
        .WEB(macro_web[gi]),
        .A(macro_a[gi]),
        .D(macro_d[gi]),
        .BWEB(macro_bweb[gi]),
        .RTSEL(2'b00),
        .WTSEL(2'b00),
        .Q(macro_q[gi])
      );
    end
  endgenerate

  assign rdata = rdata_reg;
endmodule
