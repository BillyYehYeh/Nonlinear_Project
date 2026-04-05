module Reduction_Module (
  input  logic        clk,
  input  logic        rst,
  input  logic [15:0] Input_Vector,
  output logic [31:0] Output_Sum
);
  logic [31:0] e0, e1, e2, e3;
  logic [31:0] s0, s1;

  always @* begin
    e0 = 32'd65536 >> Input_Vector[3:0];
    e1 = 32'd65536 >> Input_Vector[7:4];
    e2 = 32'd65536 >> Input_Vector[11:8];
    e3 = 32'd65536 >> Input_Vector[15:12];
    s0 = e0 + e1;
    s1 = e2 + e3;
  end

  always @(posedge clk) begin
    if (rst) Output_Sum <= 32'd0;
    else Output_Sum <= s0 + s1;
  end
endmodule
