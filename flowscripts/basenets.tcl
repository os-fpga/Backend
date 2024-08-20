#!/nfs_cadtools/softwares/starvision/starvision-7.2.1/bin/starsh

set szdb "cbmapping.zdb"
set db [zdb open $szdb]

set dsmod2n [dict create]
#	logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__frac_lut6				1	\
#	logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__ff_bypass_mode_default__ff_phy	1	\
#	logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__adder_carry				1	\
#	logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy									1	\
#	logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy									1	\
#	logical_tile_io_mode_physical__iopad_mode_default__pad										1	\
#	logical_tile_io_mode_physical__iopad_mode_default__ff										1	\


proc addport { smodule spbtype sport npins } {
	for { set i 0 } { $i < $npins } { incr i } {
		dict set ::dsmod2n "$smodule.${spbtype}_$sport\[$i\]" 1
	}
}

# always drop global_resetn, scan_en, scan_mode

# bram_phy, bram_phy always prepended, drop RAM_ID, PL_*, sc_in sc_out , (drop nonroutable in general)
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "WDATA_A1_i" 18 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "WDATA_A2_i" 18 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "RDATA_A1_o" 18 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "RDATA_A2_o" 18 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "ADDR_A1_i" 15 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "ADDR_A2_i" 14 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "CLK_A1_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "CLK_A2_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "REN_A1_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "REN_A2_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "WEN_A1_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "WEN_A2_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "BE_A1_i" 2 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "BE_A2_i" 2 	; # ok
##
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "WDATA_B1_i" 18 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "WDATA_B2_i" 18 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "RDATA_B1_o" 18 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "RDATA_B2_o" 18 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "ADDR_B1_i" 15 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "ADDR_B2_i" 14 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "CLK_B1_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "CLK_B2_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "REN_B1_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "REN_B2_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "WEN_B1_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "WEN_B2_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "BE_B1_i" 2 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "BE_B2_i" 2 	; # ok
## checked
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "FLUSH1_i" 1 	; # ok
addport "logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy" "bram_phy" "FLUSH2_i" 1 	; # ok

# see frac_lut6 ; adder_carry prepended to pin names, remove cin/cout (not routable)
addport "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__adder_carry" "adder_carry" "g" 1 
addport "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__adder_carry" "adder_carry" "p" 1 
addport "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__adder_carry" "adder_carry" "sumout" 1 

# from fle[physical].fabric.ff_bypass.ff_phy , ff_phy prepended to  pin names, drop SI SO
addport "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__ff_bypass_mode_default__ff_phy" "ff_phy" "C" 1
addport "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__ff_bypass_mode_default__ff_phy" "ff_phy" "D" 1
addport "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__ff_bypass_mode_default__ff_phy" "ff_phy" "E" 1
addport "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__ff_bypass_mode_default__ff_phy" "ff_phy" "Q" 1
addport "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__ff_bypass_mode_default__ff_phy" "ff_phy" "R" 1

# from fle[physical].fabric_mode.frac_lut6 , but frac_lut6_ prepended to pin names
addport "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__frac_lut6" "frac_lut6" "in" 6
addport "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__frac_lut6" "frac_lut6" "lut5_out" 2
addport "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__frac_lut6" "frac_lut6" "lut6_out" 1

# from [physical].dsp_phy, remove sc_in / sc_out
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "a_i" 20
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "acc_fir_i" 6
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "b_i" 18
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "load_acc" 1
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "lreset" 1
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "feedback" 3
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "unsigned_a" 1
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "unsigned_b" 1
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "saturate_enable" 1
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "shift_right" 6
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "round" 1
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "subtract" 1
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "clk" 1
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "z_o" 38
addport "logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy" "dsp_phy" "dly_b_o" 18

# from [physical].iopad.ff or .pad      drop SI SO
addport "logical_tile_io_mode_physical__iopad_mode_default__ff" "ff" "D" 1
addport "logical_tile_io_mode_physical__iopad_mode_default__ff" "ff" "Q" 1
addport "logical_tile_io_mode_physical__iopad_mode_default__ff" "ff" "clk" 1
addport "logical_tile_io_mode_physical__iopad_mode_default__pad" "pad" "clk" 1
addport "logical_tile_io_mode_physical__iopad_mode_default__pad" "pad" "outpad" 1
addport "logical_tile_io_mode_physical__iopad_mode_default__pad" "pad" "inpad" 1
# we add the weird prefix for the outward pins
addport "logical_tile_io_mode_physical__iopad_mode_default__pad" "gfpga_pad_QL_PREIO" "F2A_CLK" 1
addport "logical_tile_io_mode_physical__iopad_mode_default__pad" "gfpga_pad_QL_PREIO" "F2A" 1
addport "logical_tile_io_mode_physical__iopad_mode_default__pad" "gfpga_pad_QL_PREIO" "A2F" 1

set dsdir2schar [dict create	"input"     "i" "output"     "o" "inout"     "b"     "unknown" "u" \
				"input.neg" "i" "output.neg" "o" "inout.neg" "b" "unknown.neg" "u" ]

set dsval2b [dict create 0 0 1 1]

set otopmod [list "module" "fpga_top" "fpga_top"]
#set visits 0
$db flat foreach signal $otopmod osignal {

	set bnet 0
	# 1. we must have -addHier, otherwise little is enumerated.
	# 2. -stopHier prevents enumerating pins with nothing under them,
	# the reverse of what the docs say. For us, it could be
	# dangerous if standard cells are not defined.
	#db flat foreach pin -addHier -stopHier $osignal opin 
	$db flat foreach pin -addHier           $osignal opin {
		#incr visits
		set stype [$db oid type $opin]
		if { $stype != "port" } { continue }

		set smod [$db oid module $opin]
		set spin [$db oid oname  $opin]

		# routable pin on pb_type or routing mux?
		if {	[dict exists $dsmod2n "$smod.$spin"] ||			\
			[regexp {^(clock_)?mux.*_size\d+$} $smod dummy] &&	\
		       	[regexp {^(in|out)(\[[0-9][0-9]*\])?$} $spin dummy] } {

			if { $bnet == 0 } {
				set netval [$db value $osignal]
				if { [dict exists $dsval2b $netval] } {
					puts "NET $netval"
				} else {
					# this drop fpga_top
					set snet [join [lrange $osignal 2 end] "."]
					puts "NET $snet"
				}
				set bnet 1
			}

			set lspath [$db oid path $opin]
			set spath [join $lspath "."]
			set schar [dict get $dsdir2schar [$db directionOf $opin]]
			puts "PIN $spath $smod $spin $schar"
		}
	}
}

#puts "$visits visits"
