# GENERATED BY PTBIT# USAGE:# module load synopsys/1.0# pt_shell -file ptbit.tcl# ------------
# LEAF CELLS
# ------------
# set search_path []
# set search_path [concat $search_path /cadlib/gemini/TSMC16NMFFC/library/std_cells/tsmc/7.5t/tcbn16ffcllbwp7d5t16p96cpd_100i/tcbn16ffcllbwp7d5t16p96cpd_100d_ccs/TSMCHOME/digital/Front_End/timing_power_noise/CCS/tcbn16ffcllbwp7d5t16p96cpd_100d ]
# set search_path [concat $search_path /cadlib/gemini/TSMC16NMFFC/library/std_cells/tsmc/7.5t/tcbn16ffcllbwp7d5t16p96cpdlvt_100i/tcbn16ffcllbwp7d5t16p96cpdlvt_100d_ccs/TSMCHOME/digital/Front_End/timing_power_noise/CCS/tcbn16ffcllbwp7d5t16p96cpdlvt_100d ]
# set search_path [concat $search_path /cadlib/gemini/TSMC16NMFFC/library/memory/dti/memories/rev_111022/101122_tsmc16ffc_DP_RAPIDSILICON_GEMINI_rev1p0p3_BE/dti_dp_tm16ffcll_1024x18_t8bw2x_m_hc ]
# # slow/slow, -40C (temperature inversion WC)
# set link_library   [concat * \
# tcbn16ffcllbwp7d5t16p96cpdssgnp0p72vm40c_ccs.db \
# tcbn16ffcllbwp7d5t16p96cpdlvtssgnp0p72vm40c_ccs.db \
# dti_dp_tm16ffcll_1024x18_t8bw2x_m_hc_ssgn40c.db \
# ]

# ------------
# READ NETLIST
# ------------
#set LEAF ../../../CommonFiles/task/CustomModules
#set SRC ../../../FPGA12x12_gemini_legacy_pnr/fabric_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/SRC
#set SRC ../../../FPGA10x8_dp_castor_pnr/FPGA10x8_dp_castor_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/SRC
#set SRC /home/rliston/openfpga-pd-castor-rs.liwu/k6n8_TSMC16nm_7.5T/FPGA10x8_gemini_compact_pnr/fabric_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/SRC

#set SRC ../../../FPGA10x8_gemini_compact_pnr/fabric_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/SRC
set SRC ../../../FPGA82x68_gemini_compact_pnr/fabric_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/SRC
read_file $SRC/fpga_top.v -format verilog

# read_file [ glob $LEAF/*.v $SRC/lb/*.v $SRC/routing/*.v $SRC/sub_module/*.v $SRC/fpga_top.v ] -format verilog
# current_design grid_clb
# link
# report_reference -nosplit -hierarchy
#

#exit

current_design fpga_top
link

#set_case_analysis 0 config_enable
# suppress warnings for zero wlm and timing loops
#suppress_message OPT-314
#suppress_message OPT-170
report_reference -hierarchy
