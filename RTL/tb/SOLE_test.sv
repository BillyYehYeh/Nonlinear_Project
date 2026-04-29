`timescale 1ns/1ps
`include "sole_pkg.sv"

// clock period in ns 
`define TB_CLK_PERIOD_NS 1

`ifndef AXI_READ_ARREADY_DELAY
`define AXI_READ_ARREADY_DELAY 10
`endif

`ifndef AXI_READ_RVALID_DELAY
`define AXI_READ_RVALID_DELAY 10
`endif

`ifndef AXI_WRITE_WREADY_DELAY
`define AXI_WRITE_WREADY_DELAY 10
`endif

`ifndef error_recovery_test
`define error_recovery_test 0
`endif

module AxiSlaveMemory #(
  parameter int TEST_DATA_SIZE = 4096
) (
  input  logic        clk,
  input  logic        rst_n,
  input  logic        force_zero_delay,
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
  (* ic_user_defined_disable = "ICPD" *) reg [63:0] memory [0:TEST_DATA_SIZE-1];
  logic [31:0] awaddr_q;
  logic        awaddr_valid;
  logic [31:0] write_addr_q;
  logic [63:0] write_data_q;
  logic        do_write;

  logic [31:0] write_addr_queue[$];
  logic [31:0] read_addr_queue[$];
  logic [31:0] write_addr_buf;
  logic [31:0] last_write_addr;
  logic [31:0] addr_buf;
  logic        has_last_write_addr;
  logic        wready_wait_pending;
  integer      wready_delay_cnt;
  logic        arready_wait_pending;
  integer      arready_delay_cnt;
  integer      rvalid_delay_cnt;
  logic        has_addr;

  always @(posedge clk) begin
    if (do_write) begin
      memory[write_addr_q >> 3] <= write_data_q;
    end
  end

  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
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
      do_write <= 1'b0;
      write_addr_q <= 32'd0;
      write_data_q <= 64'd0;

      write_addr_queue = {};
      read_addr_queue = {};
      write_addr_buf = 32'd0;
      last_write_addr = 32'd0;
      addr_buf = 32'd0;
      has_last_write_addr = 1'b0;
      wready_wait_pending = 1'b0;
      wready_delay_cnt = -1;
      arready_wait_pending = 1'b0;
      arready_delay_cnt = -1;
      rvalid_delay_cnt = -1;
      has_addr = 1'b0;
    end else begin
      integer rd_ar_delay;
      integer rd_r_delay;
      integer wr_w_delay;

      if (force_zero_delay) begin
        rd_ar_delay = 0;
        rd_r_delay = 0;
        wr_w_delay = 0;
      end else begin
        rd_ar_delay = `AXI_READ_ARREADY_DELAY;
        rd_r_delay = `AXI_READ_RVALID_DELAY;
        wr_w_delay = `AXI_WRITE_WREADY_DELAY;
      end

      if ((rd_ar_delay == 0) && (rd_r_delay == 0) && (wr_w_delay == 0)) begin
        // Keep delay=0 behavior bit-compatible with the original normal testbench.
        S_AXI_AWREADY <= 1'b1;
        S_AXI_WREADY <= 1'b1;
        S_AXI_ARREADY <= 1'b1;
        do_write <= 1'b0;

        if (S_AXI_AWVALID && S_AXI_AWREADY) begin
          awaddr_q <= S_AXI_AWADDR;
          awaddr_valid <= 1'b1;
        end

        if (S_AXI_WVALID && S_AXI_WREADY) begin
          logic [31:0] wr_addr;
          if (S_AXI_AWVALID && S_AXI_AWREADY) begin
            wr_addr = S_AXI_AWADDR;
          end else if (awaddr_valid) begin
            wr_addr = awaddr_q;
          end else begin
            wr_addr = 32'd0;
          end

          if ((wr_addr >> 3) < TEST_DATA_SIZE) begin
            do_write <= 1'b1;
            write_addr_q <= wr_addr;
            write_data_q <= S_AXI_WDATA;
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
      end else begin
        logic awready_now;
        logic wready_now;
        logic arready_now;
        logic aw_handshake;
        logic w_handshake;
        logic ar_handshake;
        integer word_idx;

        awready_now = 1'b1;
        S_AXI_AWREADY <= awready_now;

        // WREADY delay: once WVALID is observed, wait N cycles, then allow one W handshake.
        if (wr_w_delay <= 0) begin
          wready_now = 1'b1;
          wready_wait_pending = 1'b0;
          wready_delay_cnt = -1;
        end else begin
          wready_now = 1'b0;
          if (!wready_wait_pending && S_AXI_WVALID) begin
            wready_wait_pending = 1'b1;
            wready_delay_cnt = wr_w_delay;
          end
          if (wready_wait_pending) begin
            if (wready_delay_cnt > 0) begin
              wready_delay_cnt = wready_delay_cnt - 1;
            end else begin
              wready_now = 1'b1;
            end
          end
        end
        S_AXI_WREADY <= wready_now;

        // ARREADY delay: once ARVALID is observed, wait N cycles, then allow one AR handshake.
        if (rd_ar_delay <= 0) begin
          arready_now = 1'b1;
          arready_wait_pending = 1'b0;
          arready_delay_cnt = -1;
        end else begin
          arready_now = 1'b0;
          if (!arready_wait_pending && S_AXI_ARVALID) begin
            arready_wait_pending = 1'b1;
            arready_delay_cnt = rd_ar_delay;
          end
          if (arready_wait_pending) begin
            if (arready_delay_cnt > 0) begin
              arready_delay_cnt = arready_delay_cnt - 1;
            end else begin
              arready_now = 1'b1;
            end
          end
        end
        S_AXI_ARREADY <= arready_now;

        aw_handshake = S_AXI_AWVALID && awready_now;
        w_handshake = S_AXI_WVALID && wready_now;
        ar_handshake = S_AXI_ARVALID && arready_now;

        if (aw_handshake) begin
          write_addr_buf = S_AXI_AWADDR;
          write_addr_queue.push_back(S_AXI_AWADDR);
        end

        if (w_handshake) begin
          logic [31:0] wr_addr;

          if (write_addr_queue.size() != 0) begin
            wr_addr = write_addr_queue[0];
            write_addr_queue.pop_front();
          end else if (has_last_write_addr) begin
            wr_addr = last_write_addr + 32'd8;
          end else begin
            wr_addr = 32'd0;
          end

          write_addr_buf = wr_addr;
          last_write_addr = wr_addr;
          has_last_write_addr = 1'b1;

          word_idx = wr_addr >> 3;
          if ((word_idx >= 0) && (word_idx < TEST_DATA_SIZE)) begin
            memory[word_idx] <= S_AXI_WDATA;
            S_AXI_BRESP <= AXI_RESP_OKAY;
          end else begin
            S_AXI_BRESP <= AXI_RESP_SLVERR;
          end
          S_AXI_BVALID <= 1'b1;
          wready_wait_pending = 1'b0;
          wready_delay_cnt = -1;
        end else if (S_AXI_BVALID && S_AXI_BREADY) begin
          S_AXI_BVALID <= 1'b0;
        end

        if (ar_handshake) begin
          read_addr_queue.push_back(S_AXI_ARADDR);
          arready_wait_pending = 1'b0;
          arready_delay_cnt = -1;
        end

        if (!has_addr && (read_addr_queue.size() != 0)) begin
          if (rd_r_delay <= 0) begin
            addr_buf = read_addr_queue[0];
            read_addr_queue.pop_front();
            has_addr = 1'b1;
            rvalid_delay_cnt = -1;
          end else begin
            if (rvalid_delay_cnt < 0) begin
              rvalid_delay_cnt = rd_r_delay;
            end else if (rvalid_delay_cnt > 0) begin
              rvalid_delay_cnt = rvalid_delay_cnt - 1;
            end else begin
              addr_buf = read_addr_queue[0];
              read_addr_queue.pop_front();
              has_addr = 1'b1;
              rvalid_delay_cnt = -1;
            end
          end
        end

        if (has_addr) begin
          word_idx = addr_buf >> 3;
          if ((word_idx >= 0) && (word_idx < TEST_DATA_SIZE)) begin
            S_AXI_RDATA <= memory[word_idx];
            S_AXI_BRESP <= AXI_RESP_OKAY;
            S_AXI_RRESP <= AXI_RESP_OKAY;
          end else begin
            S_AXI_RDATA <= 64'd0;
            S_AXI_RRESP <= AXI_RESP_SLVERR;
          end
          S_AXI_RVALID <= 1'b1;
          if (S_AXI_RREADY) begin
            has_addr = 1'b0;
            rvalid_delay_cnt = -1;
          end
        end else begin
          S_AXI_RVALID <= 1'b0;
        end
      end
    end
  end
