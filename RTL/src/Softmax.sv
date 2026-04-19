module Softmax (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        start,
  input  logic [63:0] src_addr_base,
  input  logic [63:0] dst_addr_base,
  input  logic [63:0] data_length,
  output logic [31:0] status_o,
  output logic [31:0] M_AXI_AWADDR,
  output logic        M_AXI_AWVALID,
  input  logic        M_AXI_AWREADY,
  output logic [63:0] M_AXI_WDATA,
  output logic [7:0]  M_AXI_WSTRB,
  output logic        M_AXI_WVALID,
  input  logic        M_AXI_WREADY,
  input  logic [1:0]  M_AXI_BRESP,
  input  logic        M_AXI_BVALID,
  output logic        M_AXI_BREADY,
  output logic [31:0] M_AXI_ARADDR,
  output logic        M_AXI_ARVALID,
  input  logic        M_AXI_ARREADY,
  input  logic [63:0] M_AXI_RDATA,
  input  logic [1:0]  M_AXI_RRESP,
  input  logic        M_AXI_RVALID,
  output logic        M_AXI_RREADY
);
  localparam int DATA_LENGTH_MAX = 4096;
  localparam logic [1:0] AXI_RESP_OKAY = 2'b00;

  localparam logic [1:0] STATE_IDLE     = 2'd0;
  localparam logic [1:0] STATE_PROCESS1 = 2'd1;
  localparam logic [1:0] STATE_PROCESS2 = 2'd2;
  localparam logic [1:0] STATE_PROCESS3 = 2'd3;

  // Internal status/state
  logic [1:0] state;
  logic [3:0] error_code;
  logic       has_error;
  logic       done_pulse;

  // Enable signals (same partition as SystemC)
  logic process_1_enable;
  logic process_2_enable;
  logic process_3_enable;

  // Buffer registers (SystemC Buffer_Update)
  logic [15:0] global_max_reg;
  logic [15:0] global_max_for_p1_d1_reg;
  logic [15:0] global_max_for_p1_d2_reg;
  logic [31:0] sum_buffer_reg;

  // Process interconnect signals
  logic [15:0] Global_Max_Buffer_In;
  logic [15:0] Power_of_Two_Vector_Signal;
  logic [31:0] Sum_Buffer_In;
  logic [3:0]  Leading_One_Pos_Out_Signal;
  logic [15:0] Mux_Result_Out_Signal;
  logic [15:0] Local_Max_Signal;
  logic [15:0] Output_Buffer_In_Signal;
  logic [63:0] Process3_Output_Vector;

  // Valid/stall signals
  logic process1_read_data_valid;
  logic process1_stage1_valid;
  logic process1_stage5_valid;
  logic process3_read_data_valid;
  logic process3_stage2_valid;
  logic process3_stage4_valid;
  logic process3_stall;
  logic stall_process2_output_Signal;

  // FIFO control/status
  logic        max_fifo_write_en;
  logic        max_fifo_read_en;
  logic        max_fifo_read_data_valid;
  logic        max_fifo_clear;
  logic        max_fifo_full;
  logic        max_fifo_empty;
  logic [11:0] max_fifo_count;

  logic        output_fifo_write_en;
  logic        output_fifo_read_en;
  logic        output_fifo_read_data_valid;
  logic        output_fifo_clear;
  logic        output_fifo_full;
  logic        output_fifo_empty;
  logic [11:0] output_fifo_count;

  // AXI counters (SystemC parity)
  logic [31:0] read_addr_sent_num;
  logic [31:0] read_data_received_num;
  logic [31:0] write_addr_sent_num;
  logic [31:0] write_data_sent_num;
  logic [31:0] write_response_received_num;
  logic        aw_active;
  logic        w_active;

  logic process1_finish_flag;
  logic process3_finish_flag;
  logic read_resp_error;
  logic write_resp_error;

  logic [31:0] process2_cycle_counter;

  // ---------------- Local helper ----------------
  function automatic logic [15:0] fp16_max(input logic [15:0] a, input logic [15:0] b);
    logic a_sign, b_sign;
    logic [14:0] a_rest, b_rest;
    begin
      a_sign = a[15];
      b_sign = b[15];
      a_rest = a[14:0];
      b_rest = b[14:0];

      if (a_sign == 1'b0 && b_sign == 1'b1) return a;
      if (a_sign == 1'b1 && b_sign == 1'b0) return b;

      if (a_sign == 1'b0) begin
        return (a_rest >= b_rest) ? a : b;
      end else begin
        return (a_rest <= b_rest) ? a : b;
      end
    end
  endfunction

  // ---------------- Submodule instantiation ----------------
  PROCESS_1_Module u_p1 (
    .clk(clk),
    .rst_n(rst_n),
    .enable(process_1_enable),
    .DataIn_64bits(M_AXI_RDATA),
    .Global_Max(global_max_for_p1_d1_reg),
    .Sum_Buffer_In(sum_buffer_reg),
    .data_valid(process1_read_data_valid),
    .Power_of_Two_Vector(Power_of_Two_Vector_Signal),
    .Sum_Buffer_Update(Sum_Buffer_In),
    .Local_Max_Output(Global_Max_Buffer_In),
    .stage1_valid(process1_stage1_valid),
    .stage5_valid(process1_stage5_valid)
  );

  PROCESS_2_Module u_p2 (
    .clk(clk),
    .rst_n(rst_n),
    .enable(process_2_enable),
    .stall_output(stall_process2_output_Signal),
    .Pre_Compute_In(sum_buffer_reg),
    .Leading_One_Pos_Out(Leading_One_Pos_Out_Signal),
    .Mux_Result_Out(Mux_Result_Out_Signal)
  );

  PROCESS_3_Module u_p3 (
    .clk(clk),
    .rst_n(rst_n),
    .enable(process_3_enable),
    .stall(process3_stall),
    .input_data_valid(process3_read_data_valid),
    .Local_Max(Local_Max_Signal),
    .Global_Max(global_max_reg),
    .ks_In(Leading_One_Pos_Out_Signal),
    .Mux_Result_In(Mux_Result_Out_Signal),
    .Output_Buffer_In(Output_Buffer_In_Signal),
    .Output_Vector(Process3_Output_Vector),
    .stage2_valid(process3_stage2_valid),
    .stage4_valid(process3_stage4_valid)
  );

  Max_FIFO u_max_fifo (
    .clk(clk),
    .rst_n(rst_n),
    .data_in(Global_Max_Buffer_In),
    .write_en(max_fifo_write_en),
    .data_out(Local_Max_Signal),
    .read_ready(max_fifo_read_en),
    .read_valid(max_fifo_read_data_valid),
    .clear(max_fifo_clear),
    .full(max_fifo_full),
    .empty(max_fifo_empty),
    .count(max_fifo_count)
  );

  Output_FIFO u_output_fifo (
    .clk(clk),
    .rst_n(rst_n),
    .data_in(Power_of_Two_Vector_Signal),
    .write_en(output_fifo_write_en),
    .read_ready(output_fifo_read_en),
    .read_valid(output_fifo_read_data_valid),
    .clear(output_fifo_clear),
    .data_out(Output_Buffer_In_Signal),
    .full(output_fifo_full),
    .empty(output_fifo_empty),
    .count(output_fifo_count)
  );

  // ---------------- Combinational controls ----------------
  always_comb begin
    process_1_enable = (state == STATE_PROCESS1) && !has_error;
    process_2_enable = (state == STATE_PROCESS2) && !has_error;
    process_3_enable = (state == STATE_PROCESS3) && !has_error;

    // SystemC: stall PROCESS_2 output outside PROCESS2 state
    stall_process2_output_Signal = (state != STATE_PROCESS2);

    // SystemC: stall PROCESS_3 when output valid but AXI write data not ready
    process3_stall = (state == STATE_PROCESS3) && process3_stage4_valid && !M_AXI_WREADY;

    // SystemC validity update mapping
    process1_read_data_valid = M_AXI_RVALID && M_AXI_RREADY;
    process3_read_data_valid = max_fifo_read_data_valid;

    // FIFO controls (SystemC manage_fifo_control)
    max_fifo_write_en = (state == STATE_PROCESS1) && !has_error && process1_stage1_valid;
    output_fifo_write_en = (state == STATE_PROCESS1) && !has_error && process1_stage5_valid;

    max_fifo_read_en = (state == STATE_PROCESS3) && !has_error && !process3_stall;
    output_fifo_read_en = (state == STATE_PROCESS3) && !has_error && !process3_stall && process3_stage2_valid;

    max_fifo_clear = has_error;
    output_fifo_clear = has_error;

    // Transition flags (SystemC state_transition_flag)
    process1_finish_flag = (state == STATE_PROCESS1)
                        && ((read_data_received_num * 32'd4) >= data_length)
                        && (({20'd0, max_fifo_count} * 32'd4) >= data_length)
                        && (({20'd0, output_fifo_count} * 32'd4) >= data_length);

    process3_finish_flag = (state == STATE_PROCESS3)
                        && ((write_addr_sent_num * 32'd4) >= data_length)
                        && ((write_data_sent_num * 32'd4) >= data_length)
                        && ((write_response_received_num * 32'd4) >= data_length);

    read_resp_error = (state == STATE_PROCESS1)
             && M_AXI_RVALID
             && M_AXI_RREADY
             && (M_AXI_RRESP != AXI_RESP_OKAY);

    write_resp_error = (state == STATE_PROCESS3)
            && M_AXI_BVALID
            && M_AXI_BREADY
            && (M_AXI_BRESP != AXI_RESP_OKAY);
  end

  // ---------------- FSM + process2 timing ----------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= STATE_IDLE;
      done_pulse <= 1'b0;
      has_error <= 1'b0;
      error_code <= 4'd0;
      process2_cycle_counter <= 32'd0;
    end else begin
      done_pulse <= 1'b0;

      if (read_resp_error) begin
        has_error <= 1'b1;
        error_code <= 4'h1;
      end else if (write_resp_error) begin
        has_error <= 1'b1;
        error_code <= 4'h4;
      end

      if (has_error) begin
        state <= STATE_IDLE;
        process2_cycle_counter <= 32'd0;
      end else begin
        case (state)
          STATE_IDLE: begin
            process2_cycle_counter <= 32'd0;
            if (start) begin
              if ((data_length == 64'd0) || (data_length > DATA_LENGTH_MAX)) begin
                has_error <= 1'b1;
                error_code <= 4'h9;
              end else begin
                has_error <= 1'b0;
                error_code <= 4'h0;
                state <= STATE_PROCESS1;
              end
            end
          end

          STATE_PROCESS1: begin
            if (process1_finish_flag) begin
              state <= STATE_PROCESS2;
              process2_cycle_counter <= 32'd0;
            end
          end

          STATE_PROCESS2: begin
            process2_cycle_counter <= process2_cycle_counter + 32'd1;
            if (process2_cycle_counter >= 32'd10) begin
              state <= STATE_PROCESS3;
              process2_cycle_counter <= 32'd0;
            end
          end

          STATE_PROCESS3: begin
            if (process3_finish_flag) begin
              state <= STATE_IDLE;
              done_pulse <= 1'b1;
            end
          end

          default: begin
            state <= STATE_IDLE;
          end
        endcase
      end
    end
  end

  // ---------------- Buffer update parity with SystemC ----------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      global_max_reg <= 16'd0;
      global_max_for_p1_d1_reg <= 16'd0;
      global_max_for_p1_d2_reg <= 16'd0;
      sum_buffer_reg <= 32'd0;
    end else if (state == STATE_PROCESS1) begin
      global_max_for_p1_d1_reg <= global_max_reg;
      global_max_for_p1_d2_reg <= global_max_for_p1_d1_reg;
      if (process1_stage1_valid) begin
        global_max_reg <= fp16_max(global_max_reg, Global_Max_Buffer_In);
      end
      if (process1_stage5_valid) begin
        sum_buffer_reg <= Sum_Buffer_In;
      end
    end else begin
      global_max_for_p1_d1_reg <= global_max_reg;
      global_max_for_p1_d2_reg <= global_max_for_p1_d1_reg;
    end
  end

  // ---------------- AXI read process parity ----------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n ) begin
      M_AXI_ARADDR <= 32'd0;
      M_AXI_ARVALID <= 1'b0;
      M_AXI_RREADY <= 1'b0;
      read_addr_sent_num <= 32'd0;
      read_data_received_num <= 32'd0;
    end else if (state != STATE_PROCESS1) begin 
      M_AXI_ARADDR <= 32'd0;
      M_AXI_ARVALID <= 1'b0;
      M_AXI_RREADY <= 1'b0;
      read_addr_sent_num <= 32'd0;
      read_data_received_num <= 32'd0;
    end else begin
      // Address generation
      if (!M_AXI_ARVALID && ((read_addr_sent_num * 32'd4) < data_length)) begin
        M_AXI_ARADDR <= src_addr_base[31:0] + (read_addr_sent_num * 32'd8);
        M_AXI_ARVALID <= 1'b1;
      end

      if (M_AXI_ARVALID && M_AXI_ARREADY) begin
        M_AXI_ARVALID <= 1'b0;
        read_addr_sent_num <= read_addr_sent_num + 32'd1;
      end

      // Data receive side
      M_AXI_RREADY <= ((read_data_received_num * 32'd4) < data_length);

      if (M_AXI_RVALID && M_AXI_RREADY) begin
        read_data_received_num <= read_data_received_num + 32'd1;
      end
    end
  end

  // ---------------- AXI write request parity ----------------
  always_comb begin
    aw_active = (state == STATE_PROCESS3) && process3_stage4_valid
             && ((write_addr_sent_num * 32'd4) < data_length);
    w_active = (state == STATE_PROCESS3) && process3_stage4_valid
            && ((write_data_sent_num * 32'd4) < data_length);

    if (state == STATE_PROCESS3) begin
      M_AXI_AWADDR = dst_addr_base[31:0] + (write_addr_sent_num * 32'd8);
      M_AXI_AWVALID = aw_active;
      M_AXI_WDATA = Process3_Output_Vector;
      M_AXI_WSTRB = 8'hFF;
      M_AXI_WVALID = w_active;
      M_AXI_BREADY = 1'b1;
    end else begin
      M_AXI_AWADDR = 32'd0;
      M_AXI_AWVALID = 1'b0;
      M_AXI_WDATA = 64'd0;
      M_AXI_WSTRB = 8'h00;
      M_AXI_WVALID = 1'b0;
      M_AXI_BREADY = 1'b0;
    end
  end

  // ---------------- AXI write counter update parity ----------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      write_addr_sent_num <= 32'd0;
      write_data_sent_num <= 32'd0;
      write_response_received_num <= 32'd0;
    end else if (state != STATE_PROCESS3) begin 
      write_addr_sent_num <= 32'd0;
      write_data_sent_num <= 32'd0;
      write_response_received_num <= 32'd0;
    end else begin
      if (aw_active && M_AXI_AWREADY) begin
        write_addr_sent_num <= write_addr_sent_num + 32'd1;
      end

      if (w_active && M_AXI_WREADY) begin
        write_data_sent_num <= write_data_sent_num + 32'd1;
      end

      if (M_AXI_BVALID && M_AXI_BREADY) begin
        write_response_received_num <= write_response_received_num + 32'd1;
      end

`ifndef SYNTHESIS
      // if (state == STATE_PROCESS1) begin
      //   $display("[DBG P1] t=%0t s1v=%0b s5v=%0b pow_vec=%h sum_in=%0d sum_upd=%0d gmax_in=%h gmax_buf=%h",
      //            $time,
      //            process1_stage1_valid,
      //            process1_stage5_valid,
      //            Power_of_Two_Vector_Signal,
      //            sum_buffer_reg,
      //            Sum_Buffer_In,
      //            Global_Max_Buffer_In,
      //            global_max_reg);
      // end
      // if (process3_stage4_valid && M_AXI_WVALID) begin
      //   $display("[DBG P3] t=%0t ks=%0d mux=%h local=%h gmax=%h outbuf=%h p3out=%h",
      //            $time,
      //            Leading_One_Pos_Out_Signal,
      //            Mux_Result_Out_Signal,
      //            Local_Max_Signal,
      //            global_max_reg,
      //            Output_Buffer_In_Signal,
      //            Process3_Output_Vector);
      // end
      // if (state == STATE_PROCESS3) begin
      //   $display("[DBG FIFO] t=%0t s2v=%0b of_rd_en=%0b of_valid=%0b of_cnt=%0d of_data=%h mf_rd_en=%0b mf_valid=%0b mf_cnt=%0d",
      //            $time,
      //            process3_stage2_valid,
      //            output_fifo_read_en,
      //            output_fifo_read_data_valid,
      //            output_fifo_count,
      //            Output_Buffer_In_Signal,
      //            max_fifo_read_en,
      //            max_fifo_read_data_valid,
      //            max_fifo_count);
      // end
      // if (state == STATE_PROCESS2) begin
      //   $display("[DBG P2] t=%0t sum_buffer=%0d ks=%0d mux=%h", $time, sum_buffer_reg, Leading_One_Pos_Out_Signal, Mux_Result_Out_Signal);
      // end
`endif
    end
  end

  // ---------------- Status output ----------------
  always_comb begin
    status_o = 32'd0;
    status_o[0] = done_pulse;
    status_o[2:1] = state;
    status_o[3] = has_error;
    status_o[7:4] = error_code;
  end
endmodule
