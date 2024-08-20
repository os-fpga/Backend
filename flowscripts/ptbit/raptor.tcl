# module load fpga_tools/raptor/latest
# raptor --batch --script build.tcl
create_design counter16
add_design_file -V_2001 rtl/counter16.v
set_top_module counter16

#target_device GEMINI_10x8
#target_device GEMINI_LEGACY
#set_device_size 12x12
target_device "GEMINI_COMPACT_10x8"

analyze
synth_options -no_dsp
synthesize delay
packing
place
route
sta
bitstream_config_files -key ../../../FPGA10x8_gemini_compact_pnr/fabric_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/output_fabric_key.xml
#bitstream_config_files -key ../../../FPGA10x8_dp_castor_pnr/FPGA10x8_dp_castor_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/output_fabric_key.xml
#bitstream_config_files -key ../../../FPGA82x68_final_castor_pnr/FPGA82x68_final_castor_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/output_fabric_key.xml
bitstream enable_simulation
