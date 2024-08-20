#!/bin/bash

# for each given bit_sim.sh file, do the following:
#
# 1. disable simulation
# 2. disable the openfpga-pd-castor-rs submodule check
# 3. run './bit_sim.sh "" Multiple_Devices'
# 4. run repin
# 5. run ~/bin/rerunvpr.sh vgl

if [ "$(basename $PWD)" != Testcases ] ; then
	echo You must be in the Testcases directory
	exit 1
fi

if [ ! -h ../openfpga-pd-castor-rs ] ; then
	echo ../openfpga-pd-castor-rs must be a symlink
	exit 1
fi

flat=
if [[ "$1" == *[fF]* ]] ; then
	flat=f
fi

if [[ "$1" == *[eE]* ]] ; then
	routing=educational
	shift
elif [[ "$1" == *[cC]* ]] ; then
	routing=commercial
	shift
else
	echo First argument must be c/C or e/E
	exit 1
fi

for bs in $* ; do

	d=$(dirname $bs)
	f=$(basename $bs)

	if [ "$d" == "." ] || [ "$f" != bit_sim.sh ] || [ ! -e $bs ] ; then
		echo $bs must be "<design_dir>/bit_sim.sh"
		continue
	fi
	if [ ! -e $d/vars.sh ] ; then
		echo No $d/vars.sh found -- skipping $bs
		continue
	fi

	# 1. disable simulation
	sed -i -e '/^ *[ie][nx]ternal_bitstream_simulation=/s/=.*/=false/' $bs
	sed -i -e '/echo "simulate /D' $bs
	# this prevents netlists from being copied over (needed only when sim)
	mkdir -p SRC

	# 2. disable submodule check
	sed -i '/^if . -f .xml.root.openfpga.pd.castor.rs/,/^fi/s/^ *exit  *1/#/' $bs

	# x. ensure bitstream is in XML
	sed -i 's/echo "bitstream"/echo "bitstream write_xml"/g' $bs

	# x. pass through flat routing choice to Raptor
	sed -i '/flat_routing/D' $bs
	if [ "x$flat" == "xf" ] ; then
		sed -i '/channel_width/a echo "flat_routing  true">>raptor.tcl' $bs
	else
		sed -i '/channel_width/a echo "flat_routing false">>raptor.tcl' $bs
	fi

	# 3. run bit_sim
	echo "" ; echo "RUNNING bit_sim.sh" ; echo ""
	pushd $d
	./$f "" Multiple_Devices
	echo "" ; echo "sourcing vars.sh" ; echo ""
	source vars.sh
	popd

	# 4. repin when commercial
	if [ "$routing" == commercial ] ; then
		echo "" ; echo "Repinning $d" ; echo ""
		placement=$d/${d}_golden/$DESN/run_1/synth_1_1/impl_1_1_1/placement
		~/bin/repin.py						\
			$d/raptor.log					\
			$placement/fabric_${DESN}_post_synth.place	\
			$d/vars.sh					\
			$placement/${DESN}_pin_loc.place
	fi

	# 5. re-run VPR, bitgen, LVSBIT
	echo "" ; echo "VPRx4 --> BITGEN --> LVSBIT" ; echo ""
	pushd $d
	if [ "$routing" == commercial ] ; then
		# re-run VPRx4 with new graph, bitgen, lvsbit
		~/bin/rerunvpr.sh cvgl$flat
	else
		# only bitgen (in educational mode)
		~/bin/rerunvpr.sh e.g.$flat
	fi
	popd

done
