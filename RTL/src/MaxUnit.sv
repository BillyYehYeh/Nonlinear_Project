module MaxUnit (
  input  logic        clk,
  input  logic        rst_n,
  input  logic [15:0] A,
  input  logic [15:0] B,
  input  logic [15:0] C,
  input  logic [15:0] D,
  output logic [15:0] Max_Out
);
  // Local function: FP16 maximum (from sole_pkg::fp16_max)
  function automatic logic [15:0] fp16_max(input logic [15:0] a, input logic [15:0] b);
    logic a_sign, b_sign;
    logic [4:0] a_exp, b_exp;
    logic [9:0] a_mant, b_mant;
    begin
      a_sign = a[15];
      b_sign = b[15];
      a_exp = a[14:10];
      b_exp = b[14:10];
      a_mant = a[9:0];
      b_mant = b[9:0];
      
      // Different signs: positive > negative
      if (a_sign != b_sign) return (a_sign == 1'b0) ? a : b;
      
      // Same sign: compare exponent first, then mantissa
      if (a_sign == 1'b0) begin
        // Both positive
        if (a_exp > b_exp) return a;
        if (a_exp < b_exp) return b;
        return (a_mant >= b_mant) ? a : b;
      end else begin
        // Both negative: reverse comparison
        if (a_exp < b_exp) return a;
        if (a_exp > b_exp) return b;
        return (a_mant <= b_mant) ? a : b;
      end
    end
  endfunction

  logic [15:0] r1, r2;
  logic [15:0] n1, n2;

  always @* begin
    n1 = fp16_max(A, B);
    n2 = fp16_max(C, D);
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      r1 <= 16'h0000;
      r2 <= 16'h0000;
    end else begin
      r1 <= n1;
      r2 <= n2;
    end
  end

  always @* begin
    Max_Out = fp16_max(r1, r2);
  end
endmodule
