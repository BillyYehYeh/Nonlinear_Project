module Divider_PreCompute_Module (
  input  logic [31:0] input_data,
  output logic [3:0]  Leading_One_Pos,
  output logic [15:0] Mux_Result
);
  function automatic logic [3:0] find_leading_one_pos(input logic [31:0] input_val);
    logic [15:0] integer_part;
    int i;
    begin
      integer_part = input_val[31:16];

      if (integer_part == 16'h0000) begin
        find_leading_one_pos = 4'd0;
      end else begin
        find_leading_one_pos = 4'd0;
        for (i = 15; i >= 0; i = i - 1) begin
          if (integer_part[i]) begin
            find_leading_one_pos = i[3:0];
            break;
          end
        end
      end
    end
  endfunction

  logic [3:0] leading_pos;
  logic is_over_half;
  int bit_pos;

  always @* begin
    leading_pos = find_leading_one_pos(input_data);
    Leading_One_Pos = leading_pos;

    bit_pos = 15 + leading_pos;
    is_over_half = input_data[bit_pos];

    if (is_over_half) begin
      Mux_Result = 16'h3A8B;
    end else begin
      Mux_Result = 16'h388B;
    end
  end
endmodule
