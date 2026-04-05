`include "sole_pkg.sv"
module Log2Exp (
  input  logic [15:0] fp16_in,
  output logic [3:0]  result_out
);
  logic sign;
  logic [4:0] exp;
  logic [9:0] mant;
  logic [10:0] sig11;
  integer abs_trunc;
  integer u;
  always @* begin
    sign = fp16_in[15];
    exp = fp16_in[14:10];
    mant = fp16_in[9:0];
    abs_trunc = 0;

    if (exp == 5'd31) begin
      abs_trunc = 15;
    end else if (exp >= 5'd15) begin
      u = exp - 15;
      sig11 = {1'b1, mant};
      if (u >= 10) begin
        if ((u - 10) >= 5) abs_trunc = 15;
        else abs_trunc = sig11 <<< (u - 10);
      end else begin
        abs_trunc = sig11 >>> (10 - u);
      end
    end

    // n = trunc(-x), then clamp 0..15.
    if (sign) begin
      if (abs_trunc > 15) result_out = 4'd15;
      else result_out = abs_trunc[3:0];
    end else begin
      result_out = 4'd0;
    end
  end
endmodule
