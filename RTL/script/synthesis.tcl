# ========================================================================
# DC Synthesis Script for SOLE RTL Design
# ========================================================================

echo "========================================"
echo "SOLE RTL Synthesis with Synopsys DC"
echo "========================================"

# Read in RTL design files
echo "Reading RTL design files..."
analyze -format sverilog {
  ../src/timescale.sv
  ../src/sole_pkg.sv
  ../src/utils.sv
  ../src/SRAM_TS1N16.sv
  ../src/SRAM.sv
  ../src/Max_FIFO.sv
  ../src/Output_FIFO.sv
  ../src/MaxUnit.sv
  ../src/Reduction.sv
  ../src/Log2Exp.sv
  ../src/Divider_PreCompute.sv
  ../src/Divider.sv
  ../src/Softmax.sv
  ../src/PROCESS_1.sv
  ../src/PROCESS_2.sv
  ../src/PROCESS_3.sv
  ../src/SOLE.sv
}

# Elaborate top-level design
echo "Elaborating SOLE design..."
elaborate SOLE


# Set Design Environment
set_host_options -max_core 8
current_design SOLE
link


# Load constraints
echo "Loading design constraints..."
source ../script/dc.sdc

# Design setup
check_design
uniquify
set_fix_multiple_port_nets -all -buffer_constants [get_designs *]
set_max_area 0

# Synthesize circuit
echo "Running compilation..."
compile -map_effort high -area_effort high

# Create Reports
echo "Generating reports..."
report_timing -path full -delay max -nworst 1 -max_paths 1 -significant_digits 4 -sort_by group > ../syn/timing.rpt
report_timing -path full -delay min -nworst 1 -max_paths 1 -significant_digits 4 -sort_by group > ../syn/timing_hold.rpt
report_area -nosplit > ../syn/area.rpt
report_power -analysis_effort low > ../syn/power.rpt

# Save synthesized files
echo "Writing synthesis results..."
write -hierarchy -format verilog -output ../syn/SOLE_syn.v
write -hierarchy -format ddc -output ../syn/SOLE_syn.ddc
write_sdf -version 3.0 -context verilog ../syn/SOLE_syn.sdf

echo "========================================"
echo "Synthesis completed successfully!"
echo "Results saved to ../syn/"
echo "========================================"
quit