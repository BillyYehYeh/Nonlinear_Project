`timescale 1ns/1ps

module PROCESS_2_test;
  logic        clk;
  logic        rst;
  logic        enable;
  logic        stall_output;
  logic [31:0] Pre_Compute_In;
  logic [3:0]  Leading_One_Pos_Out;
  logic [15:0] Mux_Result_Out;

  int test_count;
  int pass_count;

  PROCESS_2_Module dut (
    .clk(clk),
    .rst_n(!rst),
    .enable(enable),
    .stall_output(stall_output),
    .Pre_Compute_In(Pre_Compute_In),
    .Leading_One_Pos_Out(Leading_One_Pos_Out),
    .Mux_Result_Out(Mux_Result_Out)
  );

  function automatic [3:0] expected_lo(input [31:0] v);
    reg [15:0] integer_part;
    integer i;
    begin
      integer_part = v[31:16];
      if (integer_part == 16'h0000) begin
        expected_lo = 4'd0;
      end else begin
        expected_lo = 4'd0;
        for (i = 15; i >= 0; i = i - 1) begin
          if (integer_part[i]) begin
            expected_lo = i[3:0];
            break;
          end
        end
      end
    end
  endfunction

  function automatic [15:0] expected_mux(input [31:0] v);
    reg [3:0] lo;
    integer bit_pos;
    begin
      lo = expected_lo(v);
      bit_pos = 15 + lo;
      if (v[bit_pos]) expected_mux = 16'h3A8B;
      else expected_mux = 16'h388B;
    end
  endfunction

  task automatic check_now(input int id, input [3:0] exp_lo, input [15:0] exp_mux, input [255:0] msg);
    begin
      test_count = test_count + 1;
      if ((Leading_One_Pos_Out !== exp_lo) || (Mux_Result_Out !== exp_mux)) begin
        $display("[FAIL] case=%0d %0s exp_lo=%0d got_lo=%0d exp_mux=0x%04h got_mux=0x%04h",
                 id, msg, exp_lo, Leading_One_Pos_Out, exp_mux, Mux_Result_Out);
        $fatal(1);
      end else begin
        pass_count = pass_count + 1;
        $display("[PASS] case=%0d %0s lo=%0d mux=0x%04h", id, msg, Leading_One_Pos_Out, Mux_Result_Out);
      end
    end
  endtask

  initial begin
    clk = 0;
    rst = 0;
    enable = 1;
    stall_output = 0;
    Pre_Compute_In = 0;
    test_count = 0;
    pass_count = 0;

    // Reset
    rst = 1;
    #1;
    check_now(1, 4'd0, 16'h0000, "reset");

    rst = 0;
    Pre_Compute_In = 32'h0004_0000; // lo=2 mux=388b
    stall_output = 0;
    #1;
    check_now(2, expected_lo(Pre_Compute_In), expected_mux(Pre_Compute_In), "normal update");

    // Stall: input changes but output holds previous value
    stall_output = 1;
    Pre_Compute_In = 32'h0006_0000; // would be lo=2 mux=3a8b if not stalled
    #1;
    check_now(3, 4'd2, 16'h388B, "stall hold");

    // Release stall: output updates to current input-derived values
    stall_output = 0;
    #1;
    check_now(4, expected_lo(Pre_Compute_In), expected_mux(Pre_Compute_In), "stall release update");

    // Disable: hold outputs
    enable = 0;
    Pre_Compute_In = 32'h0008_0000;
    #1;
    check_now(5, 4'd2, 16'h3A8B, "enable low hold");

    $display("[SUMMARY] pass=%0d total=%0d", pass_count, test_count);
    if (pass_count != test_count) $fatal(1);
    $display("[PROCESS_2 RTL TEST] PASS");
    $finish;
  end

  always #5 clk = ~clk;

endmodule
