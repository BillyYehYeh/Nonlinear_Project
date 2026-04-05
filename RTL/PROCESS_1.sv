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
  logic [15:0] d0, d1, d2, d3;
  logic [15:0] max_local;
  logic [3:0] m0, m1, m2, m3;

  always @* begin
    d0 = DataIn_64bits[15:0];
    d1 = DataIn_64bits[31:16];
    d2 = DataIn_64bits[47:32];
    d3 = DataIn_64bits[63:48];
    max_local = d0;
    if (d1 > max_local) max_local = d1;
    if (d2 > max_local) max_local = d2;
    if (d3 > max_local) max_local = d3;

    m0 = 4'd0;
    m1 = 4'd0;
    m2 = 4'd0;
    m3 = 4'd0;

    Local_Max_Output = max_local;
    Power_of_Two_Vector = {m3, m2, m1, m0};
    Sum_Buffer_Update = Sum_Buffer_In + (32'd65536 >> m0) + (32'd65536 >> m1) + (32'd65536 >> m2) + (32'd65536 >> m3);
    stage1_valid = enable && data_valid;
    stage5_valid = enable && data_valid;
  end
endmodule
