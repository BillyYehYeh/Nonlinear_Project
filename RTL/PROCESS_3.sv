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
  logic [3:0] p;
  logic [15:0] d0, d1, d2, d3;

  Log2Exp u_log2exp (
    .fp16_in(Local_Max),
    .result_out(p)
  );

  Divider_Module u_div0 (.ky(Output_Buffer_In[3:0] + p), .ks(ks_In), .Mux_Result(Mux_Result_In), .Divider_Output(d0));
  Divider_Module u_div1 (.ky(Output_Buffer_In[7:4] + p), .ks(ks_In), .Mux_Result(Mux_Result_In), .Divider_Output(d1));
  Divider_Module u_div2 (.ky(Output_Buffer_In[11:8] + p), .ks(ks_In), .Mux_Result(Mux_Result_In), .Divider_Output(d2));
  Divider_Module u_div3 (.ky(Output_Buffer_In[15:12] + p), .ks(ks_In), .Mux_Result(Mux_Result_In), .Divider_Output(d3));

  always_ff @(posedge clk) begin
    if (rst) begin
      Output_Vector <= 64'd0;
      stage2_valid <= 1'b0;
      stage4_valid <= 1'b0;
    end else if (enable && !stall && input_data_valid) begin
      Output_Vector <= {d3, d2, d1, d0};
      stage2_valid <= 1'b1;
      stage4_valid <= 1'b1;
    end else begin
      stage2_valid <= 1'b0;
      stage4_valid <= 1'b0;
    end
  end
endmodule
