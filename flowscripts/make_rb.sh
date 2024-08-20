#!/bin/bash

VPRXML=/nfs_cadtools/softwares/raptor/instl_dir/03_27_2024_09_15_01/share/raptor/etc/devices/gemini_compact_10x8/gemini_vpr.xml
OPENXML=/nfs_cadtools/softwares/raptor/instl_dir/03_27_2024_09_15_01/share/raptor/etc/devices/gemini_compact_10x8/gemini_openfpga.xml
RRGRAPH=stamp.stamped.xml
VERDIR=SRC/n

/nfs_home/dhow/pof/MIN_ROUTE_CHAN_WIDTH/bs		\
	--read_vpr_arch			$VPRXML		\
	--read_openfpga_arch		$OPENXML	\
	--read_rr_graph			$RRGRAPH	\
	--write_routing_verilog_dir	$VERDIR		\
	--read_verilog			/dev/null	\
	--read_pack_file		/dev/null	\
	--read_place_file		/dev/null	\
	--read_route_file		/dev/null	\
	--write_fabric_bitstream	/dev/null	\
	--read_design_constraints	/dev/null	\
	--commercial
