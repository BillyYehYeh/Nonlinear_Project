#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RTL_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
OUT_DIR="$SCRIPT_DIR"
RAW_DIR="$OUT_DIR/raw_logs"
TSV="$OUT_DIR/post_sim_delay_scan_results.tsv"
REPORT="$OUT_DIR/SOLE_post_sim_AXI_DELAY_REPORT.md"

mkdir -p "$RAW_DIR"

# delay_name target_error_code
DELAY_NAMES=("AXI_READ_ARREADY_DELAY" "AXI_READ_RVALID_DELAY" "AXI_WRITE_WREADY_DELAY")
TARGET_CODES=("0x2" "0x2" "0x5")

run_case() {
  local label="$1"
  local ar="$2"
  local rv="$3"
  local wr="$4"
  local rec="$5"

  local case_log="$RAW_DIR/${label}.log"
  local mon_snap="$RAW_DIR/${label}_monitor.log"
  local res_snap="$RAW_DIR/${label}_result.log"

  echo "[RUN] $label (AR=$ar RV=$rv WR=$wr REC=$rec)"

  set +e
  (
    cd "$RTL_DIR"
    tcsh -c "make post_sim AXI_READ_ARREADY_DELAY=$ar AXI_READ_RVALID_DELAY=$rv AXI_WRITE_WREADY_DELAY=$wr error_recovery_test=$rec"
  ) >"$case_log" 2>&1
  local make_rc=$?
  set -e

  if [[ -f "$RTL_DIR/log/SOLE_test_Monitor_post_sim.log" ]]; then
    cp "$RTL_DIR/log/SOLE_test_Monitor_post_sim.log" "$mon_snap"
  else
    : > "$mon_snap"
  fi

  if [[ -f "$RTL_DIR/log/SOLE_test_Result_post_sim.log" ]]; then
    cp "$RTL_DIR/log/SOLE_test_Result_post_sim.log" "$res_snap"
  else
    : > "$res_snap"
  fi

  local verdict="UNKNOWN"
  if grep -q "\[RTL TEST\] PASS" "$case_log"; then
    verdict="PASS"
  elif grep -q "\[RTL TEST\] FAIL" "$case_log"; then
    verdict="FAIL"
  elif [[ "$make_rc" -ne 0 ]]; then
    verdict="MAKE_FAIL"
  fi

  local done_seen=0
  if grep -q "DONE pulse detected" "$res_snap"; then
    done_seen=1
  fi

  local error_code="0x0"
  local err_line
  err_line=$(grep "error=1" "$mon_snap" | tail -n 1 || true)
  if [[ -n "$err_line" ]]; then
    error_code=$(echo "$err_line" | sed -n 's/.*error_code=0x\([0-9a-fA-F]\+\).*/0x\1/p')
    [[ -z "$error_code" ]] && error_code="0x0"
  fi

  local first_state="-"
  first_state=$(grep -o "state=[A-Z0-9_]*" "$mon_snap" | head -n1 | cut -d= -f2 || true)
  [[ -z "$first_state" ]] && first_state="-"

  printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n" \
    "$label" "$ar" "$rv" "$wr" "$rec" "$make_rc" "$verdict" "$error_code" "$done_seen" >> "$TSV"

  echo "$make_rc|$verdict|$error_code|$done_seen|$first_state"
}

# Find minimal delay where target error code appears.
find_threshold() {
  local delay_name="$1"
  local target_code="$2"

  local lo=0
  local hi=1

  while true; do
    local ar=0 rv=0 wr=0
    if [[ "$delay_name" == "AXI_READ_ARREADY_DELAY" ]]; then ar=$hi; fi
    if [[ "$delay_name" == "AXI_READ_RVALID_DELAY" ]]; then rv=$hi; fi
    if [[ "$delay_name" == "AXI_WRITE_WREADY_DELAY" ]]; then wr=$hi; fi

    local label="${delay_name}_probe_${hi}"
    local ret
    ret=$(run_case "$label" "$ar" "$rv" "$wr" 0)
    local code
    code=$(echo "$ret" | cut -d'|' -f3)

    if [[ "$code" == "$target_code" ]]; then
      break
    fi

    lo=$hi
    hi=$((hi * 2))
    if (( hi > 256 )); then
      echo "-1"
      return 0
    fi
  done

  while (( lo + 1 < hi )); do
    local mid=$(((lo + hi) / 2))
    local ar=0 rv=0 wr=0
    if [[ "$delay_name" == "AXI_READ_ARREADY_DELAY" ]]; then ar=$mid; fi
    if [[ "$delay_name" == "AXI_READ_RVALID_DELAY" ]]; then rv=$mid; fi
    if [[ "$delay_name" == "AXI_WRITE_WREADY_DELAY" ]]; then wr=$mid; fi

    local label="${delay_name}_bin_${mid}"
    local ret
    ret=$(run_case "$label" "$ar" "$rv" "$wr" 0)
    local code
    code=$(echo "$ret" | cut -d'|' -f3)

    if [[ "$code" == "$target_code" ]]; then
      hi=$mid
    else
      lo=$mid
    fi
  done

  echo "$hi"
}

