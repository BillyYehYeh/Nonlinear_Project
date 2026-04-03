# 🔵 Max_FIFO 與 Output_FIFO 整合設計報告

## 🟢 1. 文件目的
本報告統整目前專案中兩個 FIFO 模組設計：
- Max_FIFO
- Output_FIFO

並說明以下重點：
- 兩者共同架構與差異
- skid reg 的功能與特色
- FIFO 的運作模式與時序行為
- 實際測試流程與結果

---

## 🟢 2. 兩個 FIFO 的整體架構

### 🔵 2.1 共同設計原則
兩個 FIFO 已統一為同一種資料路徑與控制語意：
- 使用 SRAM 作為主儲存體
- 使用 read_ready / read_valid 介面
- 輸出路徑採用 SRAM 直通 + skid 緩衝
- 讀端採預取策略（prefetch）
- 當 handshake 發生時，下一個 clock 可切到下一筆資料

### 🔵 2.2 位址寬度設定
兩個 FIFO 的 SRAM 深度透過各自巨集定義：
- Max_FIFO 使用 MAX_FIFO_ADDR_BITS（預設 12）
- Output_FIFO 使用 OUTPUT_FIFO_ADDR_BITS（預設 12）

意義：
- Address bits = 12
- FIFO 深度 = 2^12 = 4096 entries
- Full 判斷採 ring buffer 規則（next write ptr == read ptr）

### 🔵 2.3 對外介面（已統一）
兩者皆採以下命名風格：
- 輸入：clk, rst, data_in, write_en, read_ready, clear
- 輸出：data_out, read_valid, full, empty, count

🔴 **重點：** Output_FIFO 原本為 read_data_valid，已改為 read_valid，與 Max_FIFO 一致。

---

## 🟢 3. skid reg 的特色與功能

### 🔵 3.1 為什麼需要 skid reg
skid reg 用來承接「SRAM 本拍返回資料，但下游本拍不接受」的情況。

若沒有 skid reg：
- 返回資料可能在 read_ready 拉低時被覆蓋或失去對齊
- read_valid 與 data_out 容易脫鉤

有 skid reg 後：
- 資料可安全暫存一筆
- 下游恢復 ready 後仍能正確送出

### 🔵 3.2 內部狀態訊號
關鍵訊號：
- skid_reg：暫存未被消費的返回資料
- skid_valid：skid_reg 是否有效
- sram_output_data_valid：SRAM 返回資料在本 cycle 是否有效

### 🔵 3.3 行為特色
- 若 skid_valid=1，data_out 優先輸出 skid_reg
- 若 skid_valid=0，data_out 直接輸出 sram_rdata
- read_valid = sram_output_data_valid OR skid_valid

🔴 **重點：** 這個結構可同時保留低延遲（直通）與抗背壓能力（skid）。

---

## 🟢 4. FIFO 運作模式

### 🔵 4.1 寫入模式
- write_en=1 且 full=0 時，資料寫入 SRAM，write pointer 前進
- full=1 時阻擋寫入，避免覆寫未讀資料

### 🔵 4.2 讀取模式（含預取）
讀取 issue 條件採分流設計：
- 當 read_ready=1：
  - 允許維持較積極的 prefetch（但受輸出緩衝容量限制）
- 當 read_ready=0：
  - 僅允許保留一筆 head 在輸出路徑，避免覆寫 skid 資料

這樣可達成：
1. 輸出端盡量隨時持有下一筆可用資料
2. 不會在持續 stall 時把資料覆蓋

### 🔵 4.3 handshake 與下一拍資料更新
handshake 條件：
- read_ready=1 且 read_valid=1

設計目標：
- 當 handshake 發生後，下一個 clock 應看到下一筆待消費資料出現在 data_out

測試已加入專門檢查（HS->NXT）驗證此條件。

---

## 🟢 5. 運行過程觀察重點

在 testbench 中每 cycle 可觀察：
- in / we / out / rr / rv
- skid / skid_v / sram_out_v
- cnt / full / empty
- pending（SRAM queue + output path 中尚未消費資料總量）
- head_src（none / prefetch / skid）
- SRAM(push) 內容

### 🔵 5.1 常見疑問：為何 SRAM 第 0 筆先消失
若 read_ready 尚未 handshake，但看到 SRAM queue 的第一筆不在列表中，通常是因為：
- 該筆已被 prefetch 移到輸出路徑（sram_output_data_valid 或 skid）

這不是資料遺失。
可用 pending 與 head_src 欄位判斷資料是否仍在系統內。

---

## 🟢 6. 測試策略與結果

### 🔵 6.1 測試策略
兩個 FIFO 皆使用一致流程：
1. Reset
2. 連續寫入 10 筆資料
3. 以不規律 read_ready pattern 讀取
   - 包含連續 high
   - 連續 low
   - high/low 交錯
4. 檢查項目：
   - next-cycle data check
   - handshake 後下一拍（HS->NXT）
   - write/read 順序一致性
   - 是否缺資料或多資料

### 🔵 6.2 測試結果摘要
Max_FIFO：
- Data check errors: 0
- HS->next errors: 0
- Order match: YES
- Missing data count: 0
- Unexpected read data count: 0
- RESULT: PASS

Output_FIFO：
- Data check errors: 0
- HS->next errors: 0
- Order match: YES
- Missing data count: 0
- Unexpected read data count: 0
- RESULT: PASS

🔴 **結論：** 兩個 FIFO 在現行設定下行為一致且功能正確。

---

## 🟢 7. 結論

目前 Max_FIFO 與 Output_FIFO 已完成以下整合：
- 一致的介面命名（read_ready/read_valid）
- 一致的預取 + skid 緩衝架構
- 分別以 MAX_FIFO_ADDR_BITS / OUTPUT_FIFO_ADDR_BITS 控制深度
- 一致且可重現的測試驗證框架

此設計兼顧：
- 低延遲輸出
- 背壓安全性
- 時序可預測性
- 功能可驗證性

可作為後續 Softmax 資料路徑 FIFO 的統一基準實作。