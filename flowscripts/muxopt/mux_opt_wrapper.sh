export MUXES_VLOG=${MUXES_VLOG:-../../../../FPGA10x8_gemini_compact_latch_pnr/fabric_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/SRC/sub_module/muxes.v}
cp $MUXES_VLOG tmp_muxes.v
grep -v "default_nettype none" $MUXES_VLOG > tmp_muxes.v 
while read -r line; do
export MUX_TOP=$line
export TMP_MUX_OUT="${MUX_TOP}_tmp.v"
yosys -c mux_opt.tcl
done < <(grep -v // tmp_muxes.v | grep ^module | sed 's/^[^[:space:]]*[[:space:]]\{1,\}//' | sed 's/(.*//')

