module PROCESS_1_Module (
  input  logic        clk,
  input  logic        rst,
  input  logic        enable,
  input  logic [63:0] DataIn_64bits,
  input  logic [15:0] Global_Max,
  input  logic [31:0] Sum_Buffer_In,
  input  logic        data_valid,
  output logic [15:0] Power_of_Two_Vector,
  output logic [31:0] Sum_Buffer_Update,
  output logic [15:0] Local_Max_Output,
  output logic        stage1_valid,
  output logic        stage5_valid
);
  import utils_pkg::*;

  // ---------------- Internal module connections ----------------
  logic [15:0] datain_unpacked [0:3];
  logic [15:0] max_out_comb;
  logic [15:0] reduction_in;
  logic [31:0] reduction_output;
  logic [15:0] log2exp_in [0:5-1];
  logic [3:0]  log2exp_out [0:5-1];

  // ---------------- Pipeline stage registers ----------------
  logic [15:0] stage1_datain_reg [0:3];
  logic        stage1_valid_reg;

  logic [15:0] stage2_datain_reg [0:3];
  logic [15:0] stage2_max_reg;
  logic        stage2_valid_reg;

  logic [15:0] stage3_diff_reg [0:4];
  logic        stage3_valid_reg;

  logic [3:0]  stage4_power_reg [0:4];
  logic        stage4_valid_reg;

  logic [15:0] stage5_pow_vec_reg;
  logic [3:0]  stage5_shift_reg;
  logic        stage5_valid_reg;

  // ---------------- Pipeline next-state signals ----------------
  logic [15:0] stage1_datain_next [0:3];
  logic        stage1_valid_next;

  logic [15:0] stage2_datain_next [0:3];
  logic [15:0] stage2_max_next;
  logic        stage2_valid_next;

  logic [15:0] stage3_diff_next [0:4];
  logic        stage3_valid_next;

  logic [3:0]  stage4_power_next [0:4];
  logic        stage4_valid_next;

  logic [15:0] stage5_pow_vec_next;
  logic [3:0]  stage5_shift_next;
  logic        stage5_valid_next;

  logic [31:0] shifted_value;
  logic [31:0] final_result;

