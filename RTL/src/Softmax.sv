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
  localparam int AXI_TIMEOUT_THRESHOLD = 100;
  localparam logic [1:0] AXI_RESP_OKAY = 2'b00;

  localparam logic [1:0] STATE_IDLE     = 2'd0;
  localparam logic [1:0] STATE_PROCESS1 = 2'd1;
  localparam logic [1:0] STATE_PROCESS2 = 2'd2;
  localparam logic [1:0] STATE_PROCESS3 = 2'd3;

  localparam logic [3:0] ERR_NONE                        = 4'h0;
  localparam logic [3:0] ERR_AXI_READ_ERROR              = 4'h1;
  localparam logic [3:0] ERR_AXI_READ_TIMEOUT            = 4'h2;
  localparam logic [3:0] ERR_AXI_READ_DATA_MISSING       = 4'h3;
  localparam logic [3:0] ERR_AXI_WRITE_ERROR             = 4'h4;
  localparam logic [3:0] ERR_AXI_WRITE_TIMEOUT           = 4'h5;
  localparam logic [3:0] ERR_AXI_WRITE_RESPONSE_MISMATCH = 4'h6;
  localparam logic [3:0] ERR_MAX_FIFO_OVERFLOW           = 4'h7;
  localparam logic [3:0] ERR_OUTPUT_FIFO_OVERFLOW        = 4'h8;
  localparam logic [3:0] ERR_DATA_LENGTH_INVALID         = 4'h9;
  localparam logic [3:0] ERR_INVALID_STATE               = 4'hA;

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
  logic [31:0] write_buf_addr;
  logic [63:0] write_buf_data;
  logic        write_buf_valid;
  logic        write_buf_aw_sent;
  logic        aw_active;
  logic        w_active;

  logic process1_finish_flag;
  logic process3_finish_flag;
  logic read_resp_error;
  logic write_resp_error;
  logic invalid_state_error;
  logic data_length_invalid_error;
  logic max_fifo_overflow_error;
  logic output_fifo_overflow_error;
  logic read_data_missing_error;
  logic write_response_mismatch_error;
  logic read_timeout_error;
  logic write_timeout_error;
  logic [31:0] read_timeout_counter;
  logic [31:0] write_timeout_counter;
  logic any_read_handshake;
  logic any_write_handshake;
  logic write_outstanding;
  logic read_transfer_incomplete;
  logic write_transfer_incomplete;
  logic write_timeout_count_enable;
  logic        error_detected_next;
  logic [3:0]  error_code_next;

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

    // Hold PROCESS_3 while one write beat is buffered but not fully sent.
    process3_stall = (state == STATE_PROCESS3) && write_buf_valid;

    // SystemC validity update mapping
    process1_read_data_valid = M_AXI_RVALID && M_AXI_RREADY;
    process3_read_data_valid = max_fifo_read_en;

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

    invalid_state_error = (state > STATE_PROCESS3);
    data_length_invalid_error = (state == STATE_IDLE) && start
                             && ((data_length == 64'd0) || (data_length > DATA_LENGTH_MAX));

    max_fifo_overflow_error = max_fifo_full && process1_stage1_valid;
    output_fifo_overflow_error = output_fifo_full && process1_stage5_valid;

    read_data_missing_error = process1_finish_flag
                           && (read_addr_sent_num != read_data_received_num);

    write_response_mismatch_error = process3_finish_flag
                                 && ((write_addr_sent_num != write_data_sent_num)
                                  || (write_addr_sent_num != write_response_received_num)
                                  || (write_data_sent_num != write_response_received_num));

    any_read_handshake = (M_AXI_ARVALID && M_AXI_ARREADY)
                      || (M_AXI_RVALID && M_AXI_RREADY);
    read_transfer_incomplete = ((read_data_received_num * 32'd4) < data_length);
    read_timeout_error = (state == STATE_PROCESS1)
                      && !any_read_handshake
                      && read_transfer_incomplete
                      && (read_timeout_counter >= (AXI_TIMEOUT_THRESHOLD - 1));

    any_write_handshake = (M_AXI_AWVALID && M_AXI_AWREADY)
                       || (M_AXI_WVALID && M_AXI_WREADY)
                       || (M_AXI_BVALID && M_AXI_BREADY);
    write_outstanding = write_buf_valid
             || (write_addr_sent_num != write_data_sent_num)
             || (write_addr_sent_num != write_response_received_num)
             || (write_data_sent_num != write_response_received_num);
    write_transfer_incomplete = ((write_addr_sent_num * 32'd4) < data_length)
                             || ((write_data_sent_num * 32'd4) < data_length)
                             || ((write_response_received_num * 32'd4) < data_length);
    write_timeout_count_enable = write_transfer_incomplete
                              && !any_write_handshake
                  && (M_AXI_AWVALID || M_AXI_WVALID || write_outstanding);
    write_timeout_error = (state == STATE_PROCESS3)
                       && write_timeout_count_enable
                       && (write_timeout_counter >= (AXI_TIMEOUT_THRESHOLD - 1));

    error_detected_next = 1'b0;
    error_code_next = ERR_NONE;
    if (invalid_state_error) begin
      error_detected_next = 1'b1;
      error_code_next = ERR_INVALID_STATE;
    end else if (data_length_invalid_error) begin
      error_detected_next = 1'b1;
      error_code_next = ERR_DATA_LENGTH_INVALID;
    end else if (max_fifo_overflow_error) begin
      error_detected_next = 1'b1;
      error_code_next = ERR_MAX_FIFO_OVERFLOW;
    end else if (output_fifo_overflow_error) begin
      error_detected_next = 1'b1;
      error_code_next = ERR_OUTPUT_FIFO_OVERFLOW;
    end else if (read_resp_error) begin
      error_detected_next = 1'b1;
      error_code_next = ERR_AXI_READ_ERROR;
    end else if (read_data_missing_error) begin
      error_detected_next = 1'b1;
      error_code_next = ERR_AXI_READ_DATA_MISSING;
    end else if (write_resp_error) begin
      error_detected_next = 1'b1;
      error_code_next = ERR_AXI_WRITE_ERROR;
    end else if (write_response_mismatch_error) begin
      error_detected_next = 1'b1;
      error_code_next = ERR_AXI_WRITE_RESPONSE_MISMATCH;
    end else if (read_timeout_error) begin
      error_detected_next = 1'b1;
      error_code_next = ERR_AXI_READ_TIMEOUT;
    end else if (write_timeout_error) begin
      error_detected_next = 1'b1;
      error_code_next = ERR_AXI_WRITE_TIMEOUT;
    end
  end

  // ---------------- FSM + process2 timing ----------------
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      read_timeout_counter <= 32'd0;
      write_timeout_counter <= 32'd0;
    end else begin
      if (state != STATE_PROCESS1) begin
        read_timeout_counter <= 32'd0;
      end else if (any_read_handshake) begin
        read_timeout_counter <= 32'd0;
      end else if (read_transfer_incomplete) begin
        read_timeout_counter <= read_timeout_counter + 32'd1;
      end

      if (state != STATE_PROCESS3) begin
        write_timeout_counter <= 32'd0;
      end else if (any_write_handshake) begin
        write_timeout_counter <= 32'd0;
      end else if (write_timeout_count_enable) begin
        write_timeout_counter <= write_timeout_counter + 32'd1;
      end else begin
        write_timeout_counter <= 32'd0;
      end
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      state <= STATE_IDLE;
      done_pulse <= 1'b0;
      has_error <= 1'b0;
      error_code <= ERR_NONE;
      process2_cycle_counter <= 32'd0;
    end else begin
      done_pulse <= 1'b0;

      if (has_error) begin
        // Keep error sticky in IDLE, but allow a clean restart with valid start.
        state <= STATE_IDLE;
        process2_cycle_counter <= 32'd0;
        if (start && (data_length != 64'd0) && (data_length <= DATA_LENGTH_MAX)) begin
          has_error <= 1'b0;
          error_code <= ERR_NONE;
          state <= STATE_PROCESS1;
        end
      end else if (error_detected_next) begin
        has_error <= 1'b1;
        error_code <= error_code_next;
        state <= STATE_IDLE;
        process2_cycle_counter <= 32'd0;
      end else begin
        case (state)
          STATE_IDLE: begin
            process2_cycle_counter <= 32'd0;
            if (start) begin
              has_error <= 1'b0;
              error_code <= ERR_NONE;
              state <= STATE_PROCESS1;
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
    aw_active = (state == STATE_PROCESS3) && write_buf_valid && !write_buf_aw_sent;
    w_active = (state == STATE_PROCESS3) && write_buf_valid && write_buf_aw_sent;

    if (state == STATE_PROCESS3) begin
      M_AXI_AWADDR = write_buf_addr;
      M_AXI_AWVALID = aw_active;
      M_AXI_WDATA = write_buf_data;
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
      write_buf_addr <= 32'd0;
      write_buf_data <= 64'd0;
      write_buf_valid <= 1'b0;
      write_buf_aw_sent <= 1'b0;
    end else if (state != STATE_PROCESS3) begin 
      write_addr_sent_num <= 32'd0;
      write_data_sent_num <= 32'd0;
      write_response_received_num <= 32'd0;
      write_buf_addr <= 32'd0;
      write_buf_data <= 64'd0;
      write_buf_valid <= 1'b0;
      write_buf_aw_sent <= 1'b0;
    end else begin
      if (!write_buf_valid && process3_stage4_valid
          && ((write_data_sent_num * 32'd4) < data_length)) begin
        write_buf_addr <= dst_addr_base[31:0] + (write_data_sent_num * 32'd8);
        write_buf_data <= Process3_Output_Vector;
        write_buf_valid <= 1'b1;
        write_buf_aw_sent <= 1'b0;
      end

      if (aw_active && M_AXI_AWREADY) begin
        write_addr_sent_num <= write_addr_sent_num + 32'd1;
        write_buf_aw_sent <= 1'b1;
      end

      if (w_active && M_AXI_WREADY) begin
        write_data_sent_num <= write_data_sent_num + 32'd1;
        write_buf_valid <= 1'b0;
        write_buf_aw_sent <= 1'b0;
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