cat > "$TSV" <<'EOF'
label	AXI_READ_ARREADY_DELAY	AXI_READ_RVALID_DELAY	AXI_WRITE_WREADY_DELAY	error_recovery_test	make_rc	verdict	error_code	done_seen
EOF

declare -A THRESHOLDS

echo "[INFO] Scanning threshold for post_sim..."
for idx in 0 1 2; do
  name="${DELAY_NAMES[$idx]}"
  code="${TARGET_CODES[$idx]}"
  thr=$(find_threshold "$name" "$code")
  THRESHOLDS["$name"]="$thr"

  if [[ "$thr" != "-1" ]]; then
    m1=$((thr - 1))
    if (( m1 < 0 )); then m1=0; fi
    p1=$((thr + 1))

    ar=0; rv=0; wr=0
    if [[ "$name" == "AXI_READ_ARREADY_DELAY" ]]; then ar=$m1; fi
    if [[ "$name" == "AXI_READ_RVALID_DELAY" ]]; then rv=$m1; fi
    if [[ "$name" == "AXI_WRITE_WREADY_DELAY" ]]; then wr=$m1; fi
    run_case "${name}_confirm_d${m1}" "$ar" "$rv" "$wr" 0 >/dev/null

    ar=0; rv=0; wr=0
    if [[ "$name" == "AXI_READ_ARREADY_DELAY" ]]; then ar=$thr; fi
    if [[ "$name" == "AXI_READ_RVALID_DELAY" ]]; then rv=$thr; fi
    if [[ "$name" == "AXI_WRITE_WREADY_DELAY" ]]; then wr=$thr; fi
    run_case "${name}_confirm_d${thr}" "$ar" "$rv" "$wr" 0 >/dev/null

    ar=0; rv=0; wr=0
    if [[ "$name" == "AXI_READ_ARREADY_DELAY" ]]; then ar=$p1; fi
    if [[ "$name" == "AXI_READ_RVALID_DELAY" ]]; then rv=$p1; fi
    if [[ "$name" == "AXI_WRITE_WREADY_DELAY" ]]; then wr=$p1; fi
    run_case "${name}_confirm_d${p1}" "$ar" "$rv" "$wr" 0 >/dev/null
  fi
done

echo "[INFO] Running recovery tests..."
run_case "recovery_base" 0 0 0 1 >/dev/null
run_case "recovery_stress_ar${THRESHOLDS[AXI_READ_ARREADY_DELAY]}" "${THRESHOLDS[AXI_READ_ARREADY_DELAY]}" 0 0 1 >/dev/null
run_case "recovery_stress_rv${THRESHOLDS[AXI_READ_RVALID_DELAY]}" 0 "${THRESHOLDS[AXI_READ_RVALID_DELAY]}" 0 1 >/dev/null
run_case "recovery_stress_wr${THRESHOLDS[AXI_WRITE_WREADY_DELAY]}" 0 0 "${THRESHOLDS[AXI_WRITE_WREADY_DELAY]}" 1 >/dev/null

extract_row() {
  local key="$1"
  grep "^${key}[[:space:]]" "$TSV" | tail -n 1 || true
}

read_col() {
  local row="$1"
  local col="$2"
  echo "$row" | awk -F'\t' -v c="$col" '{print $c}'
}

ar_thr="${THRESHOLDS[AXI_READ_ARREADY_DELAY]}"
rv_thr="${THRESHOLDS[AXI_READ_RVALID_DELAY]}"
wr_thr="${THRESHOLDS[AXI_WRITE_WREADY_DELAY]}"

ar_m1=$((ar_thr > 0 ? ar_thr - 1 : 0))
rv_m1=$((rv_thr > 0 ? rv_thr - 1 : 0))
wr_m1=$((wr_thr > 0 ? wr_thr - 1 : 0))

ar_row_m1=$(extract_row "AXI_READ_ARREADY_DELAY_confirm_d${ar_m1}")
ar_row_n=$(extract_row "AXI_READ_ARREADY_DELAY_confirm_d${ar_thr}")
ar_row_p1=$(extract_row "AXI_READ_ARREADY_DELAY_confirm_d$((ar_thr + 1))")

