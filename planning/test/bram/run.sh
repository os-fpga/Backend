
# path settings
STARS_EXE_PATH=/home/tao/work/dev/checkin/Raptor/build/bin
STA_EXE_PATH=/home/tao/work/dev/checkin/Raptor/build/bin

# run stars to generate files for sta
# start stamp
echo "------------------"
echo "START DSP STA TEST"
echo "------------------"
cp bram.blif lut6_bram_function.blif
$STARS_EXE_PATH/stars k6n8_vpr.xml lut6_bram_function.blif  --gen_post_synthesis_netlist on --clock_modeling ideal --device castor10x8_heterogeneous --route_chan_width 192 --absorb_buffer_luts off --constant_net_method route --clustering_pin_feasibility_filter off --skip_sync_clustering_and_routing_results on --circuit_format eblif

# note: using existing sdc file
# Raptor/FOEDAG wrapper should prepare a sdc file when call "sta opensta"
cp bram.sdc lut6_bram_function_stars.sdc

# run sta to do timing report
$STA_EXE_PATH/sta bram.tcl

# finish stamp
echo "------------------"
echo "DONE BRAM STA TEST"
echo "------------------"
date

# exit
exit 0

