`ifndef UTILS_SV
`define UTILS_SV

package utils_pkg;
`ifndef SYNTHESIS
  function automatic logic [15:0] fp16_add(input logic [15:0] a, input logic [15:0] b);
    logic sign_res;
    logic [15:0] frac;
    logic [31:0] sig_a;
    logic [31:0] sig_b;
    logic [31:0] sig_res;
    logic [31:0] sig_main;
    int exp_res;
    int diff;
    int i;
    logic [31:0] sticky_local;
    logic [31:0] shifted_out_mask;
    logic [31:0] shifted_out;
    logic [31:0] guard;
    logic [31:0] round_bit;
    logic [31:0] sticky;
    logic increment;
    logic sign_a;
    logic sign_b;
    logic [4:0] exp_a;
    logic [4:0] exp_b;
    logic [9:0] mant_a;
    logic [9:0] mant_b;
    logic a_is_nan;
    logic b_is_nan;
    logic a_is_inf;
    logic b_is_inf;
    logic a_is_zero;
    logic b_is_zero;
    begin
      sign_a = a[15];
      sign_b = b[15];
      exp_a  = a[14:10];
      exp_b  = b[14:10];
      mant_a = a[9:0];
      mant_b = b[9:0];

      a_is_nan = (exp_a == 5'h1F) && (mant_a != 10'h000);
      b_is_nan = (exp_b == 5'h1F) && (mant_b != 10'h000);
      if (a_is_nan) return a;
      if (b_is_nan) return b;

      a_is_inf = (exp_a == 5'h1F) && (mant_a == 10'h000);
      b_is_inf = (exp_b == 5'h1F) && (mant_b == 10'h000);
      if (a_is_inf && b_is_inf) begin
        if (sign_a != sign_b) return 16'hFE00;
        return a;
      end
      if (a_is_inf) return a;
      if (b_is_inf) return b;

      a_is_zero = (exp_a == 5'd0) && (mant_a == 10'd0);
      b_is_zero = (exp_b == 5'd0) && (mant_b == 10'd0);
      if (a_is_zero && b_is_zero) return {(sign_a & sign_b), 15'h0000};
      if (a_is_zero) return b;
      if (b_is_zero) return a;

      // Match SystemC behavior: subnormal inputs are treated as zero.
      if (exp_a == 5'd0) return b;
      if (exp_b == 5'd0) return a;

      sig_a = {21'd0, 1'b1, mant_a} << 3;
      sig_b = {21'd0, 1'b1, mant_b} << 3;

      exp_res = exp_a;
      diff = $signed({1'b0, exp_a}) - $signed({1'b0, exp_b});
      if (diff > 0) begin
        exp_res = exp_a;
        if (diff > 25) begin
          sig_b = 32'd1;
        end else begin
          sticky_local = 32'd0;
          shifted_out_mask = (32'd1 << diff) - 32'd1;
          shifted_out = sig_b & shifted_out_mask;
          if (shifted_out != 0) sticky_local = 32'd1;
          sig_b = sig_b >> diff;
          sig_b = sig_b | sticky_local;
        end
      end else if (diff < 0) begin
        diff = -diff;
        exp_res = exp_b;
        if (diff > 25) begin
          sig_a = 32'd1;
        end else begin
          sticky_local = 32'd0;
          shifted_out_mask = (32'd1 << diff) - 32'd1;
          shifted_out = sig_a & shifted_out_mask;
          if (shifted_out != 0) sticky_local = 32'd1;
          sig_a = sig_a >> diff;
          sig_a = sig_a | sticky_local;
        end
      end

      if (sign_a == sign_b) begin
        sig_res = sig_a + sig_b;
        sign_res = sign_a;
        if (sig_res & (32'd1 << (11 + 3))) begin
          sig_res = sig_res >> 1;
          exp_res = exp_res + 1;
          if (exp_res >= 31) return {sign_res, 5'h1F, 10'h000};
        end
      end else begin
        if ((sig_a > sig_b) || ((sig_a == sig_b) && (exp_a >= exp_b))) begin
          sig_res = sig_a - sig_b;
          sign_res = sign_a;
        end else begin
          sig_res = sig_b - sig_a;
          sign_res = sign_b;
        end
        if (sig_res == 0) return 16'h0000;

        for (i = 0; i < 14; i = i + 1) begin
          if (((sig_res & (32'h400 << 3)) == 0) && (exp_res > 0)) begin
            sig_res = sig_res << 1;
            exp_res = exp_res - 1;
          end
        end
        if (exp_res <= 0) return {sign_res, 15'h0000};
      end

      sig_main = sig_res >> 3;
      guard = (sig_res >> 2) & 32'd1;
      round_bit = (sig_res >> 1) & 32'd1;
      sticky = sig_res & 32'd1;

      increment = 1'b0;
      if (guard != 0) begin
        if ((round_bit != 0) || (sticky != 0) || ((sig_main & 32'd1) != 0)) increment = 1'b1;
      end
      if (increment) begin
        sig_main = sig_main + 1;
        if (sig_main == (32'd1 << 11)) begin
          sig_main = sig_main >> 1;
          exp_res = exp_res + 1;
          if (exp_res >= 31) return {sign_res, 5'h1F, 10'h000};
        end
      end

      frac = sig_main & 32'h3FF;
      if (exp_res <= 0) return {sign_res, 15'h0000};
      return {sign_res, exp_res[4:0], frac[9:0]};
    end
  endfunction

  function automatic logic [15:0] fp16_subtract(input logic [15:0] a, input logic [15:0] b);
    begin
      fp16_subtract = fp16_add(a, {~b[15], b[14:0]});
    end
  endfunction
`endif
endpackage

`endif
