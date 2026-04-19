# RTL Makefile for SOLE Project
# Supports: pre_sim (RTL simulation), synthesize (DC), post_sim (gate-level simulation)

ROOT_DIR := $(PWD)
SRC_DIR := ./src
TB_DIR := ./tb
DATA_DIR := ./data
SCRIPT_DIR := ./script
BUILD_DIR := ./build
SYN_DIR := ./syn
REPORTS_DIR := ./syn_reports
LOG_DIR := ./log

# Tool paths
VCS ?= vcs
DC_SHELL ?= dc_shell
SNPSLMD_LICENSE_FILE ?= 26585@lstn
export SNPSLMD_LICENSE_FILE

# VCS variables
VCS_OPTS := -R -sverilog -debug_access+all -full64
VCS_INCDIR := +incdir+$(ROOT_DIR)/$(SRC_DIR)
VCS_DW_LIB := -y /usr/cad/synopsys/synthesis/2024.09-sp2/dw/sim_ver
VCS_POST_DEFINES := +define+POST_SIM
VCS_POST_RUN_OPTS := +notimingcheck +neg_tchk +sdfverbose +no_notifier
VCS_LIBEXT := +libext+.v+.sv
STDCELL_SIM_V := /cad/process/ADFP/Executable_Package/Collaterals/IP/stdcell/N16ADFP_StdCell/VERILOG/N16ADFP_StdCell.v
SRAM_SIM_V := /cad/process/ADFP/Executable_Package/Collaterals/IP/sram/N16ADFP_SRAM/VERILOG/N16ADFP_SRAM_100a.v
TB_DPI_SRCS := ../$(TB_DIR)/time_utils.c

# Design and testbench
DESIGN_TOP := SOLE
TB_TOP := SOLE_test
TB_FILE := $(TB_DIR)/SOLE_test.sv
DP_TB_TOP := Divider_PreCompute_test
DP_TB_FILE := $(TB_DIR)/Divider_PreCompute_test.sv
DIV_TB_TOP := Divider_test
DIV_TB_FILE := $(TB_DIR)/Divider_test.sv
LOG2EXP_TB_TOP := Log2Exp_test
LOG2EXP_TB_FILE := $(TB_DIR)/Log2Exp_test.sv
P2_TB_TOP := PROCESS_2_test
P2_TB_FILE := $(TB_DIR)/PROCESS_2_test.sv
RED_TB_TOP := Reduction_test
RED_TB_FILE := $(TB_DIR)/Reduction_test.sv
MAXUNIT_TB_TOP := MaxUnit_test
MAXUNIT_TB_FILE := $(TB_DIR)/MaxUnit_test.sv
MAXFIFO_TB_TOP := Max_FIFO_test
MAXFIFO_TB_FILE := $(TB_DIR)/Max_FIFO_test.sv
OUTFIFO_TB_TOP := Output_FIFO_test
OUTFIFO_TB_FILE := $(TB_DIR)/Output_FIFO_test.sv

# RTL source files
RTL_SRCS := \
    $(SRC_DIR)/timescale.sv \
    $(SRC_DIR)/sole_pkg.sv \
    $(SRC_DIR)/utils.sv \
	$(SRC_DIR)/SRAM_TS1N16.sv \
    $(SRC_DIR)/SRAM.sv \
    $(SRC_DIR)/Max_FIFO.sv \
    $(SRC_DIR)/Output_FIFO.sv \
    $(SRC_DIR)/MaxUnit.sv \
    $(SRC_DIR)/Reduction.sv \
    $(SRC_DIR)/Log2Exp.sv \
    $(SRC_DIR)/Divider_PreCompute.sv \
    $(SRC_DIR)/Divider.sv \
    $(SRC_DIR)/Softmax.sv \
    $(SRC_DIR)/PROCESS_1.sv \
    $(SRC_DIR)/PROCESS_2.sv \
    $(SRC_DIR)/PROCESS_3.sv \
    $(SRC_DIR)/SOLE.sv

# Use bash for pipefail support in tee pipelines
SHELL := /bin/bash

# Create directories
$(BUILD_DIR) $(SYN_DIR) $(REPORTS_DIR) $(LOG_DIR):
	mkdir -p $@

