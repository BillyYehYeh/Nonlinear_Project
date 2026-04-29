#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
SRC_TEST_DIR="$ROOT_DIR/test"
PKG_DIR="$ROOT_DIR/test/SOLE_Execution_Time_TEST"
LOG_DIR="$PKG_DIR/log"

DATA_FILE="$SRC_TEST_DIR/SOLE_test_Data.txt"
RESULT_LOG="$SRC_TEST_DIR/SOLE_test_Result.log"
MONITOR_LOG="$SRC_TEST_DIR/SOLE_test_Monitor.log"

RESULTS_CSV="$PKG_DIR/softmax_exec_time_results.csv"
OVERLIMIT_CSV="$PKG_DIR/softmax_overlimit_result.csv"
PLOT_DATA_TXT="$PKG_DIR/softmax_exec_time_plot_data.txt"
PLOT_SCRIPT="$PKG_DIR/plot_softmax_exec_time.py"
PLOT_PNG="$PKG_DIR/softmax_exec_time_plot.png"
REPORT_MD="$PKG_DIR/SOFTMAX_EXECUTION_TIME_REPORT.md"
README_TXT="$PKG_DIR/read_me.txt"

COUNTS_STR="${COUNTS:-1 2 4 8 16 32 64 96 128 160 192 256 384 512 768 1024 1536 2048 3072 4096}"
OVER_LIMIT_COUNT="${OVER_LIMIT_COUNT:-4097}"
BUILD_FIRST="${BUILD_FIRST:-1}"
COSINE_THRESHOLD="${COSINE_THRESHOLD:-0.95}"

mkdir -p "$PKG_DIR" "$LOG_DIR"

if [[ ! -f "$DATA_FILE" ]]; then
  echo "[ERROR] Missing input data file: $DATA_FILE" >&2
  exit 1
fi

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "[ERROR] Missing build directory: $BUILD_DIR" >&2
  exit 1
fi

backup_file="$(mktemp "$PKG_DIR/SOLE_test_Data.txt.backup.XXXXXX")"
cp "$DATA_FILE" "$backup_file"
restore_data_file() {
  cp "$backup_file" "$DATA_FILE" >/dev/null 2>&1 || true
  rm -f "$backup_file"
}
trap restore_data_file EXIT

if [[ "$BUILD_FIRST" == "1" ]]; then
  echo "[INFO] Building SOLE_test..."
  make -C "$BUILD_DIR" -j4 SOLE_test >/dev/null
fi

if [[ ! -x "$BUILD_DIR/SOLE_test" ]]; then
  echo "[ERROR] Missing executable: $BUILD_DIR/SOLE_test" >&2
  exit 1
fi

