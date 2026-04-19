# tmux 精簡教學（為什麼要用 + 常用語法 + 操作範例）

## 1. 為什麼需要 tmux

tmux 的重點價值：
- SSH 斷線後，遠端任務仍持續執行。
- 你可以稍後重新接回同一個工作畫面。
- 適合長時間任務（例如合成、訓練、長測試）。

一句話：tmux 是在解決「連線中斷」問題，不是「主機關機/休眠」問題。

---

## 2. tmux 常用語法（每個指令都有範例）

### 2.1 建立與連線 session

指令：
```bash
tmux new -s <session_name>
```
範例：
```bash
tmux new -s synth_job
```

指令：
```bash
tmux ls
```
範例：
```bash
tmux ls
```

指令：
```bash
tmux attach -t <session_name>
```
範例：
```bash
tmux attach -t synth_job
```

指令：
```bash
tmux kill-session -t <session_name>
```
範例：
```bash
tmux kill-session -t synth_job
```

---

### 2.2 離開但不中止（detach）

操作：
- 先按 `Ctrl+b`
- 再按 `d`

效果：
- 離開 tmux 畫面，但程式繼續跑。

範例情境：
```bash
# 在 tmux 裡跑長任務
make synthesis |& tee -a log/synthesis_tmux.log
# 然後按 Ctrl+b, d 離開
```

---

### 2.3 背景直接啟動一個 session 並執行命令

指令：
```bash
tmux new -d -s <session_name> '<command>'
```
範例：
```bash
tmux new -d -s synth_job 'tcsh -c "cd /home/users/BillyYeh/Nonlinear_Project/RTL; make synthesis |& tee -a log/synthesis_tmux.log"'
```

---

## 3. 在 tmux 裡怎麼看「上下文」與捲動（你提到的重點）

你遇到「滑鼠滾輪出現其他東西，不是原本畫面」通常是因為還在 shell 正常模式，沒有進 tmux 的 copy mode。

### 3.1 正確看歷史輸出：進入 copy mode

操作：
- 先按 `Ctrl+b`
- 再按 `[`

進入後可用：
- `PageUp` / `PageDown`：整頁捲動
- `Up` / `Down`：逐行捲動
- `g`：跳到最上面
- `G`：跳到最下面
- `/關鍵字`：往下搜尋
- `?關鍵字`：往上搜尋
- `q`：離開 copy mode

範例：
1. 先在 pane 內有輸出。
2. 按 `Ctrl+b` 再按 `[`。
3. 按 `/ERROR` 找錯誤訊息。
4. 按 `n` 跳到下一個結果。
5. 按 `q` 回到即時畫面。

### 3.2 快速捲到上一頁/下一頁

操作：
- `Ctrl+b` 再按 `PageUp`：直接進入 copy mode 並往上看
- `PageDown`：往下

範例：
- 長 log 正在跑時，按 `Ctrl+b` + `PageUp` 查看前面 1~2 頁內容。

### 3.3 想用滑鼠滾輪看歷史（可選）

指令：
```bash
tmux set -g mouse on
```
範例：
```bash
tmux set -g mouse on
```

說明：
- 開啟後可用滑鼠切 pane、調整 pane，通常也可用滾輪捲動歷史。
- 若滾輪仍異常，請優先用 `Ctrl+b` + `[` 進 copy mode，最穩定。

永久生效（寫入設定）：
```bash
echo 'set -g mouse on' >> ~/.tmux.conf
tmux source-file ~/.tmux.conf
```

---

## 4. tmux 常用操作補充（附範例）

### 4.1 視窗（window）

操作：
- `Ctrl+b`，`c`：新增視窗
- `Ctrl+b`，`n`：下一個視窗
- `Ctrl+b`，`p`：上一個視窗
- `Ctrl+b`，`w`：列出視窗

範例做法：
- 視窗 1：跑 `make synthesis`
- 視窗 2：跑 `tail -f log/synthesis_tmux.log`
- 視窗 3：跑 `grep` 或其他檢查指令

### 4.2 面板（pane）

操作：
- `Ctrl+b`，`%`：左右分割
- `Ctrl+b`，`"`：上下分割
- `Ctrl+b`，方向鍵：切換 pane
- `Ctrl+b`，`x`：關閉目前 pane

範例：
- 左邊 pane 跑合成，右邊 pane 即時 `tail -f` 看 log。

### 4.3 重新命名 session

指令：
```bash
tmux rename-session -t <old_name> <new_name>
```
範例：
```bash
tmux rename-session -t synth_job dc_run_20260418
```

---

## 5. 一頁速查（只留最常用）

```bash
# 新建 session
tmux new -s synth_job

# 離開但不中止
# (在 tmux 內按 Ctrl+b, d)

# 列出 session
tmux ls

# 接回 session
tmux attach -t synth_job

# 背景啟動並跑命令
tmux new -d -s synth_job 'tcsh -c "cd /home/users/BillyYeh/Nonlinear_Project/RTL; make synthesis |& tee -a log/synthesis_tmux.log"'

# 刪除 session
tmux kill-session -t synth_job

# 進入 copy mode 看上下文
# (在 tmux 內按 Ctrl+b, [)

# 開啟滑鼠模式（可選）
tmux set -g mouse on
```
