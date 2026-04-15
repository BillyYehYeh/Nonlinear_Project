`timescale 1ns/1ps

module Divider_test;
  logic [31:0] input_sig;
  logic [3:0]  ky_sig;
  logic [3:0]  ks_sig;
  logic [15:0] mux_sig;
  logic [15:0] out_sig;

  int test_count;
  int pass_count;

  Divider_PreCompute_Module u_pre (
    .input_data(input_sig),
    .Leading_One_Pos(ks_sig),
    .Mux_Result(mux_sig)
  );

  Divider_Module u_div (
    .ky(ky_sig),
    .ks(ks_sig),
    .Mux_Result(mux_sig),
    .Divider_Output(out_sig)
  );

  function automatic [4:0] expected_new_exp(input [4:0] exp_in, input [3:0] ky, input [3:0] ks);
    integer v;
    begin
      v = exp_in - (ky + ks);
      if (v < 0) v = 0;
      else if (v > 31) v = 31;
      expected_new_exp = v[4:0];
    end
  endfunction

  function automatic [15:0] expected_divider(input [15:0] mux_in, input [3:0] ky, input [3:0] ks);
    reg [4:0] e;
    begin
      e = expected_new_exp(mux_in[14:10], ky, ks);
      expected_divider = {mux_in[15], e, mux_in[9:0]};
    end
  endfunction

  task automatic run_one(input int id, input [31:0] in_v, input [3:0] ky_v);
    reg [15:0] exp_out;
    begin
      input_sig = in_v;
      ky_sig = ky_v;
      #1;

      exp_out = expected_divider(mux_sig, ky_sig, ks_sig);

      test_count = test_count + 1;
      if (out_sig !== exp_out) begin
        $display("[FAIL] case=%0d input=0x%08h ky=%0d ks=%0d mux=0x%04h exp=0x%04h got=0x%04h",
                 id, input_sig, ky_sig, ks_sig, mux_sig, exp_out, out_sig);
        $fatal(1);
      end else begin
        pass_count = pass_count + 1;
        $display("[PASS] case=%0d input=0x%08h ky=%0d ks=%0d mux=0x%04h out=0x%04h",
                 id, input_sig, ky_sig, ks_sig, mux_sig, out_sig);
      end
    end
  endtask

  initial begin
    test_count = 0;
    pass_count = 0;

    run_one(1,  32'h0002_0000, 4'd2);
    run_one(2,  32'h0004_0000, 4'd1);
    run_one(3,  32'h0004_0000, 4'd2);
    run_one(4,  32'h0008_0000, 4'd3);
    run_one(5,  32'h000C_0000, 4'd4);
    run_one(6,  32'h00FF_0000, 4'd7);
    run_one(7,  32'h0100_0000, 4'd8);
    run_one(8,  32'h0180_0000, 4'd9);
    run_one(9,  32'h4000_0000, 4'd14);
    run_one(10, 32'h8000_0000, 4'd15);

    $display("[SUMMARY] pass=%0d total=%0d", pass_count, test_count);
    if (pass_count != test_count) $fatal(1);
    $display("[Divider RTL TEST] PASS");
    $finish;
  end

endmodule
