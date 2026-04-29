#include <systemc.h>
#include <deque>
#include <vector>
#include <map>
#include <iostream>
#include <iomanip>
#include <sstream>
#include "Max_FIFO.h"
#include "../src/Max_FIFO.cpp"

SC_MODULE(Max_FIFO_TestBench) {
    sc_clock clk_sig{"clk_sig", 10, SC_NS};
    sc_signal<bool>   rst_sig;
    sc_signal<bool>   write_en_sig;
    sc_signal<bool>   read_ready_sig;
    sc_signal<bool>   clear_sig;
    sc_signal<sc_dt::sc_uint<16>> data_in_sig;
    sc_signal<sc_dt::sc_uint<16>> data_out_sig;
    sc_signal<bool>   read_valid_sig;
    sc_signal<bool>   full_sig;
    sc_signal<bool>   empty_sig;
    sc_signal<max_fifo_addr_t> count_sig;

    Max_FIFO *dut;

    int cycle_count = 0;
    int error_count = 0;
    int count_mismatch_count = 0;

    // Cycle-accurate model state for checking response timing/data correctness.
    std::deque<sc_dt::sc_uint<16>> model_fifo;
    bool model_skid_valid = false;
    sc_dt::sc_uint<16> model_skid_data = 0;
    bool model_sram_output_data_valid = false;
    sc_dt::sc_uint<16> model_sram_rdata = 0;

    struct PendingExpected {
        int due_cycle;
        sc_dt::sc_uint<16> data;
    };
    std::deque<PendingExpected> pending_next_cycle_checks;
    std::deque<PendingExpected> pending_handshake_next_checks;
    std::vector<sc_dt::sc_uint<16>> write_accepted_order;
    std::vector<sc_dt::sc_uint<16>> read_consumed_order;
    int handshake_next_error_count = 0;

    SC_CTOR(Max_FIFO_TestBench) {
        dut = new Max_FIFO("Max_FIFO_DUT");
        dut->clk(clk_sig);
        dut->rst(rst_sig);
        dut->write_en(write_en_sig);
        dut->read_ready(read_ready_sig);
        dut->clear(clear_sig);
        dut->data_in(data_in_sig);
        dut->data_out(data_out_sig);
        dut->read_valid(read_valid_sig);
        dut->full(full_sig);
        dut->empty(empty_sig);
        dut->count(count_sig);

        SC_THREAD(test_stimulus);
    }

    void reset_model() {
        model_fifo.clear();
        model_skid_valid = false;
        model_skid_data = 0;
        model_sram_output_data_valid = false;
        model_sram_rdata = 0;
        pending_next_cycle_checks.clear();
        pending_handshake_next_checks.clear();
        write_accepted_order.clear();
        read_consumed_order.clear();
        handshake_next_error_count = 0;
    }

    std::string sram_state_block() {
        std::ostringstream oss;
        const max_fifo_addr_t r_ptr = dut->read_addr_sig.read();
        const max_fifo_addr_t w_ptr = dut->write_addr_sig.read();
        const max_fifo_addr_t occ_sig = w_ptr - r_ptr;
        const unsigned occ = static_cast<unsigned>(occ_sig.to_uint());

        oss << "\033[32m[MAX_FIFO] SRAM(push):";
        if (occ == 0) {
            oss << "\n    <empty>\033[0m";
            return oss.str();
        }

        const unsigned entries_per_line = 6;
        for (unsigned k = 0; k < occ; ++k) {
            const unsigned idx = (static_cast<unsigned>(r_ptr.to_uint()) + k) & ((1u << MAX_FIFO_ADDR_BITS) - 1u);
            const sc_dt::sc_uint<16> data = dut->sram->debug_peek(static_cast<int>(idx));
            if (k % entries_per_line == 0) {
                oss << "\n    ";
            }
            oss << "[" << idx << ":0x"
                << std::hex << std::setw(4) << std::setfill('0')
                << static_cast<unsigned>(data)
                << std::dec << std::setfill(' ') << "] ";
        }
        oss << "\033[0m";
        return oss.str();
    }

    void print_cycle_signals(const char* note) {
        const bool skid_v = dut->skid_valid_sig.read();
        const bool sram_out_v = dut->sram_output_data_valid.read();
        const sc_dt::sc_uint<16> skid_d = dut->skid_reg_sig.read();
        const sc_dt::sc_uint<16> sram_rdata = dut->sram_rdata_sig.read();

        const unsigned queue_cnt = static_cast<unsigned>(count_sig.read());
        const unsigned out_path_cnt = (skid_v ? 1u : 0u) + (sram_out_v ? 1u : 0u);
        const unsigned pending_total = queue_cnt + out_path_cnt;

        const char* head_src = skid_v ? "skid" : (sram_out_v ? "prefetch" : "none");
        const sc_dt::sc_uint<16> head_data = skid_v
            ? skid_d
            : (sram_out_v ? sram_rdata : sc_dt::sc_uint<16>(0));

        std::cout << "\033[34m[Cycle " << std::setw(2) << cycle_count << "]\033[0m @ " << std::setw(6) << sc_time_stamp()
                  << " | " << note
                  << " | in=0x" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<unsigned>(data_in_sig.read())
                  << " we=" << std::dec << write_en_sig.read()
                  << " out=0x" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<unsigned>(data_out_sig.read())
                  << " rr=" << std::dec << read_ready_sig.read()
                  << " rv=" << read_valid_sig.read()
                  << " skid=0x" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<unsigned>(skid_d)
                  << " sram_rdata=0x" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<unsigned>(sram_rdata)
                  << std::dec << " skid_v=" << skid_v
                  << " sram_out_v=" << sram_out_v
                  << " cnt=" << queue_cnt
                  << " pending=" << pending_total
                  << " head_src=" << head_src
                  << " head=0x" << std::hex << std::setw(4) << std::setfill('0')
                  << static_cast<unsigned>(head_data)
                  << std::dec
                  << " f/e=" << full_sig.read() << "/" << empty_sig.read()
                  << std::endl
                  << "  " << sram_state_block()
                  << std::endl;
    }

    void print_sequence(const char* title, const std::vector<sc_dt::sc_uint<16>>& seq) {
        std::cout << title;
        if (seq.empty()) {
            std::cout << " <empty>" << std::endl;
            return;
        }

        std::cout << std::endl;
        for (std::size_t i = 0; i < seq.size(); ++i) {
            std::cout << "  [" << i << "] 0x"
                      << std::hex << std::setw(4) << std::setfill('0')
                      << static_cast<unsigned>(seq[i])
                      << std::dec << std::setfill(' ') << std::endl;
        }
    }

    void check_due_expectations() {
        while (!pending_next_cycle_checks.empty()
            && pending_next_cycle_checks.front().due_cycle == cycle_count) {
            const sc_dt::sc_uint<16> expected = pending_next_cycle_checks.front().data;
            pending_next_cycle_checks.pop_front();

            const bool rv = read_valid_sig.read();
            const sc_dt::sc_uint<16> dout = data_out_sig.read();
            const bool pass = rv && (dout == expected);

            std::cout << "  [Check @ cycle " << cycle_count << "] "
                      << "expect next-cycle data=0x" << std::hex << std::setw(4)
                      << std::setfill('0') << static_cast<unsigned>(expected)
                      << std::dec << std::setfill(' ') << " -> "
                      << (pass ? "PASS" : "FAIL") << std::endl;

            if (!pass) {
                std::cout << "    observed read_valid=" << rv
                          << " data_out=0x" << std::hex << std::setw(4)
                          << std::setfill('0') << static_cast<unsigned>(dout)
                          << std::dec << std::setfill(' ') << std::endl;
                error_count++;
            }
        }

        while (!pending_handshake_next_checks.empty()
            && pending_handshake_next_checks.front().due_cycle == cycle_count) {
            const sc_dt::sc_uint<16> expected = pending_handshake_next_checks.front().data;
            pending_handshake_next_checks.pop_front();

            const bool rv = read_valid_sig.read();
            const sc_dt::sc_uint<16> dout = data_out_sig.read();
            const bool pass = rv && (dout == expected);

            std::cout << "  [HS->NXT @ cycle " << cycle_count << "] "
                      << "expect data_out(next)=0x" << std::hex << std::setw(4)
                      << std::setfill('0') << static_cast<unsigned>(expected)
                      << std::dec << std::setfill(' ') << " -> "
                      << (pass ? "PASS" : "FAIL") << std::endl;

            if (!pass) {
                std::cout << "    observed read_valid=" << rv
                          << " data_out=0x" << std::hex << std::setw(4)
                          << std::setfill('0') << static_cast<unsigned>(dout)
                          << std::dec << std::setfill(' ') << std::endl;
                handshake_next_error_count++;
            }
        }
    }

    void check_count_consistency() {
        const unsigned dut_count = static_cast<unsigned>(count_sig.read().to_uint());
        const unsigned model_count = static_cast<unsigned>(model_fifo.size())
                                   + (model_skid_valid ? 1u : 0u)
                                   + (model_sram_output_data_valid ? 1u : 0u);
        const bool pass = (dut_count == model_count);

        std::cout << "  [COUNT @ cycle " << cycle_count << "] "
                  << "dut=" << dut_count
                  << " model_total=" << model_count
                  << " -> " << (pass ? "PASS" : "FAIL") << std::endl;

        if (!pass) {
            count_mismatch_count++;
        }
    }

    void model_step(bool write_en, sc_dt::sc_uint<16> data_in, bool read_ready) {
        const bool fifo_full = (model_fifo.size() >= 1023u);
        const bool fifo_empty = model_fifo.empty();
        const unsigned buffered = (model_skid_valid ? 1u : 0u)
                                + (model_sram_output_data_valid ? 1u : 0u);

        bool issue_read = false;
        if (!fifo_empty) {
            if (read_ready) {
                issue_read = (buffered < 2u);
            } else {
                issue_read = !model_skid_valid && !model_sram_output_data_valid;
            }
        }
        sc_dt::sc_uint<16> issued_data = 0;
        if (issue_read) {
            issued_data = model_fifo.front();
            model_fifo.pop_front();
            pending_next_cycle_checks.push_back({cycle_count + 1, issued_data});
        }

        if (write_en && !fifo_full) {
            model_fifo.push_back(data_in);
            write_accepted_order.push_back(data_in);
        }

        bool skid_valid_next = model_skid_valid;
        sc_dt::sc_uint<16> skid_data_next = model_skid_data;

        if (read_ready && skid_valid_next) {
            skid_valid_next = false;
        }

        if (model_sram_output_data_valid) {
            const bool bypass_consumed = read_ready && !model_skid_valid;
            if (!bypass_consumed) {
                skid_valid_next = true;
                skid_data_next = model_sram_rdata;
            }
        }

        model_skid_valid = skid_valid_next;
        model_skid_data = skid_data_next;
        model_sram_output_data_valid = issue_read;
        if (issue_read) {
            model_sram_rdata = issued_data;
        }
    }

    void apply_cycle(bool write_en, sc_dt::sc_uint<16> data_in, bool read_ready, const char* note) {
        write_en_sig.write(write_en);
        data_in_sig.write(data_in);
        read_ready_sig.write(read_ready);
        clear_sig.write(false);

        wait(clk_sig.posedge_event());
        wait(SC_ZERO_TIME);

        cycle_count++;
        print_cycle_signals(note);
        check_due_expectations();

        if (read_ready_sig.read() && read_valid_sig.read()) {
            read_consumed_order.push_back(data_out_sig.read());

            const std::size_t consumed = read_consumed_order.size();
            if (consumed < write_accepted_order.size()) {
                pending_handshake_next_checks.push_back({
                    cycle_count + 1,
                    write_accepted_order[consumed]
                });
            }
        }

        if (!rst_sig.read()) {
            model_step(write_en, data_in, read_ready);
        }

        // update_flags is sensitive to pointer signals and may update count in
        // a later delta cycle than pointer_update.
        wait(SC_ZERO_TIME);
        check_count_consistency();
    }

    void test_stimulus() {
        std::cout << "\n===== Max_FIFO read_ready pattern test =====" << std::endl;
        std::cout << "Goal: write 10 entries, then verify next-cycle read behavior under irregular read_ready patterns." << std::endl;

        rst_sig.write(true);
        write_en_sig.write(false);
        read_ready_sig.write(false);
        clear_sig.write(false);
        data_in_sig.write(0);
        reset_model();

        apply_cycle(false, 0x0000, false, "reset cycle 0");
        apply_cycle(false, 0x0000, false, "reset cycle 1");

        rst_sig.write(false);
        reset_model();

        // Phase 1: write 10 consecutive entries, no reads.
        for (unsigned i = 0; i < 10; ++i) {
            const sc_dt::sc_uint<16> v = static_cast<sc_dt::sc_uint<16>>(0x4100u + (i << 4));
            apply_cycle(true, v, false, "write phase");
        }
        apply_cycle(false, 0x0000, false, "write->read transition idle");

        // Phase 2: irregular read_ready pattern (no writes) with long high/low runs and alternation.
        const bool read_pattern[] = {
            true, true, false, false, true, false, true, true,
            false, true, false, false, true, true, true, false,
            true, false, true, true, false, false, true, true,
            true, false, true, false, true, true
        };

        for (bool rr : read_pattern) {
            apply_cycle(false, 0x0000, rr, "read phase irregular read_ready");
        }

        std::cout << "\n===== Summary =====" << std::endl;
        std::cout << "Total checked cycles: " << cycle_count << std::endl;
        std::cout << "Pending checks left:  " << pending_next_cycle_checks.size() << std::endl;
        std::cout << "Data check errors:    " << error_count << std::endl;
        std::cout << "HS->next errors:      " << handshake_next_error_count << std::endl;
        std::cout << "Count mismatches:     " << count_mismatch_count << std::endl;

        print_sequence("Write accepted order:", write_accepted_order);
        print_sequence("Read consumed order:", read_consumed_order);

        bool same_order = (write_accepted_order.size() == read_consumed_order.size());
        int first_mismatch = -1;
        const std::size_t compare_len = std::min(write_accepted_order.size(), read_consumed_order.size());
        for (std::size_t i = 0; i < compare_len; ++i) {
            if (write_accepted_order[i] != read_consumed_order[i]) {
                same_order = false;
                first_mismatch = static_cast<int>(i);
                break;
            }
        }
        if (write_accepted_order.size() != read_consumed_order.size()) {
            same_order = false;
        }

        std::map<unsigned, int> value_balance;
        for (const auto& v : write_accepted_order) {
            value_balance[static_cast<unsigned>(v)]++;
        }
        for (const auto& v : read_consumed_order) {
            value_balance[static_cast<unsigned>(v)]--;
        }

        std::vector<unsigned> missing_values;
        std::vector<unsigned> unexpected_values;
        for (const auto& kv : value_balance) {
            if (kv.second > 0) {
                for (int i = 0; i < kv.second; ++i) missing_values.push_back(kv.first);
            } else if (kv.second < 0) {
                for (int i = 0; i < -kv.second; ++i) unexpected_values.push_back(kv.first);
            }
        }

        std::cout << "Order match (write == read): " << (same_order ? "YES" : "NO") << std::endl;
        if (!same_order && first_mismatch >= 0) {
            std::cout << "First mismatch index: " << first_mismatch << std::endl;
        }
        std::cout << "Missing data count: " << missing_values.size() << std::endl;
        if (!missing_values.empty()) {
            std::cout << "Missing data values:" << std::endl;
            for (unsigned v : missing_values) {
                std::cout << "  0x" << std::hex << std::setw(4) << std::setfill('0') << v
                          << std::dec << std::setfill(' ') << std::endl;
            }
        }
        std::cout << "Unexpected read data count: " << unexpected_values.size() << std::endl;
        if (!unexpected_values.empty()) {
            std::cout << "Unexpected read data values:" << std::endl;
            for (unsigned v : unexpected_values) {
                std::cout << "  0x" << std::hex << std::setw(4) << std::setfill('0') << v
                          << std::dec << std::setfill(' ') << std::endl;
            }
        }

        if (error_count == 0 && handshake_next_error_count == 0
            && count_mismatch_count == 0
            && pending_next_cycle_checks.empty() && pending_handshake_next_checks.empty()
            && same_order && missing_values.empty() && unexpected_values.empty()) {
            std::cout << "RESULT: PASS" << std::endl;
        } else {
            std::cout << "RESULT: FAIL" << std::endl;
        }

        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    Max_FIFO_TestBench tb("Max_FIFO_TestBench");
    sc_start();
    return 0;
}
