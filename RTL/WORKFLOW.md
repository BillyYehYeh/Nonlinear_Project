# SOLE RTL 综合和仿真工作流指南

## 项目结构配置

```
RTL/
├── Makefile              # 新建 - 支持三阶段工作流
├── src/                  # RTL 源文件 (15个 .sv 文件)
├── tb/                   # Testbench (SOLE_test.sv)
├── data/                 # 测试数据
├── build/                # 编译输出目录（自动创建）
├── syn/                  # 合成结果目录（自动创建）
└── script/
    ├── synthesis.tcl     # 更新 - SOLE specific
    ├── dc_sole.sdc       # 新建 - SOLE 约束文件
    ├── synopsys_dc.setup # 已有 - DC 环境配置
    └── dc.sdc            # 保留（备用）
```

## 工作流三阶段

### 1️⃣ **PRE-SIM** - RTL 行为仿真
验证 RTL 设计的功能正确性

```bash
cd RTL/
make pre_sim
```

**执行内容**：
- 使用 VCS 编译 `src/` 中的所有 RTL 文件
- 与 `tb/SOLE_test.sv` 仿真
- 在 `build/` 目录生成 `pre_sim_exec` 可执行文件
- 生成仿真日志：`SOLE_test_Result_rtl.log`, `SOLE_test_Monitor_rtl.log`

---

### 2️⃣ **SYNTHESIZE** - DC RTL 综合
转换 RTL → 门级网表（Gate-level Netlist）

```bash
cd RTL/
make synthesize
```

**执行内容**：
- DC 读入 `src/` 中的所有 RTL 源文件
- 应用 `script/dc_sole.sdc` 的时序约束（20ns 时钟周期）
- 生成门级网表：`syn/SOLE_syn.v` (Verilog)
- 生成 SDF 文件：`syn/SOLE_syn.sdf` （用于时延模拟）
- 生成报告：
  - `syn/timing.rpt` - 时序分析
  - `syn/area.rpt` - 面积报告
  - `syn/power.rpt` - 功耗估算

---

### 3️⃣ **POST-SIM** - 门级仿真
验证综合后的设计（含时延效应）

```bash
cd RTL/
make post_sim
```

**执行内容**：
- 使用 VCS 编译合成网表 `syn/SOLE_syn.v`
- 与相同的 `tb/SOLE_test.sv` 运行仿真
- 对比 pre_sim/post_sim 结果验证综合正确性
- 生成门级仿真日志

---

## 完整工作流示例

```bash
cd ~/Nonlinear_Project/RTL/

# 阶段 1: 验证 RTL 设计
make pre_sim

# 阶段 2: 进行 DC 综合
make synthesize

# 查看综合结果
make reports

# 阶段 3: 验证综合后的设计
make post_sim

# 清理编译文件
make clean          # 只清理 build/
make clean_all      # 清理 build/ 和 syn/
```

---

## Makefile 目标详解

| Target | 说明 | 输入 | 输出 |
|--------|------|------|------|
| `pre_sim` | RTL 行为仿真 | `src/*.sv` + `tb/SOLE_test.sv` | `build/pre_sim_exec`, 仿真日志 |
| `synthesize` | DC 综合 | `src/*.sv` + `script/dc_sole.sdc` | `syn/SOLE_syn.v`, 时序/面积/功耗报告 |
| `dv` | Design Vision GUI | 同 synthesize | DC 交互式 GUI 界面 |
| `post_sim` | 门级仿真 | `syn/SOLE_syn.v` + `tb/SOLE_test.sv` | `build/post_sim_exec`, 仿真日志 |
| `clean` | 清理编译 | - | 删除 `build/` |
| `clean_all` | 完全清理 | - | 删除 `build/` 和 `syn/` |
| `reports` | 显示报告 | - | 终端输出 timing/area 报告 |
| `help` | 显示帮助 | - | 终端输出命令说明 |

---

## 关键修改说明

### 📝 **Makefile 变更**
- ✅ 支持 VCS 仿真器（参考原 makefile 用法）
- ✅ 分离 RTL 和门级仿真
- ✅ 保留 DC GUI (`dv`) 和非交互综合 (`synthesize`) 两种模式
- ✅ 简化的目录结构（适应您的项目布局）

### 📝 **synthesis.tcl 更新**
- ✅ 改为读入 SOLE 相关的 15 个源文件
- ✅ 改为 `elaborate SOLE`（而非 `top`）
- ✅ 输出文件改为 `SOLE_syn.v` 等

### 📝 **新增 dc_sole.sdc**
- ✅ 为 SOLE 设计优化的约束文件
- ✅ 20ns 时钟周期（50MHz）
- ✅ 异步复位 (`rst`) false path
- ✅ AXI4-Lite 接口的输入/输出延延迟

---

## 常见问题

### Q: 如何修改时钟约束？
**A**: 编辑 `script/dc_sole.sdc`，修改第 7 行的 `set clk_period 20.0`

### Q: 综合失败，要如何排查？
**A**: 
1. 检查 DC 工作目录日志：`build/dc.log`
2. 查看约束文件是否正确加载
3. 验证 RTL 可综合性：确保没有不可综合的构造

### Q: post_sim 和 pre_sim 的差异在哪？
**A**:
- **pre_sim**: 使用 RTL，即时计算，快速仿真
- **post_sim**: 使用门级网表，加入实际门的延延迟，更接近实际硅行为

---

## VCS 仿真输出解析

仿真完成后，在 `RTL/build/` 中查看：

```
SOLE_test_Result_rtl.log    # 仿真详细日志
SOLE_test_Monitor_rtl.log   # 状态监控日志
```

---

## 接下来的步骤

1. **运行 pre_sim**：验证基本功能
   ```bash
   make pre_sim
   ```

2. **检查综合报告**（可选）
   ```bash
   make synthesize
   make reports
   ```

3. **进行 post_sim**：验证综合后行为
   ```bash
   make post_sim
   ```

4. **对比结果**：pre_sim vs post_sim 的仿真日志应该相同或非常接近

---

**版本**: 2026-04-05  
**适用项目**: Nonlinear_Project/RTL (SOLE 设计)
