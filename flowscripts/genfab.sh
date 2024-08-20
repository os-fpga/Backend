#!/bin/bash

PNRABS=$PWD

# 10x8 rigel compact com pnr
SIZE=$(basename $PNRABS | sed -e 's/FPGA//' -e 's/_/ /g' | awk '{print $1}')
FAMILY=$(basename $PNRABS | sed -e 's/FPGA//' -e 's/_/ /g' | awk '{print $2}')
if [ "$FAMILY" == "gemini" ] ; then
	# inconsistent naming
	FAMILY=castor
fi
DEVICE=${FAMILY}${SIZE}_heterogeneous
CHAN_WIDTH=160

RUNREL=fabric_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH
FLOWABS=/nfs_cadtools/softwares/opensource/sw_openfpga/orgnl/Backend/OpenFPGA/openfpga_flow
VPRXML=$PNRABS/$RUNREL/arch/k6n8_vpr.xml
OPENXML=$PNRABS/$RUNREL/arch/openfpga_arch.xml
VPRXML=$PNRABS/fabric_task/flow_inputs/k6n8_vpr_annotated.xml
OPENXML=$PNRABS/fabric_task/flow_inputs/openfpga_arch_annotated.xml
OPENSIM=$FLOWABS/openfpga_simulation_settings/auto_sim_openfpga.xml
OPENBIT=$PNRABS/fabric_task/arch/bitstream_setting.xml

OPENSCRIPT=$PNRABS/$RUNREL/top_run.openfpga

VPRBLIF=$PNRABS/$RUNREL/top.blif
VPRACT=$PNRABS/$RUNREL/top_ace_out.act
RRGRAPH=rr_graph_out.xml
VPROPTS="--clock_modeling ideal --device $DEVICE --route_chan_width $CHAN_WIDTH  --absorb_buffer_luts off --place_delta_delay_matrix_calculation_method dijkstra   --write_rr_graph $RRGRAPH --skip_sync_clustering_and_routing_results on"

# set up directories
mkdir -p $RUNREL
/bin/rm -f _run_dir
ln -s $RUNREL _run_dir

# set up .blif file
/bin/rm -f $VPRBLIF
cat << EOF > $VPRBLIF
.model and2
.inputs a b
.outputs c

.names a b c
11 1

.end
EOF

# set up .act file
cat << EOF > $VPRACT
a 0.5 0.5
b 0.5 0.5
c 0.25 0.25
EOF

# set up .openfpga file
/bin/rm -f $OPENSCRIPT
cat << EOF > $OPENSCRIPT
vpr $VPRXML $VPRBLIF $VPROPTS
read_openfpga_arch -f $OPENXML
read_openfpga_simulation_setting -f $OPENSIM
read_openfpga_bitstream_setting -f $OPENBIT
link_openfpga_arch --activity_file top_ace_out.act --sort_gsb_chan_node_in_edges
pb_pin_fixup
build_fabric --compress_routing --write_fabric_key output_fabric_key.xml
write_fabric_verilog --file ./SRC --explicit_port_mapping --default_net_type wire --no_time_stamp --use_relative_path --verbose
write_gsb_to_xml --verbose --file gsb
repack
build_architecture_bitstream
build_fabric_bitstream
write_fabric_hierarchy --depth 1 --file fabric_hierarchy.txt
write_fabric_bitstream --format xml --file and_openfpga_sample_bitstream.xml --no_time_stamp
report_bitstream_distribution   --file and_bitstream_distribution.xml --no_time_stamp
exit
EOF

pushd $RUNREL
openfpga --batch_execution --file $OPENSCRIPT
popd