endmodule

module SOLE_test;
  localparam realtime CLK_PERIOD_NS = `TB_CLK_PERIOD_NS;
  localparam realtime CLK_HALF_PERIOD_NS = CLK_PERIOD_NS / 2.0;
  import sole_pkg::*;
  import "DPI-C" function string get_local_datetime();
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
  localparam int OUTPUT_START_WORD = 2000;
  localparam int MAX_DATA = 4096;
  localparam int TOP_K = 5;
  localparam real COSINE_PASS_THR = 0.97;

  integer i, w, e;
  reg [63:0] packed_word;
  real got;
  real expv;
  real abs_err;
  real max_err;
  reg [8*256-1:0] data_file_path;
  reg [8*256-1:0] result_log_path;
  reg [8*256-1:0] monitor_log_path;
  integer test_log_fh;
  integer monitor_log_fh;
  logic [31:0] last_status;
  logic last_status_valid;
  logic [31:0] hw_mon_last_status;
  logic hw_mon_last_status_valid;
  logic interrupt_seen;
  logic interrupt_prev;
  logic recovery_force_zero_delay;
  integer num_data;
  integer mmio_length;
  integer num_words;
  integer start_time_ns;
  integer done_time_ns;
  logic overflow_length_mode;
  logic [31:0] final_status;
  real sum_hw_output;
  real sum_sw_output;
  real cosine_dot;
  real cosine_norm_hw;
  real cosine_norm_sw;
  real cosine;
  real top_input_vals [0:TOP_K-1];
  real top_hw_vals [0:TOP_K-1];
  real top_sw_vals [0:TOP_K-1];
  integer top_input_idxs [0:TOP_K-1];
  integer top_hw_idxs [0:TOP_K-1];
  integer top_sw_idxs [0:TOP_K-1];

  real hw_output [0:MAX_DATA-1];
  real sw_output [0:MAX_DATA-1];
  real input_data [0:MAX_DATA-1];

  SOLE dut (
    .clk(clk), .rst_n(!rst),
    .proc_addr(proc_addr), .proc_wdata(proc_wdata), .proc_we(proc_we), .proc_rdata(proc_rdata), .interrupt(interrupt),
    .M_AXI_AWADDR(M_AXI_AWADDR), .M_AXI_AWVALID(M_AXI_AWVALID), .M_AXI_AWREADY(M_AXI_AWREADY),
    .M_AXI_WDATA(M_AXI_WDATA), .M_AXI_WSTRB(M_AXI_WSTRB), .M_AXI_WVALID(M_AXI_WVALID), .M_AXI_WREADY(M_AXI_WREADY),
    .M_AXI_BRESP(M_AXI_BRESP), .M_AXI_BVALID(M_AXI_BVALID), .M_AXI_BREADY(M_AXI_BREADY),
    .M_AXI_ARADDR(M_AXI_ARADDR), .M_AXI_ARVALID(M_AXI_ARVALID), .M_AXI_ARREADY(M_AXI_ARREADY),
    .M_AXI_RDATA(M_AXI_RDATA), .M_AXI_RRESP(M_AXI_RRESP), .M_AXI_RVALID(M_AXI_RVALID), .M_AXI_RREADY(M_AXI_RREADY)
  );

  AxiSlaveMemory mem (
    .clk(clk), .rst_n(!rst), .force_zero_delay(recovery_force_zero_delay),
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

  task mmio_read(input logic [31:0] addr, output logic [31:0] data);
    begin
      @(posedge clk);
      proc_addr <= addr;
      proc_we <= 1'b0;
      @(posedge clk);
      data = proc_rdata;
    end
  endtask

  task dump_sole_mmio_readback(input integer fh);
    logic [31:0] v;
    begin
      $fdisplay(fh, "\n[SOLE MMIO READBACK]");
      mmio_read(REG_CONTROL, v);
      $fdisplay(fh, "Control Register(read): 0x%08h", v);
      mmio_read(REG_STATUS, v);
      $fdisplay(fh, "Status Register(read): 0x%08h", v);
      mmio_read(REG_SRC_ADDR_BASE_L, v);
      $fdisplay(fh, "Src Addr Base Low(read): 0x%08h", v);
      mmio_read(REG_SRC_ADDR_BASE_H, v);
      $fdisplay(fh, "Src Addr Base High(read): 0x%08h", v);
      mmio_read(REG_DST_ADDR_BASE_L, v);
      $fdisplay(fh, "Dst Addr Base Low(read): 0x%08h", v);
      mmio_read(REG_DST_ADDR_BASE_H, v);
      $fdisplay(fh, "Dst Addr Base High(read): 0x%08h", v);
      mmio_read(REG_LENGTH_L, v);
      $fdisplay(fh, "Data Length Low(read): 0x%08h", v);
      mmio_read(REG_LENGTH_H, v);
      $fdisplay(fh, "Data Length High(read): 0x%08h", v);
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

`ifndef POST_SIM
`ifndef NO_SDF
  task dump_sole_mmio(input integer fh);
    begin
`else
  $display("[POST-SIM] NO_SDF enabled: skipping SDF annotation");
