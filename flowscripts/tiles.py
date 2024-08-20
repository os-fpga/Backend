#!/usr/bin/python3
"""build phystiles"""

import sys
import re
from collections import defaultdict
import argparse
import pv

# this file builds all phystiles
# another file builds fpga_top

# magic numbers
g_xl = 1      # x: left IO column
g_xf = 2      # x: least non-IO
g_xd = 4      # x: LEFTMOST DSP COLUMN    (**)
g_xb = 7      # x: LEFTMOST BRAM COLUMN   (**)
g_xr = 10     # x: RIGHT IO COLUMN        (**)
g_yb = 1      # y: bottom IO row
g_yf = 2      # y: least non-IO
g_yt = 8      # y: TOP IO ROW             (**)
g_ht = 3      # h: multi-grid tile height
# 4 numbers (**) must come from floorplan

g_dummy = ""

g_blseen = defaultdict(set) # x --> max indices seen
g_wlseen = defaultdict(set) # y --> max indices seen

# floorplan database
g_nwh2xys = defaultdict(set)  # ("name", wo, ho) --> set((x, y))

# file locations (should be arguments)
g_rgfile = "SRC/stamp.stamped.xml"      # input: routing graph

# ensure empty or end with /
g_rtdir = "SRC_routing/"
g_lbdir = "SRC/lb/"                     # input: dir with all logic block files

# map a routing block module to the rep for its equivalence class
g_rb2master = {}

def dict_aup_nup(k):
    """sort with alpha ascending and numeric ascending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    return r
# def dict_aup_nup

def blwlsizes(gutinst, cbyinst, cbxinst, sbinst):
    """return subtiles' BL/WL sizes"""
    u2bl = {} ; u2wl = {}
    for u in [gutinst, cbyinst, cbxinst, sbinst]:
        for port in pv.getports(u):
            if port.name == "bl":
                u2bl[u] = 1 + abs(int(port.left) - int(port.right))
            if port.name == "wl":
                u2wl[u] = 1 + abs(int(port.left) - int(port.right))
    # width of left BLs
    blwid = max(u2bl[gutinst], u2bl[cbxinst])
    # width of bottom WLs
    wbwid = max(u2wl[gutinst], u2wl[cbyinst])
    return [blwid, wbwid, u2bl, u2wl]
# def blwlsizes

#
# CLB PHYSTILE
#

