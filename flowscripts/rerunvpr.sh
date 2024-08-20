#!/bin/bash

# vpr steps will run if $1 contains v or V
#	k = pack, p = place, r = route, a = analyze
# bitgen will run if $1 contains g or G
# lvsbit will run if $1 contains l or L
#
# throughout, c/C = commercial routing (default) and e/E = educational routing
flags="$1"
if [[ "$flags" == *[cC]* ]] ; then
	echo Commercial mode specified
elif [[ "$flags" == *[eE]* ]] ; then
	echo Educational mode specified
else
	echo Defaulting to commercial mode
	flags="c$flags"
fi

# device
SIZE=10x8
PART=

# design
TOPD=$(basename $PWD)	# testcase dir, *_golden under it
TOPM=$TOPD		# set_top_module in raptor.tcl
DESN=$TOPD		# create_design in raptor.tcl
if [ -e vars.sh ] ; then
	echo Found vars.sh
	source ./vars.sh
fi
if [[ "$flags" == *[cC]* ]] ; then
	PNR=FPGA${SIZE}_rigel_compact_com_pnr
	DEV=rigel${SIZE}_heterogeneous
else
	PNR=FPGA${SIZE}_gemini_compact_latch_pnr
	DEV=castor${SIZE}_heterogeneous
fi
if [ "$PART" != "" ] ; then
	echo "Pack partitioner enabled"
	PART="--use_partitioning_in_pack on --number_of_molecules_in_partition $PART"
fi
if [ "$SEED" != "" ] ; then
	echo "Seed will be $SEED"
	SEED="--seed $SEED"
fi
if [ "$CONG" != "" ] ; then
	echo "Congestion mitigation enabled"
	CONG="--enable_cascade_placer on --place_algorithm congestion_aware"	# exits
	CONG="--enable_cascade_placer on"	# exits
	CONG="--place_algorithm congestion_aware"
fi
if [ "$UTIL" != "" ] ; then
	echo "CLB input pin utilization will be $UTIL"
	UTIL="--target_ext_pin_util clb:${UTIL},1"
fi

# root
ROOT=$(echo $PWD | sed -e 's#/ArchBench/.*#/ArchBench#')

# two local symlinks
/bin/rm -f CommonFiles
/usr/bin/ln -s $ROOT/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles
/bin/rm -f $PNR
/usr/bin/ln -s $ROOT/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/$PNR
LCMN=$ROOT/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles
LCMN=CommonFiles
LPNR=$ROOT/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/$PNR
LPNR=$PNR

# derived
LEAD=$ROOT/Testcases/$TOPD/
LEAD=
VPRBIN=/nfs_cadtools/softwares/raptor/instl_dir/04_09_2024_09_15_01/bin/vpr
VPRBIN=/nfs_cadtools/softwares/raptor/instl_dir/05_20_2024_09_15_01/bin/vpr
VPRBIN=/nfs_cadtools/softwares/raptor/instl_dir/06_09_2024_09_15_01/bin/vpr
VPRBIN=vpr
SCRIPTS=$LCMN/task/arch/rr_graph_builder
VPRXML=$LPNR/fabric_task/flow_inputs/k6n8_vpr_annotated.xml
OPENXML=$LPNR/fabric_task/flow_inputs/openfpga_arch_annotated.xml
if [[ "$flags" == *[cC]* ]] ; then
RGRAPH=$LPNR/fabric_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/rr_graph_out.stamped.xml
else
RGRAPH=$LPNR/fabric_task/run001/k6n8_vpr/top/MIN_ROUTE_CHAN_WIDTH/rr_graph_out.xml
fi
BASENETS=$LPNR/basenets.pin.gz
synth_1_1=${LEAD}${TOPD}_golden/$DESN/run_1/synth_1_1
INBLIF=$synth_1_1/synthesis/fabric_${DESN}_post_synth.eblif
TRBLIF=$synth_1_1/synthesis/transform_${DESN}_post_synth.eblif
VLG=$synth_1_1/synthesis/${DESN}_post_synth.v	# was using inferior fabric_ version
OBLIF=$synth_1_1/synthesis/${DESN}_post_synth.eblif
SDC=$synth_1_1/impl_1_1_1/packing/fabric_${DESN}_openfpga.sdc
NET=$synth_1_1/impl_1_1_1/packing/fabric_${DESN}_post_synth.net
PINLOC=$synth_1_1/impl_1_1_1/placement/${DESN}_pin_loc.place
PLACE=$synth_1_1/impl_1_1_1/placement/fabric_${DESN}_post_synth.place
REPACK=$synth_1_1/impl_1_1_1/placement/${DESN}_repack_constraints.xml
ROUTE=$synth_1_1/impl_1_1_1/routing/fabric_${DESN}_post_synth.route
OFBITS=$synth_1_1/impl_1_1_1/bitstream/fabric_bitstream.xml