# =====================================================================
# PRE-SIM: RTL Simulation (behavioral, before synthesis)
# =====================================================================
pre_sim: | $(BUILD_DIR) $(LOG_DIR)
	@echo "=========================================="
	@echo "PRE-SIM: RTL Simulation (Behavioral)"
	@echo "=========================================="
	@set -o pipefail; \
	cd $(BUILD_DIR) && { \
	$(VCS) $(VCS_OPTS) $(VCS_INCDIR) \
		$(VCS_DW_LIB) \
		$(VCS_LIBEXT) \
		-top $(TB_TOP) \
		-o pre_sim_exec \
		$(addprefix ../,$(RTL_SRCS)) \
		../$(TB_FILE) \
		$(TB_DPI_SRCS) && \
	./pre_sim_exec; \
	} 2>&1 | tee ../$(LOG_DIR)/prs_sim.log
	@echo "PRE-SIM completed. Logs in $(BUILD_DIR)/"

# =====================================================================
# UNIT TEST: Divider_PreCompute RTL testbench
# =====================================================================
Divider_PreCompute_test: | $(BUILD_DIR)
	@echo "=========================================="
	@echo "UNIT TEST: Divider_PreCompute"
	@echo "=========================================="
	cd $(BUILD_DIR) && \
	$(VCS) $(VCS_OPTS) $(VCS_INCDIR) \
		$(VCS_DW_LIB) \
		$(VCS_LIBEXT) \
		-top $(DP_TB_TOP) \
		-o Divider_PreCompute_test_exec \
		../$(SRC_DIR)/timescale.sv \
		../$(SRC_DIR)/Divider_PreCompute.sv \
		../$(DP_TB_FILE) && \
	./Divider_PreCompute_test_exec
	@echo "Divider_PreCompute_test completed."

# =====================================================================
# UNIT TEST: Divider RTL testbench
# =====================================================================
Divider_test: | $(BUILD_DIR)
	@echo "=========================================="
	@echo "UNIT TEST: Divider"
	@echo "=========================================="
	cd $(BUILD_DIR) && \
	$(VCS) $(VCS_OPTS) $(VCS_INCDIR) \
		$(VCS_DW_LIB) \
		$(VCS_LIBEXT) \
		-top $(DIV_TB_TOP) \
		-o Divider_test_exec \
		../$(SRC_DIR)/timescale.sv \
		../$(SRC_DIR)/Divider_PreCompute.sv \
		../$(SRC_DIR)/Divider.sv \
		../$(DIV_TB_FILE) && \
	./Divider_test_exec
	@echo "Divider_test completed."

# =====================================================================
# UNIT TEST: Log2Exp RTL testbench
# =====================================================================
Log2Exp_test: | $(BUILD_DIR)
	@echo "=========================================="
	@echo "UNIT TEST: Log2Exp"
	@echo "=========================================="
	cd $(BUILD_DIR) && \
	$(VCS) $(VCS_OPTS) $(VCS_INCDIR) \
		$(VCS_DW_LIB) \
		$(VCS_LIBEXT) \
		-top $(LOG2EXP_TB_TOP) \
		-o Log2Exp_test_exec \
		../$(SRC_DIR)/timescale.sv \
		../$(SRC_DIR)/Log2Exp.sv \
		../$(LOG2EXP_TB_FILE) && \
	./Log2Exp_test_exec
	@echo "Log2Exp_test completed."

# =====================================================================
# UNIT TEST: PROCESS_2 RTL testbench
# =====================================================================
PROCESS_2_test: | $(BUILD_DIR)
	@echo "=========================================="
	@echo "UNIT TEST: PROCESS_2"
	@echo "=========================================="
	cd $(BUILD_DIR) && \
	$(VCS) $(VCS_OPTS) $(VCS_INCDIR) \
		$(VCS_DW_LIB) \
		$(VCS_LIBEXT) \
		-top $(P2_TB_TOP) \
		-o PROCESS_2_test_exec \
		../$(SRC_DIR)/timescale.sv \
		../$(SRC_DIR)/Divider_PreCompute.sv \
		../$(SRC_DIR)/PROCESS_2.sv \
		../$(P2_TB_FILE) && \
	./PROCESS_2_test_exec
	@echo "PROCESS_2_test completed."

