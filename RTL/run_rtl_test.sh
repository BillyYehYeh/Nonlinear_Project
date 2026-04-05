#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

if command -v iverilog >/dev/null 2>&1; then
  iverilog -g2012 -s SOLE_test -o sole_test_sim \
    SOLE_test.sv SOLE.sv Softmax.sv PROCESS_1.sv PROCESS_2.sv PROCESS_3.sv \
    Divider.sv Divider_PreCompute.sv Log2Exp.sv Reduction.sv MaxUnit.sv \
    Max_FIFO.sv Output_FIFO.sv SRAM.sv
  vvp sole_test_sim "$@"
else
  echo "[WARN] iverilog not found; running behavioral reference test instead"
  python3 rtl_reference_test.py
fi