def make_clb(t):
    """make the clb"""
    assert t.startswith("grid_clb_tile_")
    xyt = t[13:]
    base = "clb"
    pv.module(g_dummy + f"grid_{base}_tile{xyt}", port_downto=False)

    # guts would have been: grid_clb_X__Y_
    ginst = f"grid_clb{xyt}"

    # subtiles: logic block
    clbinst = pv.instance(g_lbdir +  "grid_clb.v", "grid_clb", "lb")
    # cbx services TOP pins
    cbxmod = g_rb2master[f"cbx{xyt}"]
    cbxinst = pv.instance(g_rtdir + cbxmod + ".v",     cbxmod, "tb")
    # cby services RIGHT pins
    cbymod = g_rb2master[f"cby{xyt}"]
    cbyinst = pv.instance(g_rtdir + cbymod + ".v",     cbymod, "rb")
    # sb
    sbmod = g_rb2master[f"sb{xyt}"]
    sbinst  = pv.instance(g_rtdir +  sbmod + ".v",      sbmod, "sb")

    # globals
    pv.bus(clbinst, "global_resetn", "global_resetn")
    pv.bus(clbinst, "scan_en", "scan_en")
    pv.bus(clbinst, "scan_mode", "scan_mode")
    for port in pv.getports(clbinst):
        p = port.name
        if '__pin_clk_' not in p: continue
        c = re.sub(r'^.*__pin_clk_(\d+)_.*$', r'\1', p)
        pv.bundle(clbinst, p, f"clk[{c}]")

    # chains
    pv.bundle(clbinst,      "top_width_0_height_0_subtile_0__pin_cin_0_", "cin")
    pv.bundle(clbinst,  "bottom_width_0_height_0_subtile_0__pin_cout_0_", "cout")
    pv.bundle(clbinst,   "left_width_0_height_0_subtile_0__pin_sc_in_0_", "sc_in[0]")
    pv.bundle(clbinst, "right_width_0_height_0_subtile_0__pin_sc_out_0_", "sc_out[0]")
    pv.bundle(clbinst,  "right_width_0_height_0_subtile_0__pin_sr_in_0_", "sr_in[0]")
    pv.bundle(clbinst,  "left_width_0_height_0_subtile_0__pin_sr_out_0_", "sr_out[0]")

    # pin_I / pin_O local conns
    for u in [clbinst, sbinst, cbxinst, cbyinst]:
        for port in pv.getports(u):
            p = port.name
            if '__pin_' not in p: continue
            net = re.sub(r'^.*subtile_0__pin_(\S+)_(\d+)_$', r'pin_\1[\2]', p)
            if net[4] not in "IO": continue
            pv.bundle(u, p, net)
        # for
    # for

    # chan ports
    for u in [sbinst, cbxinst, cbyinst]:
        for port in pv.getports(u):
            p = port.name
            if p.startswith("chan"):
                pv.bus(u, p, p)
        # for
    # for

    # get BL/WL indices
    (blwid, wbwid, u2bl, u2wl) = blwlsizes(clbinst, cbyinst, cbxinst, sbinst)
    wli = [ u2wl[clbinst]-1, u2wl[cbyinst]-1, wbwid+u2wl[cbxinst]-1, wbwid+u2wl[ sbinst]-1, ]
    bli = [ u2bl[clbinst]-1, u2bl[cbxinst]-1, blwid+u2bl[cbyinst]-1, blwid+u2bl[ sbinst]-1, ]

    if g_dummy:
        for xx, yy in g_nwh2xys[(base, 0, 0)]:
            g_wlseen[yy].update(wli)
            g_blseen[xx].update(bli)
        (w2, w3, b2, b3) = (0, 0, 0, 0)
    else:
        maxwl = 0
        maxbl = 0
        for xx, yy in g_nwh2xys[(base, 0, 0)]:
            maxwl = max(maxwl, max(g_wlseen[yy]))
            maxbl = max(maxbl, max(g_blseen[xx]))
        w2 = maxwl - wli[2]
        w3 = maxwl - wli[3]
        b2 = maxbl - bli[2]
        b3 = maxbl - bli[3]
        assert w2 >= 0 and w3 >= 0 and b2 >= 0 and b3 >= 0, f"C w2={w2} w3={w2} b2={b2} b3={b3}"

    # wordlines
    pv.bundle(clbinst, "wl", f"wl[0:{wli[0]}]")        # 0,0
    pv.bundle(cbyinst, "wl", f"wl[0:{wli[1]}]")        # 1,0
    pv.bundle(cbxinst, "wl", f"wl[{w2+wbwid}:{w2+wli[2]}]")  # 0,1
    pv.bundle( sbinst, "wl", f"wl[{w3+wbwid}:{w3+wli[3]}]")  # 1,1

    # bitlines
    pv.bundle(clbinst, "bl", f"bl[0:{bli[0]}]")        # 0,0
    pv.bundle(cbxinst, "bl", f"bl[0:{bli[1]}]")        # 0,1
    pv.bundle(cbyinst, "bl", f"bl[{b2+blwid}:{b2+bli[2]}]")  # 1,0
    pv.bundle( sbinst, "bl", f"bl[{b3+blwid}:{b3+bli[3]}]")  # 1,1

    pv.endmodule()

    return ginst
# def make_clb

#
# IO PHYSTILES
#

