module Divider_Module (
  input  logic [3:0]  ky,
  input  logic [3:0]  ks,
  input  logic [15:0] Mux_Result,
  output logic [15:0] Divider_Output
);
  int ky_signed;
  int ks_signed;
  int right_shift;
  logic [4:0] exponent;
  int exp_val;
  int new_exp_val;
  logic sign;
  logic [9:0] mantissa;
  logic [15:0] output_val;

  always @* begin
    ky_signed = ky;
    ks_signed = ks;
    right_shift = ky_signed + ks_signed;

    exponent = Mux_Result[14:10];
    exp_val = exponent;

    new_exp_val = exp_val - right_shift;
    if (new_exp_val < 0) begin
      new_exp_val = 0;
    end else if (new_exp_val > 31) begin
      new_exp_val = 31;
    end

    sign = Mux_Result[15];
    mantissa = Mux_Result[9:0];

    output_val = {sign, new_exp_val[4:0], mantissa};
    Divider_Output = output_val;
  end
endmodule