# =====================================================================
# UNIT TEST: Reduction RTL testbench
# =====================================================================
Reduction_test: | $(BUILD_DIR)
	@echo "=========================================="
	@echo "UNIT TEST: Reduction"
	@echo "=========================================="
	cd $(BUILD_DIR) && \
	$(VCS) $(VCS_OPTS) $(VCS_INCDIR) \
		$(VCS_DW_LIB) \
		$(VCS_LIBEXT) \
		-top $(RED_TB_TOP) \
		-o Reduction_test_exec \
		../$(SRC_DIR)/timescale.sv \
		../$(SRC_DIR)/Reduction.sv \
		../$(RED_TB_FILE) && \
	./Reduction_test_exec
	@echo "Reduction_test completed."

# =====================================================================
# UNIT TEST: MaxUnit RTL testbench
# =====================================================================
MaxUnit_test: | $(BUILD_DIR)
	@echo "=========================================="
	@echo "UNIT TEST: MaxUnit"
	@echo "=========================================="
	cd $(BUILD_DIR) && \
	$(VCS) $(VCS_OPTS) $(VCS_INCDIR) \
		$(VCS_DW_LIB) \
		$(VCS_LIBEXT) \
		-top $(MAXUNIT_TB_TOP) \
		-o MaxUnit_test_exec \
		../$(SRC_DIR)/timescale.sv \
		../$(SRC_DIR)/MaxUnit.sv \
		../$(MAXUNIT_TB_FILE) && \
	./MaxUnit_test_exec
	@echo "MaxUnit_test completed."

# =====================================================================
# UNIT TEST: Max_FIFO RTL testbench
# =====================================================================
Max_FIFO_test: | $(BUILD_DIR)
	@echo "=========================================="
	@echo "UNIT TEST: Max_FIFO"
	@echo "=========================================="
	cd $(BUILD_DIR) && \
	$(VCS) $(VCS_OPTS) $(VCS_INCDIR) \
		$(VCS_DW_LIB) \
		$(VCS_LIBEXT) \
		-top $(MAXFIFO_TB_TOP) \
		-o Max_FIFO_test_exec \
		../$(SRC_DIR)/timescale.sv \
		../$(SRC_DIR)/SRAM_TS1N16.sv \
		../$(SRC_DIR)/SRAM.sv \
		../$(SRC_DIR)/Max_FIFO.sv \
		../$(MAXFIFO_TB_FILE) && \
	./Max_FIFO_test_exec
	@echo "Max_FIFO_test completed."

# =====================================================================
# UNIT TEST: Output_FIFO RTL testbench
# =====================================================================
Output_FIFO_test: | $(BUILD_DIR)
	@echo "=========================================="
	@echo "UNIT TEST: Output_FIFO"
	@echo "=========================================="
	cd $(BUILD_DIR) && \
	$(VCS) $(VCS_OPTS) $(VCS_INCDIR) \
		$(VCS_DW_LIB) \
		$(VCS_LIBEXT) \
		-top $(OUTFIFO_TB_TOP) \
		-o Output_FIFO_test_exec \
		../$(SRC_DIR)/timescale.sv \
		../$(SRC_DIR)/SRAM_TS1N16.sv \
		../$(SRC_DIR)/SRAM.sv \
		../$(SRC_DIR)/Output_FIFO.sv \
		../$(OUTFIFO_TB_FILE) && \
	./Output_FIFO_test_exec
	@echo "Output_FIFO_test completed."

# =====================================================================
# SYNTHESIZE: DC RTL to Gate-Level Synthesis
# =====================================================================
synthesize: | $(BUILD_DIR) $(SYN_DIR) $(REPORTS_DIR) $(LOG_DIR)
	@test -f $(SCRIPT_DIR)/synthesis.tcl || (echo "[ERROR] Missing $(SCRIPT_DIR)/synthesis.tcl" && exit 1)
	@echo "=========================================="
	@echo "DC Synthesis: RTL -> Gate-Level"
	@echo "=========================================="
	cp $(SCRIPT_DIR)/synopsys_dc.setup $(BUILD_DIR)/.synopsys_dc.setup
	@set -o pipefail; \
	cd $(BUILD_DIR) && \
	$(DC_SHELL) -no_home_init -f ../$(SCRIPT_DIR)/synthesis.tcl 2>&1 | tee ../$(LOG_DIR)/synthesize.log
	@echo "Synthesis completed. Results in $(SYN_DIR)/"

