`timescale 1ns/1ps

module Reduction_test;
  logic        clk;
  logic        rst;
  logic [15:0] Input_Vector;
  logic [31:0] Output_Sum;

  int test_count;
  int pass_count;

  Reduction_Module dut (
    .clk(clk),
    .rst(rst),
    .Input_Vector(Input_Vector),
    .Output_Sum(Output_Sum)
  );

  function automatic [31:0] expected_sum(input [15:0] v);
    reg [31:0] e0, e1, e2, e3;
    begin
      e0 = 32'h0001_0000 >> v[3:0];
      e1 = 32'h0001_0000 >> v[7:4];
      e2 = 32'h0001_0000 >> v[11:8];
      e3 = 32'h0001_0000 >> v[15:12];
      expected_sum = e0 + e1 + e2 + e3;
    end
  endfunction

  task automatic run_case(input int id, input [15:0] vec);
    reg [31:0] exp;
    begin
      exp = expected_sum(vec);
      Input_Vector = vec;
      @(posedge clk);
      #1;
      test_count = test_count + 1;
      if (Output_Sum !== exp) begin
        $display("[FAIL] case=%0d in=0x%04h exp=0x%08h got=0x%08h", id, vec, exp, Output_Sum);
        $fatal(1);
      end else begin
        pass_count = pass_count + 1;
        $display("[PASS] case=%0d in=0x%04h out=0x%08h", id, vec, Output_Sum);
      end
    end
  endtask

  always #5 clk = ~clk;

  initial begin
    clk = 0;
    rst = 1;
    Input_Vector = 16'h0000;
    test_count = 0;
    pass_count = 0;

    @(posedge clk);
    #1;
    if (Output_Sum !== 32'd0) begin
      $display("[FAIL] reset exp=0 got=0x%08h", Output_Sum);
      $fatal(1);
    end

    rst = 0;

    run_case(1, 16'h0000);
    run_case(2, 16'h3120);
    run_case(3, 16'h1111);
    run_case(4, 16'h7654);
    run_case(5, 16'h0123);
    run_case(6, 16'hFEDC);

    $display("[SUMMARY] pass=%0d total=%0d", pass_count, test_count);
    if (pass_count != test_count) $fatal(1);
    $display("[Reduction RTL TEST] PASS");
    $finish;
  end
endmodule