def make_ios(io):
    """make io phystiles"""

    # which direction to stitch the io scan chain
    # case order determines corners
    sc_inp = 0 if 'right' in io else 1 if 'left' in io else 0 if 'bottom' in io else 1

    (x, y) = re.search(r'^\S+_(\d+)__(\d+)_$', io).groups()
    (x, y) = (int(x), int(y))

    # which guts to use?
    # order is important (corner dominance)
    if   'left' in io:
        guts = "grid_io_left"
    elif 'right' in io:
        guts = "grid_io_right"
    elif 'bottom' in io:
        guts = "grid_io_bottom"
    elif 'top' in io:
        guts = "grid_io_top"
    else:
        assert 0
    base = guts[5:]

    pv.module(g_dummy + io, port_downto=False)
    xyt = f"_{x}__{y}_"

    # guts would have been: grid_io_{bottom,left,right,top}_X__Y_
    ginst = f"{guts}{xyt}"

    # subtiles
    gutinst = pv.instance(g_lbdir +   guts + ".v",   guts,    "lb")
    # cbx is for TOP pins
    cbxmod = g_rb2master[f"cbx{xyt}"]
    cbxinst = pv.instance(g_rtdir + cbxmod + ".v", cbxmod,    "tb")
    # cbx is for RIGHT pins
    cbymod = g_rb2master[f"cby{xyt}"]
    cbyinst = pv.instance(g_rtdir + cbymod + ".v", cbymod,    "rb")
    # sb
    sbmod = g_rb2master[f"sb{xyt}"]
    sbinst  = pv.instance(g_rtdir +  sbmod + ".v",  sbmod,    "sb")

    # globals
    pv.bus(gutinst, "global_resetn", "global_resetn")
    pv.bus(gutinst, "scan_en", "scan_en")
    #v.bus(gutinst, "scan_mode", "scan_mode")
    for port in pv.getports(gutinst):
        p = port.name
        if '__pin_clk_' not in p: continue
        c = re.sub(r'^.*__pin_clk_(\d+)_.*$', r'\1', p)
        pv.bundle(gutinst, p, f"clk[{c}]")

    # promote
    port = pv.getport(gutinst, "gfpga_pad_QL_PREIO_A2F")
    slots = 1 + abs(int(port.left) - int(port.right))
    ins = slots // 3    # lower-numbered bits
    outs = slots - ins  # upper-numbered bits
    itxt = "{" + ','.join([f"a2f[0:{ins-1}]"] + (["1'b0"] * outs)) + "}"
    otxt = "{" + f"nc_f2a[0:{ins-1}]" + "," + f"f2a[0:{outs-1}]" + "}"
    ctxt = "{" + f"nc_clk[0:{ins-1}]" + "," + f"clk_out[0:{outs-1}]" + "}"
    pv.declare("wire", "nc_f2a")
    pv.declare("wire", "nc_clk")
    pv.bundle(gutinst, "gfpga_pad_QL_PREIO_A2F", itxt)
    pv.bundle(gutinst, "gfpga_pad_QL_PREIO_F2A", otxt)
    pv.bundle(gutinst, "gfpga_pad_QL_PREIO_F2A_CLK", ctxt)
    # these busses aren't promoted on corners
    if x in (g_xl, g_xr) and y in (g_yb, g_yt):
        pv.declare("wire", "a2f")
        pv.assign(f"a2f[0:{ins-1}]", "{" + ','.join(["1'b0"] * ins) + "}")
        pv.declare("wire", "f2a")
        pv.declare("wire", "clk_out")

    # scan chains
    sc = defaultdict(dict)
    for port in pv.getports(gutinst):
        g = re.search(r'_subtile_(\d+)__pin_sc_(in|out)_0_$', port.name)
        if not g: continue
        (s, di) = g.groups()
        sc[di][int(s)] = port.name
    # for
    # sc_inp>0 means max slot is input, otherwise 0 
    islot = (slots - 1) if sc_inp else 0
    oslot = (slots - 1) - islot             # other end
    idiff = 1 if sc_inp else -1             # input from greater or lesser slot
    for s in range(slots):
        net = f"sc[{s + idiff}]" if s != islot else "iosc_in"
        pv.bundle(gutinst, sc["in" ][s], net)
        net = f"sc[{s        }]" if s != oslot else "iosc_out"
        pv.bundle(gutinst, sc["out"][s], net)
    # for

    # pin_ local conns
    driven = defaultdict(int)
    for i, u in enumerate([gutinst, sbinst, cbxinst, cbyinst], start=0):
        for port in pv.getports(u):
            p = port.name
            if '__pin_f2a_i' in p or '__pin_a2f_o' in p:
                net = re.sub(r'^.*subtile_(\d+)__pin_(\S+)_0_$', r'pin_\2[\1]', p)
                pv.bundle(u, p, net)
                if '__pin_f2a_i' in p and not i:
                    driven[net] |= 0            # sink
                if '__pin_f2a_i' in p and 1 < i:
                    driven[net] |= 1            # driver
        # for
    # for
    undriven = [net for net in sorted(driven, key=dict_aup_nup) if not driven[net]]
    if undriven:
        umin = re.sub(r'^.*\[(\d+)\]$', r'\1', undriven[ 0])
        umax = re.sub(r'^.*\[(\d+)\]$', r'\1', undriven[-1])
        assert 1 + int(umax) - int(umin) == len(undriven)
        pv.assign(f"pin_f2a_i[{umin}:{umax}]", "{" + ','.join(["1'b0"] * len(undriven)) + "}")

    # chan ports
    for u in [sbinst, cbxinst, cbyinst]:
        for port in pv.getports(u):
            p = port.name
            if p.startswith("chan"):
                pv.bus(u, p, p)
        # for
    # for

    # get BL/WL indices
    (blwid, wbwid, u2bl, u2wl) = blwlsizes(gutinst, cbyinst, cbxinst, sbinst)
    wli = [ u2wl[gutinst]-1, u2wl[cbyinst]-1, wbwid+u2wl[cbxinst]-1, wbwid+u2wl[ sbinst]-1, ]
    bli = [ u2bl[gutinst]-1, u2bl[cbxinst]-1, blwid+u2bl[cbyinst]-1, blwid+u2bl[ sbinst]-1, ]

    if g_dummy:
        for xx, yy in g_nwh2xys[(base, 0, 0)]:
            g_wlseen[yy].update(wli)
            g_blseen[xx].update(bli)
        (w2, w3, b2, b3) = (0, 0, 0, 0)
    else:
        maxwl = 0
        maxbl = 0
        for xx, yy in g_nwh2xys[(base, 0, 0)]:
            maxwl = max(maxwl, max(g_wlseen[yy]))
            maxbl = max(maxbl, max(g_blseen[xx]))
        w2 = maxwl - wli[2]
        w3 = maxwl - wli[3]
        b2 = maxbl - bli[2]
        b3 = maxbl - bli[3]
        assert w2 >= 0 and w3 >= 0 and b2 >= 0 and b3 >= 0, f"I w2={w2} w3={w2} b2={b2} b3={b3}"

    # wordlines
    pv.bundle(gutinst, "wl", f"wl[0:{wli[0]}]")        # 0,0
    pv.bundle(cbyinst, "wl", f"wl[0:{wli[1]}]")        # 1,0
    pv.bundle(cbxinst, "wl", f"wl[{w2+wbwid}:{w2+wli[2]}]")  # 0,1
    pv.bundle( sbinst, "wl", f"wl[{w3+wbwid}:{w3+wli[3]}]")  # 1,1

    # bitlines
    pv.bundle(gutinst, "bl", f"bl[0:{bli[0]}]")        # 0,0
    pv.bundle(cbxinst, "bl", f"bl[0:{bli[1]}]")        # 0,1
    pv.bundle(cbyinst, "bl", f"bl[{b2+blwid}:{b2+bli[2]}]")  # 1,0
    pv.bundle( sbinst, "bl", f"bl[{b3+blwid}:{b3+bli[3]}]")  # 1,1

    pv.endmodule()

    return ginst
