`timescale 1ns/1ps

module Max_FIFO_test;
  localparam int FIFO_DEPTH = 1024;

  logic        clk;
  logic        rst;
  logic [15:0] data_in;
  logic        write_en;
  logic [15:0] data_out;
  logic        read_ready;
  logic        read_valid;
  logic        clear;
  logic        full;
  logic        empty;
  logic [11:0] count;

  int cycle_count;
  int error_count;
  int handshake_next_error_count;
  int count_mismatch_count;
  int stress_error_count;
  bit verbose_log;

  Max_FIFO dut (
    .clk(clk), .rst_n(!rst),
    .data_in(data_in), .write_en(write_en),
    .data_out(data_out), .read_ready(read_ready), .read_valid(read_valid),
    .clear(clear), .full(full), .empty(empty), .count(count)
  );

  typedef struct {
    int due_cycle;
    logic [15:0] data;
  } expected_t;

  logic [15:0] model_fifo[$];
  logic        model_skid_valid;
  logic [15:0] model_skid_data;
  logic        model_sram_output_data_valid;
  logic [15:0] model_sram_rdata;

  expected_t pending_next_cycle_checks[$];
  expected_t pending_handshake_next_checks[$];

  logic [15:0] write_accepted_order[$];
  logic [15:0] read_consumed_order[$];

  task automatic reset_model;
    begin
      model_fifo.delete();
      model_skid_valid = 1'b0;
      model_skid_data = 16'd0;
      model_sram_output_data_valid = 1'b0;
      model_sram_rdata = 16'd0;
      pending_next_cycle_checks.delete();
      pending_handshake_next_checks.delete();
      write_accepted_order.delete();
      read_consumed_order.delete();
      error_count = 0;
      handshake_next_error_count = 0;
      count_mismatch_count = 0;
    end
  endtask

  task automatic check_due_expectations;
    expected_t item;
    logic pass;
    begin
      while ((pending_next_cycle_checks.size() > 0)
          && (pending_next_cycle_checks[0].due_cycle == cycle_count)) begin
        item = pending_next_cycle_checks.pop_front();
        pass = read_valid && (data_out == item.data);
        if (!pass) begin
          $display("[FAIL][NEXT] cycle=%0d exp=0x%04h got=0x%04h rv=%0b",
                   cycle_count, item.data, data_out, read_valid);
          error_count = error_count + 1;
        end
      end

      while ((pending_handshake_next_checks.size() > 0)
          && (pending_handshake_next_checks[0].due_cycle == cycle_count)) begin
        item = pending_handshake_next_checks.pop_front();
        pass = read_valid && (data_out == item.data);
        if (!pass) begin
          $display("[FAIL][HS->NXT] cycle=%0d exp=0x%04h got=0x%04h rv=%0b",
                   cycle_count, item.data, data_out, read_valid);
          handshake_next_error_count = handshake_next_error_count + 1;
        end
      end
    end
  endtask

  task automatic check_count_consistency;
    int model_count;
    begin
      model_count = model_fifo.size()
                  + (model_skid_valid ? 1 : 0)
                  + (model_sram_output_data_valid ? 1 : 0);
      if (count !== model_count[11:0]) begin
        $display("[FAIL][COUNT] cycle=%0d dut=%0d model=%0d", cycle_count, count, model_count);
        count_mismatch_count = count_mismatch_count + 1;
      end
    end
  endtask

  task automatic model_step(input bit write_en_i, input logic [15:0] data_in_i, input bit read_ready_i);
    bit fifo_full;
    bit fifo_empty;
    int buffered;
    bit issue_read;
    logic [15:0] issued_data;
    expected_t item;
    bit skid_valid_next;
    logic [15:0] skid_data_next;
    bit bypass_consumed;
    begin
      buffered = (model_skid_valid ? 1 : 0) + (model_sram_output_data_valid ? 1 : 0);
      fifo_full = ((model_fifo.size() + buffered) >= FIFO_DEPTH);
      fifo_empty = (model_fifo.size() == 0);

      issue_read = 1'b0;
      if (!fifo_empty) begin
        if (read_ready_i) issue_read = (buffered < 2);
        else issue_read = (!model_skid_valid && !model_sram_output_data_valid);
      end

      issued_data = 16'd0;
      if (issue_read) begin
        issued_data = model_fifo.pop_front();
        item.due_cycle = cycle_count + 1;
        item.data = issued_data;
        pending_next_cycle_checks.push_back(item);
      end

      if (write_en_i && !fifo_full) begin
        model_fifo.push_back(data_in_i);
        write_accepted_order.push_back(data_in_i);
      end

      skid_valid_next = model_skid_valid;
      skid_data_next = model_skid_data;

      if (read_ready_i && skid_valid_next) begin
        skid_valid_next = 1'b0;
      end

      if (model_sram_output_data_valid) begin
        bypass_consumed = read_ready_i && !model_skid_valid;
        if (!bypass_consumed) begin
          skid_valid_next = 1'b1;
          skid_data_next = model_sram_rdata;
        end
      end

      model_skid_valid = skid_valid_next;
      model_skid_data = skid_data_next;
      model_sram_output_data_valid = issue_read;
      if (issue_read) model_sram_rdata = issued_data;
    end
  endtask

  task automatic step_basic(input bit write_en_i, input logic [15:0] data_in_i, input bit read_ready_i, input [127:0] note);
    begin
      write_en = write_en_i;
      data_in = data_in_i;
      read_ready = read_ready_i;
      clear = 1'b0;

      @(posedge clk);
      #1;
      cycle_count = cycle_count + 1;

      if (verbose_log && ((cycle_count < 100) || ((cycle_count % 128) == 0))) begin
        $display("[CYCLE %0d] %0s in=0x%04h we=%0b out=0x%04h rr=%0b rv=%0b cnt=%0d f=%0b e=%0b",
                 cycle_count, note, data_in, write_en, data_out, read_ready, read_valid, count, full, empty);
      end
    end
  endtask

  task automatic apply_cycle(input bit write_en_i, input logic [15:0] data_in_i, input bit read_ready_i, input [127:0] note);
    expected_t item;
    int consumed;
    begin
      write_en = write_en_i;
      data_in = data_in_i;
      read_ready = read_ready_i;
      clear = 1'b0;

      @(posedge clk);
      #0;

      cycle_count = cycle_count + 1;
      if (verbose_log) begin
        $display("[CYCLE %0d] %0s in=0x%04h we=%0b out=0x%04h rr=%0b rv=%0b cnt=%0d",
                 cycle_count, note, data_in, write_en, data_out, read_ready, read_valid, count);
      end

      check_due_expectations();

      if (read_ready && read_valid) begin
        read_consumed_order.push_back(data_out);
        consumed = read_consumed_order.size();
        if (consumed < write_accepted_order.size()) begin
          item.due_cycle = cycle_count + 1;
          item.data = write_accepted_order[consumed];
          pending_handshake_next_checks.push_back(item);
        end
      end

      if (!rst) begin
        model_step(write_en_i, data_in_i, read_ready_i);
      end

      #1;
      check_count_consistency();
    end
  endtask

  always #5 clk = ~clk;

  initial begin
    bit read_pattern [0:29];
    bit same_order;
    int first_mismatch;
    int i;
    int compare_len;

    clk = 0;
    rst = 1;
    write_en = 0;
    read_ready = 0;
    clear = 0;
    data_in = 0;
    cycle_count = 0;
    stress_error_count = 0;
    verbose_log = 1'b1;

    reset_model();

    apply_cycle(0, 16'h0000, 0, "reset cycle 0");
    apply_cycle(0, 16'h0000, 0, "reset cycle 1");

    rst = 0;
    reset_model();

    // Phase 1: 10-cycle continuous write burst.
    for (i = 0; i < 10; i = i + 1) begin
      apply_cycle(1, (16'h4100 + (i << 4)), 0, "write phase");
    end
    apply_cycle(0, 16'h0000, 0, "write->read transition idle");

    // Phase 2: irregular read_ready pattern, no writes.
    read_pattern[0] = 1;  read_pattern[1] = 1;  read_pattern[2] = 0;  read_pattern[3] = 0;
    read_pattern[4] = 1;  read_pattern[5] = 0;  read_pattern[6] = 1;  read_pattern[7] = 1;
    read_pattern[8] = 0;  read_pattern[9] = 1;  read_pattern[10] = 0; read_pattern[11] = 0;
    read_pattern[12] = 1; read_pattern[13] = 1; read_pattern[14] = 1; read_pattern[15] = 0;
    read_pattern[16] = 1; read_pattern[17] = 0; read_pattern[18] = 1; read_pattern[19] = 1;
    read_pattern[20] = 0; read_pattern[21] = 0; read_pattern[22] = 1; read_pattern[23] = 1;
    read_pattern[24] = 1; read_pattern[25] = 0; read_pattern[26] = 1; read_pattern[27] = 0;
    read_pattern[28] = 1; read_pattern[29] = 1;

    for (i = 0; i < 30; i = i + 1) begin
      apply_cycle(0, 16'h0000, read_pattern[i], "read phase irregular read_ready");
    end

    // Phase 3: capacity/flag/overflow stress test (1024 entries).
    rst = 1'b1;
    reset_model();
    apply_cycle(0, 16'h0000, 0, "stress reset 0");
    apply_cycle(0, 16'h0000, 0, "stress reset 1");
    rst = 1'b0;
    reset_model();
    apply_cycle(0, 16'h0000, 0, "stress reset release");

    verbose_log = 1'b0;
    for (i = 0; i < FIFO_DEPTH; i = i + 1) begin
      apply_cycle(1, (16'h6000 + i[15:0]), 0, "fill-1024");
    end
    verbose_log = 1'b1;

    if (!full) begin
      $display("[FAIL][STRESS] full should assert after %0d writes", FIFO_DEPTH);
      stress_error_count = stress_error_count + 1;
    end
    if (empty) begin
      $display("[FAIL][STRESS] empty should deassert after fill");
      stress_error_count = stress_error_count + 1;
    end
    if (count != FIFO_DEPTH) begin
      $display("[FAIL][STRESS] count after fill exp=%0d got=%0d", FIFO_DEPTH, count);
      stress_error_count = stress_error_count + 1;
    end

    // Overflow protection: writes while full must be ignored.
    verbose_log = 1'b0;
    for (i = 0; i < 8; i = i + 1) begin
      apply_cycle(1, (16'hDE00 + i[15:0]), 0, "overflow-write-guard");
    end
    verbose_log = 1'b1;
    if (!full || (count != FIFO_DEPTH)) begin
      $display("[FAIL][STRESS] overflow write changed fifo state full=%0b count=%0d", full, count);
      stress_error_count = stress_error_count + 1;
    end

    // Drain all 1024 entries.
    verbose_log = 1'b0;
    while ((read_consumed_order.size() < FIFO_DEPTH) && (cycle_count < 7000)) begin
      apply_cycle(0, 16'h0000, 1, "drain-1024");
    end
    verbose_log = 1'b1;

    if (read_consumed_order.size() != FIFO_DEPTH) begin
      $display("[FAIL][STRESS] drain count exp=%0d got=%0d", FIFO_DEPTH, read_consumed_order.size());
      stress_error_count = stress_error_count + 1;
    end

    for (i = 0; i < FIFO_DEPTH; i = i + 1) begin
      if (read_consumed_order[i] !== (16'h6000 + i[15:0])) begin
        $display("[FAIL][STRESS] drain order idx=%0d exp=0x%04h got=0x%04h",
                 i, (16'h6000 + i[15:0]), read_consumed_order[i]);
        stress_error_count = stress_error_count + 1;
        i = FIFO_DEPTH;
      end
    end

    apply_cycle(0, 16'h0000, 1, "post-drain check");
    if (!empty || full || (count != 0)) begin
      $display("[FAIL][STRESS] empty/full/count abnormal after drain e=%0b f=%0b c=%0d", empty, full, count);
      stress_error_count = stress_error_count + 1;
    end
    if (read_valid) begin
      $display("[FAIL][STRESS] read_valid should be 0 after full drain");
      stress_error_count = stress_error_count + 1;
    end

    same_order = (write_accepted_order.size() == read_consumed_order.size());
    first_mismatch = -1;
    compare_len = (write_accepted_order.size() < read_consumed_order.size())
                ? write_accepted_order.size() : read_consumed_order.size();
    for (i = 0; i < compare_len; i = i + 1) begin
      if (write_accepted_order[i] !== read_consumed_order[i]) begin
        same_order = 0;
        first_mismatch = i;
        i = compare_len;
      end
    end

    $display("[SUMMARY] cycles=%0d next_err=%0d hs_next_err=%0d count_err=%0d pending_next=%0d pending_hs=%0d",
             cycle_count,
             error_count,
             handshake_next_error_count,
             count_mismatch_count,
             pending_next_cycle_checks.size(),
             pending_handshake_next_checks.size());

    $display("[SUMMARY] write_count=%0d read_count=%0d same_order=%0b first_mismatch=%0d",
             write_accepted_order.size(), read_consumed_order.size(), same_order, first_mismatch);
    $display("[SUMMARY] stress_errors=%0d", stress_error_count);

    if ((error_count == 0)
      && (handshake_next_error_count == 0)
      && (count_mismatch_count == 0)
      && (pending_next_cycle_checks.size() == 0)
      && (pending_handshake_next_checks.size() == 0)
      && same_order
      && (stress_error_count == 0)) begin
      $display("[Max_FIFO RTL TEST] PASS");
    end else begin
      $display("[Max_FIFO RTL TEST] FAIL");
      $fatal(1);
    end

    $finish;
  end
endmodule
