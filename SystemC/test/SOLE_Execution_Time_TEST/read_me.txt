SOLE Execution Time Test Package
================================

1) Run full default test (20 cases + over-limit):
   ./run_softmax_timing_report.sh

2) Run without rebuilding:
   BUILD_FIRST=0 ./run_softmax_timing_report.sh

3) Custom case list:
   COUNTS="1 8 32 256 1024 4096" ./run_softmax_timing_report.sh

4) Custom over-limit case:
   OVER_LIMIT_COUNT=5000 ./run_softmax_timing_report.sh

5) Custom cosine threshold (default is 0.95):
   COSINE_THRESHOLD=0.95 ./run_softmax_timing_report.sh

Generated files:
- SOFTMAX_EXECUTION_TIME_REPORT.md
- softmax_exec_time_results.csv
- softmax_overlimit_result.csv
- softmax_exec_time_plot_data.txt
- softmax_exec_time_plot.png
- plot_softmax_exec_time.py
- log/*.log

Notes:
- Script restores test/SOLE_test_Data.txt after run.
- The DUT executable must be build/SOLE_test.
