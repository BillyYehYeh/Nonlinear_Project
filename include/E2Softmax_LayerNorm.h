// E2Softmax_LayerNorm.h
#ifndef E2SOFTMAX_LAYERNORM_H
#define E2SOFTMAX_LAYERNORM_H

#include <systemc>
#include <vector>

using namespace sc_core;
using namespace sc_dt;
using namespace std;

// 函式宣告（由 cpp 實作）
std::vector<int16_t> E2Softmax_Function(const std::vector<int16_t>& input_q4);
std::vector<int16_t> AILayerNorm_Function(const std::vector<int16_t>& input_q8);

// SystemC 模組宣告
SC_MODULE(E2Softmax_LayerNorm) {
    sc_fifo_in<int16_t> in_fifo;
    sc_fifo_in<int> len_fifo;
    sc_fifo_out<int16_t> out_fifo;
    sc_in<bool> clk;

    void process();

    SC_CTOR(E2Softmax_LayerNorm);
};

#endif