FLAT=
if [[ "$flags" == *[fF]* ]] || [ "x$FLAT" != x ] ; then
	FLAT="--flat_routing on"
fi
VPRCMD="$VPRBIN $VPRXML $INBLIF"
VPRCMD+=" --sdc_file $SDC"
VPRCMD+=" --route_chan_width 160"
VPRCMD+=" --read_rr_graph $RGRAPH"
VPRCMD+=" --suppress_warnings check_rr_node_warnings.log,check_rr_node"
VPRCMD+=" --clock_modeling ideal"
VPRCMD+=" --absorb_buffer_luts off"
VPRCMD+=" --skip_sync_clustering_and_routing_results off"
VPRCMD+=" --constant_net_method route"
VPRCMD+=" --post_place_timing_report ${DESN}_post_place_timing.rpt"
VPRCMD+=" --device $DEV"
VPRCMD+=" --allow_unrelated_clustering on"
VPRCMD+=" --allow_dangling_combinational_nodes on"
VPRCMD+=" --place_delta_delay_matrix_calculation_method dijkstra"
VPRCMD+=" --post_synth_netlist_unconn_inputs gnd"
VPRCMD+=" --inner_loop_recompute_divider 1"
VPRCMD+=" --max_router_iterations 1500"
VPRCMD+=" --timing_report_detail detailed"
VPRCMD+=" --timing_report_npaths 100"
VPRCMD+=" --top $TOPM"
VPRCMD+=" --net_file $NET"
VPRCMD+=" --place_file $PLACE"
VPRCMD+=" --route_file $ROUTE"
VPRCMD+=" $PART $SEED $CONG $UTIL $FLAT "

if [ -e $PINLOC ] ; then
	echo Found $PINLOC
	fix_clusters="--fix_clusters $PINLOC"
fi

echo ""

# run the four steps of VPR (except I leave log files in the run directory)
gpsn="--gen_post_synthesis_netlist on"

if [[ "$flags" == *[vVkK]* ]] ; then
	echo PACK
	export -n PRINT_TRANSFORMED_EBLIF_FILE
	time $VPRCMD --pack  $gpsn 			> /dev/null
	rc=$?
	/usr/bin/mv         {,packing_}vpr_stdout.log
	if [ "$rc" = 0 ] ; then echo COMPLETED ; else echo FAILED ; exit 1 ; fi
	echo ""
fi

if [[ "$flags" == *[vVpP]* ]] ; then
	echo PLACE
	export -n PRINT_TRANSFORMED_EBLIF_FILE
	time $VPRCMD --place $gpsn $fix_clusters	> /dev/null
	rc=$?
	/usr/bin/mv           {,place_}vpr_stdout.log
	if [ "$rc" = 0 ] ; then echo COMPLETED ; else echo FAILED ; exit 1 ; fi
	echo ""
fi

if [[ "$flags" == *[vVrR]* ]] ; then
	echo ROUTE
	export -n PRINT_TRANSFORMED_EBLIF_FILE
	time $VPRCMD --route $gpsn 			> /dev/null
	rc=$?
	/usr/bin/mv         {,routing_}vpr_stdout.log
	if [ "$rc" = 0 ] ; then echo COMPLETED ; else echo FAILED ; exit 1 ; fi
	echo ""
fi

