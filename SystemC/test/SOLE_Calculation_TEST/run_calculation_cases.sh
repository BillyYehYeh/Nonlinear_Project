#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
SRC_TEST_DIR="$ROOT_DIR/test"
CASE_DIR="$SCRIPT_DIR/testcases"
LOG_DIR="$SCRIPT_DIR/log"

DATA_FILE="$SRC_TEST_DIR/SOLE_test_Data.txt"
RESULT_LOG="$SRC_TEST_DIR/SOLE_test_Result.log"
MONITOR_LOG="$SRC_TEST_DIR/SOLE_test_Monitor.log"
RESULTS_CSV="$SCRIPT_DIR/cosine_results.csv"
REPORT_MD="$SCRIPT_DIR/SOLE_CALCULATION_TEST_REPORT.md"
README_TXT="$SCRIPT_DIR/read_me.txt"

BUILD_FIRST="${BUILD_FIRST:-1}"
COSINE_THRESHOLD="${COSINE_THRESHOLD:-0.95}"

mkdir -p "$LOG_DIR"

if [[ ! -d "$CASE_DIR" ]]; then
  echo "[ERROR] Missing testcase directory: $CASE_DIR" >&2
  exit 1
fi

if [[ ! -f "$DATA_FILE" ]]; then
  echo "[ERROR] Missing source data file: $DATA_FILE" >&2
  exit 1
fi

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "[ERROR] Missing build directory: $BUILD_DIR" >&2
  exit 1
fi

backup_file="$(mktemp "$SCRIPT_DIR/SOLE_test_Data.txt.backup.XXXXXX")"
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

echo "case_name,input_count,execution_time_ns,cosine_similarity,cosine_gt_threshold,timeout_detected" > "$RESULTS_CSV"

run_case() {
  local case_file="$1"
  local case_name
  case_name="$(basename "$case_file" .txt)"
  local input_count
  input_count="$(wc -l < "$case_file" | tr -d ' ')"

  if [[ "$input_count" -lt 100 ]]; then
    echo "[WARN] Skip $case_name: input count < 100 ($input_count)"
    return
  fi

  cp "$case_file" "$DATA_FILE"

  local run_log="$LOG_DIR/${case_name}.log"
  rm -f "$RESULT_LOG" "$MONITOR_LOG"
  (
    cd "$BUILD_DIR"
    ./SOLE_test
  ) > "$run_log" 2>&1 || true

  if [[ -f "$RESULT_LOG" ]]; then
    cp "$RESULT_LOG" "$LOG_DIR/${case_name}_SOLE_test_Result.log"
  fi
  if [[ -f "$MONITOR_LOG" ]]; then
    cp "$MONITOR_LOG" "$LOG_DIR/${case_name}_SOLE_test_Monitor.log"
  fi

  local exec_ns
  exec_ns="$(grep -oE "\[EXECUTION TIME\] SOLE Execution Time: [0-9]+ ns" "$RESULT_LOG" 2>/dev/null | tail -n1 | grep -oE "[0-9]+" | tail -n1 || true)"
  [[ -z "${exec_ns:-}" ]] && exec_ns="NA"

  local cosine
  cosine="$(grep -oE "\[ANALYSIS\] Cosine Similarity \(HW vs SW\) = [0-9]+(\.[0-9]+)?" "$RESULT_LOG" 2>/dev/null | tail -n1 | awk '{print $NF}' || true)"
  [[ -z "${cosine:-}" ]] && cosine="NA"

  local pass="NA"
  if [[ "$cosine" != "NA" ]]; then
    pass="$(awk -v c="$cosine" -v t="$COSINE_THRESHOLD" 'BEGIN{if ((c+0) > (t+0)) print "yes"; else print "no"}')"
  fi

  local timeout="no"
  if grep -q "\[TIMEOUT\]" "$run_log" 2>/dev/null || grep -q "ERROR: Timeout waiting for done signal" "$RESULT_LOG" 2>/dev/null; then
    timeout="yes"
  fi

  echo "$case_name,$input_count,$exec_ns,$cosine,$pass,$timeout" >> "$RESULTS_CSV"
  echo "[CASE] $case_name count=$input_count exec=${exec_ns}ns cosine=$cosine pass(>$COSINE_THRESHOLD)=$pass timeout=$timeout"
}

echo "[INFO] Running SOLE calculation testcases..."
while IFS= read -r case_file; do
  run_case "$case_file"
done < <(find "$CASE_DIR" -maxdepth 1 -type f -name "*.txt" | sort)

run_time_str="$(date '+%Y-%m-%d %H:%M:%S %Z')"
case_count="$(awk 'END{print NR-1}' "$RESULTS_CSV")"
pass_count="$(awk -F, 'NR>1 && $5=="yes" {c++} END{print c+0}' "$RESULTS_CSV")"
timeout_count="$(awk -F, 'NR>1 && $6=="yes" {c++} END{print c+0}' "$RESULTS_CSV")"

