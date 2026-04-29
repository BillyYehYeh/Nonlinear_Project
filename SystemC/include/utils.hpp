#ifndef UTILS_H
#define UTILS_H

#include <systemc.h>
#include <string>
#include <iostream>
#include <fstream>
typedef uint16_t fp16_t; 

using sc_uint16 = sc_dt::sc_uint<16>;

fp16_t fp16_add(fp16_t a, fp16_t b);
fp16_t fp16_mul(fp16_t a, fp16_t b);
sc_uint16 fp16_max(sc_uint16 a_bits, sc_uint16 b_bits);

class Logger {
public:
    Logger(const std::string& module_name, const std::string& filename, bool append = true)
        : filename(filename), module_name(module_name)
    {
        of.open(filename, append ? std::ios::app : std::ios::out);
        if (!of.is_open()) {
            std::cerr << "Logger Error: Could not open file " << filename << std::endl;
        } else {
            of << "=== Log Start for " << module_name << " ===" << std::endl;
        }
    }

    ~Logger() {
        if (of.is_open()) of.close();
    }

    // Write a message to the log with module prefix
    void log(const std::string& msg) {
        if (!of.is_open()) return;
        of << "[" << module_name << "] " << msg << std::endl;
    }

    // Convenience stream-style logging
    template<typename T>
    Logger& operator<<(const T& v) {
        if (of.is_open()) of << v;
        return *this;
    }

private:
    std::string filename;
    std::string module_name;
    std::ofstream of;
};

#endif // UTILS_H