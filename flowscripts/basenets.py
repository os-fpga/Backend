#!/nfs_home/dhow/bin/python3
#!/usr/bin/env python3
"""enumerate all base die flat nets touching muxes or pb_types"""

import re
#from collections import defaultdict

## pylint: disable-next=import-error,unused-import
from traceback_with_variables import activate_by_import, default_format

import vwalk

## part of traceback_with_variables
default_format.max_value_str_len = 10000

g_dsmod2n = {}

def addport(smodule, spbtype, sport, npins):
    """record mux/pbtype ports"""
    for i in range(int(npins)):
        #_dsmod2n[f"{smodule}.{spbtype}_{sport}[{i}]"] = 1
        g_dsmod2n[(smodule, f"{spbtype}_{sport}", i)] = 1
    # for
# def addport

def basenets():
    """generate all basenets between muxes"""
    # always drop global_resetn, scan_en, scan_mode
    g_ports = '''
# bram_phy, bram_phy always prepended
# drop RAM_ID, PL_*, sc_in sc_out , (drop nonroutable in general)
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy WDATA_A1_i 18 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy WDATA_A2_i 18 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy RDATA_A1_o 18 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy RDATA_A2_o 18 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy ADDR_A1_i 15 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy ADDR_A2_i 14 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy CLK_A1_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy CLK_A2_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy REN_A1_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy REN_A2_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy WEN_A1_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy WEN_A2_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy BE_A1_i 2 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy BE_A2_i 2 	
##
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy WDATA_B1_i 18 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy WDATA_B2_i 18 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy RDATA_B1_o 18 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy RDATA_B2_o 18 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy ADDR_B1_i 15 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy ADDR_B2_i 14 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy CLK_B1_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy CLK_B2_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy REN_B1_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy REN_B2_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy WEN_B1_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy WEN_B2_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy BE_B1_i 2 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy BE_B2_i 2 	
## checked
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy FLUSH1_i 1 	
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy bram_phy FLUSH2_i 1 	

# see frac_lut6 ; adder_carry prepended to pin names, remove cin/cout (not routable)
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__adder_carry adder_carry g 1 
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__adder_carry adder_carry p 1 
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__adder_carry adder_carry sumout 1 
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__adder_carry adder_carry cin 1 
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__adder_carry adder_carry cout 1 

# from fle[physical].fabric.ff_bypass.ff_phy , ff_phy prepended to  pin names, drop SI SO
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__ff_bypass_mode_default__ff_phy ff_phy C 1
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__ff_bypass_mode_default__ff_phy ff_phy D 1
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__ff_bypass_mode_default__ff_phy ff_phy E 1
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__ff_bypass_mode_default__ff_phy ff_phy Q 1
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__ff_bypass_mode_default__ff_phy ff_phy R 1

# from fle[physical].fabric_mode.frac_lut6 , but frac_lut6_ prepended to pin names
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__frac_lut6 frac_lut6 in 6
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__frac_lut6 frac_lut6 lut5_out 2
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__ \\
        fabric_mode_default__frac_lut6 frac_lut6 lut6_out 1

# from [physical].dsp_phy, remove sc_in / sc_out
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy a_i 20
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy acc_fir_i 6
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy b_i 18
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy load_acc 1
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy lreset 1
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy feedback 3
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy unsigned_a 1
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy unsigned_b 1
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy saturate_enable 1
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy shift_right 6
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy round 1
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy subtract 1
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy clk 1
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy z_o 38
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy dsp_phy dly_b_o 18

# from [physical].iopad.ff or .pad      drop SI SO
logical_tile_io_mode_physical__iopad_mode_default__ff ff D 1
logical_tile_io_mode_physical__iopad_mode_default__ff ff Q 1
logical_tile_io_mode_physical__iopad_mode_default__ff ff clk 1
logical_tile_io_mode_physical__iopad_mode_default__pad pad clk 1
logical_tile_io_mode_physical__iopad_mode_default__pad pad outpad 1
logical_tile_io_mode_physical__iopad_mode_default__pad pad inpad 1
# we add the weird prefix for the outward pins
logical_tile_io_mode_physical__iopad_mode_default__pad gfpga_pad_QL_PREIO F2A_CLK 1
logical_tile_io_mode_physical__iopad_mode_default__pad gfpga_pad_QL_PREIO F2A 1
logical_tile_io_mode_physical__iopad_mode_default__pad gfpga_pad_QL_PREIO A2F 1
'''
    # NB whitespace before/after \ removed, so tokens are merged
    words = re.sub(r'\s*\\\s*\n\s*', r'', re.sub(r'#.*', r'', g_ports)).split()
    nl = "\n"
    assert 0 == len(words) % 4, f"words={nl.join(words)}"
    for smodule,        spbtype,        sport,          npins in zip(
        words[0::4],    words[1::4],    words[2::4],    words[3::4]):
        addport(smodule, spbtype, sport, npins)
    # for
    dsdir2schar = { "input":        "i",    "output":       "o",
                    "inout":        "b",    "wire":         "w", }
    consts = { "1'b0", "1'b1", }
    inout = { "in", "out", }

    nnet = 0 ; npin = 0
    for net in vwalk.walknets("fpga_top"):
        (npath, nmod, nbus, nb) = net ; bnet = 1
        #npath = list(npath) # maybe critical!
        tnpath = '.'.join(npath + [nbus])
        if nb is not None: tnpath += f'[{nb}]'
        for pin in vwalk.walkpins(npath, nmod, nbus, nb):
            (ppath, pmod, pbus, pb) = pin
            # routable pin on pb_type or routing mux?
            if ((pmod, pbus, pb) in g_dsmod2n or
                pbus in inout and
                re.match(r'(clock_)?mux.*_size\d+$', pmod)):
                if bnet:
                    if nbus in consts:
                        print(f"NET {nbus[-1]}")
                    else:
                        #tnpath = '.'.join(npath + [nbus])
                        #if nb is not None: tnpath += f'[{nb}]'
                        print(f"NET {tnpath}")
                    nnet += 1 ; bnet = 0
                # if
                dchar = dsdir2schar[vwalk.getportinfo(pmod, pbus)[0]]
                if pb is not None: pbus += f"[{pb}]"
                print(f"PIN {'.'.join(ppath)} {pmod} {pbus} {dchar}")
                npin += 1
            # if
        # for
    # for
    vwalk.log(
f"{vwalk.elapsed()}\tWrote {nnet:,d} net{vwalk.plural(nnet)} " +
f"with {npin:,d} pin{vwalk.plural(npin)}\n")
# def basenets

vwalk.load()
basenets()

# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
