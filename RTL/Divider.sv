`include "sole_pkg.sv"
module Divider_Module (
  input  logic [3:0]  ky,
  input  logic [3:0]  ks,
  input  logic [15:0] Mux_Result,
  output logic [15:0] Divider_Output
);
  int diff;
  int exp0;
  int exp1;
  logic [4:0] e;

  always @* begin
    e = Mux_Result[14:10];
    diff = $signed({1'b0, ky}) - $signed({1'b0, ks});
    exp0 = $signed({1'b0, e}) - diff;
    exp1 = sole_pkg::clamp_int(exp0, 0, 31);
    Divider_Output = {Mux_Result[15], logic'(exp1[4:0]), Mux_Result[9:0]};
  end
endmodule
