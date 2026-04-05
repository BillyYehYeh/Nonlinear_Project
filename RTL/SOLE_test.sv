`timescale 1ns/1ps
`include "sole_pkg.sv"

module AxiSlaveMemory #(
  parameter int TEST_DATA_SIZE = 2048
) (
  input  logic        clk,
  input  logic        rst,
  input  logic [31:0] S_AXI_AWADDR,
  input  logic        S_AXI_AWVALID,
  output logic        S_AXI_AWREADY,
  input  logic [63:0] S_AXI_WDATA,
  input  logic [7:0]  S_AXI_WSTRB,
  input  logic        S_AXI_WVALID,
  output logic        S_AXI_WREADY,
  output logic [1:0]  S_AXI_BRESP,
  output logic        S_AXI_BVALID,
  input  logic        S_AXI_BREADY,
  input  logic [31:0] S_AXI_ARADDR,
  input  logic        S_AXI_ARVALID,
  output logic        S_AXI_ARREADY,
  output logic [63:0] S_AXI_RDATA,
  output logic [1:0]  S_AXI_RRESP,
  output logic        S_AXI_RVALID,
  input  logic        S_AXI_RREADY
);
  import sole_pkg::*;
  logic [63:0] memory [0:TEST_DATA_SIZE-1];
  logic [31:0] awaddr_q;
  logic        awaddr_valid;
  integer wi;

  initial begin
    for (wi = 0; wi < TEST_DATA_SIZE; wi++) memory[wi] = 64'd0;
  end

  always_ff @(posedge clk) begin
    if (rst) begin
      S_AXI_AWREADY <= 1'b0;
      S_AXI_WREADY <= 1'b0;
      S_AXI_BVALID <= 1'b0;
      S_AXI_BRESP <= AXI_RESP_OKAY;
      S_AXI_ARREADY <= 1'b0;
      S_AXI_RVALID <= 1'b0;
      S_AXI_RRESP <= AXI_RESP_OKAY;
      S_AXI_RDATA <= 64'd0;
      awaddr_q <= 32'd0;
      awaddr_valid <= 1'b0;
    end else begin
      S_AXI_AWREADY <= 1'b1;
      S_AXI_WREADY <= 1'b1;
      S_AXI_ARREADY <= 1'b1;

      if (S_AXI_AWVALID && S_AXI_AWREADY) begin
        awaddr_q <= S_AXI_AWADDR;
        awaddr_valid <= 1'b1;
      end

      if (S_AXI_WVALID && S_AXI_WREADY) begin
        logic [31:0] wr_addr;
        // Match SystemC behavior: when AW/W handshake in same cycle, pair them directly.
        if (S_AXI_AWVALID && S_AXI_AWREADY) begin
          wr_addr = S_AXI_AWADDR;
        end else if (awaddr_valid) begin
          wr_addr = awaddr_q;
        end else begin
          wr_addr = 32'd0;
        end

        if ((wr_addr >> 3) < TEST_DATA_SIZE) begin
          memory[wr_addr >> 3] <= S_AXI_WDATA;
          S_AXI_BRESP <= AXI_RESP_OKAY;
        end else begin
          S_AXI_BRESP <= AXI_RESP_SLVERR;
        end
        awaddr_valid <= 1'b0;
        S_AXI_BVALID <= 1'b1;
      end else if (S_AXI_BVALID && S_AXI_BREADY) begin
        S_AXI_BVALID <= 1'b0;
      end

      if (S_AXI_ARVALID && S_AXI_ARREADY) begin
        if ((S_AXI_ARADDR >> 3) < TEST_DATA_SIZE) begin
          S_AXI_RDATA <= memory[S_AXI_ARADDR >> 3];
          S_AXI_RRESP <= AXI_RESP_OKAY;
        end else begin
          S_AXI_RDATA <= 64'd0;
          S_AXI_RRESP <= AXI_RESP_SLVERR;
        end
        S_AXI_RVALID <= 1'b1;
      end else if (S_AXI_RVALID && S_AXI_RREADY) begin
        S_AXI_RVALID <= 1'b0;
      end
    end
  end
endmodule

