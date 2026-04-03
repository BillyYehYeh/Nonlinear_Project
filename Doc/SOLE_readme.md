
<h1 style="color:#60a5fa">SOLE Technical Specification</h1>

<div style="text-align: right; color: #888; font-size: 0.95em; margin: 10px 0 20px 0;">
<strong>Date: &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;2026/04/04&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;</strong>
<strong>Author:&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Billy Yeh&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;
</div></strong> 



> 本文件內容以目前程式碼與測試結果為準。



<h2 id="toc" style="color:#86efac">目錄</h2>

- [1) SOLE 功能與架構特色](#section-1)
- [2) SOLE MMIO 對照表](#section-2)
- [3) State_t 與 ErrorCode_t 詳細表](#section-3)
- [4) SOLE 操作步驟](#section-4)
- [5) 目前已完成測試總覽](#section-5)

---

<h2 id="section-1" style="color:#86efac">1) SOLE 功能與架構特色</h2>

- 功能重點：Processor 用 MMIO 設定參數（來源/目的/長度/啟動），SOLE 內部 Softmax 引擎透過 AXI4-Lite 自主讀寫記憶體並輸出結果。
- 最大輸入：目前已驗證 **最多 4096 筆 FP16**（超過 4096 的測試會觸發 length error）。
- 架構特色：
  - 輕量 MMIO（4-signal）作為控制平面。  
  - 資料平面由 Softmax 引擎驅動 AXI4-Lite Master（自動搬資料）。  
  - `REG_STATUS`（狀態/錯誤/錯誤碼）可供輪詢或觸發 interrupt。
  - `CTRL_MODE_BIT` 預留模式切換（0=Softmax，1=Norm，後者未完整實作）。

---

<h2 id="section-2" style="color:#86efac">2) SOLE MMIO 對照表</h2>

> 依 `include/SOLE_MMIO.hpp` 與目前 `src/SOLE.cpp` 的行為整理。


<h3 style="color:#4fa076">2.1 MMIO Register 總表</h3>

| Offset | 位元寬度 | 名稱 | Processor 權限 | 功能說明 |
|---|---:|---|---|---|
| `0x00` | 32-bit | `REG_CONTROL` | Read/Write | 控制暫存器。含 START（bit0）與 MODE（bit31）。 |
| `0x04` | 32-bit | `REG_STATUS` | Read Only（Write Ignore） | 狀態暫存器。由運算引擎更新，Processor 用來輪詢 DONE/STATE/ERROR。 |
| `0x08` | 32-bit | `REG_SRC_ADDR_BASE_L` | Read/Write | 來源位址低 32-bit（byte address）。 |
| `0x0C` | 32-bit | `REG_SRC_ADDR_BASE_H` | Read/Write | 來源位址高 32-bit。 |
| `0x10` | 32-bit | `REG_DST_ADDR_BASE_L` | Read/Write | 目的位址低 32-bit（byte address）。 |
| `0x14` | 32-bit | `REG_DST_ADDR_BASE_H` | Read/Write | 目的位址高 32-bit。 |
| `0x18` | 32-bit | `REG_LENGTH_L` | Read/Write | 資料長度低 32-bit（單位：FP16 element 數）。 |
| `0x1C` | 32-bit | `REG_LENGTH_H` | Read/Write | 資料長度高 32-bit。 |
| `0x20` | 32-bit | `REG_RESERVED` | Read Only（固定 0） / Write Ignore | 保留欄位，供後續擴充。 |

補充：
- Processor 位址會經過 `ADDR_OFFSET_MASK = 0xFF`，僅使用低 8-bit 當 register offset。
- 測試中 AXI 資料寬度為 64-bit，因此地址通常以 8-byte 對齊。

<h3 style="color:#4fa076">2.2 REG_CONTROL bit 定義（0x00）</h3>

| Bit | 名稱 | 說明 |
|---|---|---|
| `[0]` | `CTRL_START_BIT` | 寫 1 啟動運算，啟動後再寫回 0。 |
| `[31]` | `CTRL_MODE_BIT` | 0: Softmax；1: Normalization（預留/待完整化）。 |
| 其他 | Reserved | 目前保留。 |

<h3 style="color:#4fa076">2.3 REG_STATUS bit 定義（0x04）</h3>

| Bit 範圍 | 名稱 | 說明 |
|---|---|---|
| `[0]` | DONE | 完成旗標。運算完成時會出現完成脈波/完成狀態。 |
| `[2:1]` | STATE | 狀態機編碼：0=IDLE、1=PROCESS1、2=PROCESS2、3=PROCESS3。 |
| `[3]` | ERROR | 錯誤旗標。發生錯誤時為 1。 |
| `[7:4]` | ERROR_CODE | 4-bit 錯誤代碼，細節見 ErrorCode_t 表格。 |
| `[31:8]` | Reserved | 目前保留。 |