{
  echo "# SOLE Calculation Multi-Case Validation Report"
  echo
  echo "Generated at: $run_time_str"
  echo
  echo "## Test Configuration"
  echo "- Testbench: test/SOLE_test.cpp"
  echo "- Testcase directory: test/SOLE_Calculation_TEST/testcases"
  echo "- Cosine pass threshold: > $COSINE_THRESHOLD"
  echo "- Build before run: $BUILD_FIRST"
  echo
  echo "## Summary"
  echo "- Total testcases executed: $case_count"
  echo "- Cosine pass cases: $pass_count"
  echo "- Timeout cases: $timeout_count"
  echo
  echo "## Per-Case Cosine Results"
  echo "Legend: 🔵 Case | 🟢 Pass | 🔴 Fail"
  echo
  echo "\`\`\`text"
  printf "%-4s %-62s %-12s %-20s %-22s %-14s %-10s\n" "Tag" "Case Name" "InputCount" "ExecutionTime(ns)" "CosineSimilarity" "CosineCheck" "Timeout"
  printf "%-4s %-62s %-12s %-20s %-22s %-14s %-10s\n" "----" "--------------------------------------------------------------" "------------" "--------------------" "----------------------" "--------------" "----------"
  awk -F, '
    NR>1 {
      case_cell = $1;
      tag_cell = "🔵";

      if ($5 == "yes") {
        cosine_cell = "🟢 " $4;
        pass_cell = "🟢 PASS";
      } else if ($5 == "no") {
        cosine_cell = "🔴 " $4;
        pass_cell = "🔴 FAIL";
      } else {
        cosine_cell = "⚪ " $4;
        pass_cell = "⚪ N/A";
      }

      if ($6 == "yes") {
        timeout_cell = "🔴 YES";
      } else if ($6 == "no") {
        timeout_cell = "🟢 NO";
      } else {
        timeout_cell = "⚪ N/A";
      }

      printf "%-4s %-62s %-12s %-20s %-22s %-14s %-10s\n", tag_cell, case_cell, $2, $3, cosine_cell, pass_cell, timeout_cell;
    }
  ' "$RESULTS_CSV"
  echo "\`\`\`"
  echo
  echo "## testcase"
  echo "- 01_uniform_distribution"
  echo "  - 目的: 驗證模型在完全平均輸入下是否穩定。"
  echo "  - 範例: [1, 1, 1, 1, ..., 1]"
  echo "- 02_high_contrast_1_2_10"
  echo "  - 目的: 驗證高對比輸入時最大值主導情境。"
  echo "  - 範例: [1, 2, 10, 1, 2, 10, ...]"
  echo "- 03_extreme_large_1000_1001_1002"
  echo "  - 目的: 驗證極端大值輸入時的數值表現。"
  echo "  - 範例: [1000, 1001, 1002, ...]"
  echo "- 04_close_values_1p001_1p002_1p003"
  echo "  - 目的: 驗證非常接近輸入的分辨能力。"
  echo "  - 範例: [1.001, 1.002, 1.003, ...]"
  echo "- 05_all_zeros"
  echo "  - 目的: 驗證全零輸入行為。"
  echo "  - 範例: [0, 0, 0, 0, ...]"
  echo "- 06_alternating_sign_pm5"
  echo "  - 目的: 驗證正負交錯輸入。"
  echo "  - 範例: [-5, 5, -5, 5, ...]"
  echo "- 07_single_spike"
  echo "  - 目的: 驗證單一尖峰值的主導效應。"
  echo "  - 範例: [0, 0, ..., 20, ..., 0]"
  echo "- 08_increasing_ramp_minus4_to_4"
  echo "  - 目的: 驗證單調遞增輸入。"
  echo "  - 範例: [-4.0, -3.9, ..., 4.0]"
  echo "- 09_decreasing_ramp_4_to_minus4"
  echo "  - 目的: 驗證單調遞減輸入。"
  echo "  - 範例: [4.0, 3.9, ..., -4.0]"
  echo "- 10_bimodal_clusters"
  echo "  - 目的: 驗證雙峰分布輸入。"
  echo "  - 範例: [-2, -2, ..., 2, 2, ...]"
  echo "- 11_tiny_magnitudes"
  echo "  - 目的: 驗證接近零的小幅度輸入。"
  echo "  - 範例: [-0.001, 0.0, 0.001, 0.002, ...]"
  echo
  echo "## Artifacts"
  echo "- Testcases: test/SOLE_Calculation_TEST/testcases/*.txt"
  echo "- Raw logs: test/SOLE_Calculation_TEST/log/*.log"
  echo "- CSV results: test/SOLE_Calculation_TEST/cosine_results.csv"
  echo "- Report: test/SOLE_Calculation_TEST/SOLE_CALCULATION_TEST_REPORT.md"
} > "$REPORT_MD"

{
  echo "SOLE Calculation Test Package 使用說明"
  echo "===================================="
  echo
  echo "1) 執行全部 testcase（建議）"
  echo "   ./run_calculation_cases.sh"
  echo
  echo "2) 不重新編譯，直接跑測試"
  echo "   BUILD_FIRST=0 ./run_calculation_cases.sh"
  echo
  echo "3) 設定 cosine 門檻（預設 0.95）"
  echo "   COSINE_THRESHOLD=0.95 ./run_calculation_cases.sh"
  echo
  echo "輸出檔案："
  echo "- SOLE_CALCULATION_TEST_REPORT.md : 測試報告（含每個 case 的 cosine）"
  echo "- cosine_results.csv              : 機器可讀結果"
  echo "- log/*.log                       : 每次測試原始 log"
  echo
  echo "testcases 規則："
  echo "- 每個 .txt 一行一個浮點數"
  echo "- 少於 100 筆會自動略過"
  echo
  echo "注意事項："
  echo "- 腳本執行後會自動還原 test/SOLE_test_Data.txt"
  echo "- 需要存在可執行檔 build/SOLE_test"
} > "$README_TXT"

echo "[DONE] Results CSV: $RESULTS_CSV"
echo "[DONE] Report: $REPORT_MD"
echo "[DONE] Readme: $README_TXT"
echo "[DONE] Logs dir: $LOG_DIR"
