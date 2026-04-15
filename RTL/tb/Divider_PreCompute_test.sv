`timescale 1ns/1ps

module Divider_PreCompute_test;
  logic [31:0] input_data;
  logic [3:0]  Leading_One_Pos;
  logic [15:0] Mux_Result;

  int test_count;
  int pass_count;

  Divider_PreCompute_Module dut (
    .input_data(input_data),
    .Leading_One_Pos(Leading_One_Pos),
    .Mux_Result(Mux_Result)
  );

  function automatic [3:0] expected_leading_pos(input logic [31:0] v);
    logic [15:0] integer_part;
    int i;
    begin
      integer_part = v[31:16];
      if (integer_part == 16'h0000) begin
        expected_leading_pos = 4'd0;
      end else begin
        expected_leading_pos = 4'd0;
        for (i = 15; i >= 0; i = i - 1) begin
          if (integer_part[i]) begin
            expected_leading_pos = i[3:0];
            break;
          end
        end
      end
    end
  endfunction

  function automatic [15:0] expected_mux(input logic [31:0] v);
    logic [3:0] lop;
    int bit_pos;
    begin
      lop = expected_leading_pos(v);
      bit_pos = 15 + lop;
      if (v[bit_pos]) expected_mux = 16'h3A8B;
      else expected_mux = 16'h388B;
    end
  endfunction

  task automatic run_one(
    input int id,
    input logic [31:0] in_v,
    input logic [3:0] exp_pos,
    input logic [15:0] exp_mux
  );
    begin
      input_data = in_v;
      #1;

      test_count = test_count + 1;
      if ((Leading_One_Pos !== exp_pos) || (Mux_Result !== exp_mux)) begin
        $display("[FAIL] case=%0d input=0x%08h exp_pos=%0d got_pos=%0d exp_mux=0x%04h got_mux=0x%04h",
                 id, in_v, exp_pos, Leading_One_Pos, exp_mux, Mux_Result);
        $fatal(1);
      end else begin
        pass_count = pass_count + 1;
        $display("[PASS] case=%0d input=0x%08h pos=%0d mux=0x%04h", id, in_v, Leading_One_Pos, Mux_Result);
      end
    end
  endtask

  initial begin
    test_count = 0;
    pass_count = 0;

    run_one(1,  32'h0001_0000, 4'd0, 16'h388B);
    run_one(2,  32'h0001_8000, 4'd0, 16'h3A8B);
    run_one(3,  32'h0002_0000, 4'd1, 16'h388B);
    run_one(4,  32'h0003_0000, 4'd1, 16'h3A8B);
    run_one(5,  32'h0004_0000, 4'd2, 16'h388B);
    run_one(6,  32'h0006_0000, 4'd2, 16'h3A8B);
    run_one(7,  32'h0008_0000, 4'd3, 16'h388B);
    run_one(8,  32'h000C_0000, 4'd3, 16'h3A8B);
    run_one(9,  32'h00FF_0000, 4'd7, 16'h3A8B);
    run_one(10, 32'h0080_0000, 4'd7, 16'h388B);
    run_one(11, 32'h0100_0000, 4'd8, 16'h388B);
    run_one(12, 32'h0180_0000, 4'd8, 16'h3A8B);
    run_one(13, 32'h4000_0000, 4'd14, 16'h388B);
    run_one(14, 32'h6000_0000, 4'd14, 16'h3A8B);
    run_one(15, 32'h8000_0000, 4'd15, 16'h388B);
    run_one(16, 32'hC000_0000, 4'd15, 16'h3A8B);
    run_one(17, 32'h0000_0001, 4'd0, 16'h388B);
    run_one(18, 32'h0000_8000, 4'd0, 16'h3A8B);
    run_one(19, 32'h0000_0000, 4'd0, 16'h388B);

    $display("[SUMMARY] pass=%0d total=%0d", pass_count, test_count);
    if (pass_count != test_count) begin
      $fatal(1);
    end
    $display("[Divider_PreCompute RTL TEST] PASS");
    $finish;
  end

endmodule
