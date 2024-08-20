#!/bin/bash

# stderr to file and screen
# stdout to file
exec 2> >(tee gb.err >&2)
exec  >       gb.out

# contents not design specific
vpr_arch=k6n8_vpr.xml
echo "VPRXML  =" $vpr_arch 1>&2
open_arch=openfpga_arch.xml
echo "OPENXML =" $open_arch 1>&2
rgraph=rr_graph_out.json.gz
echo "RGRAPH  =" $rgraph 1>&2
bitmap=fabric_bitstream.map.gz
echo "BITMAP  =" $bitmap 1>&2

muxdump=muxdump
echo "MUXDUMP =" $muxdump 1>&2

# design specific to read
design=${PWD##*/}
design=$(echo $design | sed 's/_golden$//')
verilog=`find . -name fabric_"$design"_post_synth.v -print`
echo "VERILOG =" $verilog 1>&2
pack=`find . -name fabric_"$design"_post_synth.net.post_routing -print`
echo "PACK    =" $pack 1>&2
place=`find . -name fabric_"$design"_post_synth.place -print`
echo "PLACE   =" $place 1>&2
route=`find . -name fabric_"$design"_post_synth.route -print`
echo "ROUTE   =" $route 1>&2
repack=fpga_repack_constraints.xml
echo "REPACK  =" $repack 1>&2
gold=golden.csv
echo "GOLD    =" $gold 1>&2

# design specific to write
eblif=`find . -name "$design"_post_synth.eblif -print`
echo "EBLIF   =" $eblif 1>&2
prymap=`find $design/run_1 -name PrimaryPinMapping.xml -print`
echo "PRYMAP  =" $prymap 1>&2
pinmap=`find $design/run_1 -name PinMapping.xml -print`
echo "PINMAP  =" $pinmap 1>&2
curr=top.csv
bits="top.bit top.xml.gz $curr"
bits="top.bit            $curr"
echo "BITS    =" $bits 1>&2

echo "" 1>&2
echo RUNNING 1>&2
echo "" 1>&2

# handle rrgraph start up
if [ "x$1" == xstart ] ; then
rrgraph="--read_rr_graph rr_graph_out.xml --write_rr_graph $rgraph"
else
rrgraph="--read_rr_graph $rgraph"
fi

opts="--default_clock 8"
opts=

     ./bs								\
	--read_vpr_arch			$vpr_arch			\
	--read_openfpga_arch		$open_arch			\
	$rrgraph							\
	--write_device_muxes		$muxdump			\
	--read_verilog			$verilog			\
	--read_place_file		$place				\
	--read_design_constraints	$repack				\
	--read_pack_file		$pack				\
	--read_eblif			$eblif				\
	--write_primary_mapping		$prymap				\
	--write_pin_mapping		$pinmap				\
	--read_fabric_bitstream		$gold				\
	--read_route_file		$route				\
	--read_map			$bitmap				\
	--write_fabric_bitstream	$bits				\
	$opts

if [ $? != 0 ] ; then
	exit $?
fi

# since we were previously writing these in the run directory
cp $prymap .
cp $pinmap .

echo "" 1>&2
echo COMPARING 1>&2
echo "" 1>&2

~/bin/cdiff $gold $curr | tee dq | ~/bin/cats | tee dq.cats 1>&2

exit $?
