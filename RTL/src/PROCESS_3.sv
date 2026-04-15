module PROCESS_3_Module (
  input  logic        clk,
  input  logic        rst,
  input  logic        enable,
  input  logic        stall,
  input  logic        input_data_valid,
  input  logic [15:0] Local_Max,
  input  logic [15:0] Global_Max,
  input  logic [3:0]  ks_In,
  input  logic [15:0] Mux_Result_In,
  input  logic [15:0] Output_Buffer_In,
  output logic [63:0] Output_Vector,
  output logic        stage2_valid,
  output logic        stage4_valid
);
  import utils_pkg::*;

  // ---------- Stage registers ----------
  logic [15:0] stage1_sub_reg;
  logic        stage1_valid_reg;

  logic [3:0]  stage2_power_reg;
  logic        stage2_valid_reg;

  logic [15:0] stage3_mux_reg;
  logic [3:0]  stage3_ks_reg;
  logic [3:0]  stage3_ky_reg [0:3];
  logic        stage3_valid_reg;

  logic [63:0] stage4_out_reg;
  logic        stage4_valid_reg;

  // ---------- Next-state ----------
  logic [15:0] stage1_sub_next;
  logic        stage1_valid_next;

  logic [3:0]  stage2_power_next;
  logic        stage2_valid_next;

  logic [15:0] stage3_mux_next;
  logic [3:0]  stage3_ks_next;
  logic [3:0]  stage3_ky_next [0:3];
  logic        stage3_valid_next;

  logic [63:0] stage4_out_next;
  logic        stage4_valid_next;

  logic [15:0] divider_out [0:3];
  logic [3:0]  log2exp_out;

  // ---------- Submodules ----------
  Log2Exp u_log2exp (
    .fp16_in(stage1_sub_reg),
    .result_out(log2exp_out)
  );

  genvar di;
  generate
    for (di = 0; di < 4; di++) begin : GEN_DIV
      Divider_Module u_div (
        .ky(stage3_ky_reg[di]),
        .ks(stage3_ks_reg),
        .Mux_Result(stage3_mux_reg),
        .Divider_Output(divider_out[di])
      );
    end
  endgenerate

  // Stage1_Comb: Sub_Result = Local_Max - Global_Max, valid pass-through
  always_comb begin
    stage1_sub_next = fp16_subtract(Local_Max, Global_Max);
    stage1_valid_next = input_data_valid;
  end

  // Stage2_Comb: Log2Exp(Stage1_Reg.Sub_Result)
  always_comb begin
    stage2_power_next = log2exp_out;
    stage2_valid_next = stage1_valid_reg;
  end

  // Stage3_Comb: ky[i] = sat4(Power + ky_buf[i]); pass ks,mux/valid
  integer i;
  logic [4:0] ky_sum;
  always_comb begin
    stage3_mux_next = Mux_Result_In;
    stage3_ks_next = ks_In;
    stage3_valid_next = stage2_valid_reg;

    for (i = 0; i < 4; i++) begin
      ky_sum = {1'b0, stage2_power_reg} + {1'b0, Output_Buffer_In[i*4 +: 4]};
      if (ky_sum > 5'd15) stage3_ky_next[i] = 4'd15;
      else stage3_ky_next[i] = ky_sum[3:0];
    end
  end

  // Stage4_Comb: pack 4 divider outputs
  always_comb begin
    stage4_out_next = {divider_out[3], divider_out[2], divider_out[1], divider_out[0]};
    stage4_valid_next = stage3_valid_reg;
  end

  // Pipeline_Update (SystemC parity)
  always_ff @(posedge clk) begin
    if (rst) begin
      stage1_sub_reg <= 16'd0;
      stage1_valid_reg <= 1'b0;
      stage2_power_reg <= 4'd0;
      stage2_valid_reg <= 1'b0;
      stage3_mux_reg <= 16'd0;
      stage3_ks_reg <= 4'd0;
      stage3_ky_reg[0] <= 4'd0;
      stage3_ky_reg[1] <= 4'd0;
      stage3_ky_reg[2] <= 4'd0;
      stage3_ky_reg[3] <= 4'd0;
      stage3_valid_reg <= 1'b0;
      stage4_out_reg <= 64'd0;
      stage4_valid_reg <= 1'b0;
    end else if (!enable) begin
      // hold
    end else if (stall) begin
      // hold
    end else begin
      stage1_sub_reg <= stage1_sub_next;
      stage1_valid_reg <= stage1_valid_next;
      stage2_power_reg <= stage2_power_next;
      stage2_valid_reg <= stage2_valid_next;
      stage3_mux_reg <= stage3_mux_next;
      stage3_ks_reg <= stage3_ks_next;
      stage3_ky_reg[0] <= stage3_ky_next[0];
      stage3_ky_reg[1] <= stage3_ky_next[1];
      stage3_ky_reg[2] <= stage3_ky_next[2];
      stage3_ky_reg[3] <= stage3_ky_next[3];
      stage3_valid_reg <= stage3_valid_next;
      stage4_out_reg <= stage4_out_next;
      stage4_valid_reg <= stage4_valid_next;
    end
  end

  // Output_Comb
  always_comb begin
    Output_Vector = stage4_out_reg;
    stage2_valid = stage2_valid_reg;
    stage4_valid = stage4_valid_reg;
  end
endmodule