# =====================================================================
# DESIGN VISION: DC Interactive GUI (for manual optimization)
# =====================================================================
dv: | $(BUILD_DIR) $(SYN_DIR)
	@echo "=========================================="
	@echo "Launching Design Vision (DC GUI)"
	@echo "=========================================="
	cp $(SCRIPT_DIR)/synopsys_dc.setup $(BUILD_DIR)/.synopsys_dc.setup
	cd $(BUILD_DIR) && \
	$(DC_SHELL) -gui -no_home_init

# =====================================================================
# POST-SIM: Gate-Level Simulation (after synthesis)
# Gate-level netlist from $(SYN_DIR)/SOLE_syn.v
# =====================================================================
post_sim: $(SYN_DIR)/SOLE_syn.v $(SYN_DIR)/SOLE_syn.sdf | $(BUILD_DIR) $(LOG_DIR)
	@echo "=========================================="
	@echo "POST-SIM: Gate-Level Simulation"
	@echo "=========================================="
	@set -o pipefail; \
	cd $(BUILD_DIR) && { \
	$(VCS) $(VCS_OPTS) $(VCS_INCDIR) \
		$(VCS_DW_LIB) \
		$(VCS_LIBEXT) \
		$(VCS_POST_DEFINES) \
		-top $(TB_TOP) \
		-o post_sim_exec \
		../$(SRC_DIR)/timescale.sv \
		$(STDCELL_SIM_V) \
		$(SRAM_SIM_V) \
		../$(SYN_DIR)/SOLE_syn.v \
		../$(TB_FILE) \
		$(TB_DPI_SRCS) && \
	./post_sim_exec $(VCS_POST_RUN_OPTS); \
	} 2>&1 | tee ../$(LOG_DIR)/post_sim.log
	@echo "POST-SIM completed. Logs in $(BUILD_DIR)/"

# Check if synthesized netlist exists
$(SYN_DIR)/SOLE_syn.v:
	@echo "[ERROR] Synthesized netlist not found: $@"
	@echo "Please run 'make synthesize' first"
	@exit 1

$(SYN_DIR)/SOLE_syn.sdf:
	@echo "[ERROR] SDF file not found: $@"
	@echo "Please run 'make synthesize' first"
	@exit 1

# =====================================================================
# UTILITY TARGETS
# =====================================================================

# Clean build artifacts
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)
	@echo "Done."

# Clean all (including synthesis results)
clean_all: clean
	@echo "Cleaning synthesis directory..."
	rm -rf $(SYN_DIR)
	@echo "Done."

# View synthesis reports
reports:
	@echo "========== Timing Report =========="
	@cat $(SYN_DIR)/timing.rpt 2>/dev/null || echo "No timing report found"
	@echo ""
	@echo "========== Area Report =========="
	@cat $(SYN_DIR)/area.rpt 2>/dev/null || echo "No area report found"

# =====================================================================
# PHONY TARGETS
# =====================================================================
.PHONY: pre_sim Divider_PreCompute_test Divider_test Log2Exp_test PROCESS_2_test Reduction_test MaxUnit_test Max_FIFO_test Output_FIFO_test synthesize dv post_sim clean clean_all reports help

# =====================================================================
# Help target
# =====================================================================
help:
	@echo "Available targets:"
	@echo "  pre_sim       - Run RTL behavioral simulation"
	@echo "  Divider_PreCompute_test - Run Divider_PreCompute RTL unit test"
	@echo "  Divider_test  - Run Divider RTL unit test"
	@echo "  Log2Exp_test  - Run Log2Exp RTL unit test"
	@echo "  PROCESS_2_test - Run PROCESS_2 RTL unit test"
	@echo "  Reduction_test - Run Reduction RTL unit test"
	@echo "  MaxUnit_test  - Run MaxUnit RTL unit test"
	@echo "  Max_FIFO_test - Run Max_FIFO RTL unit test"
	@echo "  Output_FIFO_test - Run Output_FIFO RTL unit test"
	@echo "  synthesize    - Run DC synthesis (RTL to gate-level)"
	@echo "  dv            - Launch Design Vision GUI"
	@echo "  post_sim      - Run gate-level simulation"
	@echo "  clean         - Remove build artifacts"
	@echo "  clean_all     - Remove build and synthesis results"
	@echo "  reports       - View synthesis reports"
	@echo "  help          - Show this help message"
	@echo ""
	@echo "Workflow:"
	@echo "  1. make pre_sim       # Verify RTL behavior"
	@echo "  2. make synthesize    # Create gate-level netlist"
	@echo "  3. make post_sim      # Verify gate-level behavior"

.PHONY: help
