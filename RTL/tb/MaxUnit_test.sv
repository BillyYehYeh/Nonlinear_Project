`timescale 1ns/1ps

module MaxUnit_test;
  logic        clk;
  logic        rst;
  logic [15:0] A, B, C, D;
  logic [15:0] Max_Out;

  int test_count;
  int pass_count;

  MaxUnit dut (
    .clk(clk),
    .rst(rst),
    .A(A), .B(B), .C(C), .D(D),
    .Max_Out(Max_Out)
  );

  function automatic [15:0] fp16_max_ref(input [15:0] a, input [15:0] b);
    reg a_sign, b_sign;
    reg [4:0] a_exp, b_exp;
    reg [9:0] a_mant, b_mant;
    begin
      a_sign = a[15]; b_sign = b[15];
      a_exp = a[14:10]; b_exp = b[14:10];
      a_mant = a[9:0]; b_mant = b[9:0];
      if (a_sign != b_sign) fp16_max_ref = (a_sign == 1'b0) ? a : b;
      else if (a_sign == 1'b0) begin
        if (a_exp > b_exp) fp16_max_ref = a;
        else if (a_exp < b_exp) fp16_max_ref = b;
        else fp16_max_ref = (a_mant >= b_mant) ? a : b;
      end else begin
        if (a_exp < b_exp) fp16_max_ref = a;
        else if (a_exp > b_exp) fp16_max_ref = b;
        else fp16_max_ref = (a_mant <= b_mant) ? a : b;
      end
    end
  endfunction

  function automatic [15:0] expected_max4(input [15:0] a, input [15:0] b, input [15:0] c, input [15:0] d);
    begin
      expected_max4 = fp16_max_ref(fp16_max_ref(a, b), fp16_max_ref(c, d));
    end
  endfunction

  task automatic run_case(input int id, input [15:0] a, input [15:0] b, input [15:0] c, input [15:0] d);
    reg [15:0] exp;
    begin
      A = a; B = b; C = c; D = d;
      exp = expected_max4(a, b, c, d);
      @(posedge clk);
      @(posedge clk);
      #1;
      test_count = test_count + 1;
      if (Max_Out !== exp) begin
        $display("[FAIL] case=%0d exp=0x%04h got=0x%04h (A=%04h B=%04h C=%04h D=%04h)",
                 id, exp, Max_Out, a, b, c, d);
        $fatal(1);
      end else begin
        pass_count = pass_count + 1;
        $display("[PASS] case=%0d out=0x%04h", id, Max_Out);
      end
    end
  endtask

  always #5 clk = ~clk;

  initial begin
    clk = 0;
    rst = 1;
    A = 0; B = 0; C = 0; D = 0;
    test_count = 0;
    pass_count = 0;

    @(posedge clk);
    #1;
    if (Max_Out !== 16'h0000) begin
      $display("[FAIL] reset exp=0000 got=%04h", Max_Out);
      $fatal(1);
    end
    rst = 0;

    run_case(1, 16'h4100, 16'h4200, 16'h3D00, 16'h4000);
    run_case(2, 16'hC400, 16'hC600, 16'h4400, 16'h3C00);
    run_case(3, 16'hBC00, 16'hC000, 16'hB800, 16'hC400);
    run_case(4, 16'hC800, 16'h4080, 16'hBE00, 16'h4100);
    run_case(5, 16'h3C00, 16'h3C00, 16'h3C00, 16'h3C00);

    $display("[SUMMARY] pass=%0d total=%0d", pass_count, test_count);
    if (pass_count != test_count) $fatal(1);
    $display("[MaxUnit RTL TEST] PASS");
    $finish;
  end
endmodule