if [[ "$flags" == *[vVaA]* ]] ; then
	echo ANALYZE
	export    PRINT_TRANSFORMED_EBLIF_FILE=$TRBLIF
	time $VPRCMD --analysis				> /dev/null
	rc=$?
	/usr/bin/mv {,timing_analysis_}vpr_stdout.log
	if [ "$rc" = 0 ] ; then echo COMPLETED ; else echo FAILED ; exit 1 ; fi

	echo ""
	/usr/bin/grep succeeded {packing,place,routing,timing_analysis}_vpr_stdout.log
	echo ""
fi

DEBUG=
if [[ "$flags" == *[dD]* ]] ; then
	DEBUG="python3 -m pdb"
fi

if [[ "$flags" == *[gG]* ]] ; then

	echo ROUTE2REPACK
	script -e -f -q -c "$DEBUG $SCRIPTS/route2repack.py			\
		$ROUTE								\
		$REPACK" route2repack.script
	rc=$?
	sed -i 's/\r//g' route2repack.script
	if [ "$rc" = 0 ] ; then echo COMPLETED ; else echo FAILED ; exit 1; fi
	echo ""

fi

if [ -e $REPACK ] ; then
	echo Found $REPACK
	repack=$REPACK
	echo ""
else
	repack=/dev/null
fi

if [[ "$flags" == *[gG]* ]] ; then

	echo BITGEN
	if [[ "$flags" == *[cC]* ]] ; then
		commercial="--commercial"
		ofbits=""
		csv=""
	else
		commercial=""
		# bit comparison happens if NOT commercial, NOT flat, and NO LVS
		if [ -f $OFBITS ] && [[ "$flags" != *[cCfFlL]* ]] ; then
			ocsv=$(basename $OFBITS.csv)
			first="$(/bin/ls -t $OFBITS $ocsv 2>/dev/null | head -1)"
			if [ "$first" != "$ocsv" ] ; then
				echo Resorting $ocsv "<==" $OFBITS
				sed -n	-e 's/^.* value="\([01]\)" path="\([^"]*\)".*$/\1,\2/'	\
					-e '/^[01],/p' $OFBITS | LC_ALL=C sort -t, -k 2 -V > tmp.$ocsv
				mv tmp.$ocsv $ocsv
			fi
			ofbits="--read_fabric_bitstream $ocsv"
			csv="bitstream.csv"
		else
			ofbits=""
			csv=""
		fi
	fi
	script -e -f -q -c "$DEBUG $SCRIPTS/bitgen.py				\
		 $commercial							\
		--read_vpr_arch                 $VPRXML				\
		--read_openfpga_arch            $OPENXML			\
		--read_rr_graph			$RGRAPH				\
		--read_verilog                  $VLG				\
		--read_place_file               $PLACE				\
		--read_design_constraints	$repack				\
		--write_set_muxes		set_muxes.txt			\
		--read_pack_file                $NET				\
		--read_eblif                    $OBLIF				\
		--write_primary_mapping         PrimaryPinMapping.xml		\
		--write_pin_mapping             PinMapping.xml			\
		$ofbits								\
		--read_route_file               $ROUTE				\
		--write_fabric_bitstream        bitstream.xml $csv" bitgen.script
	rc=$?
	if [ "x$csv" != "x" ] && [ -f $csv ] ; then
		LC_ALL=C sort -t, -k 2 -V $csv > $csv.sort
		diff -qs $(basename $OFBITS.csv) $csv.sort
	fi
	sed -i 's/\r//g' bitgen.script
	if [ "$rc" = 0 ] ; then echo COMPLETED ; else echo FAILED ; exit 1 ; fi
	echo ""

fi

if [[ "$flags" == *[lL]* ]] ; then
	echo LVSBIT
	script -e -f -q -c "$DEBUG $SCRIPTS/lvsbit.py				\
		--read_place			$PLACE				\
		--read_pack			$NET				\
		--read_design_constraints	$repack				\
		--read_eblif			$TRBLIF				\
		--read_base_nets		$BASENETS			\
		--read_set_muxes		set_muxes.txt			\
		--read_bitstream		bitstream.xml" lvsbit.script
	rc=$?
	sed -i 's/\r//g' lvsbit.script
	if [ "$rc" != 0 ] ; then echo FAILED ; exit 1 ; fi
	echo COMPLETED

	#	--read_verilog			$VLG				\
	#	--read_map READ_MAP

	echo ""
fi