# def make_ios

#
# MULTI-GRID PHYSTILES (BRAM/DSP)
#

def make_multi(t):
    """make multi-grid phystiles"""
    # don't generate if not in this array
    if '_0_' in t: return
    # pull out parts of name
    (guts, x, y) = re.search(r'^(\S+)_tile_(\d+)__(\d+)_$', t).groups()
    (x, y) = (int(x), int(y))
    base = guts[5:]

    pv.module(g_dummy + t, port_downto=False)
    xyt = f"_{x}__{y}_"

    # guts would have been: grid_{bram,dsp}_X__Y_
    ginst = f"{guts}{xyt}"

    # subtiles
    gutinst = pv.instance(g_lbdir + guts + ".v",  guts,  "lb")
    cbxinst = [None] * g_ht
    cbyinst = [None] * g_ht
    sbinst  = [None] * g_ht
    for yo in range(g_ht):
        xyo = f"_{x}__{y + yo}_"
        ys = f"y{yo}" if yo else ""
        # cbx is for TOP pins
        cbxmod = g_rb2master[f"cbx{xyo}"]
        cbxinst[yo] = pv.instance(g_rtdir + cbxmod + ".v", cbxmod, "tb" + ys)
        # cby is for RIGHT pins
        cbymod = g_rb2master[f"cby{xyo}"]
        cbyinst[yo] = pv.instance(g_rtdir + cbymod + ".v", cbymod, "rb" + ys)
        # sb
        sbmod = g_rb2master[f"sb{xyo}"]
        sbinst [yo] = pv.instance(g_rtdir +  sbmod + ".v",  sbmod, "sb" + ys)
    # for

    # globals
    pv.bus(gutinst, "global_resetn", "global_resetn")
    pv.bus(gutinst, "scan_en", "scan_en")
    pv.bus(gutinst, "scan_mode", "scan_mode")

    # promotions
    for port in pv.getports(gutinst):
        p = port.name
        g = re.search(
r'__pin_(clk|PL_\S+|sc_in|sc_out|sr_in|sr_out|plr_[io]|RAM_ID_i)_(\d+)_$', p)
        if g:
            (bus, idx) = g.groups()
            pv.bundle(gutinst, p, f"{bus}[{idx}]")
    # for

    # pin_ local conns
    for yo in range(g_ht):
        for u in [gutinst, sbinst[yo], cbxinst[yo], cbyinst[yo]]:
            if yo and y == gutinst: continue
            for port in pv.getports(u):
                p = port.name
                if '__pin_I' in p or '__pin_O' in p:
                    net = re.sub(r'^.*subtile_0__pin_(\S+)_(\d+)_$', r'pin_\1[\2]', p)
                    pv.bundle(u, p, net)
            # for
        # for
    # for

    # chan ports
    in2out = { "chany_bottom_in": "chany_top_out", "chany_top_in": "chany_bottom_out", }
    for w in range(g_ht):
        uu = [sbinst, cbxinst, cbyinst][w]
        for yo in range(g_ht):
            u = uu[yo]
            for port in pv.getports(u):
                p = port.name
                if p.startswith("chanx"):
                    net = f"{p}{yo}" if yo else p
                    pv.bus(u, p, net)
                if p.startswith("chany"):
                    ye = 0 if 'bottom' in p else 2
                    o = in2out.get(p, p)
                    net = ( p if yo == ye else
                            f"{u}__{p}" if 'out' in p else
                            f"{sbinst[yo - 1]}__{o}" if 'bottom' in p else
                            f"{sbinst[yo + 1]}__{o}" )
                    pv.bus(u, p, net)
            # for
        # for
    # for

    # get BL/WL indices (assumes 0, 1, 2 are all the same)
    (blwid, wbwid, u2bl, u2wl) = blwlsizes(gutinst, cbyinst[0], cbxinst[0], sbinst[0])
    wli = [ u2wl[gutinst]-1, u2wl[cbyinst[0]]-1, wbwid+u2wl[cbxinst[0]]-1, wbwid+u2wl[ sbinst[0]]-1, ] 
    bli = [ u2bl[gutinst]-1, u2bl[cbxinst[0]]-1, blwid+u2bl[cbyinst[0]]-1, blwid+u2bl[ sbinst[0]]-1, ] 

    # if guts have all their config bits at 0,0,
    # and routing blocks are standard and omnipresent,
    # need consider only 0,0.
    if g_dummy:
        for xx, yy in g_nwh2xys[(base, 0, 0)]:
            g_wlseen[yy].update(wli)
            g_blseen[xx].update(bli)
        (w2, w3, b2, b3) = (0, 0, 0, 0)
    else:
        maxwl = 0
        maxbl = 0
        for xx, yy in g_nwh2xys[(base, 0, 0)]:
            maxwl = max(maxwl, max(g_wlseen[yy]))
            maxbl = max(maxbl, max(g_blseen[xx]))
        w2 = maxwl - wli[2]
        w3 = maxwl - wli[3]
        b2 = maxbl - bli[2]
        b3 = maxbl - bli[3]
        assert w2 >= 0 and w3 >= 0 and b2 >= 0 and b3 >= 0, f"M w2={w2} w3={w2} b2={b2} b3={b3}"

    # wordlines
    pv.bundle(gutinst,         "wl", f"wl[0:{wli[0]}]")        # 0,0
    for yo in range(g_ht):
        n = f"wl{yo}" if yo else "wl"
        pv.bundle(cbyinst[yo], "wl", f"{n}[0:{wli[1]}]")       # 1,0
        pv.bundle(cbxinst[yo], "wl", f"{n}[{w2+wbwid}:{w2+wli[2]}]") # 0,1
        pv.bundle( sbinst[yo], "wl", f"{n}[{w3+wbwid}:{w3+wli[3]}]") # 1,1

    # bitlines
    pv.bundle(gutinst,         "bl", f"bl[0:{bli[0]}]")        # 0,0
    for yo in range(g_ht):
        pv.bundle(cbxinst[yo], "bl", f"bl[0:{bli[1]}]")        # 0,1
        pv.bundle(cbyinst[yo], "bl", f"bl[{b2+blwid}:{b2+bli[2]}]")  # 1,0
        pv.bundle( sbinst[yo], "bl", f"bl[{b3+blwid}:{b3+bli[3]}]")  # 1,1

    pv.endmodule()

    return ginst
