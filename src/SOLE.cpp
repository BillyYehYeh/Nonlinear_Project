// main.cpp
#include "E2Softmax_LayerNorm.h"
#include <systemc>
#include <iostream>
using namespace std;

SC_MODULE(Testbench) {
    sc_fifo<int16_t> in_fifo;
    sc_fifo<int> len_fifo;
    sc_fifo<int16_t> out_fifo;
    sc_clock clk;
    E2Softmax_LayerNorm *dut;

    SC_CTOR(Testbench)
        : in_fifo(1024), len_fifo(4), out_fifo(1024),
          clk("clk", sc_time(10, SC_NS)) {
        dut = new E2Softmax_LayerNorm("dut");
        dut->in_fifo(in_fifo);
        dut->len_fifo(len_fifo);
        dut->out_fifo(out_fifo);
        dut->clk(clk);

        SC_THREAD(feed);
        SC_THREAD(drain);
    }

    void feed() {
        wait(5, SC_NS);
        vector<double> vals = {1.0, 2.0, 0.5};
        len_fifo.write(vals.size());
        for (auto v : vals) in_fifo.write((int16_t)round(v * 16.0));
    }

    void drain() {
        for (int i = 0; i < 3; ++i) {
            int16_t out;
            out_fifo.read(out);
            cout << "Output[\" << i << \"] = \" << (out / 256.0) " << endl;
        }
        sc_stop();
    }
};

int sc_main(int argc, char* argv[]) {
    Testbench tb("tb");
    sc_start();
    return 0;
}