`ifndef SYNTHESIS
  // initial begin
  //   $display("[DBG P1 SUBCHK] 4500-4800=%h 4600-4800=%h 4700-4800=%h 4800-4800=%h",
  //            fp16_subtract(16'h4500, 16'h4800),
  //            fp16_subtract(16'h4600, 16'h4800),
  //            fp16_subtract(16'h4700, 16'h4800),
  //            fp16_subtract(16'h4800, 16'h4800));
  // end
`endif

  // ---------------- Submodule instantiations ----------------
  MaxUnit u_max_unit (
    .clk(clk),
    .rst(rst),
    .A(datain_unpacked[0]),
    .B(datain_unpacked[1]),
    .C(datain_unpacked[2]),
    .D(datain_unpacked[3]),
    .Max_Out(max_out_comb)
  );

  Reduction_Module u_reduction_unit (
    .clk(clk),
    .rst(rst),
    .Input_Vector(reduction_in),
    .Output_Sum(reduction_output)
  );

  genvar gi;
  generate
    for (gi = 0; gi < 5; gi++) begin : GEN_LOG2EXP
      Log2Exp u_log2exp (
        .fp16_in(log2exp_in[gi]),
        .result_out(log2exp_out[gi])
      );
    end
  endgenerate

  // ---------------- Stage 1 combinational ----------------
  always_comb begin
    stage1_datain_next[0] = DataIn_64bits[15:0];
    stage1_datain_next[1] = DataIn_64bits[31:16];
    stage1_datain_next[2] = DataIn_64bits[47:32];
    stage1_datain_next[3] = DataIn_64bits[63:48];
    stage1_valid_next = data_valid;

    datain_unpacked[0] = stage1_datain_next[0];
    datain_unpacked[1] = stage1_datain_next[1];
    datain_unpacked[2] = stage1_datain_next[2];
    datain_unpacked[3] = stage1_datain_next[3];
  end

  // ---------------- Stage 2 combinational ----------------
  always_comb begin
    stage2_datain_next[0] = stage1_datain_reg[0];
    stage2_datain_next[1] = stage1_datain_reg[1];
    stage2_datain_next[2] = stage1_datain_reg[2];
    stage2_datain_next[3] = stage1_datain_reg[3];
    stage2_max_next = max_out_comb;
    stage2_valid_next = stage1_valid_reg;

    Local_Max_Output = max_out_comb;
    stage1_valid = stage1_valid_reg;
  end

  // ---------------- Stage 3 combinational ----------------
  always_comb begin
    stage3_diff_next[0] = fp16_subtract(stage2_datain_reg[0], stage2_max_reg);
    stage3_diff_next[1] = fp16_subtract(stage2_datain_reg[1], stage2_max_reg);
    stage3_diff_next[2] = fp16_subtract(stage2_datain_reg[2], stage2_max_reg);
    stage3_diff_next[3] = fp16_subtract(stage2_datain_reg[3], stage2_max_reg);
    stage3_diff_next[4] = fp16_subtract(stage2_max_reg, Global_Max);
    stage3_valid_next = stage2_valid_reg;
  end

  // ---------------- Stage 4 combinational ----------------
  always_comb begin
    log2exp_in[0] = stage3_diff_reg[0];
    log2exp_in[1] = stage3_diff_reg[1];
    log2exp_in[2] = stage3_diff_reg[2];
    log2exp_in[3] = stage3_diff_reg[3];
    log2exp_in[4] = stage3_diff_reg[4];

    stage4_power_next[0] = log2exp_out[0];
    stage4_power_next[1] = log2exp_out[1];
    stage4_power_next[2] = log2exp_out[2];
    stage4_power_next[3] = log2exp_out[3];
    stage4_power_next[4] = log2exp_out[4];
    stage4_valid_next = stage3_valid_reg;
  end

  // ---------------- Stage 5 combinational ----------------
  always_comb begin
    stage5_pow_vec_next = {
      stage4_power_reg[3],
      stage4_power_reg[2],
      stage4_power_reg[1],
      stage4_power_reg[0]
    };
    stage5_shift_next = stage4_power_reg[4];
    stage5_valid_next = stage4_valid_reg;

    reduction_in = stage5_pow_vec_next;
  end

  // ---------------- Sequential pipeline update ----------------
  always_ff @(posedge clk) begin
    if (rst) begin
      stage1_datain_reg[0] <= 16'd0;
      stage1_datain_reg[1] <= 16'd0;
      stage1_datain_reg[2] <= 16'd0;
      stage1_datain_reg[3] <= 16'd0;
      stage1_valid_reg <= 1'b0;

      stage2_datain_reg[0] <= 16'd0;
      stage2_datain_reg[1] <= 16'd0;
      stage2_datain_reg[2] <= 16'd0;
      stage2_datain_reg[3] <= 16'd0;
      stage2_max_reg <= 16'd0;
      stage2_valid_reg <= 1'b0;

      stage3_diff_reg[0] <= 16'd0;
      stage3_diff_reg[1] <= 16'd0;
      stage3_diff_reg[2] <= 16'd0;
      stage3_diff_reg[3] <= 16'd0;
      stage3_diff_reg[4] <= 16'd0;
      stage3_valid_reg <= 1'b0;

      stage4_power_reg[0] <= 4'd0;
      stage4_power_reg[1] <= 4'd0;
      stage4_power_reg[2] <= 4'd0;
      stage4_power_reg[3] <= 4'd0;
      stage4_power_reg[4] <= 4'd0;
      stage4_valid_reg <= 1'b0;

      stage5_pow_vec_reg <= 16'd0;
      stage5_shift_reg <= 4'd0;
      stage5_valid_reg <= 1'b0;
    end else if (enable) begin
      stage1_datain_reg[0] <= stage1_datain_next[0];
      stage1_datain_reg[1] <= stage1_datain_next[1];
      stage1_datain_reg[2] <= stage1_datain_next[2];
      stage1_datain_reg[3] <= stage1_datain_next[3];
      stage1_valid_reg <= stage1_valid_next;

      stage2_datain_reg[0] <= stage2_datain_next[0];
      stage2_datain_reg[1] <= stage2_datain_next[1];
      stage2_datain_reg[2] <= stage2_datain_next[2];
      stage2_datain_reg[3] <= stage2_datain_next[3];
      stage2_max_reg <= stage2_max_next;
      stage2_valid_reg <= stage2_valid_next;

      stage3_diff_reg[0] <= stage3_diff_next[0];
      stage3_diff_reg[1] <= stage3_diff_next[1];
      stage3_diff_reg[2] <= stage3_diff_next[2];
      stage3_diff_reg[3] <= stage3_diff_next[3];
      stage3_diff_reg[4] <= stage3_diff_next[4];
      stage3_valid_reg <= stage3_valid_next;

      stage4_power_reg[0] <= stage4_power_next[0];
      stage4_power_reg[1] <= stage4_power_next[1];
      stage4_power_reg[2] <= stage4_power_next[2];
      stage4_power_reg[3] <= stage4_power_next[3];
      stage4_power_reg[4] <= stage4_power_next[4];
      stage4_valid_reg <= stage4_valid_next;

      stage5_pow_vec_reg <= stage5_pow_vec_next;
      stage5_shift_reg <= stage5_shift_next;
      stage5_valid_reg <= stage5_valid_next;
    end
  end

  // ---------------- Output combinational ----------------
  always_comb begin
    Power_of_Two_Vector = stage5_pow_vec_reg;
    shifted_value = Sum_Buffer_In >> stage5_shift_reg;
    final_result = shifted_value + reduction_output;
    Sum_Buffer_Update = final_result;
    stage5_valid = stage5_valid_reg;

`ifndef SYNTHESIS
    // if (stage5_valid_reg) begin
    //   $display("[DBG P1 DIFF] t=%0t d0=%h d1=%h d2=%h d3=%h d4=%h max=%h gmax=%h in0=%h in1=%h in2=%h in3=%h",
    //            $time,
    //            stage3_diff_reg[0],
    //            stage3_diff_reg[1],
    //            stage3_diff_reg[2],
    //            stage3_diff_reg[3],
    //            stage3_diff_reg[4],
    //            stage2_max_reg,
    //        Global_Max,
    //            stage2_datain_reg[0],
    //            stage2_datain_reg[1],
    //            stage2_datain_reg[2],
    //            stage2_datain_reg[3]);
    //   $display("[DBG P1 CORE] t=%0t pwr={%0d,%0d,%0d,%0d,%0d} pow_vec=%h shift=%0d red=%0d sum_in=%0d sum_upd=%0d",
    //            $time,
    //            stage4_power_reg[4],
    //            stage4_power_reg[3],
    //            stage4_power_reg[2],
    //            stage4_power_reg[1],
    //            stage4_power_reg[0],
    //            stage5_pow_vec_reg,
    //            stage5_shift_reg,
    //            reduction_output,
    //            Sum_Buffer_In,
    //            final_result);
    // end
`endif
  end
endmodule
