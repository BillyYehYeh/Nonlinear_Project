# SOLE Calculation Multi-Case Validation Report

Generated at: 2026-03-28 21:17:14 CST

## Test Configuration
- Testbench: test/SOLE_test.cpp
- Testcase directory: test/SOLE_Calculation_TEST/testcases
- Cosine pass threshold: > 0.95
- Build before run: 0

## Summary
- Total testcases executed: 11
- Cosine pass cases: 11
- Timeout cases: 0

## Per-Case Cosine Results
Legend: 🔵 Case | 🟢 Pass | 🔴 Fail

```text
Tag  Case Name                                                      InputCount   ExecutionTime(ns)    CosineSimilarity       CosineCheck    Timeout   
---- -------------------------------------------------------------- ------------ -------------------- ---------------------- -------------- ----------
🔵    01_uniform_distribution                                        128          93                   🟢 1.000000000          🟢 PASS         🟢 NO      
🔵    02_high_contrast_1_2_10                                        128          93                   🟢 0.999999974          🟢 PASS         🟢 NO      
🔵    03_extreme_large_1000_1001_1002                                128          93                   🟢 1.000000000          🟢 PASS         🟢 NO      
🔵    04_close_values_1p001_1p002_1p003                              128          93                   🟢 1.000000000          🟢 PASS         🟢 NO      
🔵    05_all_zeros                                                   128          93                   🟢 1.000000000          🟢 PASS         🟢 NO      
🔵    06_alternating_sign_pm5                                        128          93                   🟢 0.999999619          🟢 PASS         🟢 NO      
🔵    07_single_spike                                                128          93                   🟢 0.999999882          🟢 PASS         🟢 NO      
🔵    08_increasing_ramp_minus4_to_4                                 128          93                   🟢 0.996702240          🟢 PASS         🟢 NO      
🔵    09_decreasing_ramp_4_to_minus4                                 128          93                   🟢 0.996704890          🟢 PASS         🟢 NO      
🔵    10_bimodal_clusters                                            128          93                   🟢 1.000000000          🟢 PASS         🟢 NO      
🔵    11_tiny_magnitudes                                             128          93                   🟢 1.000000000          🟢 PASS         🟢 NO      
```

## testcase
- 01_uniform_distribution
  - 目的: 驗證模型在完全平均輸入下是否穩定。
  - 範例: [1, 1, 1, 1, ..., 1]
- 02_high_contrast_1_2_10
  - 目的: 驗證高對比輸入時最大值主導情境。
  - 範例: [1, 2, 10, 1, 2, 10, ...]
- 03_extreme_large_1000_1001_1002
  - 目的: 驗證極端大值輸入時的數值表現。
  - 範例: [1000, 1001, 1002, ...]
- 04_close_values_1p001_1p002_1p003
  - 目的: 驗證非常接近輸入的分辨能力。
  - 範例: [1.001, 1.002, 1.003, ...]
- 05_all_zeros
  - 目的: 驗證全零輸入行為。
  - 範例: [0, 0, 0, 0, ...]
- 06_alternating_sign_pm5
  - 目的: 驗證正負交錯輸入。
  - 範例: [-5, 5, -5, 5, ...]
- 07_single_spike
  - 目的: 驗證單一尖峰值的主導效應。
  - 範例: [0, 0, ..., 20, ..., 0]
- 08_increasing_ramp_minus4_to_4
  - 目的: 驗證單調遞增輸入。
  - 範例: [-4.0, -3.9, ..., 4.0]
- 09_decreasing_ramp_4_to_minus4
  - 目的: 驗證單調遞減輸入。
  - 範例: [4.0, 3.9, ..., -4.0]
- 10_bimodal_clusters
  - 目的: 驗證雙峰分布輸入。
  - 範例: [-2, -2, ..., 2, 2, ...]
- 11_tiny_magnitudes
  - 目的: 驗證接近零的小幅度輸入。
  - 範例: [-0.001, 0.0, 0.001, 0.002, ...]

## Artifacts
- Testcases: test/SOLE_Calculation_TEST/testcases/*.txt
- Raw logs: test/SOLE_Calculation_TEST/log/*.log
- CSV results: test/SOLE_Calculation_TEST/cosine_results.csv
- Report: test/SOLE_Calculation_TEST/SOLE_CALCULATION_TEST_REPORT.md
