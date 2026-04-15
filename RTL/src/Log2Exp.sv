module Log2Exp (
  input  logic [15:0] fp16_in,
  output logic [3:0]  result_out
);
  logic [4:0] exp_bits;
  logic [9:0] mant_bits;
  int exponent_val;

  logic [17:0] mant_val;
  logic [17:0] op1;
  logic [17:0] op2;
  logic [17:0] op3;
  logic [17:0] sum;

  int actual_exp_val;
  logic [17:0] shifted_value;
  logic [3:0] shift_res;
  logic [3:0] output_val;

  always @* begin
    exp_bits = fp16_in[14:10];
    mant_bits = fp16_in[9:0];
    exponent_val = exp_bits;

    mant_val = ((18'd1 << 14) | ({8'd0, mant_bits} << 4)) & 18'h3FFFF;
    op1 = mant_val;
    op2 = mant_val >> 1;
    op3 = mant_val >> 4;

    sum = op1 + op2 - op3;

    actual_exp_val = exponent_val - 15;
    if (actual_exp_val <= 0) begin
      shifted_value = sum >> (-actual_exp_val);
    end else begin
      shifted_value = sum << actual_exp_val;
    end

    shift_res = shifted_value[17:14];

    if (exponent_val <= 13) begin
      output_val = 4'h0;
    end else if (exponent_val >= 19) begin
      output_val = 4'hF;
    end else if ((exponent_val == 18) && (sum[15] == 1'b1)) begin
      output_val = 4'hF;
    end else begin
      output_val = shift_res;
    end

    result_out = output_val;
  end
endmodule
