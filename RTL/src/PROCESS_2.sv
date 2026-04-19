module PROCESS_2_Module (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        enable,
  input  logic        stall_output,
  input  logic [31:0] Pre_Compute_In,
  output logic [3:0]  Leading_One_Pos_Out,
  output logic [15:0] Mux_Result_Out
);
  logic [3:0]  lo;
  logic [15:0] mr;

  Divider_PreCompute_Module u_pre (
    .input_data(Pre_Compute_In),
    .Leading_One_Pos(lo),
    .Mux_Result(mr)
  );

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      Leading_One_Pos_Out <= 4'd0;
      Mux_Result_Out <= 16'd0;
    end else if (enable && !stall_output) begin
      Leading_One_Pos_Out <= lo;
      Mux_Result_Out <= mr;
    end else begin
      Leading_One_Pos_Out <= Leading_One_Pos_Out;
      Mux_Result_Out <= Mux_Result_Out;
    end
  end
endmodule
