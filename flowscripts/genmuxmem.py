#!/usr/bin/env python3
"""mux & mem modules: read what we have, what we want, and generate the difference"""

import sys
import re
import math

g_rambit = "RS_LATCH"

def dict_aup_nup(k):
    """sort with alpha ascending and numeric ascending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    return r
# def dict_aup_nup

def write_mem(ofile, mod, w):
    """write memory register to hold bits for mux"""
    # w >= 2 or an assign would have been used
    b = int(math.ceil(math.log2(w)))
    l = b - 1
    # indices are NOT suppressed on mem side
    ofile.write(f'''
`default_nettype wire
module {mod} (bl, wl, mem_out, mem_outb);
input [0:{l}] bl;
input [0:{l}] wl;
output [0:{l}] mem_out;
output [0:{l}] mem_outb;
''')
    for i in range(b):
        ofile.write(
f"{g_rambit} {g_rambit}_{i}_ " +
f"( .bl(bl[{i}]), .wl(wl[{i}]), .Q(mem_out[{i}]), .QN(mem_outb[{i}]) );\n")
    # for
    ofile.write('''endmodule
`default_nettype none
''')
# def write_mem

def write_mux(ofile, mod, w):
    """write mux definition whose selects work as OpenFPGA expects"""
    # w >= 2 or an assign would have been used
    b = int(math.ceil(math.log2(w)))
    l = b - 1
    # indices suppressed on mux side for w==2
    indices = f"[0:{l}] " if l else ""
    ofile.write(f'''
`default_nettype wire
module {mod} (in, sram, sram_inv, out);
output out;
input {indices}sram_inv;
input {indices}sram;
input [0:{w - 1}] in;
''')

    ins = list(range(w))
    p = 0   # position in the shrinking list of unused inputs
    s = 1   # one more than next wire number
    i = 0   # which ram input currently driving selects 
    while len(ins) > 1:
        (l, r) = ins[p : p + 2]
        # nonnegative are primary inputs, negative are intermediate results
        l = f"in[{l}]" if l >= 0 else f"w{-l - 1}"
        r = f"in[{r}]" if r >= 0 else f"w{-r - 1}"
        # intermediate or final result?
        (c, o) = ("wire", f"w{s - 1}") if len(ins) > 2 else ("assign", "out")
        # which select
        select = f"sram[{i}]" if indices != "" else "sram"
        # generate the mux
        #file.write(f"{c} {o} = {select} ? {l} : {r};\n")
        ofile.write(f"generic_mux2 u{o} (.z({o}), .s({select}), .d1({l}), .d0({r}));\n")
        # updates
        ins[p : p + 2] = [-s]
        p += 1
        s += 1
        # unused inputs down to a power of 2?
        if len(ins) & (len(ins) - 1): continue
        # yes, restart and move to next ram input
        p = 0
        i += 1
    # while

    ofile.write('''endmodule
`default_nettype none
''')
# def write_mux

def toplogic():
    """read what we have, what we want, and generate the difference"""
    # OLD: script sub_module/muxes.v sub_module/memories.v routing/*.v
    # update first 2 args
    # NEW: script sub_module/memories.v ../CommonFiles/task/CustomModules/muxes_synth.v routing/*.v
    # update first 1 arg

    modwant = set()
    modhave = set()
    for fname in sys.argv[1:]:
        with open(fname, encoding='ascii') as ifile:
            sys.stderr.write(f"Opened {fname}\n")
            for line in ifile:
                line = line.strip()
                g = re.match(r'\s*(mux\S*size\d+\S*)\s+(\S+?)\s*[(]\s*$', line)
                if g: modwant.add(g.group(1))
                g = re.match(r'\s*module\s+(\S+?)\s*[(]', line)
                if g: modhave.add(g.group(1))
            # for
        # with
        sys.stderr.write(f"Closed {fname}\n")
    # for
	# mux_tree_tapbuf_L1SB_size16 mux_left_track_31 (
	# mux_tree_tapbuf_L1SB_size16_mem mem_right_track_0 (

    modmake = sorted(modwant - modhave, key=dict_aup_nup)
    muxmake = [m for m in modmake if not m.endswith("_mem")]
    memmake = [m for m in modmake if     m.endswith("_mem")]

    for modlist, arg, func in [[muxmake, 1, write_mux, ], [memmake, 1, write_mem, ], ]:
        if not modlist: continue
        with open(sys.argv[arg], 'a', encoding='ascii') as ofile:
            sys.stderr.write(f"Opened {sys.argv[arg]}\n")
            for mod in modlist:
                g = re.search(r'size(\d+)', mod)
                assert g
                w = int(g.group(1))
                func(ofile, mod, w)
            # for
        # with
        sys.stderr.write(f"Closed {sys.argv[arg]}\n")
    # for

    sys.stderr.write(f"Added {len(muxmake)} mux modules and {len(memmake)} memory modules\n")
    #sys.stderr.write(f"\nmodwant={modwant}\n")

# def toplogic()

toplogic()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
