`ifndef SOLE_PKG_SV
`define SOLE_PKG_SV
package sole_pkg;
  parameter int AXI_ADDR_WIDTH = 32;
  parameter int AXI_DATA_WIDTH = 64;
  parameter int AXI_STRB_WIDTH = 8;
  parameter int DATA_LENGTH_MAX = 4096;
  parameter int FIFO_ADDR_BITS = 12;

  localparam logic [1:0] AXI_RESP_OKAY = 2'b00;
  localparam logic [1:0] AXI_RESP_SLVERR = 2'b10;

  localparam logic [7:0] REG_CONTROL         = 8'h00;
  localparam logic [7:0] REG_STATUS          = 8'h04;
  localparam logic [7:0] REG_SRC_ADDR_BASE_L = 8'h08;
  localparam logic [7:0] REG_SRC_ADDR_BASE_H = 8'h0C;
  localparam logic [7:0] REG_DST_ADDR_BASE_L = 8'h10;
  localparam logic [7:0] REG_DST_ADDR_BASE_H = 8'h14;
  localparam logic [7:0] REG_LENGTH_L        = 8'h18;
  localparam logic [7:0] REG_LENGTH_H        = 8'h1C;

  localparam int CTRL_START_BIT = 0;
  localparam int CTRL_MODE_BIT = 31;

  typedef enum logic [1:0] {
    STATE_IDLE = 2'd0,
    STATE_PROCESS1 = 2'd1,
    STATE_PROCESS2 = 2'd2,
    STATE_PROCESS3 = 2'd3
  } state_t;

  function automatic int clamp_int(input int v, input int lo, input int hi);
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
  endfunction

  function automatic int leading_one_pos32(input logic [31:0] x);
    int i;
    leading_one_pos32 = 0;
    for (i = 31; i >= 0; i--) begin
      if (x[i]) begin
        leading_one_pos32 = i;
        return leading_one_pos32;
      end
    end
  endfunction

`ifndef SYNTHESIS
  function automatic real fp16_to_real(input logic [15:0] h);
    int s;
    int e;
    int m;
    real val;
    s = h[15];
    e = h[14:10];
    m = h[9:0];

    if (e == 0) begin
      if (m == 0) begin
        val = 0.0;
      end else begin
        val = (m / 1024.0) * (2.0 ** -14);
      end
    end else if (e == 31) begin
      if (m == 0) begin
        val = 1.0e30;
      end else begin
        val = 0.0;
      end
    end else begin
      val = (1.0 + (m / 1024.0)) * (2.0 ** (e - 15));
    end

    if (s) val = -val;
    return val;
  endfunction

  function automatic logic [15:0] real_to_fp16(input real v);
    int sign;
    real a;
    int exp2;
    real norm;
    int mant;
    logic [4:0] exp_field;
    logic [9:0] mant_field;

    if (v == 0.0) return 16'h0000;

    sign = (v < 0.0);
    a = sign ? -v : v;

    exp2 = 0;
    norm = a;
    while (norm >= 2.0) begin
      norm = norm / 2.0;
      exp2++;
    end
    while (norm < 1.0) begin
      norm = norm * 2.0;
      exp2--;
    end

    if (exp2 > 15) begin
      return {logic'(sign), 5'h1F, 10'h000};
    end

    if (exp2 < -14) begin
      if (exp2 < -24) begin
        return {logic'(sign), 5'h00, 10'h000};
      end

      mant = $rtoi(a * (2.0 ** 24) + 0.5);
      if (mant <= 0) begin
        return {logic'(sign), 5'h00, 10'h000};
      end
      if (mant >= 1024) begin
        return {logic'(sign), 5'h01, 10'h000};
      end
      return {logic'(sign), 5'h00, logic'(mant[9:0])};
    end

    exp_field = exp2 + 15;
    mant = $rtoi((norm - 1.0) * 1024.0 + 0.5);
    if (mant >= 1024) begin
      mant = 0;
      exp_field = exp_field + 1;
      if (exp_field >= 31) begin
        return {logic'(sign), 5'h1F, 10'h000};
      end
    end
    mant_field = mant[9:0];
    return {logic'(sign), exp_field, mant_field};
  endfunction

  function automatic logic [15:0] fp16_from_small_real(input real v);
    return real_to_fp16(v);
  endfunction
`endif

  function automatic logic fp16_is_nan(input logic [15:0] h);
    fp16_is_nan = (h[14:10] == 5'h1F) && (h[9:0] != 10'h000);
  endfunction

  function automatic logic fp16_is_zero(input logic [15:0] h);
    fp16_is_zero = (h[14:0] == 15'h0000);
  endfunction

  function automatic logic fp16_mag_gt(input logic [15:0] a, input logic [15:0] b);
    if (a[14:10] > b[14:10]) return 1'b1;
    if (a[14:10] < b[14:10]) return 1'b0;
    return (a[9:0] > b[9:0]);
  endfunction

  function automatic logic fp16_gt(input logic [15:0] a, input logic [15:0] b);
    logic a_nan;
    logic b_nan;
    logic a_sign;
    logic b_sign;
    logic a_zero;
    logic b_zero;
    begin
      a_nan = fp16_is_nan(a);
      b_nan = fp16_is_nan(b);

      if (a_nan && b_nan) return 1'b0;
      if (a_nan) return 1'b0;
      if (b_nan) return 1'b1;

      a_zero = fp16_is_zero(a);
      b_zero = fp16_is_zero(b);
      if (a_zero && b_zero) return 1'b0;

      a_sign = a[15];
      b_sign = b[15];
      if (a_sign != b_sign) return (a_sign == 1'b0);

      if (!a_sign) begin
        return fp16_mag_gt(a, b);
      end else begin
        return fp16_mag_gt(b, a);
      end
    end
  endfunction

  function automatic logic [15:0] fp16_max(input logic [15:0] a, input logic [15:0] b);
    if (fp16_gt(a, b)) return a;
    return b;
  endfunction
endpackage
`endif