module SOLE_test;
  import sole_pkg::*;
  logic clk;
  logic rst;
  logic [31:0] proc_addr;
  logic [31:0] proc_wdata;
  logic proc_we;
  logic [31:0] proc_rdata;
  logic interrupt;

  logic [31:0] M_AXI_AWADDR;
  logic M_AXI_AWVALID;
  logic M_AXI_AWREADY;
  logic [63:0] M_AXI_WDATA;
  logic [7:0] M_AXI_WSTRB;
  logic M_AXI_WVALID;
  logic M_AXI_WREADY;
  logic [1:0] M_AXI_BRESP;
  logic M_AXI_BVALID;
  logic M_AXI_BREADY;
  logic [31:0] M_AXI_ARADDR;
  logic M_AXI_ARVALID;
  logic M_AXI_ARREADY;
  logic [63:0] M_AXI_RDATA;
  logic [1:0] M_AXI_RRESP;
  logic M_AXI_RVALID;
  logic M_AXI_RREADY;

  localparam int INPUT_START_WORD = 100;
  localparam int OUTPUT_START_WORD = 500;
  localparam int NUM_DATA = 100;
  localparam int NUM_WORDS = (NUM_DATA + 3) / 4;
  localparam real ERR_TOL = 0.005;

  integer i, w, e;
  reg [63:0] packed_word;
  real got;
  real expv;
  real abs_err;
  real max_err;
  integer failures;
  reg [8*256-1:0] data_file_path;
  integer test_log_fh;
  integer monitor_log_fh;
  integer last_status;
  integer start_time_ns;
  integer done_time_ns;
  real sum_hw_output;
  real sum_sw_output;
  real cosine_dot;
  real cosine_norm_hw;
  real cosine_norm_sw;
  real cosine;

  real hw_output [0:NUM_DATA-1];
  real sw_output [0:NUM_DATA-1];
  real input_data [0:NUM_DATA-1];

  SOLE dut (
    .clk(clk), .rst(rst),
    .proc_addr(proc_addr), .proc_wdata(proc_wdata), .proc_we(proc_we), .proc_rdata(proc_rdata), .interrupt(interrupt),
    .M_AXI_AWADDR(M_AXI_AWADDR), .M_AXI_AWVALID(M_AXI_AWVALID), .M_AXI_AWREADY(M_AXI_AWREADY),
    .M_AXI_WDATA(M_AXI_WDATA), .M_AXI_WSTRB(M_AXI_WSTRB), .M_AXI_WVALID(M_AXI_WVALID), .M_AXI_WREADY(M_AXI_WREADY),
    .M_AXI_BRESP(M_AXI_BRESP), .M_AXI_BVALID(M_AXI_BVALID), .M_AXI_BREADY(M_AXI_BREADY),
    .M_AXI_ARADDR(M_AXI_ARADDR), .M_AXI_ARVALID(M_AXI_ARVALID), .M_AXI_ARREADY(M_AXI_ARREADY),
    .M_AXI_RDATA(M_AXI_RDATA), .M_AXI_RRESP(M_AXI_RRESP), .M_AXI_RVALID(M_AXI_RVALID), .M_AXI_RREADY(M_AXI_RREADY)
  );

  AxiSlaveMemory mem (
    .clk(clk), .rst(rst),
    .S_AXI_AWADDR(M_AXI_AWADDR), .S_AXI_AWVALID(M_AXI_AWVALID), .S_AXI_AWREADY(M_AXI_AWREADY),
    .S_AXI_WDATA(M_AXI_WDATA), .S_AXI_WSTRB(M_AXI_WSTRB), .S_AXI_WVALID(M_AXI_WVALID), .S_AXI_WREADY(M_AXI_WREADY),
    .S_AXI_BRESP(M_AXI_BRESP), .S_AXI_BVALID(M_AXI_BVALID), .S_AXI_BREADY(M_AXI_BREADY),
    .S_AXI_ARADDR(M_AXI_ARADDR), .S_AXI_ARVALID(M_AXI_ARVALID), .S_AXI_ARREADY(M_AXI_ARREADY),
    .S_AXI_RDATA(M_AXI_RDATA), .S_AXI_RRESP(M_AXI_RRESP), .S_AXI_RVALID(M_AXI_RVALID), .S_AXI_RREADY(M_AXI_RREADY)
  );

  task mmio_write(input logic [31:0] addr, input logic [31:0] data);
    begin
      @(posedge clk);
      proc_addr <= addr;
      proc_wdata <= data;
      proc_we <= 1'b1;
      @(posedge clk);
      proc_we <= 1'b0;
    end
  endtask

  function [8*16-1:0] status_state_name(input integer st);
    begin
      case ((st >> 1) & 3)
        0: status_state_name = "IDLE";
        1: status_state_name = "PROCESS1";
        2: status_state_name = "PROCESS2";
        3: status_state_name = "PROCESS3";
        default: status_state_name = "UNKNOWN";
      endcase
    end
  endfunction

  task dump_memory_words(input integer fh, input integer start_word, input integer nwords);
    begin
      $fdisplay(fh, "\n[MEMORY DUMP]");
      $fdisplay(fh, " Address(word) | Data (Hex)");
      for (w = 0; w < nwords; w = w + 1) begin
        $fdisplay(fh, "   %5d  :  0x %04h %04h %04h %04h",
                  start_word + w,
                  mem.memory[start_word + w][63:48],
                  mem.memory[start_word + w][47:32],
                  mem.memory[start_word + w][31:16],
                  mem.memory[start_word + w][15:0]);
      end
    end
  endtask

  task dump_sole_mmio(input integer fh);
    begin
      $fdisplay(fh, "\n[SOLE MMIO DUMP]");
      $fdisplay(fh, "Control Register: 0x%08h", dut.reg_control);
      $fdisplay(fh, "Status Register: 0x%08h", dut.reg_status);
      $fdisplay(fh, "Src Addr Base Low: 0x%08h", dut.reg_src_addr_base_l);
      $fdisplay(fh, "Src Addr Base High: 0x%08h", dut.reg_src_addr_base_h);
      $fdisplay(fh, "Src Addr Base (64-bit): 0x%016h", {dut.reg_src_addr_base_h, dut.reg_src_addr_base_l});
      $fdisplay(fh, "Dst Addr Base Low: 0x%08h", dut.reg_dst_addr_base_l);
      $fdisplay(fh, "Dst Addr Base High: 0x%08h", dut.reg_dst_addr_base_h);
      $fdisplay(fh, "Dst Addr Base (64-bit): 0x%016h", {dut.reg_dst_addr_base_h, dut.reg_dst_addr_base_l});
      $fdisplay(fh, "Data Length Low: 0x%08h", dut.reg_length_l);
      $fdisplay(fh, "Data Length High: 0x%08h", dut.reg_length_h);
      $fdisplay(fh, "Data Length (64-bit): 0x%016h", {dut.reg_length_h, dut.reg_length_l});
    end
  endtask

  function automatic real expected_sole_softmax(input int i, input int n);
    int m;
    int j;
    int sum_i;
    int lop;
    real maxv;
    real maxq;
    real xq;
    real y;
    real muxv;
    real divv;
    begin
      maxv = input_data[0];
      for (j = 1; j < n; j++) begin
        if (input_data[j] > maxv) maxv = input_data[j];
      end
      maxq = $floor(maxv * 65536.0) / 65536.0;

      sum_i = 0;
      for (j = 0; j < n; j++) begin
        xq = $floor(input_data[j] * 65536.0) / 65536.0;
        y = (xq - maxq) * 1.4375;
        m = $rtoi(-y);
        if (m < 0) m = 0;
        if (m > 16) m = 16;
        sum_i += (65536 >> m);
      end

      xq = $floor(input_data[i] * 65536.0) / 65536.0;
      y = (xq - maxq) * 1.4375;
      m = $rtoi(-y);
      if (m < 0) m = 0;
      if (m > 16) m = 16;
      lop = leading_one_pos32(sum_i);
      if (lop > 0 && ((sum_i >> (lop - 1)) & 1)) muxv = 0.568;
      else muxv = 0.818;
      divv = 2.0 ** ((lop - 16) + m);
      expected_sole_softmax = muxv / divv;
    end
  endfunction

  function automatic integer sim_time_ns();
    begin
      sim_time_ns = $rtoi($realtime);
    end
  endfunction

  // Continuous status monitor (similar to SystemC monitor log)
  always @(posedge clk) begin
    integer st;
    st = dut.reg_status;
    if (st != last_status) begin
      $display("[STATUS_HW] 0x%08h | done=%0d | state=%0s | error=%0d | error_code=0x%0h @%0d ns",
               st,
               (st & 1),
               status_state_name(st),
               ((st >> 3) & 1),
               ((st >> 4) & 4'hF),
               sim_time_ns());
      if (monitor_log_fh != 0) begin
        $fdisplay(monitor_log_fh,
                  "[STATUS_HW] 0x%08h | done=%0d | state=%0s | error=%0d | error_code=0x%0h @%0d ns",
                  st,
                  (st & 1),
                  status_state_name(st),
                  ((st >> 3) & 1),
                  ((st >> 4) & 4'hF),
                  sim_time_ns());
      end
      last_status = st;
    end
  end

  always @(posedge clk) begin
    if (interrupt) begin
      if (monitor_log_fh != 0) begin
        $fdisplay(monitor_log_fh,
                  "[INTERRUPT] interrupt=1 | done=%0d | error=%0d | status=0x%08h @%0d ns",
                  dut.reg_status[0], dut.reg_status[3], dut.reg_status, sim_time_ns());
      end
    end
  end

  initial begin
    integer data_fh;
    integer rc;
    real in_val;
    clk = 0;
    rst = 1;
    proc_addr = 0;
    proc_wdata = 0;
    proc_we = 0;
    max_err = 0.0;
    failures = 0;
    last_status = 32'hFFFF_FFFF;
    sum_hw_output = 0.0;
    sum_sw_output = 0.0;
    cosine_dot = 0.0;
    cosine_norm_hw = 0.0;
    cosine_norm_sw = 0.0;

    test_log_fh = $fopen("SOLE_test_Result_rtl.log", "w");
    monitor_log_fh = $fopen("SOLE_test_Monitor_rtl.log", "w");
    if (test_log_fh == 0) begin
      $display("[ERROR] Failed to create SOLE_test_Result_rtl.log");
      $fatal(1);
    end
    if (monitor_log_fh == 0) begin
      $display("[ERROR] Failed to create SOLE_test_Monitor_rtl.log");
      $fatal(1);
    end

    $fdisplay(test_log_fh, "===== SOLE TEST LOG =====");
    $fdisplay(monitor_log_fh, "[CONTINUOUS MONITOR STARTED]");

    data_file_path = "SOLE_test_Data_rtl.txt";
    if ($value$plusargs("DATA_FILE=%s", data_file_path)) begin
      $display("[RTL TEST] Using DATA_FILE=%0s", data_file_path);
    end else begin
      $display("[RTL TEST] Using default DATA_FILE=%0s", data_file_path);
    end

    data_fh = $fopen(data_file_path, "r");
    if (data_fh == 0) begin
      $display("[ERROR] Failed to open input data file: %0s", data_file_path);
      $fatal(1);
    end
    for (i = 0; i < NUM_DATA; i = i + 1) begin
      rc = $fscanf(data_fh, "%f", in_val);
      if (rc != 1) begin
        $display("[ERROR] Input data file parse failed at index %0d (%0s)", i, data_file_path);
        $fatal(1);
      end
      input_data[i] = in_val;
    end
    $fclose(data_fh);

    repeat (10) @(posedge clk);
    rst = 0;

    $fdisplay(test_log_fh, "\n[1] Memory Input Data Write");
    $fdisplay(test_log_fh, "Index | InputFloat | InputFP16Hex | MemWordIdx | ElemInWord");
    for (w = 0; w < NUM_WORDS; w++) begin
      packed_word = 64'd0;
      for (e = 0; e < 4; e++) begin
        i = w * 4 + e;
        if (i < NUM_DATA) begin
          packed_word[e*16 +: 16] = real_to_fp16(input_data[i]);
          $fdisplay(test_log_fh, "%4d  %10f       0x%04h       %0d           %0d",
                    i,
                    input_data[i],
                    packed_word[e*16 +: 16],
                    INPUT_START_WORD + w,
                    e);
        end
      end
      mem.memory[INPUT_START_WORD + w] = packed_word;
    end
    dump_memory_words(test_log_fh, INPUT_START_WORD, NUM_WORDS);

    $fdisplay(test_log_fh, "\n[2] Testbench MMIO Configuration");
    $fdisplay(test_log_fh, "TimeNs,Reg,ValueHex,Note");

    mmio_write(REG_SRC_ADDR_BASE_L, INPUT_START_WORD * 8);
    $fdisplay(test_log_fh, "%0d ns,REG_SRC_ADDR_BASE_L,0x%0h,source base byte address", sim_time_ns(), INPUT_START_WORD * 8);
    mmio_write(REG_SRC_ADDR_BASE_H, 0);
    $fdisplay(test_log_fh, "%0d ns,REG_SRC_ADDR_BASE_H,0x0,source high", sim_time_ns());
    mmio_write(REG_DST_ADDR_BASE_L, OUTPUT_START_WORD * 8);
    $fdisplay(test_log_fh, "%0d ns,REG_DST_ADDR_BASE_L,0x%0h,destination base byte address", sim_time_ns(), OUTPUT_START_WORD * 8);
    mmio_write(REG_DST_ADDR_BASE_H, 0);
    $fdisplay(test_log_fh, "%0d ns,REG_DST_ADDR_BASE_H,0x0,destination high", sim_time_ns());
    mmio_write(REG_LENGTH_L, NUM_DATA);
    $fdisplay(test_log_fh, "%0d ns,REG_LENGTH_L,0x%0h,number of FP16 elements", sim_time_ns(), NUM_DATA);
    mmio_write(REG_LENGTH_H, 0);
    $fdisplay(test_log_fh, "%0d ns,REG_LENGTH_H,0x0,length high", sim_time_ns());
    start_time_ns = sim_time_ns();
    mmio_write(REG_CONTROL, 32'h0000_0001);
    $fdisplay(test_log_fh, "%0d ns,REG_CONTROL,0x1,mode=softmax start=1", sim_time_ns());
    mmio_write(REG_CONTROL, 32'h0000_0000);

    dump_sole_mmio(test_log_fh);

    $fdisplay(test_log_fh, "\n[3] Softmax Start Running");
    $fdisplay(test_log_fh, "TimeNs,Status,State,Done,ErrorCode");
    $fdisplay(test_log_fh, "\n[Stage 1: Waiting for PROCESS1 state]");
    while (((dut.reg_status >> 1) & 3) != 1) @(posedge clk);
    $fdisplay(test_log_fh, "time: %0d ns Event: Entered PROCESS1", sim_time_ns());

    $fdisplay(test_log_fh, "\n[Stage 2: Waiting for PROCESS2 state]");
    while (((dut.reg_status >> 1) & 3) != 2) @(posedge clk);
    $fdisplay(test_log_fh, "time: %0d ns Event: Entered PROCESS2", sim_time_ns());

    $fdisplay(test_log_fh, "\n[Stage 3: Waiting for PROCESS3 state]");
    while (((dut.reg_status >> 1) & 3) != 3) @(posedge clk);
    $fdisplay(test_log_fh, "time: %0d ns Event: Entered PROCESS3", sim_time_ns());

    $fdisplay(test_log_fh, "\n[Stage 4: Waiting for softmax_done]");
    $fdisplay(test_log_fh, "Stage 4a baseline status: 0x%08h @%0d ns", dut.reg_status, sim_time_ns());
    $fdisplay(test_log_fh, "Stage 4b: Entering tight polling loop (direct HW register read)...");

    for (i = 0; i < 20000; i = i + 1) begin
      @(posedge clk);
      if (interrupt === 1'b1) begin
        done_time_ns = sim_time_ns();
        i = 20000;
      end
    end
    if (interrupt !== 1'b1) begin
      $display("[RTL TEST] FAIL: timeout waiting for interrupt");
      $fatal(1);
    end
    repeat (5) @(posedge clk);

    $fdisplay(test_log_fh, "time: %0d ns Event: DONE pulse detected!", done_time_ns);
    $fdisplay(test_log_fh, "[TIMING] Start time: %0d ns", start_time_ns);
    $fdisplay(test_log_fh, "[TIMING] Done time: %0d ns", done_time_ns);
    $fdisplay(test_log_fh, "[TIMING] Execution time: %0d ns", done_time_ns - start_time_ns);

    $fdisplay(test_log_fh, "\n[5] Output Stored Back to Memory via AXI4_Lite");
    dump_memory_words(test_log_fh, OUTPUT_START_WORD, NUM_WORDS);

    $display("[MEM DUMP] OUTPUT memory words from %0d, count=%0d", OUTPUT_START_WORD, NUM_WORDS);
    for (w = 0; w < NUM_WORDS; w = w + 1) begin
      $display("[MEM] word=%0d data=0x%04h_%04h_%04h_%04h",
               OUTPUT_START_WORD + w,
               mem.memory[OUTPUT_START_WORD + w][63:48],
               mem.memory[OUTPUT_START_WORD + w][47:32],
               mem.memory[OUTPUT_START_WORD + w][31:16],
               mem.memory[OUTPUT_START_WORD + w][15:0]);
    end

    $fdisplay(test_log_fh, "\n[4] Softmax Compute Results");
    $fdisplay(test_log_fh, "Index |  Input  |  HW_Output  |  SW_Output  |  AbsError");

    for (i = 0; i < NUM_DATA; i++) begin
      w = OUTPUT_START_WORD + (i / 4);
      e = i % 4;
      got = fp16_to_real(mem.memory[w][e*16 +: 16]);
      expv = expected_sole_softmax(i, NUM_DATA);
      hw_output[i] = got;
      sw_output[i] = expv;
      abs_err = (got > expv) ? (got - expv) : (expv - got);
      sum_hw_output = sum_hw_output + got;
      sum_sw_output = sum_sw_output + expv;
      cosine_dot = cosine_dot + (got * expv);
      cosine_norm_hw = cosine_norm_hw + (got * got);
      cosine_norm_sw = cosine_norm_sw + (expv * expv);

      $fdisplay(test_log_fh, "%3d    %7f  %12.9f  %12.9f   %12.6e",
                i, input_data[i], got, expv, abs_err);

      if (abs_err > max_err) max_err = abs_err;
      if (abs_err > ERR_TOL) begin
        failures++;
        if (failures <= 12) begin
          $display("[RTL TEST][MISMATCH] idx=%0d got=%f exp=%f abs_err=%f word=%0d lane=%0d", i, got, expv, abs_err, w, e);
        end
      end
    end

    if (cosine_norm_hw > 0.0 && cosine_norm_sw > 0.0) begin
      cosine = cosine_dot / ($sqrt(cosine_norm_hw) * $sqrt(cosine_norm_sw));
    end else begin
      cosine = 0.0;
    end

    $fdisplay(test_log_fh, "\n================== FINAL REPORT ================= ");
    $fdisplay(test_log_fh, "[EXECUTION TIME] SOLE Execution Time: %0d ns", done_time_ns - start_time_ns);
    $fdisplay(test_log_fh, "\n[ANALYSIS] Cosine Similarity (HW vs SW) = %0.9f", cosine);
    $fdisplay(test_log_fh, "\n[ANALYSIS] Sum & MaxAbsError");
    $fdisplay(test_log_fh, "HW_Sum=%0.9f", sum_hw_output);
    $fdisplay(test_log_fh, "SW_Sum=%0.9f", sum_sw_output);
    $fdisplay(test_log_fh, "MaxAbsError=%0.9e", max_err);
    $fdisplay(test_log_fh, "\n============ END OF SOLE TEST LOG ============");

    $display("[RTL TEST] NUM_DATA=%0d max_err=%f failures=%0d", NUM_DATA, max_err, failures);
    if (failures == 0) begin
      $display("[RTL TEST] PASS: input 1..100 output is correct within tolerance");
    end else begin
      $display("[RTL TEST] FAIL");
      $fatal(1);
    end
    $fdisplay(monitor_log_fh, "\n[CONTINUOUS MONITOR STOPPED]");
    $fclose(monitor_log_fh);
    $fclose(test_log_fh);
    $finish;
  end

  // 0.5ns high/low => 1ns clock period (aligned with SystemC testbench)
  always #0.5 clk = ~clk;
endmodule
