source tech.tcl
set WORKLIB  "WORK"
exec mkdir -p ${WORKLIB}
define_design_lib WORK -path $WORKLIB

# -------------------
# READ FABRIC NETLIST
# -------------------
set LEAF ../../../CommonFiles/task/CustomModules
#set SRC ../../../FPGA10x8_gemini_compact_pnr/fabric_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/SRC
set SRC ../../../FPGA10x8_gemini_compact_TSMC_pnr/fabric_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/SRC

read_file [ glob $LEAF/*.v $SRC/lb/*.v $SRC/routing/*.v $SRC/sub_module/*.v $SRC/fpga_top.v ] -format verilog -define TSMC
current_design fpga_top
link
report_collection -type statistics [get_flat_pins]
set_disable_timing [get_flat_pins]
report_reference -nosplit -hierarchy

report_reference
group [get_cells *_2__2_*] -design_name grid_2__2_ -cell_name grid_2__2_
report_reference

current_design grid_2__2_
set_disable_timing [get_flat_pins]
report_port -nosplit
report_reference