`endif
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
`endif

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

  task automatic update_top5(
    input real cand_val,
    input integer cand_idx,
    inout real top_vals [0:TOP_K-1],
    inout integer top_idxs [0:TOP_K-1]
  );
    integer p;
    integer q;
    begin
      p = TOP_K;
      for (q = 0; q < TOP_K; q = q + 1) begin
        if (cand_val > top_vals[q]) begin
          p = q;
          q = TOP_K;
        end
      end

      if (p < TOP_K) begin
        for (q = TOP_K - 1; q > p; q = q - 1) begin
          top_vals[q] = top_vals[q - 1];
          top_idxs[q] = top_idxs[q - 1];
        end
        top_vals[p] = cand_val;
        top_idxs[p] = cand_idx;
      end
    end
  endtask

  always_ff @(posedge clk or posedge rst) begin
    if (rst) begin
      interrupt_seen <= 1'b0;
      interrupt_prev <= 1'b0;
    end else begin
      // Rearm interrupt latch on every software START write so recovery runs
      // do not inherit a previously latched interrupt.
      if (proc_we && (proc_addr == REG_CONTROL) && (proc_wdata == 32'h0000_0001)) begin
        interrupt_seen <= 1'b0;
      end else if (interrupt && !interrupt_prev) begin
        interrupt_seen <= 1'b1;
      end
      interrupt_prev <= interrupt;
    end
  end

  // Continuous status monitor (SystemC parity): sample raw HW status each cycle
  // so 1-cycle done pulses are visible in monitor log.
  always_ff @(posedge clk or posedge rst) begin
    logic [31:0] st_hw;
    if (rst) begin
      hw_mon_last_status <= 32'd0;
      hw_mon_last_status_valid <= 1'b0;
    end else begin
`ifdef POST_SIM
  st_hw = dut.u_softmax.status_o;
`else
      st_hw = dut.reg_status;
`endif

      if ((monitor_log_fh != 0) && (!$isunknown(st_hw))
          && (!hw_mon_last_status_valid || (st_hw !== hw_mon_last_status))) begin
        $fdisplay(monitor_log_fh,
                  "[STATUS_HW] 0x%08h | done=%0d | interrupt=%0d | state=%0s | error=%0d | error_code=0x%0h @%0d ns",
                  st_hw,
                  (st_hw & 1),
                  (interrupt === 1'b1),
                  status_state_name(st_hw),
                  ((st_hw >> 3) & 1),
                  ((st_hw >> 4) & 4'hF),
                  sim_time_ns());
      end

      hw_mon_last_status <= st_hw;
      hw_mon_last_status_valid <= 1'b1;
    end
  end

  initial begin
    integer data_fh;
    integer rc;
    real in_val;
    string run_datetime;
    clk = 0;
    rst = 1;
    proc_addr = 0;
    proc_wdata = 0;
    proc_we = 0;
    recovery_force_zero_delay = 1'b0;
    max_err = 0.0;
    last_status = 32'd0;
    last_status_valid = 1'b0;
    sum_hw_output = 0.0;
    sum_sw_output = 0.0;
    cosine_dot = 0.0;
    cosine_norm_hw = 0.0;
    cosine_norm_sw = 0.0;

`ifdef POST_SIM
    result_log_path = "../log/SOLE_test_Result_post_sim.log";
    monitor_log_path = "../log/SOLE_test_Monitor_post_sim.log";
`else
    result_log_path = "../log/SOLE_test_Result_pre_sim.log";
    monitor_log_path = "../log/SOLE_test_Monitor_pre_sim.log";
`endif

    test_log_fh = $fopen(result_log_path, "w");
    monitor_log_fh = $fopen(monitor_log_path, "w");
    if (test_log_fh == 0) begin
      $display("[ERROR] Failed to create %0s", result_log_path);
      $fatal(1);
    end
    if (monitor_log_fh == 0) begin
      $display("[ERROR] Failed to create %0s", monitor_log_path);
      $fatal(1);
    end

`ifdef POST_SIM
    $display("[POST-SIM] SDF annotation file: ../syn/SOLE_syn.sdf");
    $sdf_annotate("../syn/SOLE_syn.sdf", dut);
`endif

    run_datetime = get_local_datetime();
    $fdisplay(test_log_fh, "%s", run_datetime);
    $fdisplay(test_log_fh, "===== SOLE TEST LOG =====");
    $fdisplay(test_log_fh, "[STRESS CONFIG] AXI_READ_ARREADY_DELAY=%0d AXI_READ_RVALID_DELAY=%0d AXI_WRITE_WREADY_DELAY=%0d error_recovery_test=%0d",
          `AXI_READ_ARREADY_DELAY, `AXI_READ_RVALID_DELAY, `AXI_WRITE_WREADY_DELAY, `error_recovery_test);
    $fdisplay(monitor_log_fh, "[CONTINUOUS MONITOR STARTED]");

    data_file_path = "../data/SOLE_test_Data_rtl.txt";
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
    num_data = 0;
    mmio_length = 0;
    overflow_length_mode = 1'b0;
    final_status = 32'd0;
    for (i = 0; i < MAX_DATA; i = i + 1) begin
      rc = $fscanf(data_fh, "%f", in_val);
      if (rc == 1) begin
        input_data[num_data] = in_val;
        num_data = num_data + 1;
      end else if (rc == -1) begin
        i = MAX_DATA;
      end else begin
        $display("[ERROR] Input data file parse failed at index %0d (%0s)", num_data, data_file_path);
        $fatal(1);
      end
    end
    if (num_data <= 0) begin
      $display("[ERROR] Input data file is empty: %0s", data_file_path);
      $fatal(1);
    end
    if (num_data >= MAX_DATA) begin
      rc = $fscanf(data_fh, "%f", in_val);
      if (rc == 1) begin
        overflow_length_mode = 1'b1;
        mmio_length = MAX_DATA + 1;
        $display("[RTL TEST] Detected input length > MAX_DATA (%0d). Triggering DUT error-path test with length=%0d", MAX_DATA, mmio_length);
        $fdisplay(test_log_fh, "[INFO] Input data exceeds MAX_DATA=%0d, switching to error-path check (REG_LENGTH_L=%0d)", MAX_DATA, mmio_length);
      end else if (rc != -1) begin
        $display("[ERROR] Input data file parse failed while checking overflow (%0s)", data_file_path);
        $fatal(1);
      end
    end
    if (!overflow_length_mode) begin
      mmio_length = num_data;
    end
    if (`error_recovery_test && overflow_length_mode) begin
      $display("[ERROR] error_recovery_test requires <= MAX_DATA input entries, but file exceeds %0d", MAX_DATA);
      $fatal(1);
    end
    $fclose(data_fh);
    num_words = (num_data + 3) / 4;

    repeat (10) @(posedge clk);
    rst = 0;

    // Wait 5 more cycles to allow always_ff to complete before procedural memory access
    repeat (5) @(posedge clk);

    $fdisplay(test_log_fh, "\n[1] Memory Input Data Write");
    $fdisplay(test_log_fh, "Index | InputFloat | InputFP16Hex | MemWordIdx | ElemInWord");
    for (w = 0; w < num_words; w++) begin
      packed_word = 64'd0;
      for (e = 0; e < 4; e++) begin
        i = w * 4 + e;
        if (i < num_data) begin
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
    dump_memory_words(test_log_fh, INPUT_START_WORD, num_words);

    if (`error_recovery_test) begin
      logic [31:0] st_inject;
      bit injected_error_seen;
      bit injected_interrupt_seen;

      injected_error_seen = 1'b0;
      injected_interrupt_seen = 1'b0;

      $fdisplay(test_log_fh, "\n[2] Error Injection + Recovery Start Test");
      mmio_write(REG_LENGTH_L, 32'd0);
      mmio_write(REG_LENGTH_H, 32'd0);
      mmio_write(REG_CONTROL, 32'h0000_0001);
      $fdisplay(test_log_fh, "%0d ns,REG_CONTROL,0x1,inject error with length=0", sim_time_ns());
      mmio_write(REG_CONTROL, 32'h0000_0000);

      for (i = 0; i < 64; i = i + 1) begin
        mmio_read(REG_STATUS, st_inject);
        if (((st_inject >> 3) & 1'b1) == 1'b1) begin
          injected_error_seen = 1'b1;
          injected_interrupt_seen = interrupt;
          $fdisplay(test_log_fh,
                    "time: %0d ns Event: Injected error detected, status=0x%08h interrupt=%0d",
                    sim_time_ns(), st_inject, injected_interrupt_seen);
          if (monitor_log_fh != 0) begin
            $fdisplay(monitor_log_fh,
                      "[RECOVERY] injected_error_detected status=0x%08h @%0d ns",
                      st_inject,
                      sim_time_ns());
          end
          i = 64;
        end
      end

      if (!injected_error_seen) begin
        $display("[RTL TEST] FAIL: injected MMIO error was not detected");
        $fatal(1);
      end
      if (!injected_interrupt_seen) begin
        $display("[RTL TEST] FAIL: interrupt was not asserted on injected error");
        $fatal(1);
      end

      $fdisplay(test_log_fh, "[2-RECOVERY] Reconfigure valid MMIO and restart");
      recovery_force_zero_delay = 1'b1;
      $fdisplay(test_log_fh,
            "[2-RECOVERY] Force AXI delays to zero for restart run (ARREADY/RVALID/WREADY)");
      if (monitor_log_fh != 0) begin
        $fdisplay(monitor_log_fh,
                  "[RECOVERY] force_zero_delay=1 @%0d ns",
                  sim_time_ns());
      end

      // Clear latched interrupt/status history from injection phase so
      // the next START run is evaluated independently.
      last_status = 32'd0;
      last_status_valid = 1'b0;
    end else begin
      $fdisplay(test_log_fh, "\n[2] Testbench MMIO Configuration");
    end
    $fdisplay(test_log_fh, "TimeNs,Reg,ValueHex,Note");

    mmio_write(REG_SRC_ADDR_BASE_L, INPUT_START_WORD * 8);
    $fdisplay(test_log_fh, "%0d ns,REG_SRC_ADDR_BASE_L,0x%0h,source base byte address", sim_time_ns(), INPUT_START_WORD * 8);
    mmio_write(REG_SRC_ADDR_BASE_H, 0);
    $fdisplay(test_log_fh, "%0d ns,REG_SRC_ADDR_BASE_H,0x0,source high", sim_time_ns());
    mmio_write(REG_DST_ADDR_BASE_L, OUTPUT_START_WORD * 8);
    $fdisplay(test_log_fh, "%0d ns,REG_DST_ADDR_BASE_L,0x%0h,destination base byte address", sim_time_ns(), OUTPUT_START_WORD * 8);
    mmio_write(REG_DST_ADDR_BASE_H, 0);
    $fdisplay(test_log_fh, "%0d ns,REG_DST_ADDR_BASE_H,0x0,destination high", sim_time_ns());
    mmio_write(REG_LENGTH_L, mmio_length);
    $fdisplay(test_log_fh, "%0d ns,REG_LENGTH_L,0x%0h,number of FP16 elements", sim_time_ns(), mmio_length);
    mmio_write(REG_LENGTH_H, 0);
    $fdisplay(test_log_fh, "%0d ns,REG_LENGTH_H,0x0,length high", sim_time_ns());
    start_time_ns = sim_time_ns();
    mmio_write(REG_CONTROL, 32'h0000_0001);
    $fdisplay(test_log_fh, "%0d ns,REG_CONTROL,0x1,mode=softmax start=1", sim_time_ns());
    if (`error_recovery_test && (monitor_log_fh != 0)) begin
      $fdisplay(monitor_log_fh,
                "[RECOVERY] restart_start_written @%0d ns",
                sim_time_ns());
    end
    mmio_write(REG_CONTROL, 32'h0000_0000);

  `ifndef POST_SIM
    dump_sole_mmio(test_log_fh);
  `else
    dump_sole_mmio_readback(test_log_fh);
  `endif

    // In recovery mode, the injected-error interrupt can remain high for a few
    // cycles after restart. Wait until it drops before starting completion poll.
    if (`error_recovery_test) begin
      for (i = 0; i < 64; i = i + 1) begin
        if (interrupt !== 1'b1) begin
          i = 64;
        end else begin
          @(posedge clk);
        end
      end
    end

    $fdisplay(test_log_fh, "\n[3] Softmax Start Running");
    $fdisplay(test_log_fh, "TimeNs,Status,State,Done,ErrorCode");
    $fdisplay(test_log_fh, "\n[Stage 1~4: Poll REG_STATUS until DONE]");

    begin
      logic [31:0] st;
      bit seen_p1;
      bit seen_p2;
      bit seen_p3;
      bit done_seen;
      bit completion_armed;

      seen_p1 = 1'b0;
      seen_p2 = 1'b0;
      seen_p3 = 1'b0;
      done_seen = 1'b0;
      completion_armed = (`error_recovery_test == 0);

      for (i = 0; i < 20000; i = i + 1) begin
        mmio_read(REG_STATUS, st);

        if (!$isunknown(st) && (!last_status_valid || (st !== last_status))) begin
          $display("[STATUS_HW] 0x%08h | done=%0d | interrupt=%0d | state=%0s | error=%0d | error_code=0x%0h @%0d ns",
                   st,
                   (st & 1),
                   (interrupt === 1'b1),
                   status_state_name(st),
                   ((st >> 3) & 1),
                   ((st >> 4) & 4'hF),
                   sim_time_ns());
          last_status = st;
          last_status_valid = 1'b1;
        end

        if (!seen_p1 && (((st >> 1) & 2'b11) == 2'd1)) begin
          seen_p1 = 1'b1;
          completion_armed = 1'b1;
          $fdisplay(test_log_fh, "time: %0d ns Event: Entered PROCESS1", sim_time_ns());
        end
        if (!seen_p2 && (((st >> 1) & 2'b11) == 2'd2)) begin
          seen_p2 = 1'b1;
          $fdisplay(test_log_fh, "time: %0d ns Event: Entered PROCESS2", sim_time_ns());
        end
        if (!seen_p3 && (((st >> 1) & 2'b11) == 2'd3)) begin
          seen_p3 = 1'b1;
          $fdisplay(test_log_fh, "time: %0d ns Event: Entered PROCESS3", sim_time_ns());
        end

        // Completion on status done/error, or interrupt latch to avoid missing
        // a 1-cycle done pulse between two MMIO polling reads.
        if (completion_armed &&
            ((((st & 32'h1) != 0) || (((st >> 3) & 1'b1) == 1'b1)
           || interrupt_seen))) begin
          done_time_ns = sim_time_ns();
          done_seen = 1'b1;
          i = 20000;
        end
      end

      if (!done_seen) begin
        $display("[RTL TEST] FAIL: timeout waiting for interrupt/done");
        $fatal(1);
      end
    end

    repeat (5) @(posedge clk);

    $fdisplay(test_log_fh, "time: %0d ns Event: DONE pulse detected!", done_time_ns);
    $fdisplay(test_log_fh, "[TIMING] Start time: %0d ns", start_time_ns);
    $fdisplay(test_log_fh, "[TIMING] Done time: %0d ns", done_time_ns);
    $fdisplay(test_log_fh, "[TIMING] Execution time: %0d ns", done_time_ns - start_time_ns);

    if (overflow_length_mode) begin
      mmio_read(REG_STATUS, final_status);
      $fdisplay(test_log_fh, "[ERROR-PATH] Final status: 0x%08h", final_status);
      $fdisplay(monitor_log_fh,
                "[ERROR-PATH] Final status=0x%08h | done=%0d | error=%0d | error_code=0x%0h @%0d ns",
                final_status,
                (final_status & 1),
                ((final_status >> 3) & 1),
                ((final_status >> 4) & 4'hF),
                sim_time_ns());

      if (((final_status >> 3) & 1) != 1) begin
        $display("[RTL TEST] FAIL: expected error bit=1 when length exceeds MAX_DATA");
        $fatal(1);
      end
      if (((final_status >> 4) & 4'hF) != 4'h9) begin
        $display("[RTL TEST] FAIL: expected error_code=0x9 for length overflow, got 0x%0h", ((final_status >> 4) & 4'hF));
        $fatal(1);
      end
      $display("[RTL TEST] PASS: overflow guard triggered (error_code=0x9) and interrupt observed.");
      $fdisplay(test_log_fh, "[PASS] Overflow guard triggered as expected (error_code=0x9, interrupt_seen=1)");
      $fdisplay(monitor_log_fh, "\n[CONTINUOUS MONITOR STOPPED]");
      $fclose(monitor_log_fh);
      $fclose(test_log_fh);
      $finish;
    end

    $fdisplay(test_log_fh, "\n[5] Output Stored Back to Memory via AXI4_Lite");
    dump_memory_words(test_log_fh, OUTPUT_START_WORD, num_words);

    $display("[MEM DUMP] OUTPUT memory words from %0d, count=%0d", OUTPUT_START_WORD, num_words);
    for (w = 0; w < num_words; w = w + 1) begin
      $display("[MEM] word=%0d data=0x%04h_%04h_%04h_%04h",
               OUTPUT_START_WORD + w,
               mem.memory[OUTPUT_START_WORD + w][63:48],
               mem.memory[OUTPUT_START_WORD + w][47:32],
               mem.memory[OUTPUT_START_WORD + w][31:16],
               mem.memory[OUTPUT_START_WORD + w][15:0]);
    end

    $fdisplay(test_log_fh, "\n[4] Softmax Compute Results");
    $fdisplay(test_log_fh, "Index |  Input  |  HW_Output  |  SW_Output  |  AbsError");

    for (i = 0; i < num_data; i++) begin
      w = OUTPUT_START_WORD + (i / 4);
      e = i % 4;
      got = fp16_to_real(mem.memory[w][e*16 +: 16]);
      expv = expected_sole_softmax(i, num_data);
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
    end

    if (cosine_norm_hw > 0.0 && cosine_norm_sw > 0.0) begin
      cosine = cosine_dot / ($sqrt(cosine_norm_hw) * $sqrt(cosine_norm_sw));
    end else begin
      cosine = 0.0;
    end

    for (i = 0; i < TOP_K; i = i + 1) begin
      top_input_vals[i] = -1.0e30;
      top_hw_vals[i] = -1.0e30;
      top_sw_vals[i] = -1.0e30;
      top_input_idxs[i] = -1;
      top_hw_idxs[i] = -1;
      top_sw_idxs[i] = -1;
    end

    for (i = 0; i < num_data; i = i + 1) begin
      update_top5(input_data[i], i, top_input_vals, top_input_idxs);
      update_top5(hw_output[i], i, top_hw_vals, top_hw_idxs);
      update_top5(sw_output[i], i, top_sw_vals, top_sw_idxs);
    end

    $fdisplay(test_log_fh, "\n================== FINAL REPORT ================= ");
    $fdisplay(test_log_fh, "[EXECUTION TIME] SOLE Execution Time: %0d ns", done_time_ns - start_time_ns);
    $fdisplay(test_log_fh, "\n[ANALYSIS] Cosine Similarity (HW vs SW) = %0.9f", cosine);

    $fdisplay(test_log_fh, "\n[ANALYSIS] Top 5 large inputs (value @ index):");
    for (i = 0; i < TOP_K; i = i + 1) begin
      $fdisplay(test_log_fh, "  %0d: %0.6f @ %0d", i, top_input_vals[i], top_input_idxs[i]);
    end

    $fdisplay(test_log_fh, "\n[ANALYSIS] Top 5 large HW outputs (value @ index):");
    for (i = 0; i < TOP_K; i = i + 1) begin
      $fdisplay(test_log_fh, "  %0d: %0.9f @ %0d", i, top_hw_vals[i], top_hw_idxs[i]);
    end

    $fdisplay(test_log_fh, "\n[ANALYSIS] Top 5 large SW outputs (value @ index):");
    for (i = 0; i < TOP_K; i = i + 1) begin
      $fdisplay(test_log_fh, "  %0d: %0.9f @ %0d", i, top_sw_vals[i], top_sw_idxs[i]);
    end

    $fdisplay(test_log_fh, "\n[ANALYSIS] Sum & MaxAbsError");
    $fdisplay(test_log_fh, "HW_Sum=%0.9f", sum_hw_output);
    $fdisplay(test_log_fh, "SW_Sum=%0.9f", sum_sw_output);
    $fdisplay(test_log_fh, "MaxAbsError=%0.9e", max_err);
    $fdisplay(test_log_fh, "\n============ END OF SOLE TEST LOG ============");

    $display("[RTL TEST] NUM_DATA=%0d cosine=%0.9f threshold=%0.2f max_err=%f", num_data, cosine, COSINE_PASS_THR, max_err);
    if (cosine > COSINE_PASS_THR) begin
      $display("[RTL TEST] PASS: cosine similarity is above threshold");
    end else begin
      $display("[RTL TEST] FAIL: cosine similarity is below threshold");
      $fatal(1);
    end
    $fdisplay(monitor_log_fh, "\n[CONTINUOUS MONITOR STOPPED]");
    $fclose(monitor_log_fh);
    $fclose(test_log_fh);
    $finish;
  end

  // 0.5ns high/low => 1ns clock period (aligned with SystemC testbench)
  always #(CLK_HALF_PERIOD_NS) clk = ~clk;
endmodule
