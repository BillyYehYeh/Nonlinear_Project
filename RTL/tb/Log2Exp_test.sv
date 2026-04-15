`timescale 1ns/1ps

module Log2Exp_test;
  logic [15:0] fp16_in;
  logic [3:0]  result_out;

  int test_count;
  int pass_count;

  Log2Exp dut (
    .fp16_in(fp16_in),
    .result_out(result_out)
  );

  function automatic [3:0] expected_log2exp(input [15:0] v);
    reg [4:0] exp_bits;
    reg [9:0] mant_bits;
    integer exponent_val;
    reg [17:0] mant_val;
    reg [17:0] op1;
    reg [17:0] op2;
    reg [17:0] op3;
    reg [17:0] sum;
    integer actual_exp_val;
    reg [17:0] shifted_value;
    reg [3:0] shift_res;
    begin
      exp_bits = v[14:10];
      mant_bits = v[9:0];
      exponent_val = exp_bits;

      mant_val = ((18'd1 << 14) | ({8'd0, mant_bits} << 4)) & 18'h3FFFF;
      op1 = mant_val;
      op2 = mant_val >> 1;
      op3 = mant_val >> 4;
      sum = op1 + op2 - op3;

      actual_exp_val = exponent_val - 15;
      if (actual_exp_val <= 0) shifted_value = sum >> (-actual_exp_val);
      else shifted_value = sum << actual_exp_val;

      shift_res = shifted_value[17:14];

      if (exponent_val <= 13) expected_log2exp = 4'h0;
      else if (exponent_val >= 19) expected_log2exp = 4'hF;
      else if ((exponent_val == 18) && (sum[15] == 1'b1)) expected_log2exp = 4'hF;
      else expected_log2exp = shift_res;
    end
  endfunction

  task automatic run_one(input int id, input [15:0] in_v);
    reg [3:0] exp_out;
    begin
      fp16_in = in_v;
      #1;

      exp_out = expected_log2exp(in_v);
      test_count = test_count + 1;

      if (result_out !== exp_out) begin
        $display("[FAIL] case=%0d in=0x%04h exp=%0d got=%0d", id, in_v, exp_out, result_out);
        $fatal(1);
      end else begin
        pass_count = pass_count + 1;
        $display("[PASS] case=%0d in=0x%04h out=%0d", id, in_v, result_out);
      end
    end
  endtask

  initial begin
    test_count = 0;
    pass_count = 0;

    run_one(1,  16'hBC00);
    run_one(2,  16'hB800);
    run_one(3,  16'hB400);
    run_one(4,  16'hB000);
    run_one(5,  16'hAC00);
    run_one(6,  16'hA800);
    run_one(7,  16'hB17F);
    run_one(8,  16'hAD00);
    run_one(9,  16'hBBEB);
    run_one(10, 16'hB999);
    run_one(11, 16'hB985);
    run_one(12, 16'hD100);

    $display("[SUMMARY] pass=%0d total=%0d", pass_count, test_count);
    if (pass_count != test_count) $fatal(1);
    $display("[Log2Exp RTL TEST] PASS");
    $finish;
  end

endmodule
