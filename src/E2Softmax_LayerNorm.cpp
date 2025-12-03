// E2Softmax_LayerNorm.cpp
#include "E2Softmax_LayerNorm.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <numeric>

std::vector<float> E2Softmax_Function(const std::vector<float>& input_fp16) {
    int L = input_fp16.size();
    std::vector<float> exp_buf(L);
    std::vector<float> output(L);

    float global_max = *std::max_element(input_fp16.begin(), input_fp16.end());

    float reduced_sum = 0.0f;
    for (int i = 0; i < L; ++i) {
        float diff = input_fp16[i] - global_max;
        float exp_val = exp2f(diff);
        exp_buf[i] = exp_val;
        reduced_sum += exp_val;
    }

    for (int i = 0; i < L; ++i)
        output[i] = exp_buf[i] / reduced_sum;

    return output;
}

std::vector<float> AILayerNorm_Function(const std::vector<float>& input_fp16) {
    int L = input_fp16.size();
    std::vector<float> output(L);

    float mean = std::accumulate(input_fp16.begin(), input_fp16.end(), 0.0f) / L;

    float var = 0.0f;
    for (auto v : input_fp16) {
        float diff = v - mean;
        var += diff * diff;
    }
    var /= L;
    float denom = sqrtf(var + 1e-6f);

    for (int i = 0; i < L; ++i) {
        float norm = (input_fp16[i] - mean) / denom;
        output[i] = norm;
    }
    return output;
}

// FP16 version of E2Softmax_LayerNorm
class E2Softmax_LayerNorm : public sc_module {
public:
    sc_in<bool> clk;
    sc_fifo_in<int> len_fifo;
    sc_fifo_in<float> in_fifo;
    sc_fifo_out<float> out_fifo;

    SC_HAS_PROCESS(E2Softmax_LayerNorm);

    E2Softmax_LayerNorm(sc_module_name name) : sc_module(name) {
        SC_THREAD(process);
        sensitive << clk.pos();
        dont_initialize();
    }

private:
    void process() {
        while (true) {
            wait();
            int L;
            if (!len_fifo.nb_read(L)) continue;
            std::vector<float> input_fp16(L);
            for (int i = 0; i < L; ++i) in_fifo.read(input_fp16[i]);
            auto softmax_out = E2Softmax_Function(input_fp16);
            auto layernorm_out = AILayerNorm_Function(softmax_out);
            for (auto v : layernorm_out) out_fifo.write(v);
        }
    }
};
