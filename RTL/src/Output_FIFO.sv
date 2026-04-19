module Output_FIFO (
  input  logic                        clk,
  input  logic                        rst_n,
  input  logic [15:0]                 data_in,
  input  logic                        write_en,
  input  logic                        read_ready,
  output logic                        read_valid,
  input  logic                        clear,
  output logic [15:0]                 data_out,
  output logic                        full,
  output logic                        empty,
  output logic [11:0]                 count
);
  // Effective FIFO storage is one 16-bit entry per 4 fp16 input values.
  // For DATA_LENGTH_MAX=4096 inputs, required entries are 4096/4=1024 => ADDR_BITS=10.
  localparam int FIFO_ADDR_BITS = 10;
  localparam int FIFO_DEPTH = (1 << FIFO_ADDR_BITS);

  logic [FIFO_ADDR_BITS-1:0] wptr;
  logic [FIFO_ADDR_BITS-1:0] rptr;
  logic [11:0] count_reg;

  logic [15:0] sram_rdata;
  logic        we_sig;
  logic        re_sig;

  logic [15:0] skid_reg;
  logic        skid_valid;
  logic        sram_output_data_valid;

  SRAM #(.ADDR_BITS(FIFO_ADDR_BITS), .DATA_BITS(16)) u_sram (
    .clk(clk), .rst_n(rst_n),
    .we(we_sig), .re(re_sig),
    .waddr(wptr), .raddr(rptr),
    .wdata(data_in), .rdata(sram_rdata)
  );

  always_comb begin
    empty = (count_reg == 12'd0);
    full = (count_reg == FIFO_DEPTH);
    count = count_reg;
  end

  always_comb begin
    int unsigned buffered;
    int storage_avail;
    buffered = (skid_valid ? 1 : 0) + (sram_output_data_valid ? 1 : 0);
    storage_avail = count_reg - buffered;

    we_sig = 1'b0;
    re_sig = 1'b0;
    if (!clear) begin
      we_sig = write_en && !full;
      if (storage_avail > 0) begin
        if (read_ready) begin
          re_sig = (buffered < 2);
        end else begin
          re_sig = !skid_valid && !sram_output_data_valid;
        end
      end
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      wptr <= '0;
      rptr <= '0;
    end else if (clear) begin
      rptr <= wptr;
    end else begin
      if (we_sig) wptr <= wptr + 1'b1;
      if (re_sig) rptr <= rptr + 1'b1;
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      count_reg <= 12'd0;
    end else if (clear) begin
      count_reg <= 12'd0;
    end else begin
      logic accepted_read_handshake;
      accepted_read_handshake = read_ready && read_valid;
      if (we_sig && !accepted_read_handshake) begin
        count_reg <= count_reg + 12'd1;
      end else if (!we_sig && accepted_read_handshake && (count_reg != 12'd0)) begin
        count_reg <= count_reg - 12'd1;
      end
    end
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      skid_reg <= 16'd0;
      skid_valid <= 1'b0;
      sram_output_data_valid <= 1'b0;
    end else if (clear) begin
      skid_reg <= 16'd0;
      skid_valid <= 1'b0;
      sram_output_data_valid <= 1'b0;
    end else begin
      logic [15:0] skid_reg_next;
      logic        skid_valid_next;
      logic        consume_skid;
      logic        new_data_valid;
      logic        bypass_consumed;

      skid_reg_next = skid_reg;
      skid_valid_next = skid_valid;

      consume_skid = read_ready && skid_valid_next;
      if (consume_skid) begin
        skid_valid_next = 1'b0;
      end

      new_data_valid = sram_output_data_valid;
      if (new_data_valid) begin
        bypass_consumed = read_ready && !skid_valid;
        if (!bypass_consumed) begin
          skid_valid_next = 1'b1;
          skid_reg_next = sram_rdata;
        end
      end

      skid_reg <= skid_reg_next;
      skid_valid <= skid_valid_next;
      sram_output_data_valid <= re_sig;
    end
  end

  always_comb begin
    if (skid_valid) data_out = skid_reg;
    else data_out = sram_rdata;

    read_valid = sram_output_data_valid || skid_valid;
  end
endmodule
