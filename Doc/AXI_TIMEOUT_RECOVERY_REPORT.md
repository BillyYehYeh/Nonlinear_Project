<h1 style="color:#60a5fa">SOLE Stress, Error-Code, and Recovery Verification Report</h1>

<div style="text-align: right; color: #888; font-size: 0.95em; margin: 10px 0 20px 0;">
<strong>Date: 2026/04/22</strong>
</div>

> This document tracks AXI stress thresholds, timeout error-code behavior, and error recovery behavior in the current RTL testbench version.

<h2 style="color:#86efac">Scope</h2>

- Verify timeout-trigger thresholds for:
  - AXI_READ_ARREADY_DELAY
  - AXI_READ_RVALID_DELAY
  - AXI_WRITE_WREADY_DELAY
- Verify error_recovery_test behavior in the current implementation

<h2 style="color:#86efac">1) Test Method</h2>

All cases were executed through Makefile pre-sim using tcsh.
Current flow passes testbench defines directly as Make variables:

- tcsh -c "make pre_sim AXI_READ_ARREADY_DELAY=<N> AXI_READ_RVALID_DELAY=<N> AXI_WRITE_WREADY_DELAY=<N> [error_recovery_test=1]"

DUT status extraction source:

- [STATUS_HW] 0x........ | ... | error_code=0x.

Threshold definition used in this report:

- Minimal delay N such that DUT error bit/error_code indicates timeout path for that AXI channel.

<h2 style="color:#86efac">2) AXI Stress Threshold Results</h2>

<h3 style="color:#4fa076">2.1 Final Thresholds</h3>

| Delay Define | First Trigger Delay N | Timeout Error Code | Final Status Example |
|---|---:|---:|---|
| AXI_READ_ARREADY_DELAY | 98 | 0x2 | 0x00000028 |
| AXI_READ_RVALID_DELAY | 99 | 0x2 | 0x00000028 |
| AXI_WRITE_WREADY_DELAY | 100 | 0x5 | 0x00000058 |

<span style="color:#22c55e"><strong>Key Point:</strong></span>
- WREADY delay at 99 does not trigger write-timeout.
- WREADY delay first trigger is 100 (error_code=0x5).

Source:

- log/axi_timeout_scan/thresholds.env
- log/axi_timeout_scan/scan_results.tsv

<h3 style="color:#4fa076">2.2 Boundary Confirmation (N-1, N, N+1)</h3>

| Define | N-1 | N | N+1 |
|---|---|---|---|
| AXI_READ_ARREADY_DELAY (N=98) | d97 => error_code=0x0 | d98 => error_code=0x2 | d99 => error_code=0x2 |
| AXI_READ_RVALID_DELAY (N=99) | d98 => error_code=0x0 | d99 => error_code=0x2 | d100 => error_code=0x2 |
| AXI_WRITE_WREADY_DELAY (N=100) | d99 => error_code=0x0 | d100 => error_code=0x5 | d101 => error_code=0x5 |

Evidence rows:

- log/axi_timeout_scan/scan_results.tsv
  - AXI_READ_ARREADY_DELAY_confirm_d97 / d98 / d99
  - AXI_READ_RVALID_DELAY_confirm_d98 / d99 / d100
  - AXI_WRITE_WREADY_DELAY_confirm_d99 / d100 / d101

<h2 style="color:#86efac">3) Error-Code and Recovery Behavior</h2>

<h3 style="color:#4fa076">3.1 Current Recovery Flow (As Implemented)</h3>

Case:

- error_recovery_test=1
- Example stress run:
  - AXI_READ_ARREADY_DELAY=95
  - AXI_READ_RVALID_DELAY=0
  - AXI_WRITE_WREADY_DELAY=99

Observed sequence:

- [2] Error Injection + Recovery Start Test
- REG_CONTROL,0x1,inject error with length=0
- [STATUS_HW] 0x00000098 ... error=1 | error_code=0x9 @22 ns
- [RECOVERY] injected_error_detected status=0x00000098 @24 ns
- [RECOVERY] force_zero_delay=1 @24 ns
- [2-RECOVERY] Reconfigure valid MMIO and restart
- [RECOVERY] restart_start_written @38 ns
- [STATUS_HW] ... PROCESS1 @40 ns
- [STATUS_HW] ... PROCESS2 @97 ns
- [STATUS_HW] ... PROCESS3 @108 ns
- [STATUS_HW] ... done=1 @189 ns

<span style="color:#ef4444"><strong>Important Clarification:</strong></span>
- The early error at ~22 ns is injected length error (error_code=0x9).
- It is not AXI write-timeout and not caused by AXI_WRITE_WREADY_DELAY=99.

<span style="color:#f59e0b"><strong>Recovery Semantics in Current Code:</strong></span>
- Restart run sets force_zero_delay=1.
- ARREADY/RVALID/WREADY delays are forced to 0 during restart run.
- Restart run is intentionally protected from timeout stress.

Result:

- Recovery path returns to normal processing.
- Final restarted run reaches done with error_code=0x0.

Evidence:

- log/axi_timeout_scan/recovery_base_result.log
- log/axi_timeout_scan/recovery_base_console.log
- log/SOLE_test_Monitor_pre_sim.log
- log/SOLE_test_Result_pre_sim.log

<h3 style="color:#4fa076">3.2 Recovery vs Timeout Stress (Current Version)</h3>

Behavior in current version:

- Under error_recovery_test=1, first error is injected length error (0x9).
- Restart run uses force_zero_delay=1 and does not preserve configured AXI delays.
- Timeout-level AXI delay settings do not trigger timeout error codes in restart run.

If timeout characterization is the target:

- Run with error_recovery_test=0.
- Sweep each AXI delay define using section 2 as baseline.

<h2 style="color:#86efac">4) Conclusions</h2>

1. AXI timeout detection is functioning for all three channels.
2. Measured timeout trigger thresholds in current pre-sim environment:
   - AXI_READ_ARREADY_DELAY >= 98 -> error_code 0x2
   - AXI_READ_RVALID_DELAY >= 99 -> error_code 0x2
   - AXI_WRITE_WREADY_DELAY >= 100 -> error_code 0x5
3. Recovery flow is functioning in current testbench semantics:
   - Injection error (0x9) is detected first
   - Restart executes with force_zero_delay=1
   - Restart run completes with done pulse

<h2 style="color:#86efac">5) Artifacts</h2>

- log/axi_timeout_scan/scan_results.tsv
- log/axi_timeout_scan/thresholds.env
- log/axi_timeout_scan/recovery_base_result.log
- log/axi_timeout_scan/recovery_base_console.log
- log/axi_timeout_scan/recovery_ar98_result.log (historical, before current force_zero_delay recovery behavior)
- log/axi_timeout_scan/recovery_ar98_console.log (historical, before current force_zero_delay recovery behavior)
- log/SOLE_test_Monitor_pre_sim.log
- log/SOLE_test_Result_pre_sim.log
