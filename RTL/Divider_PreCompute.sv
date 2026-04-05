`include "sole_pkg.sv"
module Divider_PreCompute_Module (
  input  logic [31:0] input_data,
  output logic [3:0]  Leading_One_Pos,
  output logic [15:0] Mux_Result
);
  int lop;
  logic bit_before;
  always @* begin
    lop = sole_pkg::leading_one_pos32(input_data);
    Leading_One_Pos = lop[3:0];
    if (lop > 0) bit_before = input_data[lop-1];
    else bit_before = 1'b0;
    Mux_Result = bit_before ? 16'h3A8B : 16'h388B;
  end
endmodule