rm -f "$LOG_DIR"/*.log

echo "input_count,execution_time_ns,cosine_similarity,cosine_gt_0_95,timeout_detected" > "$RESULTS_CSV"

run_one_case() {
  local n="$1"
  local run_log="$LOG_DIR/sole_run_${n}.log"

  awk -v n="$n" 'BEGIN{for(i=1;i<=n;i++) printf "%.6f\n", (1.0 + ((i-1)%17)*0.1)}' > "$DATA_FILE"

  rm -f "$RESULT_LOG" "$MONITOR_LOG"
  (
    cd "$BUILD_DIR"
    ./SOLE_test
  ) > "$run_log" 2>&1 || true

  if [[ -f "$RESULT_LOG" ]]; then
    cp "$RESULT_LOG" "$LOG_DIR/SOLE_test_Result_case_${n}.log"
  fi
  if [[ -f "$MONITOR_LOG" ]]; then
    cp "$MONITOR_LOG" "$LOG_DIR/SOLE_test_Monitor_case_${n}.log"
  fi

  local exec_ns
  exec_ns="$(grep -oE "\[EXECUTION TIME\] SOLE Execution Time: [0-9]+ ns" "$RESULT_LOG" 2>/dev/null | tail -n1 | grep -oE "[0-9]+" | tail -n1 || true)"
  if [[ -z "${exec_ns:-}" ]]; then
    exec_ns="NA"
  fi

  local cosine
  cosine="$(grep -oE "\[ANALYSIS\] Cosine Similarity \(HW vs SW\) = [0-9]+(\.[0-9]+)?" "$RESULT_LOG" 2>/dev/null | tail -n1 | awk '{print $NF}' || true)"
  if [[ -z "${cosine:-}" ]]; then
    cosine="NA"
  fi

  local cosine_pass="NA"
  if [[ "$cosine" != "NA" ]]; then
    cosine_pass="$(awk -v c="$cosine" -v t="$COSINE_THRESHOLD" 'BEGIN{if ((c+0) > (t+0)) print "yes"; else print "no"}')"
  fi

  local timeout="no"
  if grep -q "\[TIMEOUT\]" "$run_log" 2>/dev/null || grep -q "ERROR: Timeout waiting for done signal" "$RESULT_LOG" 2>/dev/null; then
    timeout="yes"
  fi

  echo "$n,$exec_ns,$cosine,$cosine_pass,$timeout" >> "$RESULTS_CSV"
  echo "[CASE] n=$n exec=${exec_ns}ns cosine=$cosine pass(>${COSINE_THRESHOLD})=$cosine_pass timeout=$timeout"
}

echo "[INFO] Running timing sweep..."
for n in $COUNTS_STR; do
  run_one_case "$n"
done

echo "[INFO] Running over-limit case: $OVER_LIMIT_COUNT"
over_run_log="$LOG_DIR/sole_run_${OVER_LIMIT_COUNT}.log"
awk -v n="$OVER_LIMIT_COUNT" 'BEGIN{for(i=1;i<=n;i++) printf "%.6f\n", (1.0 + ((i-1)%17)*0.1)}' > "$DATA_FILE"
rm -f "$RESULT_LOG" "$MONITOR_LOG"
(
  cd "$BUILD_DIR"
  ./SOLE_test
) > "$over_run_log" 2>&1 || true

if [[ -f "$RESULT_LOG" ]]; then
  cp "$RESULT_LOG" "$LOG_DIR/SOLE_test_Result_case_${OVER_LIMIT_COUNT}.log"
fi
if [[ -f "$MONITOR_LOG" ]]; then
  cp "$MONITOR_LOG" "$LOG_DIR/SOLE_test_Monitor_case_${OVER_LIMIT_COUNT}.log"
fi

over_exec_ns="$(grep -oE "\[EXECUTION TIME\] SOLE Execution Time: [0-9]+ ns" "$RESULT_LOG" 2>/dev/null | tail -n1 | grep -oE "[0-9]+" | tail -n1 || true)"
if [[ -z "${over_exec_ns:-}" ]]; then
  over_exec_ns="NA"
fi

over_cosine="$(grep -oE "\[ANALYSIS\] Cosine Similarity \(HW vs SW\) = [0-9]+(\.[0-9]+)?" "$RESULT_LOG" 2>/dev/null | tail -n1 | awk '{print $NF}' || true)"
if [[ -z "${over_cosine:-}" ]]; then
  over_cosine="NA"
fi

over_cosine_pass="NA"
if [[ "$over_cosine" != "NA" ]]; then
  over_cosine_pass="$(awk -v c="$over_cosine" -v t="$COSINE_THRESHOLD" 'BEGIN{if ((c+0) > (t+0)) print "yes"; else print "no"}')"
fi

over_timeout="no"
if grep -q "\[TIMEOUT\]" "$over_run_log" 2>/dev/null || grep -q "ERROR: Timeout waiting for done signal" "$RESULT_LOG" 2>/dev/null; then
  over_timeout="yes"
fi

{
  echo "input_count,execution_time_ns,cosine_similarity,cosine_gt_0_95,timeout_detected"
  echo "$OVER_LIMIT_COUNT,$over_exec_ns,$over_cosine,$over_cosine_pass,$over_timeout"
} > "$OVERLIMIT_CSV"

{
  echo "case_type,input_count,execution_time_ns,cosine_similarity,cosine_gt_threshold,timeout_detected"
  awk -F, 'NR>1 {printf "within_limit,%s,%s,%s,%s,%s\n", $1, $2, $3, $4, $5}' "$RESULTS_CSV"
  awk -F, 'NR>1 {printf "over_limit,%s,%s,%s,%s,%s\n", $1, $2, $3, $4, $5}' "$OVERLIMIT_CSV"
} > "$PLOT_DATA_TXT"

plot_status="not_run"
plot_message="Python plot script not found"
if [[ -f "$PLOT_SCRIPT" ]]; then
  if command -v python3 >/dev/null 2>&1; then
    if python3 "$PLOT_SCRIPT" --input "$PLOT_DATA_TXT" --output "$PLOT_PNG" --title "SOLE Input Count vs Execution Time"; then
      plot_status="ok"
      plot_message="Plot generated successfully"
    else
      plot_status="failed"
      plot_message="Plot generation failed (check Python/matplotlib)"
    fi
  else
    plot_status="failed"
    plot_message="python3 not found"
  fi
fi

x_vals="$(awk -F, 'NR>1 && $2!="NA" {if(c++) printf ", "; printf "%s", $1}' "$RESULTS_CSV")"
y_vals="$(awk -F, 'NR>1 && $2!="NA" {if(c++) printf ", "; printf "%s", $2}' "$RESULTS_CSV")"
y_max="$(awk -F, 'NR>1 && $2!="NA" {if($2>m)m=$2} END{if(m<1)m=1; printf "%d", int(m*1.15+10)}' "$RESULTS_CSV")"

case_count="$(awk 'END{print NR-1}' "$RESULTS_CSV")"
cosine_pass_count="$(awk -F, 'NR>1 && $4=="yes" {c++} END{print c+0}' "$RESULTS_CSV")"
timeout_count="$(awk -F, 'NR>1 && $5=="yes" {c++} END{print c+0}' "$RESULTS_CSV")"

run_time_str="$(date '+%Y-%m-%d %H:%M:%S %Z')"

{
  echo "# SOLE Softmax Timing Report (Auto Generated)"
  echo
  echo "Generated at: $run_time_str"
  echo
  echo "## Test Configuration"
  echo "- Testbench: test/SOLE_test.cpp"
  echo "- Timeout macro: MAX_TIMEOUT_CYCLES=10000"
  echo "- Build before run: $BUILD_FIRST"
  echo "- Cosine pass threshold: > $COSINE_THRESHOLD"
  echo "- Tested counts: $COUNTS_STR"
  echo
  echo "## Summary"
  echo "- Total cases: $case_count"
  echo "- Cosine > $COSINE_THRESHOLD cases: $cosine_pass_count"
  echo "- Timeout cases within 1..4096 sweep: $timeout_count"
  echo
  echo "## Per-Case Results"
  echo "Legend: 🟠 Input Count/Execution Time | 🟢 Pass | 🔴 Fail"
  echo
  echo "\`\`\`text"
  printf "%-12s %-20s %-20s %-14s %-10s\n" "InputCount" "ExecutionTime(ns)" "CosineSimilarity" "CosineCheck" "Timeout"
  printf "%-12s %-20s %-20s %-14s %-10s\n" "------------" "--------------------" "--------------------" "--------------" "----------"
  awk -F, '
    NR>1 {
      input_cell = "🟠 " $1;
      exec_cell = "🟠 " $2;

      if ($4 == "yes") status_cell = "🟢 PASS";
      else if ($4 == "no") status_cell = "🔴 FAIL";
      else status_cell = "⚪ N/A";

      if ($5 == "yes") timeout_cell = "🔴 YES";
      else if ($5 == "no") timeout_cell = "🟢 NO";
      else timeout_cell = "⚪ N/A";

      printf "%-12s %-20s %-20s %-14s %-10s\n", input_cell, exec_cell, $3, status_cell, timeout_cell;
    }
  ' "$RESULTS_CSV"
  echo "\`\`\`"
  echo
  echo "## Input Count vs Execution Time"
  echo
  if [[ "$plot_status" == "ok" ]]; then
    echo "![SOLE Input Count vs Execution Time](softmax_exec_time_plot.png)"
    echo
    echo "Plot status: $plot_message"
  else
    echo "Plot status: $plot_message"
    echo "(Fallback data is available in softmax_exec_time_plot_data.txt)"
    echo
    echo "\`\`\`text"
    echo "x values (input count): [$x_vals]"
    echo "y values (execution time ns): [$y_vals]"
    echo "\`\`\`"
  fi
  echo
  echo "## Over-limit Check (>4096)"
  echo "\`\`\`text"
  printf "%-12s %-20s %-20s %-14s %-10s\n" "InputCount" "ExecutionTime(ns)" "CosineSimilarity" "CosineCheck" "Timeout"
  printf "%-12s %-20s %-20s %-14s %-10s\n" "------------" "--------------------" "--------------------" "--------------" "----------"
  awk -F, '
    NR>1 {
      input_cell = "🟠 " $1;
      exec_cell = "🟠 " $2;

      if ($4 == "yes") status_cell = "🟢 PASS";
      else if ($4 == "no") status_cell = "🔴 FAIL";
      else status_cell = "⚪ N/A";

      if ($5 == "yes") timeout_cell = "🔴 YES";
      else if ($5 == "no") timeout_cell = "🟢 NO";
      else timeout_cell = "⚪ N/A";

      printf "%-12s %-20s %-20s %-14s %-10s\n", input_cell, exec_cell, $3, status_cell, timeout_cell;
    }
  ' "$OVERLIMIT_CSV"
  echo "\`\`\`"
  echo
  if [[ "$over_timeout" == "yes" ]]; then
    echo "Conclusion: input count > 4096 triggers timeout in this run."
  else
    echo "Conclusion: input count > 4096 did not trigger timeout in this run."
  fi
  echo
  echo "## Artifacts"
  echo "- CSV: test/SOLE_Execution_Time_TEST/softmax_exec_time_results.csv"
  echo "- Over-limit CSV: test/SOLE_Execution_Time_TEST/softmax_overlimit_result.csv"
  echo "- Plot data TXT: test/SOLE_Execution_Time_TEST/softmax_exec_time_plot_data.txt"
  echo "- Plot image: test/SOLE_Execution_Time_TEST/softmax_exec_time_plot.png"
  echo "- Plot script: test/SOLE_Execution_Time_TEST/plot_softmax_exec_time.py"
  echo "- Main report: test/SOLE_Execution_Time_TEST/SOFTMAX_EXECUTION_TIME_REPORT.md"
  echo "- Raw logs: test/SOLE_Execution_Time_TEST/log/*.log"
} > "$REPORT_MD"

{
  echo "SOLE Execution Time Test Package"
  echo "================================"
  echo
  echo "1) Run full default test (20 cases + over-limit):"
  echo "   ./run_softmax_timing_report.sh"
  echo
  echo "2) Run without rebuilding:"
  echo "   BUILD_FIRST=0 ./run_softmax_timing_report.sh"
  echo
  echo "3) Custom case list:"
  echo "   COUNTS=\"1 8 32 256 1024 4096\" ./run_softmax_timing_report.sh"
  echo
  echo "4) Custom over-limit case:"
  echo "   OVER_LIMIT_COUNT=5000 ./run_softmax_timing_report.sh"
  echo
  echo "5) Custom cosine threshold (default is 0.95):"
  echo "   COSINE_THRESHOLD=0.95 ./run_softmax_timing_report.sh"
  echo
  echo "Generated files:"
  echo "- SOFTMAX_EXECUTION_TIME_REPORT.md"
  echo "- softmax_exec_time_results.csv"
  echo "- softmax_overlimit_result.csv"
  echo "- softmax_exec_time_plot_data.txt"
  echo "- softmax_exec_time_plot.png"
  echo "- plot_softmax_exec_time.py"
  echo "- log/*.log"
  echo
  echo "Notes:"
  echo "- Script restores test/SOLE_test_Data.txt after run."
  echo "- The DUT executable must be build/SOLE_test."
} > "$README_TXT"

echo "[DONE] Results CSV: $RESULTS_CSV"
echo "[DONE] Over-limit CSV: $OVERLIMIT_CSV"
echo "[DONE] Plot data TXT: $PLOT_DATA_TXT"
echo "[DONE] Plot PNG: $PLOT_PNG"
echo "[DONE] Report: $REPORT_MD"
echo "[DONE] Readme: $README_TXT"
echo "[DONE] Logs dir: $LOG_DIR"
