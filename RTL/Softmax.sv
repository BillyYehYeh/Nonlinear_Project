`include "sole_pkg.sv"
module Softmax (
  input  logic                           clk,
  input  logic                           rst,
  input  logic                           start,
  input  logic [63:0]                    src_addr_base,
  input  logic [63:0]                    dst_addr_base,
  input  logic [63:0]                    data_length,
  output logic [31:0]                    status_o,
  output logic [sole_pkg::AXI_ADDR_WIDTH-1:0] M_AXI_AWADDR,
  output logic                           M_AXI_AWVALID,
  input  logic                           M_AXI_AWREADY,
  output logic [sole_pkg::AXI_DATA_WIDTH-1:0] M_AXI_WDATA,
  output logic [sole_pkg::AXI_STRB_WIDTH-1:0] M_AXI_WSTRB,
  output logic                           M_AXI_WVALID,
  input  logic                           M_AXI_WREADY,
  input  logic [1:0]                     M_AXI_BRESP,
  input  logic                           M_AXI_BVALID,
  output logic                           M_AXI_BREADY,
  output logic [sole_pkg::AXI_ADDR_WIDTH-1:0] M_AXI_ARADDR,
  output logic                           M_AXI_ARVALID,
  input  logic                           M_AXI_ARREADY,
  input  logic [sole_pkg::AXI_DATA_WIDTH-1:0] M_AXI_RDATA,
  input  logic [1:0]                     M_AXI_RRESP,
  input  logic                           M_AXI_RVALID,
  output logic                           M_AXI_RREADY
);
  import sole_pkg::*;

  PROCESS_1_Module u_p1 (
    .clk(clk), .rst(rst), .enable(1'b0), .DataIn_64bits(64'd0),
    .Global_Max(16'd0), .Sum_Buffer_In(32'd0), .data_valid(1'b0),
    .Power_of_Two_Vector(), .Sum_Buffer_Update(), .Local_Max_Output(),
    .stage1_valid(), .stage5_valid()
  );
  PROCESS_2_Module u_p2 (
    .clk(clk), .rst(rst), .enable(1'b0), .stall_output(1'b1), .Pre_Compute_In(32'd0),
    .Leading_One_Pos_Out(), .Mux_Result_Out()
  );
  PROCESS_3_Module u_p3 (
    .clk(clk), .rst(rst), .enable(1'b0), .stall(1'b1), .input_data_valid(1'b0),
    .Local_Max(16'd0), .Global_Max(16'd0), .ks_In(4'd0), .Mux_Result_In(16'd0), .Output_Buffer_In(16'd0),
    .Output_Vector(), .stage2_valid(), .stage4_valid()
  );

  logic [1:0] state;
  logic has_error;
  logic [3:0] error_code;
  logic done_pulse;

  integer i;
  integer idx;
  integer total_elems;
  integer total_words;
  integer read_word_idx;
  integer write_word_idx;
  logic read_waiting_r;
  logic write_waiting_b;
  logic aw_done;
  logic w_done;
  logic aw_hs;
  logic w_hs;

  logic signed [31:0] in_q16 [0:DATA_LENGTH_MAX-1];
  logic [15:0] out_fp16 [0:DATA_LENGTH_MAX-1];
  integer shift_m [0:DATA_LENGTH_MAX-1];

  logic calc_done;
  logic [31:0] sum_i_reg;
  reg [63:0] wr_packed_word;

  function automatic logic signed [31:0] fp16_to_q16(input logic [15:0] h);
    logic sign;
    logic [4:0] exp;
    logic [9:0] mant;
    logic [10:0] sig11;
    integer sh;
    logic signed [31:0] mag;
    begin
      sign = h[15];
      exp = h[14:10];
      mant = h[9:0];

      if (exp == 5'd0) begin
        mag = {22'd0, mant} >>> 8;
      end else if (exp == 5'd31) begin
        mag = 32'sh7fff_ffff;
      end else begin
        sig11 = {1'b1, mant};
        sh = exp - 9;
        if (sh >= 0) begin
          if (sh >= 31) mag = 32'sh7fff_ffff;
          else mag = $signed({21'd0, sig11}) <<< sh;
        end else begin
          mag = $signed({21'd0, sig11}) >>> (-sh);
        end
      end

      if (sign) fp16_to_q16 = -mag;
      else fp16_to_q16 = mag;
    end
  endfunction

  function automatic logic [15:0] fp16_lut_from_shift(input logic use_lsb_mux, input integer exp_shift);
    integer s;
    begin
      s = exp_shift;
      if (s < 0) s = 0;
      if (s > 32) s = 32;

      if (use_lsb_mux) begin
        case (s)
          0: fp16_lut_from_shift = 16'h388b;
          1: fp16_lut_from_shift = 16'h348b;
          2: fp16_lut_from_shift = 16'h308b;
          3: fp16_lut_from_shift = 16'h2c8b;
          4: fp16_lut_from_shift = 16'h288b;
          5: fp16_lut_from_shift = 16'h248b;
          6: fp16_lut_from_shift = 16'h208b;
          7: fp16_lut_from_shift = 16'h1c8b;
          8: fp16_lut_from_shift = 16'h188b;
          9: fp16_lut_from_shift = 16'h148b;
          10: fp16_lut_from_shift = 16'h108b;
          11: fp16_lut_from_shift = 16'h0c8b;
          12: fp16_lut_from_shift = 16'h088b;
          13: fp16_lut_from_shift = 16'h048b;
          14: fp16_lut_from_shift = 16'h0246;
          15: fp16_lut_from_shift = 16'h0123;
          16: fp16_lut_from_shift = 16'h0091;
          17: fp16_lut_from_shift = 16'h0049;
          18: fp16_lut_from_shift = 16'h0024;
          19: fp16_lut_from_shift = 16'h0012;
          20: fp16_lut_from_shift = 16'h0009;
          21: fp16_lut_from_shift = 16'h0005;
          22: fp16_lut_from_shift = 16'h0002;
          23: fp16_lut_from_shift = 16'h0001;
          24: fp16_lut_from_shift = 16'h0001;
          default: fp16_lut_from_shift = 16'h0000;
        endcase
      end else begin
        case (s)
          0: fp16_lut_from_shift = 16'h3a8b;
          1: fp16_lut_from_shift = 16'h368b;
          2: fp16_lut_from_shift = 16'h328b;
          3: fp16_lut_from_shift = 16'h2e8b;
          4: fp16_lut_from_shift = 16'h2a8b;
          5: fp16_lut_from_shift = 16'h268b;
          6: fp16_lut_from_shift = 16'h228b;
          7: fp16_lut_from_shift = 16'h1e8b;
          8: fp16_lut_from_shift = 16'h1a8b;
          9: fp16_lut_from_shift = 16'h168b;
          10: fp16_lut_from_shift = 16'h128b;
          11: fp16_lut_from_shift = 16'h0e8b;
          12: fp16_lut_from_shift = 16'h0a8b;
          13: fp16_lut_from_shift = 16'h068b;
          14: fp16_lut_from_shift = 16'h0346;
          15: fp16_lut_from_shift = 16'h01a3;
          16: fp16_lut_from_shift = 16'h00d1;
          17: fp16_lut_from_shift = 16'h0069;
          18: fp16_lut_from_shift = 16'h0034;
          19: fp16_lut_from_shift = 16'h001a;
          20: fp16_lut_from_shift = 16'h000d;
          21: fp16_lut_from_shift = 16'h0007;
          22: fp16_lut_from_shift = 16'h0003;
          23: fp16_lut_from_shift = 16'h0002;
          24: fp16_lut_from_shift = 16'h0001;
          default: fp16_lut_from_shift = 16'h0000;
        endcase
      end
    end
  endfunction

  task automatic compute_approx_softmax;
    integer max_q16;
    integer d_q16;
    integer neg_d_q16;
    integer tmp;
    integer m;
    integer lop;
    logic lsb;
    integer exp_shift;
    begin
      if (total_elems <= 0) begin
        calc_done = 1'b1;
      end else begin
        max_q16 = in_q16[0];
        for (i = 1; i < total_elems; i++) begin
          if (in_q16[i] > max_q16) max_q16 = in_q16[i];
        end

        sum_i_reg = 32'd0;
        for (i = 0; i < total_elems; i++) begin
          d_q16 = in_q16[i] - max_q16;
          neg_d_q16 = (d_q16 < 0) ? -d_q16 : 0;
          tmp = neg_d_q16 * 23;
          m = tmp >>> 20;
          if (m < 0) m = 0;
          if (m > 16) m = 16;
          shift_m[i] = m;
          sum_i_reg = sum_i_reg + (32'd65536 >> m);
        end

        lop = leading_one_pos32(sum_i_reg);
        for (i = 0; i < total_elems; i++) begin
          if (lop > 0) lsb = (sum_i_reg >> (lop - 1)) & 1;
          else lsb = 1'b0;
          exp_shift = (lop - 16) + shift_m[i];
          out_fp16[i] = fp16_lut_from_shift(lsb, exp_shift);
        end

        calc_done = 1'b1;
      end
    end
  endtask

  always @* begin
    aw_hs = M_AXI_AWVALID && M_AXI_AWREADY;
    w_hs = M_AXI_WVALID && M_AXI_WREADY;
  end

  always @(posedge clk) begin
    if (rst) begin
      state <= STATE_IDLE;
      has_error <= 1'b0;
      error_code <= 4'd0;
      done_pulse <= 1'b0;
      calc_done <= 1'b0;
      total_elems <= 0;
      total_words <= 0;
      read_word_idx <= 0;
      write_word_idx <= 0;
      read_waiting_r <= 1'b0;
      write_waiting_b <= 1'b0;
      aw_done <= 1'b0;
      w_done <= 1'b0;
      M_AXI_ARVALID <= 1'b0;
      M_AXI_ARADDR <= '0;
      M_AXI_RREADY <= 1'b0;
      M_AXI_AWVALID <= 1'b0;
      M_AXI_AWADDR <= '0;
      M_AXI_WVALID <= 1'b0;
      M_AXI_WDATA <= '0;
      M_AXI_WSTRB <= 8'hFF;
      M_AXI_BREADY <= 1'b0;
    end else begin
      done_pulse <= 1'b0;
      M_AXI_WSTRB <= 8'hFF;

      case (state)
        STATE_IDLE: begin
          M_AXI_ARVALID <= 1'b0;
          M_AXI_RREADY <= 1'b0;
          M_AXI_AWVALID <= 1'b0;
          M_AXI_WVALID <= 1'b0;
          M_AXI_BREADY <= 1'b0;
          read_waiting_r <= 1'b0;
          write_waiting_b <= 1'b0;
          aw_done <= 1'b0;
          w_done <= 1'b0;
          calc_done <= 1'b0;

          if (start) begin
            if (data_length == 0 || data_length > DATA_LENGTH_MAX) begin
              has_error <= 1'b1;
              error_code <= 4'h9;
            end else begin
              has_error <= 1'b0;
              error_code <= 4'h0;
              total_elems <= data_length;
              total_words <= (data_length + 3) / 4;
              read_word_idx <= 0;
              write_word_idx <= 0;
              state <= STATE_PROCESS1;
            end
          end
        end

        STATE_PROCESS1: begin
          M_AXI_RREADY <= 1'b1;

          if (!read_waiting_r && (read_word_idx < total_words) && !M_AXI_ARVALID) begin
            M_AXI_ARADDR <= src_addr_base[31:0] + (read_word_idx * 8);
            M_AXI_ARVALID <= 1'b1;
          end

          if (M_AXI_ARVALID && M_AXI_ARREADY) begin
            M_AXI_ARVALID <= 1'b0;
            read_waiting_r <= 1'b1;
          end

          if (read_waiting_r && M_AXI_RVALID) begin
            if (M_AXI_RRESP != AXI_RESP_OKAY) begin
              has_error <= 1'b1;
              error_code <= 4'h1;
              state <= STATE_IDLE;
            end else begin
              for (idx = 0; idx < 4; idx++) begin
                if ((read_word_idx * 4 + idx) < total_elems) begin
                  in_q16[read_word_idx * 4 + idx] = fp16_to_q16(M_AXI_RDATA[idx*16 +: 16]);
                end
              end
              read_word_idx <= read_word_idx + 1;
              read_waiting_r <= 1'b0;

              if ((read_word_idx + 1) >= total_words) begin
                M_AXI_RREADY <= 1'b0;
                state <= STATE_PROCESS2;
              end
            end
          end
        end

        STATE_PROCESS2: begin
          if (!calc_done) begin
            compute_approx_softmax();
          end else begin
            state <= STATE_PROCESS3;
            write_word_idx <= 0;
            M_AXI_AWVALID <= 1'b0;
            M_AXI_WVALID <= 1'b0;
            M_AXI_BREADY <= 1'b0;
            write_waiting_b <= 1'b0;
            aw_done <= 1'b0;
            w_done <= 1'b0;
          end
        end

        STATE_PROCESS3: begin
          if ((write_word_idx < total_words) && !write_waiting_b) begin
              if (!M_AXI_AWVALID && !M_AXI_WVALID && !aw_done && !w_done) begin
              wr_packed_word = 64'd0;
              for (idx = 0; idx < 4; idx++) begin
                if ((write_word_idx * 4 + idx) < total_elems) begin
                  wr_packed_word[idx*16 +: 16] = out_fp16[write_word_idx * 4 + idx];
                end
              end
              M_AXI_AWADDR <= dst_addr_base[31:0] + (write_word_idx * 8);
              M_AXI_WDATA <= wr_packed_word;
              M_AXI_AWVALID <= 1'b1;
              M_AXI_WVALID <= 1'b1;
            end

            if (aw_hs) begin
              M_AXI_AWVALID <= 1'b0;
              aw_done <= 1'b1;
            end
            if (w_hs) begin
              M_AXI_WVALID <= 1'b0;
              w_done <= 1'b1;
            end

            if ((aw_done || aw_hs) && (w_done || w_hs)) begin
              M_AXI_BREADY <= 1'b1;
              write_waiting_b <= 1'b1;
            end
          end

          if (write_waiting_b && M_AXI_BVALID) begin
            M_AXI_BREADY <= 1'b0;
            write_waiting_b <= 1'b0;
            aw_done <= 1'b0;
            w_done <= 1'b0;
            if (M_AXI_BRESP != AXI_RESP_OKAY) begin
              has_error <= 1'b1;
              error_code <= 4'h4;
              state <= STATE_IDLE;
            end else begin
              write_word_idx <= write_word_idx + 1;
              if ((write_word_idx + 1) >= total_words) begin
                done_pulse <= 1'b1;
                state <= STATE_IDLE;
              end
            end
          end
        end

        default: begin
          state <= STATE_IDLE;
        end
      endcase
    end
  end

  always @* begin
    status_o = 32'd0;
    status_o[0] = done_pulse;
    status_o[2:1] = state;
    status_o[3] = has_error;
    status_o[7:4] = error_code;
  end
endmodule