---

<h2 id="section-3" style="color:#86efac">3) State_t 與 ErrorCode_t 詳細表</h2>

> 依 `include/Status.hpp` 定義整理。

<h3 style="color:#4fa076">3.1 State_t</h3>

| Enum | 值 | 狀態意義 | Softmax 運作 |
|---|---:|---|---|
| `STATE_IDLE` | 0 | 閒置 | 尚未啟動或已結束返回待命 |
| `STATE_PROCESS1` | 1 | 第 1 階段 | AXI Read / Log2Exp calculation |
| `STATE_PROCESS2` | 2 | 第 2 階段 | Approximate Divider PreCompute |
| `STATE_PROCESS3` | 3 | 第 3 階段 | Softmax final calculation / AXI Write back to memory |

<h3 style="color:#4fa076">3.2 ErrorCode_t</h3>

| Enum | Hex | 說明 | 可能原因（例） |
|---|---:|---|---|
| `ERR_NONE` | `0x0` | 無錯誤 | 正常執行 |
| `ERR_AXI_READ_ERROR` | `0x1` | AXI 讀回應錯誤 | `RRESP` 非 OKAY |
| `ERR_AXI_READ_TIMEOUT` | `0x2` | AXI 讀取逾時 | `RVALID` 未在預期時間出現 |
| `ERR_AXI_READ_DATA_MISSING` | `0x3` | AXI 讀資料遺失 | 讀取資料流中斷/不完整 |
| `ERR_AXI_WRITE_ERROR` | `0x4` | AXI 寫回應錯誤 | `BRESP` 非 OKAY |
| `ERR_AXI_WRITE_TIMEOUT` | `0x5` | AXI 寫入逾時 | `BVALID` 未在預期時間出現 |
| `ERR_AXI_WRITE_RESPONSE_MISMATCH` | `0x6` | AXI 寫回應不一致 | 寫入交易回應異常 |
| `ERR_MAX_FIFO_OVERFLOW` | `0x7` | Max FIFO overflow | 上游/下游節奏失衡 |
| `ERR_OUTPUT_FIFO_OVERFLOW` | `0x8` | Output FIFO overflow | 回寫端阻塞造成累積 |
| `ERR_DATA_LENGTH_INVALID` | `0x9` | 長度非法（如 0） | 長度未正確配置 |
| `ERR_INVALID_STATE` | `0xA` | 非法狀態轉移 | 狀態機轉移條件衝突 |

---

<h2 id="section-4" style="color:#86efac">4) SOLE 操作步驟</h2>

> 這裡採用 `test/SOLE_test.cpp` 的正常啟動路徑，不含除錯注入流程。

<h3 style="color:#4fa076">4.1 前置準備</h3>

1. 準備輸入資料（FP16），並放到來源記憶體區段。
2. 確認輸出記憶體區段可容納結果。
3. 完成 reset 流程（先 assert 再 deassert）。

<h3 style="color:#4fa076">4.2 設定 MMIO 參數</h3>

依序寫入：
1. `REG_SRC_ADDR_BASE_L`：來源基底位址（byte address）。
2. `REG_SRC_ADDR_BASE_H`：來源高位（無高位時可 0）。
3. `REG_DST_ADDR_BASE_L`：目的基底位址（byte address）。
4. `REG_DST_ADDR_BASE_H`：目的高位（無高位時可 0）。
5. `REG_LENGTH_L`：要處理的 FP16 element 數量。
6. `REG_LENGTH_H`：高位長度（通常 0）。

<h3 style="color:#4fa076">4.3 啟動運算</h3>

1. 寫 `REG_CONTROL = 0x00000001`（Softmax mode + START）。
2. 之後可將 START 清回 0。

<h3 style="color:#4fa076">4.4 監控執行狀態</h3>

1. 輪詢 `REG_STATUS`，觀察 `STATE` 進度：
   - PROCESS1 -> PROCESS2 -> PROCESS3
2. 監看 DONE / ERROR：
   - DONE=1 代表完成。
   - ERROR=1 則需解析 ERROR_CODE。
3. 若系統有接中斷，可利用 `interrupt`（DONE 或 ERROR 時為高）減少輪詢負擔。

<h3 style="color:#4fa076">4.5 讀回結果並驗證</h3>

1. 從目的位址讀回 FP16 結果。
2. 轉成 float 後與軟體 golden（如 C-model Softmax）比對。
3. 建議至少檢查：
   - cosine similarity
   - 每筆誤差與總和（softmax 輸出總和接近 1）

---

<h2 id="section-5" style="color:#86efac">5) 目前已完成測試總覽</h2>

<h3 style="color:#4fa076">5.1 單元/整合測試（CMake 測項）</h3>