rv_row_m1=$(extract_row "AXI_READ_RVALID_DELAY_confirm_d${rv_m1}")
rv_row_n=$(extract_row "AXI_READ_RVALID_DELAY_confirm_d${rv_thr}")
rv_row_p1=$(extract_row "AXI_READ_RVALID_DELAY_confirm_d$((rv_thr + 1))")

wr_row_m1=$(extract_row "AXI_WRITE_WREADY_DELAY_confirm_d${wr_m1}")
wr_row_n=$(extract_row "AXI_WRITE_WREADY_DELAY_confirm_d${wr_thr}")
wr_row_p1=$(extract_row "AXI_WRITE_WREADY_DELAY_confirm_d$((wr_thr + 1))")

rec_base=$(extract_row "recovery_base")
rec_ar=$(extract_row "recovery_stress_ar${ar_thr}")
rec_rv=$(extract_row "recovery_stress_rv${rv_thr}")
rec_wr=$(extract_row "recovery_stress_wr${wr_thr}")

cat > "$REPORT" <<EOF
# SOLE Post-Sim AXI Delay Stress / Error-Code / Recovery Report (clock period = 1ns)

- Date: $(date +%Y-%m-%d)
- Flow: make post_sim (tcsh)
- Script: tb/SOLE_AXI_DELAY_test/run_post_sim_delay_scan.sh

## 1. Method

- Sweep each delay define independently with the other two held at 0.
- For each delay, detect the first value that triggers timeout error code:
  - AXI_READ_ARREADY_DELAY -> target error_code=0x2
  - AXI_READ_RVALID_DELAY -> target error_code=0x2
  - AXI_WRITE_WREADY_DELAY -> target error_code=0x5
- Boundary confirmation for N-1, N, N+1.
- Recovery checks (error_recovery_test=1):
  - base case (all delay=0)
  - stress at each measured threshold

## 2. Threshold Results (post_sim)

| Delay Define | First Trigger N | Target Error Code |
|---|---:|---:|
| AXI_READ_ARREADY_DELAY | $ar_thr | 0x2 |
| AXI_READ_RVALID_DELAY | $rv_thr | 0x2 |
| AXI_WRITE_WREADY_DELAY | $wr_thr | 0x5 |

## 3. Boundary Confirmation

| Define | N-1 (error_code/verdict) | N (error_code/verdict) | N+1 (error_code/verdict) |
|---|---|---|---|
| AXI_READ_ARREADY_DELAY | $(read_col "$ar_row_m1" 8)/$(read_col "$ar_row_m1" 7) | $(read_col "$ar_row_n" 8)/$(read_col "$ar_row_n" 7) | $(read_col "$ar_row_p1" 8)/$(read_col "$ar_row_p1" 7) |
| AXI_READ_RVALID_DELAY | $(read_col "$rv_row_m1" 8)/$(read_col "$rv_row_m1" 7) | $(read_col "$rv_row_n" 8)/$(read_col "$rv_row_n" 7) | $(read_col "$rv_row_p1" 8)/$(read_col "$rv_row_p1" 7) |
| AXI_WRITE_WREADY_DELAY | $(read_col "$wr_row_m1" 8)/$(read_col "$wr_row_m1" 7) | $(read_col "$wr_row_n" 8)/$(read_col "$wr_row_n" 7) | $(read_col "$wr_row_p1" 8)/$(read_col "$wr_row_p1" 7) |

## 4. Recovery Test Result (post_sim)

| Case | Delay Setting | verdict | error_code | done_seen |
|---|---|---|---|---:|
| recovery_base | ar=0 rv=0 wr=0 | $(read_col "$rec_base" 7) | $(read_col "$rec_base" 8) | $(read_col "$rec_base" 9) |
| recovery_stress_ar | ar=$ar_thr rv=0 wr=0 | $(read_col "$rec_ar" 7) | $(read_col "$rec_ar" 8) | $(read_col "$rec_ar" 9) |
| recovery_stress_rv | ar=0 rv=$rv_thr wr=0 | $(read_col "$rec_rv" 7) | $(read_col "$rec_rv" 8) | $(read_col "$rec_rv" 9) |
| recovery_stress_wr | ar=0 rv=0 wr=$wr_thr | $(read_col "$rec_wr" 7) | $(read_col "$rec_wr" 8) | $(read_col "$rec_wr" 9) |

## 5. Artifacts

- tb/SOLE_AXI_DELAY_test/post_sim_delay_scan_results.tsv
- tb/SOLE_AXI_DELAY_test/raw_logs/
- tb/SOLE_AXI_DELAY_test/SOLE_post_sim_AXI_DELAY_REPORT.md

EOF

echo "[DONE] TSV: $TSV"
echo "[DONE] Report: $REPORT"