# def make_multi

def toplogic():
    """create all tiles twice"""
    # first pass collects info for BL/WL widths,
    # second pass respects it

    parser = argparse.ArgumentParser(
                        description="Generate tile Verilog")
    # --read_rr_graph rr_graph.xml
    parser.add_argument('--read_rr_graph',
                        required=True,
                        help="Routing graph XML file")
    # --route_box_dir SRC/routing
    parser.add_argument('--route_box_dir',
                        required=True,
                        help="Routing box Verilog directory")
    # --logic_block_dir SRC/lb
    parser.add_argument('--logic_block_dir',
                        required=True,
                        help="Logic block Verilog directory")
    # process args
    args = parser.parse_args()
    global g_rgfile, g_rtdir, g_lbdir
    g_rgfile = args.read_rr_graph
    g_rtdir  = args.route_box_dir
    g_lbdir  = args.logic_block_dir
    # must end with / if non-blank
    if g_rtdir != "" and g_rtdir[-1] != "/": g_rtdir += "/"
    if g_lbdir != "" and g_lbdir[-1] != "/": g_lbdir += "/"

    # read in masters.csv for routing blocks
    filename = f"{g_rtdir}rmasters.csv"
    with open(filename, encoding='ascii') as ifile:
        for line in ifile:
            line = line.strip()
            if not line: continue
            ff = line.split(',')
            for other in ff:
                g_rb2master[other] = ff[0]
        # for
    # with
    sys.stderr.write(f"Read {filename}\n")

    # read in a formatted routing graph for floorplan info
    interesting = {'rr_nodes', 'block_type', 'grid_loc', }
    id2name = {}                # "#" --> "name"
    ram_min = 0
    dsp_min = 0
    col_max = 0
    row_max = 0
    g_xy2what = {}
    filename = g_rgfile
    with open(filename, encoding='ascii') as ifile:
        for line in ifile:
            line = re.sub(r'[<>"/]', r'', line)
            ff = line.split()
            if not ff or ff[0] not in interesting: continue
            if ff[0] == 'rr_nodes': break
            h = dict([f.split('=') for f in ff[1:]])
            if not h: continue
            if ff[0] == "block_type":
                id2name[h['id']] = h['name']
                continue
            (bid, x, y, wo, ho) = [h[a] for a in ( 'block_type_id', 'x', 'y',
                                    'width_offset', 'height_offset', )]
            (x, y, wo, ho) = [int(a) for a in (x, y, wo, ho)]
            bname = id2name[bid]
            g_nwh2xys[(bname, wo, ho)].add((x, y))
            if bname == "EMPTY": continue
            if bname == "bram" and (not ram_min or x < ram_min): ram_min = x
            if bname == "dsp"  and (not dsp_min or x < dsp_min): dsp_min = x
            if x > col_max: col_max = x
            if y > row_max: row_max = y
            if wo or ho: continue
            g_xy2what[(x, y)] = [bname, "", "", ""]
        # for
    # with
    sys.stderr.write(f"Read {filename}\n")
    sys.stderr.write(f"{col_max}x{row_max} array with first DSP at {dsp_min} and first BRAM at {ram_min}\n\n")
    global g_xd, g_xb, g_xr, g_yt
    g_xd = dsp_min
    g_xb = ram_min
    g_xr = col_max
    g_yt = row_max

    # here we generate master tiles
    masterquad2xys = defaultdict(set)
    for xy, quad in g_xy2what.items():
        (x, y) = xy
        sbname = f"sb_{x}__{y}_"
        cbxname = f"cbx_{x}__{y}_"
        cbyname = f"cby_{x}__{y}_"
        quad[1] = sbmaster = g_rb2master[sbname]
        quad[2] = cbxmaster = g_rb2master[cbxname]
        quad[3] = cbymaster = g_rb2master[cbyname]
        masterquad2xys[tuple(quad)].add((x, y))

    # write out our masters and their instance names
    filename = "tmasters.csv"
    todo = []
    grouping = {}
    xy2tmod = {}    # (x, y) ==> tile mod name
    with open(filename, "w", encoding='ascii') as ofile:
        for quad, xys in masterquad2xys.items():
            xys = sorted(xys)
            unique = len(xys) == 1
            (x, y) = xys[0]
            bname = quad[0]
            if 'io' in bname:
                if   x == g_xl and y == g_yb and unique:
                    tile = "bottom_left"
                elif x == g_xl and y == g_yt and unique:
                    tile = "top_left"
                elif x == g_xr and y == g_yb and unique:
                    tile = "bottom_right"
                elif x == g_xr and y == g_yt and unique:
                    tile = "top_right"
                elif x == g_xl:
                    tile = "left"
                elif x == g_xr:
                    tile = "right"
                elif y == g_yb:
                    tile = "bottom"
                elif y == g_yt:
                    tile = "top"
                else:
                    assert 0
                tile = f"{tile}_io_tile_{x}__{y}_"
            else:
                tile = f"grid_{bname}_tile_{x}__{y}_"
            # block type, modname, x, y
            todo.append((bname, tile, x, y))
            tiles = [f"tile_{x}__{y}_" for x, y in xys]
            ofile.write(f"{tile},{','.join(tiles)}\n")
            for xy in xys:
                xy2tmod[xy] = tile
        # for
    # with

    # DEBUG
    # sys.exit(1)

    global g_dummy
    g_dummy = "dummy_"  # for Pass 1
    mod2guse = {}       # given tile module, return OpenFPGA guts instance name
    for p in range(2):

        # make all the different tiles
        for t in [t[1] for t in todo if t[0] == 'clb']:
            mod2guse[t] = make_clb(t)
        for t in [t[1] for t in todo if '_io_' in t[1]]:
            mod2guse[t] = make_ios(t)
        for t in [t[1] for t in todo if t[0] in ('bram', 'dsp')]:
            mod2guse[t] = make_multi(t)
        if p: break

        g_dummy = ""    # for Pass 2
        wid2count = defaultdict(int)
        for x in g_blseen:
            wid2count[1 + max(g_blseen[x])] += 1
        stats = [f"{wid}#{wid2count[wid]}" for wid in sorted(wid2count)]
        sys.stderr.write(f"\nBL columns' widths are {' '.join(stats)}\n")
        wid2count = defaultdict(int)
        for y in g_wlseen:
            wid2count[1 + max(g_wlseen[y])] += 1
        stats = [f"{wid}#{wid2count[wid]}" for wid in sorted(wid2count)]
        sys.stderr.write(f"WL rows' widths are {' '.join(stats)}\n\n")
    # for

    # write out unflattening map
    filename = "grouping.csv"
    with open(filename, "w", encoding='ascii') as ofile:
        for xy in sorted(xy2tmod):
            tm = xy2tmod[xy]                    # tile mod @ this loc
            (x, y) = xy
            (xm, ym) = re.search(r'^\S+_tile_(\d+)__(\d+)_$', tm).groups()
            (xm, ym) = (int(xm), int(ym))
            tile = f"tile_{x}__{y}_"

            # map guts
            gh = mod2guse[tm]
            gf = re.sub(r'_\d+__\d+_$', fr'_{x}__{y}_', gh)
            ofile.write(f"{gf},{tile}.lb\n")

            # map routing blocks
            ht = g_ht if 'bram' in tm or 'dsp' in tm else 1
            for yo in range(ht):
                ys = f"y{yo}" if yo else ""
                sbh  = "sb" + ys
                cbxh = "tb" + ys    # cbx: TOP pins box
                cbyh = "rb" + ys    # cby: RIGHT pins box
                sbf  = f"sb_{ x }__{y +yo}_"
                cbxf = f"cbx_{x }__{y +yo}_"
                cbyf = f"cby_{x }__{y +yo}_"
                ofile.write(f"{ sbf},{tile}.{ sbh}\n")
                ofile.write(f"{cbxf},{tile}.{cbxh}\n")
                ofile.write(f"{cbyf},{tile}.{cbyh}\n")
        # for
    # with

# def

toplogic()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
