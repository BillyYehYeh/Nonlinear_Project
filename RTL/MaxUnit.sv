`include "sole_pkg.sv"
module MaxUnit (
  input  logic        clk,
  input  logic        rst,
  input  logic [15:0] A,
  input  logic [15:0] B,
  input  logic [15:0] C,
  input  logic [15:0] D,
  output logic [15:0] Max_Out
);
  logic [15:0] r1, r2;
  logic [15:0] n1, n2;

  always @* begin
    n1 = sole_pkg::fp16_max(A, B);
    n2 = sole_pkg::fp16_max(C, D);
  end

  always @(posedge clk) begin
    if (rst) begin
      r1 <= 16'h0000;
      r2 <= 16'h0000;
    end else begin
      r1 <= n1;
      r2 <= n2;
    end
  end

  always @* begin
    Max_Out = sole_pkg::fp16_max(r1, r2);
  end
endmodule