目前專案內可見的主要測試目標包含：
- `MaxUnit_test`
- `Log2Exp_test`
- `Reduction_test`
- `Divider_PreCompute_test`
- `Divider_test`
- `Max_FIFO_test`
- `Output_FIFO_test`
- `PROCESS_2_test`
- `SOLE_test`

說明：
- 以上覆蓋 Softmax 管線關鍵子模組與 SOLE 整體流程。

<h3 style="color:#4fa076">5.2 Calculation 測試（多測資型態）</h3>

`test/SOLE_Calculation_TEST/SOLE_CALCULATION_TEST_REPORT.md` 顯示：
- 總案例：11
- 通過（cosine > 0.95）：11/11
- timeout：0

案例類型包含：
- 均勻分布
- 高對比輸入
- 極大值輸入
- 極接近值輸入
- 全零
- 正負交錯
- 單一 spike
- 單調遞增/遞減
- 雙峰分布
- 微小量級輸入

<h3 style="color:#4fa076">5.3 Execution Time vs Input Count</h3>

`test/SOLE_Execution_Time_TEST/SOFTMAX_EXECUTION_TIME_REPORT.md` 顯示：
- 測試點：1, 2, 4, ..., 4096（共 20 組）
- cosine > 0.95：20/20
- timeout（1..4096）：0
- 4096 執行時間：2077 ns（該報告環境）

趨勢重點：
- 輸入筆數增加時，執行時間近似線性增長。
- 在目前設定下，1..4096 皆可穩定完成。

<h3 style="color:#4fa076">5.4 Error 測試</h3>

目前可見的錯誤路徑驗證重點：
- `test/SOLE_test.cpp` 內建 error recovery 測試路徑（`error_recovery_test` 巨集）
  - 包含長度為 0 的錯誤注入（對應 `ERR_DATA_LENGTH_INVALID`）。
  - 驗證錯誤後 interrupt 行為。
  - 驗證清除 START 後錯誤可清除，再重新配置成功執行。
- Over-limit 測試
  - `test/SOLE_Execution_Time_TEST/softmax_overlimit_result.csv` 顯示 4097 觸發 timeout。

---

## 補充：整合使用時最重要的注意事項

<h3 style="color:#4fa076">A. 位址單位與對齊</h3>

- MMIO 設定的 src/dst base 是 **byte address**。
- AXI 資料通道是 64-bit，一個 word 可承載 4 個 FP16。
- 實作時請確保位址對齊與資料打包一致（避免讀寫錯位）。

<h3 style="color:#4fa076">B. 長度單位</h3>

- `REG_LENGTH_L/H` 代表的是 **FP16 element 數量**，不是 byte 數。

<h3 style="color:#4fa076">C. 狀態判斷建議</h3>

- 不要只看 DONE，請同時檢查 ERROR 與 ERROR_CODE。
- 量測時建議記錄狀態轉移（PROCESS1/2/3），可快速定位瓶頸階段。

<h3 style="color:#4fa076">D. 目前能力邊界（建議直接標在系統規格）</h3>

- 已驗證最大輸入長度：4096（現有測試條件下）。
- 若要支援更大長度，需同步檢查：
  - testbench / 系統記憶體配置
  - timeout 參數
  - FIFO 深度與 AXI backpressure 行為

---

## 快速操作範例（MMIO 寫入順序）

```c
// 1) 設來源位址
MMIO_WRITE(REG_SRC_ADDR_BASE_L, src_addr_low);
MMIO_WRITE(REG_SRC_ADDR_BASE_H, src_addr_high);

// 2) 設目的位址
MMIO_WRITE(REG_DST_ADDR_BASE_L, dst_addr_low);
MMIO_WRITE(REG_DST_ADDR_BASE_H, dst_addr_high);

// 3) 設長度（FP16 element count）
MMIO_WRITE(REG_LENGTH_L, len_low);
MMIO_WRITE(REG_LENGTH_H, len_high);

// 4) 啟動 Softmax
MMIO_WRITE(REG_CONTROL, 0x00000001);

// 5) 需要時可清 START
MMIO_WRITE(REG_CONTROL, 0x00000000);

// 6) 輪詢狀態
uint32_t st;
do {
    st = MMIO_READ(REG_STATUS);
} while (((st >> 0) & 0x1) == 0 && ((st >> 3) & 0x1) == 0);

// 7) 完成後讀回結果
```

---

## 參考檔案

- `include/SOLE_MMIO.hpp`
- `include/Status.hpp`
- `include/SOLE.h`
- `src/SOLE.cpp`
- `test/SOLE_test.cpp`
- `test/SOLE_Calculation_TEST/SOLE_CALCULATION_TEST_REPORT.md`
- `test/SOLE_Execution_Time_TEST/SOFTMAX_EXECUTION_TIME_REPORT.md`
- `test/SOLE_Execution_Time_TEST/softmax_overlimit_result.csv`
