SOLE Calculation Test Package 使用說明
====================================

1) 執行全部 testcase（建議）
   ./run_calculation_cases.sh

2) 不重新編譯，直接跑測試
   BUILD_FIRST=0 ./run_calculation_cases.sh

3) 設定 cosine 門檻（預設 0.95）
   COSINE_THRESHOLD=0.95 ./run_calculation_cases.sh

輸出檔案：
- SOLE_CALCULATION_TEST_REPORT.md : 測試報告（含每個 case 的 cosine）
- cosine_results.csv              : 機器可讀結果
- log/*.log                       : 每次測試原始 log

testcases 規則：
- 每個 .txt 一行一個浮點數
- 少於 100 筆會自動略過

注意事項：
- 腳本執行後會自動還原 test/SOLE_test_Data.txt
- 需要存在可執行檔 build/SOLE_test
