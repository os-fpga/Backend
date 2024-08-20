#!/usr/bin/python3
"""generate fpga_top"""

import sys
import re 
#from collections import defaultdict
import argparse 

import pv

# this file builds fpga_top

# magic numbers
g_xl = 1      # x: left IO column
g_xf = 2      # x: least non-IO
g_xd = 4      # x: LEFTMOST DSP COLUMN    (**)
g_xb = 7      # x: LEFTMOST BRAM COLUMN   (**)
g_xr = 10     # x: RIGHT IO COLUMN        (**)
g_yb = 1      # y: bottom IO row
g_yf = 2      # y: least non-IO
g_yt = 8      # y: TOP IO ROW             (**)
# 4 numbers (**) must come from floorplan

# floorplan database
g_name2height = {}              # bname --> height
g_xy2nwh = {}                   # (x, y) --> (bname, wo, ho)

# file locations (should be arguments)
g_rgfile = "SRC/stamp.stamped.xml"      # input: routing graph
g_tiledir = ""                          # input: tile directory

def toplogic():
    """read floorplan and create fpga_top.v"""

    parser = argparse.ArgumentParser(
                        description="Generate top array Verilog")
    # --read_rr_graph rr_graph.xml
    parser.add_argument('--read_rr_graph',
                        required=True,
                        help="Routing graph XML file")
    # --tile_dir SRC/tile
    parser.add_argument('--tile_dir',
                        required=True,
                        help="Tile Verilog directory")
    # process args
    args = parser.parse_args()
    global g_rgfile, g_tiledir
    g_rgfile = args.read_rr_graph
    g_tiledir  = args.tile_dir
    # must end with / if non-blank
    if g_tiledir != "" and g_tiledir[-1] != "/": g_tiledir += "/"

    # read in a formatted routing graph for floorplan info
    interesting = {'rr_nodes', 'block_type', 'grid_loc', }
    id2name = {}                # "#" --> "name"
    ram_min = 0
    dsp_min = 0
    col_max = 0
    row_max = 0
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
                g_name2height[h['name']] = int(h['height'])
                continue
            (bid, x, y, wo, ho) = [h[a] for a in ( 'block_type_id', 'x', 'y',
                                    'width_offset', 'height_offset', )]
            (x, y, wo, ho) = [int(a) for a in (x, y, wo, ho)]
            bname = id2name[bid]
            g_xy2nwh[(x, y)] = (bname, wo, ho)
            if bname == "EMPTY": continue
            if bname == "bram" and (not ram_min or x < ram_min): ram_min = x
            if bname == "dsp"  and (not dsp_min or x < dsp_min): dsp_min = x
            if x > col_max: col_max = x
            if y > row_max: row_max = y
        # for
    # with
    sys.stderr.write(f"Read {filename}\n")
    sys.stderr.write(f"{col_max}x{row_max} array with first DSP at {dsp_min} and first BRAM at {ram_min}\n\n")
    global g_xd, g_xb, g_xr, g_yt
    g_xd = dsp_min
    g_xb = ram_min
    g_xr = col_max
    g_yt = row_max

    # read in tmasters.csv
    filename =  f"{g_tiledir}tmasters.csv"
    tile2master = {}
    with open(filename, encoding='ascii') as ifile:
        for line in ifile:
            line = line.strip()
            if not line: continue
            ff = line.split(',')
            if not ff: continue
            for tile in ff[1:]:
                assert tile not in tile2master
                tile2master[tile] = ff[0]
            # for
        # for
    # with

    #
    # build fpga_top.v
    #
    pv.module("fpga_top", port_downto=False)

    #corners = {
    #    (g_xl, g_yb): f"bottom_left_io_tile_{g_xl}__{g_yb}_",
    #    (g_xl, g_yt): f"top_left_io_tile_{g_xl}__{g_yt}_",
    #    (g_xr, g_yb): f"bottom_right_io_tile_{g_xr}__{g_yb}_",
    #    (g_xr, g_yt): f"top_right_io_tile_{g_xr}__{g_yt}_",
    #}

    ioall = []  # all IOs
    io_nc = []  # no corner IOs
    iobus = {}
    for xy in sorted(g_xy2nwh):
        (bname, wo, ho) = g_xy2nwh[xy] 
        if bname == "EMPTY" or wo or ho: continue
        (x, y) = xy
        tile = f"tile_{x}__{y}_"
        base = bname.split('_')[0]

        # determine master
        master = tile2master[tile]
        #master = ""
        #if base == "clb":
        #    master = f"grid_clb_tile_{g_xf}__{g_yf}_"
        #if base == "bram":
        #    master = f"grid_bram_tile_{g_xb}__{g_yf}_"
        #if base == "dsp":
        #    master = f"grid_dsp_tile_{g_xd}__{g_yf}_"
        #if base == "io":
        #    if   (x, y) in corners:
        #        master = corners[(x, y)]
        #    elif x == g_xl:
        #        master = f"left_io_tile_{g_xl}__{g_yf}_"
        #    elif x == g_xr:
        #        master = f"right_io_tile_{g_xr}__{g_yf}_"
        #    elif y == g_yb:
        #        master = f"bottom_io_tile_{g_xf}__{g_yb}_"
        #    elif y == g_yt:
        #        master = f"top_io_tile_{g_xf}__{g_yt}_"
        #assert master

        # tile
        inst = pv.instance(f"{g_tiledir}{master}.v", master, tile)

        # save IOTiles for later sorting and chaining
        if   x == g_xl:
            ioall.append((0, -y, inst, x, y))
            #if y not in (g_yb, g_yt):
            io_nc.append((0,  y, inst, x, y))   
        elif x == g_xr:
            ioall.append((2,  y, inst, x, y))
            #if y not in (g_yb, g_yt):
            io_nc.append((2, -y, inst, x, y))   
        elif y == g_yb:
            ioall.append((1,  x, inst, x, y))
            #if x not in (g_xl, g_xr):
            io_nc.append((3, -x, inst, x, y))   
        elif y == g_yt:
            ioall.append((3, -x, inst, x, y))
            #if x not in (g_xl, g_xr):
            io_nc.append((1,  x, inst, x, y))   

        # globals, abutments, chan
        global_set = {"global_resetn": 0, "scan_en": 0, "scan_mode": 0, "clk": 1, }
        down_set = {"sc", "c", }
        up_set = {"sr", "plr", }
        side2src = {
            "right":   ( 1,  0, "left"),
            "left":    (-1,  0, "right"),
            "top":     ( 0,  1, "bottom"),
            "bottom":  ( 0, -1, "top"),
        }
        for port in pv.getports(inst):
            p = port.name
            if p in global_set:
                if global_set[p]:
                    pv.bus(inst, p, p)
                else:
                    pv.bundle(inst, p, p)
                continue
            # NB this could be a hash p --> channel, pdir
            g = re.match(r'^(.+?)(_in|_out|_i|_o|in|out)$', p)
            if g: 
                (channel, pdir) = g.groups()
                o = 1 if 'i' in pdir else 0
                if base in ('bram', 'dsp'): o *= 3
                if channel.startswith("PL_") or channel in down_set:
                    # tie off top cin
                    if p == "cin" and y == g_yt - 1:
                        pv.bundle(inst, p, "1'b0")
                        continue
                    bus = f"{channel}__abut_{x}__{y+o}_"
                    # keep bottom PL_ local
                    if channel.startswith("PL_") and y == g_yf:
                        pv.declare("wire", bus)
                    # normal case
                    pv.bus(inst, p, bus)
                    continue
                if channel in up_set:
                    # wrap around BRAM PL_DATA/plr chain at bottom
                    if p == "plr_i" and y == g_yf:
                        channel = "PL_DATA"
                        o = 0
                    # wrap around any sc/sr chain at bottom
                    if p == "sr_in" and y == g_yf:
                        channel = "sc"
                        o = 0
                    pv.bus(inst, p, f"{channel}__abut_{x}__{y-o}_")
                    continue
            if p.startswith("chan"):
                g = re.match(r'(chan[xy])_(\w+)_(\w+?)(\d*)$', p)
                assert g
                (axis, side, cdir, rep) = g.groups()
                rep = int(rep) if rep else 0
                if cdir == "out":
                    bus = f"{axis}_{side}__abut_{x}__{y+rep}_"
                else:
                    (dx, dy, oside) = side2src[side]
                    if dy:
                        if dy > 0: dy = g_name2height[bname]
                        if dy < 0: dy -= g_xy2nwh[(x, y - 1)][2]
                    bus = f"{axis}_{oside}__abut_{x+dx}__{y+dy+rep}_"
                pv.bus(inst, p, bus)
                continue
            if p.startswith("wl"):
                rep = int(p[2:]) if p[2:] else 0
                pv.bus(inst, p, f"wl__abut_{g_xl-1}__{y+rep}_")
                continue
            if p == "bl":
                pv.bus(inst, p, f"bl__abut_{x}__{g_yt+1}_")
                continue
            # a2f is first (left) 0:23, f2a/clk_out are last (right) 0:47
            if p in ('a2f', 'f2a', 'clk_out'):
                if   x == g_xl: bus = f"left_{y}"
                elif x == g_xr: bus = f"right_{y}"
                elif y == g_yb: bus = f"bottom_{x}"
                elif y == g_yt: bus = f"top_{x}"
                else:
                    assert 0
                pv.bus(inst, p, f"{bus}_{p}")
                iobus[(x, y)] = bus
            if p == "RAM_ID_i":
                # id is X in upper 10b, and Y in lower 10b
                num = ('0' * 20) + bin((x << 10) | y)[2:]
                num = ','.join([f"1'b{b}" for b in num[-20:]])
                pv.bundle(inst, p, "{" + num + "}")
                continue
        # for port
    # for xy

    # chain the IO scan chain
    ioall = sorted(ioall)
    for i, quint in enumerate(ioall, start=0):
        _, _, inst, x, y = quint
        pv.bus(inst, "iosc_out", f"{inst}__iosc_out" if i != len(ioall) - 1 else
                f"iosc__abut_{g_xf}__{g_yt}_0")     # keep unchanged
        pv.bus(inst, "iosc_in",  f"{ioall[i-1][2]}__iosc_out" if i else
                f"iosc__abut_{g_xl}__{g_yt+1}_0")   # keep unchanged
    # for

    # create adaptors for IO connections
    # FIXME read these from somewhere
    slots = 72
    ins = slots // 3
    outs = slots - ins
    # filenames
    wfilename = "iowires.inc"
    pfilename = "ioports.inc"
    io_nc = sorted(io_nc)
    common = "gfpga_pad_QL_PREIO_"
    with    open(wfilename, "w", encoding='ascii') as wfile, \
            open(pfilename, "w", encoding='ascii') as pfile:
        # these must align
        big   = ('A2F', 'F2A', 'F2A_CLK', )
        small = ('a2f', 'f2a', 'clk_out', )
        b = slots * len(io_nc)
        for w in big:
            wfile.write(f"wire [0:{b-1}] {common}{w};\n")
        # for
        for p in range(2):
            for i, quint in enumerate(io_nc, start=0):
                _, _, inst, x, y = quint
                # skip corners
                if x in (g_xl, g_xr) and y in (g_yb, g_yt): continue
                # compute bit ranges
                b = i * slots
                (a2fmin, a2fmax, f2amin, f2amax) = (b, b + ins - 1, b + ins, b + slots - 1)
                bus = iobus[(x, y)]
                for j, w in enumerate(small, start=0):
                    if not p:
                        b = slots - ins if j else ins
                        wfile.write(f"wire [0:{b-1}] {bus}_{w};\n")
                        pfile.write(f"\t.{bus}_{w}({bus}_{w}),\n")
                    else:
                        if j:
                            bus0 = f"{common}{big[j]}[{f2amin}:{f2amax}]"
                            bus1 = f"{bus}_{w}"
                        else:
                            bus0 = f"{bus}_{w}"
                            bus1 = f"{common}{big[j]}[{a2fmin}:{a2fmax}]"
                        wfile.write(f"assign {bus0} = {bus1};\n")
                # for
            # for
        # for
    # with
    sys.stderr.write(f"Made {wfilename}\n")
    sys.stderr.write(f"Made {pfilename}\n")

    pv.endmodule()
# def

toplogic()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: syntax=on fileformat=unix autoindent hlsearch
