#!/nfs_home/dhow/bin/python3
#!/usr/bin/env python3
"""Verify array Verilog against routing graph"""

import sys
import re
import time
import math
import subprocess
from collections import defaultdict
import hashlib
import argparse

## pylint: disable-next=import-error,unused-import
from traceback_with_variables import activate_by_import, default_format

## part of traceback_with_variables
default_format.max_value_str_len = 10000

# general
g_false = False
g_true = True
g_time = 0              # used by elapsed()
g_write_all = False

# shared tables
g_chan2ax = { "CHANE": 0, "CHANW": 0, "CHANN": 1, "CHANS": 1, }
g_chan2dc = { "CHANE": 0, "CHANW": 1, "CHANN": 0, "CHANS": 1, }
g_bigseq = (
    "abcdefghijklmnopqrstuvwxyz" +
    "01234567890" +
    "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~" +
    "ZYXWVUTSRQPONMLKJIHGFEDCBA" )
g_crunch = { 'I': 'I', 'O': 'O', 'N': 'S', 'S': 'S', 'E': 'S', 'W': 'S', }

# SHA1 hash --> letter (both describe tiles)
g_hd2char = {}

# tables for routing graph processing

g_rr_idtype2dir = { "CHANXINC_DIR": "CHANE", "CHANXDEC_DIR": "CHANW",
                    "CHANYINC_DIR": "CHANN", "CHANYDEC_DIR": "CHANS", }
g_rr_keeptype = { 'CHANX': 0, 'CHANY': 0, 'IPIN': 1, 'OPIN': 1, }
g_rr_edgekeys = [ "sink_node", "src_node", ]
g_rr_nodekeys = [    "direction", "id", "type", "ptc",
                "xlow", "ylow", "xhigh", "yhigh", "segment_id", "side", ]
g_rr_pinkeys = [ 'type', 'ptc', ]
g_rr_lockeys = [ 'block_type_id', 'height_offset', 'width_offset', 'x', 'y', ]
g_rr_blockkeys = [ 'id', 'name', ]

# set when reading routing graph
(g_rr_xmax, g_rr_ymax) = (0, 0)
# (xl, yl, xh, yh, ttype, seglen) --> { nids }
g_rr_nets = defaultdict(set)
# nid --> (xl, yl, xh, yh, ptc, type, seglen)
g_rr_nid2info = {}
# { (src, snk) }
g_rr_edges = set()
# (bid, ptc) --> (ptype, pname)
g_rr_bidptc2typename = {}
# (x, y) --> bid
g_rr_xy2bid = {}
# bid --> bname
g_rr_bid2name = {}
# (x, y) --> snk --> { srcs }
g_rr_conns = defaultdict(lambda: defaultdict(set))
# nid --> bundle index
g_rr_nid2bidx = {}

# tables for base net processing

g_bd_dir2chanodd = {
        'right':    ("CHANE", 0),
        'left':     ("CHANW", 1),
        'top':      ("CHANN", 0),
        'bottom':   ("CHANS", 1),
}
# input or output? FIXME this should be read from a table
g_bd_pinbase2info = {
    'I00':      'i', 'I01':      'i', 'I02':      'i',
    'I10':      'i', 'I11':      'i', 'I12':      'i',
    'I20':      'i', 'I21':      'i', 'I22':      'i',
    'I30':      'i', 'I31':      'i', 'I32':      'i',
    'IS0':      'i', 'IS1':      'i', 'IS2':      'i',
    'f2a_i':    'i',
    'O0':       'o', 'O1':       'o', 'O2':       'o',
    'a2f_o':    'o',
}
# (xl, yl, xh, yh, chan, seglen) --> { nids }
g_bd_nids = defaultdict(set)
# { (src, snk) }
g_bd_edges = set()
# nid --> path OR (tile, xpin)
g_bd_nid2pkey = {}
# path OR (tile, xpin) --> nid
g_bd_path2nid = {}
# dnid --> { snids }
g_bd_snk2srcs = defaultdict(set)
# nid --> [xl, yl, xh, yh, ptc, chan/pin, seg/side]
g_bd_nid2info = {}
# nid --> bundle index
g_bd_nid2bidx = {}
# (x, y) --> snk --> set of srcs
g_bd_conns = defaultdict(lambda: defaultdict(set))

### STANDARD ROUTINES

def log(text):
    """write to stderr"""
    sys.stderr.write(text)
# def log

def out(text):
    """write to stdout"""
    sys.stdout.write(text)
    sys.stdout.flush()
# def out

def gzopenread(path):
    """handle various input file conventions"""
    if not path or path == "-": return sys.stdin
    if not path.endswith(".gz"):
        return open(path, encoding='ascii')
    return subprocess.Popen(
        f"gunzip -c {path}",
        shell=True, stdout=subprocess.PIPE, universal_newlines=True
    ).stdout
# def gzopenread

def gzopenwrite(path):
    """handle various output file conventions"""
    if not path or path == "-": return sys.stdout
    if not path.endswith(".gz"):
        return open(path, "w", encoding='ascii')
    return subprocess.Popen(
        f"gzip > {path}",
        shell=True, stdin=subprocess.PIPE, universal_newlines=True
    ).stdin
# def gzopenwrite

def dict_aup_ndn(k):
    """sort with alpha ascending and numeric descending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] = -int(r[i])
    return r
# def dict_aup_ndn

def dict_aup_nup(k):
    """sort with alpha ascending and numeric ascending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    return r
# def dict_aup_nup

def dict_aup_nuprev(k):
    """sort with alpha ascending and numeric ascending & reversed"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    for i in range(1, len(r)//2, 2):
        (r[i], r[-1-i]) = (r[-1-i], r[i])
    return r
# def dict_aup_nuprev

def elapsed():
    """return elapsed time"""
    global g_time
    thistime = time.time()
    if not g_time:
        g_time = thistime
        return "0:00:00"
    thistime -= g_time
    s = int(thistime % 60)
    thistime -= s
    m = int((thistime % 3600) / 60)
    thistime -= m * 60
    h = int(math.floor(thistime / 3600))
    return f"{h:01d}:{m:02d}:{s:02d}"
# def elapsed

def plural(n, notone="s", one=""):
    """return plural ending"""
    return one if n == 1 else notone
# def plural

def alltaps(seglen):
    """return all taps by length"""
    return "BQMPE" if seglen == 4 else "BE"
# def alltaps

def srcsnk2dxynames(    sxl0, syl0, sxh0, syh0, stype0, sseglen0, sidx0,
                        dxl0, dyl0, dxh0, dyh0, dtype0, dseglen0, didx0):
    """convert src & snk node ==> dest xy, dest mux, src mux"""

    (    sxl , syl , sxh , syh , stype , sseglen , sidx ,
         dxl , dyl , dxh , dyh , dtype , dseglen , didx ) = (
         sxl0, syl0, sxh0, syh0, stype0, sseglen0, sidx0,
         dxl0, dyl0, dxh0, dyh0, dtype0, dseglen0, didx0)

    assert stype == "OPIN" or isinstance(sseglen, int)
    assert dtype == "IPIN" or isinstance(dseglen, int)

    # determine coordinates containing the muxes
    match stype:
        case "CHANE":
            (smx, smy) = ( max(1, sxl - 1),        syl      )
        case "CHANN":
            (smx, smy) = (        sxl     , max(1, syl - 1) )
        case "CHANW":
            (smx, smy) = (        sxh     ,        syh      )
        case "CHANS":
            (smx, smy) = (        sxh     ,        syh      )
        case "OPIN":
            (smx, smy) = (        sxh     ,        syh      )
        case _:
            assert 0
    match dtype:
        case "CHANE":
            (dmx, dmy) = ( max(1, dxl - 1),        dyl      )
        case "CHANN":
            (dmx, dmy) = (        dxl     , max(1, dyl - 1) )
        case "CHANW":
            (dmx, dmy) = (        dxh     ,        dyh      )
        case "CHANS":
            (dmx, dmy) = (        dxh     ,        dyh      )
        case "IPIN":
            (dmx, dmy) = (        dxh     ,        dyh      )
        case _:
            assert 0
    assert smx == dmx or smy == dmy

    # adjust to full length
    # (possibly stick outside array)
    if stype != "OPIN":
        if g_chan2ax[stype] == 0: # CHANX East/West
            d = sseglen - (1 + sxh - sxl)
            assert d >= 0
            if d > 0:
                if sxl < 2:
                    sxl -= d
                else:
                    sxh += d
        else:                   # CHANY North/South
            d = sseglen - (1 + syh - syl)
            assert d >= 0
            if d > 0:
                if syl < 2:
                    syl -= d
                else:
                    syh += d
    if dtype != "IPIN":
        if g_chan2ax[dtype] == 0: # CHANX East/West
            d = dseglen - (1 + dxh - dxl)
            assert d >= 0
            if d > 0:
                if dxl < 2:
                    dxl -= d
                else:
                    dxh += d
        else:                   # CHANY North/South
            d = dseglen - (1 + dyh - dyl)
            assert d >= 0
            if d > 0:
                if dyl < 2:
                    dyl -= d
                else:
                    dyh += d

    # figure out tap names
    sftaps = alltaps(sseglen) ; dftaps = alltaps(dseglen)
    srtaps = sftaps[::-1] ; drtaps = dftaps[::-1]
    if stype != "OPIN" and dtype != "IPIN" and g_chan2ax[stype] == g_chan2ax[dtype]:
        # same axis (stitch or reversal)
        match (g_chan2dc[stype], g_chan2dc[dtype]):
            case (0, 0):
                # same axis, INC_DIR --> INC_DIR
                st = dmx - sxl + dmy - syl + 1
                stap = sftaps[st]
                dt = 0
                dtap = dftaps[dt]
            case (1, 1):
                # same axis, DEC_DIR --> DEC_DIR
                st = sxh - dmx + syh - dmy
                stap = sftaps[st]
                dt = 0
                dtap = dftaps[dt]
            case (0, 1):
                # same axis, INC_DIR --> DEC_DIR  reversal
                st = sxh - dmx + syh - dmy
                stap = srtaps[st]
                dt = dxh - dmx + dyh - dmy
                dtap = dftaps[dt]
            case (1, 0):
                # same axis, DEC_DIR --> INC_DIR  reversal
                st = sxh - dmx + syh - dmy
                stap = sftaps[st]
                dt = dmx - dxl + dmy - dyl + 1
                dtap = dftaps[dt]
            case _:
                assert 0
    else:
        # 90-degree turns
        match stype:
            case "CHANE":
                st = sxh - dmx
                stap = srtaps[st]
            case "CHANN":
                st = syh - dmy
                stap = srtaps[st]
            case "CHANW":
                st = sxh - dmx
                stap = sftaps[st]
            case "CHANS":
                st = syh - dmy
                stap = sftaps[st]
            case "OPIN":
                st = 0
            case _:
                assert 0
        match dtype:
            case "CHANE":
                dt = dxh - dmx
                dtap = drtaps[dt]
            case "CHANN":
                dt = dyh - dmy
                dtap = drtaps[dt]
            case "CHANW":
                dt = dxh - dmx
                dtap = dftaps[dt]
            case "CHANS":
                dt = dyh - dmy
                dtap = dftaps[dt]
            case "IPIN":
                dt = 0
            case _:
                assert 0
    assert st >= 0 and dt >= 0

    # FIXME make handling of sidx/didx more regular and/or table-driven
    if stype == "OPIN":
        g = re.search(r'\[(\d+)\]$', sidx)
        assert g
        tsrc = f"O{g.group(1)}"
    else:
        tsrc = f"{stype[4]}{sseglen}{stap}{sidx}"
    if dtype == "IPIN":
        tsnk = f"I{didx}"
    else:
        tsnk = f"{dtype[4]}{dseglen}{dtap}{didx}"

    return (dmx, dmy, tsnk, tsrc)
# def srcsnk2dxynames

def dumptiles(prefix, letter, xconns):
    """dump out info about tiles"""

    # now process the tiles individually and make a map
    start = len(g_hd2char)
    seq = "ABCDEFGHIJKLMNOPQRSTUVWXYZ" if letter == "A" else g_bigseq
    xy2ch = {}
    written = 0
    ecount = defaultdict(int)
    for xy in sorted(xconns):
        conns = xconns[xy]
        lines = []
        pathash = hashlib.sha1()
        for tsnk in sorted(conns, key=dict_aup_nup):
            csnk = g_crunch[tsnk[0]]
            tsrcs = sorted(conns[tsnk], key=dict_aup_nup)
            for tsrc in tsrcs:
                ecount[ g_crunch[tsrc[0]] + csnk ] += 1
            line = ' '.join([tsnk] + tsrcs)
            lines.append(line)
            pathash.update(line.encode(encoding='ascii'))
        # for
        hd = pathash.hexdigest()
        if hd in g_hd2char:
            xy2ch[xy] = g_hd2char[hd]
            unique = 0
        else:
            xy2ch[xy] = g_hd2char[hd] = seq[len(g_hd2char) - start]
            unique = 1
        if unique or g_write_all:
            filename = f"{prefix}_{xy[0]}_{xy[1]}.mux"
            with open(filename, "w", encoding='ascii') as ofile:
                if unique: log(f"{elapsed()} Writing {filename}\n")
                for line in lines:
                    ofile.write(line + "\n")
                # for
            # with
        if unique:
            written += 1
    # for
    (cos, css, csi) = (ecount['OS'], ecount['SS'], ecount['SI'])
    log(f"\tEdges: {cos:,d} OPIN-->CHAN, {css:,d} CHAN-->CHAN, {csi:,d} CHAN-->IPIN\n")
    lines = []
    for xy in sorted(xy2ch):
        while xy[1] >= len(lines):
            lines.append("")
        lines[xy[1]] += xy2ch[xy]
    # for
    hz = ''.join(str((i + 1) % 10) for i in range(g_rr_xmax))
    log(f"\t  {hz}\n")
    for y in range(g_rr_ymax, 0, -1):
        log(f"\t{y % 10} {lines[y]} {y % 10}\n")
    log(f"\t  {hz}\n")
    log(f"\t{written:,d} mux file{plural(written)} written\n")
# def dumptiles

# FIXME rewrite to use stamper's .pinmap file
def rrxyptc2name(dxl, dyl, dptc):
    """compute input pin name"""

    bid = g_rr_xy2bid[(dxl, dyl)]
    bname = g_rr_bid2name[bid]
    (_, pname) = g_rr_bidptc2typename[(bid, dptc)]

    if 'io_' in bname:
        g = re.search(r'f2a_i\[(\d+)\]$', pname)
        assert g
        ipin = int(g.group(1))
        if 24 <= ipin < 72:
            ipin -= 24
            return f"{ipin // 6}{ipin % 6}"
        if 18 <= ipin < 24:
            ipin -= 18
            return f"S{ipin}"
        assert 0

    # the ignored digit is yo
    g = re.search(r'I([S\d])\d\[(\d+)\]$', pname)
    assert g
    if g.group(1) == "S":
        return f"S{g.group(2)}"
    ipin = int(g.group(1)) * 12 + int(g.group(2))

    # the stamper makes this transformation
    ipin = ((ipin % 12) << 2) + (ipin // 12)
    return f"{ipin // 6}{ipin % 6}"

# def rrxyptc2name

def sid2len(segment_id):
    """translate segment id to logical length in grids"""
    # FIXME build a table for use here
    if segment_id == "0":
        return 1
    if segment_id == "1":
        return 4
    assert 0
# def sid2len

def read_rr_graph(filename):
    """read routing graph"""

    # working values
    incount = defaultdict(int)
    outcount = defaultdict(int)
    vals = {}
    badedge = 0

    bundles = defaultdict(int)
    with gzopenread(filename) as ifile:
        log(f"{elapsed()} Reading {filename}\n")
        for line in ifile:
            ff = line.split()
            if not ff: continue

            # extract any attributes
            vals.update(dict( re.findall(r'\s(\w+)="([^"]*)"', line) ))

            # check for edge first since most common
            # <edge sink_node="113293" src_node="98914" switch_id="3"/>
            if ff[0] == "<edge":
                (sink_node, src_node) = [
                    vals.get(k, None) for k in g_rr_edgekeys ]
                vals.clear()
                # process edge
                (sink_node, src_node) = (int(sink_node), int(src_node))
                if sink_node not in g_rr_nid2info: continue
                if src_node not in g_rr_nid2info: continue
                g_rr_edges.add((src_node, sink_node))
                outcount[src_node] += 1
                incount[sink_node] += 1

                # all nodes created, so enter edge into tiles
                (sxl, syl, sxh, syh, sptc, stype, sseglen) = g_rr_nid2info[src_node] 
                (dxl, dyl, dxh, dyh, dptc, dtype, dseglen) = g_rr_nid2info[sink_node] 
                if not stype.startswith("CHAN") and not dtype.startswith("CHAN"): continue

                if stype.startswith("CHAN"):
                    sidx = g_rr_nid2bidx[src_node]
                else:
                    bid = g_rr_xy2bid[(sxl, syl)]
                    (_, sidx) = g_rr_bidptc2typename[(bid, sptc)]
                if dtype.startswith("CHAN"):
                    didx = g_rr_nid2bidx[sink_node]
                else:
                    didx = rrxyptc2name(dxl, dyl, dptc)
                (dmx, dmy, tsnk, tsrc) = srcsnk2dxynames(
                        sxl, syl, sxh, syh, stype, sseglen, sidx,
                        dxl, dyl, dxh, dyh, dtype, dseglen, didx)
                if dmx < 0:
                    badedge += 1
                    continue

                # xy --> snk --> set of srcs
                g_rr_conns[(dmx, dmy)][tsnk].add(tsrc)
                continue
            # if

            # <node capacity="1" id="1443" type="OPIN">
            #   <loc layer="0" ptc="3" side="LEFT" xhigh="1" xlow="1" yhigh="1" ylow="1"/>
            #   <timing C="0" R="0"/>
            # </node>
            # <node capacity="1" direction="INC_DIR" id="100112" type="CHANX">
            #   <loc layer="0" ptc="0" xhigh="1" xlow="1" yhigh="0" ylow="0"/>
            #   <timing C="0" R="9"/>
            #   <segment segment_id="0"/>
            # </node>
            if ff[-1] == '</node>':
                (tdir, nid, ttype, ptc, xl, yl, xh, yh, segment_id, side) = [
                    vals.get(k, None) for k in g_rr_nodekeys ]
                vals.clear()
                # process node
                if ttype not in g_rr_keeptype: continue
                (nid, xl, yl, xh, yh, ptc) = (
                    int(nid), int(xl), int(yl), int(xh), int(yh), int(ptc))
                if g_rr_keeptype[ttype]:
                    # IPIN/OPIN
                    g_rr_nid2info[nid] = [xl, yl, xh, yh, ptc, ttype, side]
                else:
                    # CHANX/CHANY
                    ttype = g_rr_idtype2dir[ttype + tdir]
                    seglen = sid2len(segment_id)
                    g_rr_nid2info[nid] = [xl, yl, xh, yh, ptc, ttype, seglen]
                    key = (xl, yl, xh, yh, ttype, seglen)
                    g_rr_nets[key].add(nid)

                    # deduce array size
                    global g_rr_xmax, g_rr_ymax
                    if xh > g_rr_xmax: g_rr_xmax = xh
                    if yh > g_rr_ymax: g_rr_ymax = yh

                continue
            # if

            # <pin_class type="OUTPUT">
            #   <pin ptc="2">io_top[0].a2f_o[0]</pin>
            # </pin_class>
            if ff[0] == "<pin":
                (ptype, ptc) = [ vals.get(k, None) for k in g_rr_pinkeys ]
                vals.clear()
                g = re.search(r'''
                    ^\s*
                    <pin \s+ [^<>]* >
                    \w+ (?: \[ (\d+) \] )? [.] (\w+) \[ (\d+) \]
                    </pin> \s*$
                ''', line, re.VERBOSE)
                assert g
                b = int(g.group(1) or "0") + int(g.group(3))
                pname = f"{g.group(2)}[{b}]"
                g_rr_bidptc2typename[(int(bid), int(ptc))] = (ptype, pname)
                continue
            # if

            # <grid_loc block_type_id="0" height_offset="0" layer="0" width_offset="0"
            #           x="0" y="5"/>
            if ff[0] == "<grid_loc":
                # we don't need the offsets
                (bid, _, _, x, y) = [ vals.get(k, None) for k in g_rr_lockeys ]
                vals.clear()
                g_rr_xy2bid[(int(x), int(y))] = int(bid)
                continue
            # if

            # <block_type height="1" id="1" name="io_top" width="1">
            if ff[0] == "<block_type":
                (bid, bname) = [ vals.get(k, None) for k in g_rr_blockkeys ]
                vals.clear()
                g_rr_bid2name[int(bid)] = bname
                continue
            # if

            # </rr_nodes>
            if ff[-1] == '</rr_nodes>' :
                # compute ALL bundle indices
                for key, nids in g_rr_nets.items():
                    bundles[len(nids)] += 1
                    nids = sorted(nids)
                    for i, nid in enumerate(nids, start=0):
                        g_rr_nid2bidx[nid] = i
                continue
            # if

        # for
    # with

    valid = set()
    for nid, c in incount.items():
        if c < 2: continue
        if nid not in outcount: continue
        if not g_rr_nid2info[nid][5].startswith("CHAN"): continue
        valid.add(nid)
    # for

    log(f"\tRead {len(g_rr_nid2info):,d} nodes and {len(g_rr_edges):,d} edges\n")
    # pretty print
    bundles = { sz: bundles[sz] for sz in sorted(bundles) }
    log(f"\tAssigned bundle indices: counts={bundles}\n")

    allcounts = defaultdict(int)
    muxcounts = defaultdict(int)
    for key, nids in g_rr_nets.items():
        allcounts[len(nids)] += 1
        nids = [nid for nid in nids if nid in valid]
        if nids: muxcounts[len(nids)] += 1
    alltotal = 0
    for c, r in allcounts.items():
        alltotal += c * r
    muxtotal = 0
    for c, r in muxcounts.items():
        muxtotal += c * r
    log(f"\t{alltotal:,d} CHANs: counts={dict(allcounts)}\n")
    log(f"\t{muxtotal:,d} muxes: counts={dict(muxcounts)}\n")
    if badedge:
        log(f"\t{badedge:,d} bad edges dropped\n")

    dumptiles("rr", "A", g_rr_conns)
# def read_rr_graph

def path2xy(path):
    """pull x,y coordinates out of a path"""
    g = re.match(r'tile_(\d+)__(\d+)_[.][lrts]b(?:x(\d+))?(?:y(\d+))?[.]', path)
    assert g
    (x, y, xo, yo) = g.groups()
    (x, y) = (int(x), int(y))
    if xo is not None: x += int(xo)
    if yo is not None: y += int(yo)
    return (x, y)
# def path2xy

# correspondence:
#
# CHAN node
# mux output pin <==> mux out pin instance name <==> netname <==> wire
#
# edge
# mux output pin --> mux input pin
# mux out instance --> mux in instance

def proc_base_net(cl, pins, xpin, netname):
    """save driving output for each pin"""

    if not pins or cl not in (0, 1) or re.match(r'c__abut_\d+__\d+_$', netname):
        pins.clear()
        return
    # PROCESS: cl=0: OPIN-->SB
    # PROCESS: cl=1: SB-->SB or : SB-->CB
    # BUT SKIP: cl=2: CB-->LR

    # PART 1: ensure all INSTANCES have nids
    # instances are 1:1 with output pins and thus CHANX/Y
    nid = -1
    opini = -1
    for i, ff in enumerate(pins, start=0):
        (_, path, _, _, pdir) = ff
        if pdir == "o":
            opini = i
            if cl == 0:
                pkey = (path.split('.')[0], xpin)
            else:
                pkey = path
            if pkey not in g_bd_path2nid:
                g_bd_path2nid[pkey] = len(g_bd_path2nid)
                nid = g_bd_path2nid[pkey]
                g_bd_nid2pkey[nid] = pkey
            else:
                nid = g_bd_path2nid[pkey]
        else:
            if path not in g_bd_path2nid:
                g_bd_path2nid[path] = len(g_bd_path2nid)
    # for
    assert nid >= 0 and opini >= 0

    # PART 2: enter edges
    # also enter info for CB nodes
    for ff in pins:
        (_, path, _, _, pdir) = ff
        if pdir == "o":
            if cl == 0:
                pkey = (path.split('.')[0], xpin)
            else:
                pkey = path
            inid = g_bd_path2nid[pkey]
            assert inid == nid
        else:
            inid = g_bd_path2nid[path]
            assert inid != nid
            g_bd_snk2srcs[inid].add(nid)

            # insert CB input in node info
            if '_ipin_' not in path: continue
            (xl, yl) = path2xy(path)
            g = re.search(r'mux_(right|left|top|bottom)_ipin_(\d+)$', path)
            assert g
            side = g.group(1).upper()
            ptc = int(g.group(2))
            info = [xl, yl, xl, yl, ptc, "IPIN", side]
            if inid in g_bd_nid2info:
                assert g_bd_nid2info[inid] == info
            else:
                g_bd_nid2info[inid] = info
    # for

    # PART 3 : convert this !!!net!!! into a CHANX/Y:
    # collect coordinates, and then adjust into those used by VPR
    match cl:
        case 0:
            # source OPIN, destination SB (or CB someday)
            chan = "OPIN"
            (xl, yl) = path2xy(pins[0][1])  # any pin will do
            (xh, yh) = (xl, yl)
            (pinbase, pinidx) = re.sub(r'^(\w+)\[(\d+)\]$', r'\1 \2', xpin).split()
            pdir = g_bd_pinbase2info[pinbase]
            seglen = "UNKNOWN"  # doesn't matter
            ptc = int(pinidx)
        case 2:
            # source CB output, destination LR inputs
            #
            # *** UNUSED ***
            #
            chan = "LR"
            (xl, yl, xh, yh) = (0, 0, 0, 0)
            seglen = "UNKNOWN"  # doesn't matter
            ptc = 0
        case 1:
            # source SB, destination SB or CB
            seglen = -1 ; chan = "" ; ptc = -1
            ix = set() ; iy = set()
            for ff in pins:
                #PIN  path  mod  pin  pdir
                (  _, path, mod,   _, pdir) = ff
                g1 = re.search(r'''
            tile_(\d+)__(\d+)_[.]                               # 1=x 2=y
            [rst]b(?:x(\d+))?(?:y(\d+))?[.]                     # 3=xo 4=yo
            mux_(right|left|top|bottom)_(ipin|track)_(\d+)$     # 5=side 6=type 7=number
                ''', path, re.VERBOSE)
                assert g1
                (x, y, xo, yo, side, _, number) = g1.groups()
                (x, y) = (  int(x) + (0 if xo is None else int(xo)),
                            int(y) + (0 if yo is None else int(yo)) )
                if pdir == "o": 
                    g2 = re.match(r'mux.*_L(\d+)SB_size\d+$', mod)
                    assert g2
                    (chan, odd) = g_bd_dir2chanodd[side]
                    ptc = int(number)
                    assert odd == 1 & ptc
                    seglen = int(g2.group(1))
                    (ox, oy) = (x, y)
                else:
                    ix.add(x)
                    iy.add(y)
            # for
            assert seglen > 0 and ptc >= 0
            assert chan != "", f"pins={pins}"
            assert ix and iy, f"ix={ix} iy={iy} pins={pins}"
            #
            # WARNING
            #
            # math/details to convert to VPR coordinates are very delicate
            #
            # figure out xl, yl, xh, yh
            (ixmin, ixmax) = (min(ix), max(ix))
            (iymin, iymax) = (min(iy), max(iy))
            match chan:
                case "CHANE":
                    # INC_DIR logic: CAREFUL
                    assert oy == iymin == iymax
                    if 1 < ox:
                        xl = ox + 1
                        xh = min(ox + seglen, g_rr_xmax)
                    elif ox + seglen == ixmax:
                        xl = 2
                        xh = ixmax
                    else:
                        xl = 1
                        xh = ixmax
                    assert 0 <= xh - xl <= seglen - 1
                    yl = yh = oy
                case "CHANN":
                    # INC_DIR logic: CAREFUL
                    assert ox == ixmin == ixmax
                    if 1 < oy:
                        yl = oy + 1
                        yh = min(oy + seglen, g_rr_ymax)
                    elif oy + seglen == iymax:
                        yl = 2
                        yh = iymax
                    else:
                        yl = 1
                        yh = iymax
                    assert 0 <= yh - yl <= seglen - 1
                    xl = xh = ox
                case "CHANW":
                    # DEC_DIR logic: CAREFUL
                    assert oy == iymin == iymax
                    xh = ox
                    if ox < g_rr_xmax:
                        xl = max(1 + ox - seglen, 1)
                    else:
                        xl = 1 + ixmin
                    assert 0 <= xh - xl <= seglen - 1
                    yl = yh = oy
                case "CHANS":
                    # DEC_DIR logic: CAREFUL
                    assert ox == ixmin == ixmax
                    yh = oy
                    if oy < g_rr_ymax:
                        yl = max(1 + oy - seglen, 1)
                    else:
                        yl = 1 + iymin
                    assert 0 <= yh - yl <= seglen - 1
                    xl = xh = ox
                case _:
                    assert 0
            # match
        # case
    # match

    # PART 4 : enter this node's derived info
    assert nid not in g_bd_nid2info
    g_bd_nid2info[nid] = [xl, yl, xh, yh, ptc, chan, seglen]
    # enter CHANs into groups to later assign ptc's
    if cl == 1:
        key = (xl, yl, xh, yh, chan, seglen)
        g_bd_nids[key].add(nid)

    pins.clear()
# def proc_base_net

# function converting x/y/ipin/side to input name.
# remember, incoming ipin is track # from inst tail mux_SIDE_ipin_TRACK.
# FIXME should read from a table, but it doesn't exist.
# these ipin numbers are assigned by OpenFPGA,
# but it never writes out a summary.
# until improved, don't change pin orders in vpr.xml.
def xypinside2name(x, y, ipin, side):
    """convert netlist features to input name"""
    # IO?
    if x in (1, g_rr_xmax) or y in (1, g_rr_ymax):
        # specials incorrectly not split between RIGHT/TOP
        if   side == "TOP" and 0 <= ipin < 6:
            return f"S{ipin}"
        if   side == "TOP" and 6 <= ipin < 30:
            slot = ipin - 6 + 24
        elif side == "RIGHT" and 0 <= ipin < 24:
            slot = ipin - 0 + 48
        else:
            assert 0
        slot -= 24
        return f"{slot // 6}{slot % 6}"

    # normal pins
    if ipin < 24:
        if side == "RIGHT": ipin += 24
        assert 0 <= ipin < 48
        # the stamper makes this transformation
        ipin = ((ipin % 12) << 2) + (ipin // 12)
        return f"{ipin // 6}{ipin % 6}"
    # special pins
    if side == "RIGHT": ipin += 3
    assert 24 <= ipin < 30
    return f"S{ipin - 24}"
# def xypinside2name

def read_base_nets(filename):
    """read base nets"""
    netname = ""
    xpin = ""
    pins = []
    cl = 0
    with gzopenread(filename) as ifile:
        log(f"{elapsed()} Reading {filename}\n")
        for line in ifile:
            ff = line.split()
            if not ff: continue

            if ff[0] == "NET":
                # process previous batch of pins
                proc_base_net(cl, pins, xpin, netname)
                netname = ff[1]
                # channel wires
                if netname.startswith("chan"):
                    # CHAN: SB-->SB, SB-->CB
                    cl = 1
                    xpin = ""
                elif re.match(r'tile_\d+__\d+_[.]pin_', netname):
                    nn = netname.split('.')
                    assert len(nn) == 2 and nn[1].startswith("pin_")
                    xpin = nn[1][4:]
                    (pinbase, _) = re.sub(r'^pin_(.+)\[(\d+)\]$', r'\1 \2', nn[1]). split()
                    assert pinbase in g_bd_pinbase2info
                    pinbasedir = g_bd_pinbase2info[pinbase]
                    if pinbasedir == "o":
                        # OPIN: OPIN-->SB
                        cl = 0
                    else:
                        # IPIN CB-->LR
                        cl = 2
                        #print(f"BASENET found CB output: netname={netname} xpin={xpin}")
                # channel wires stuck inside multi-grid tile
                elif re.match(r'tile_\d+__\d+_[.]sb(?:x\d+)?(?:y\d+)?__chan', netname):
                    # CHAN
                    cl = 1
                    xpin = ""
                elif  ( re.match(r'tile_\d+__\d+_[.]lb[.]', netname) or
                        re.match(
                    r'(right|top|left|bottom)_\d+_(a2f|f2a|clk_out)\[\d+\]$', netname) or
                        re.match(
                    r'tile_\d+__\d+_[.](nc_f2a|f2a|nc_clk|clk_out)\[\d+\]$', netname) or
                        re.match(r'[01]$', netname) or
                        re.match(r'clk\[\d+\]$', netname) ):
                    # something inside a tile or global not routed by VPR
                    cl = 3
                    xpin = ""
                # hard (nonroutable) inter-adder carry connections like: c__abut_9__7_
                elif re.match(r'c__abut_\d+__\d+_$', netname):
                    pass
                else:
                    assert 0, f"netname={netname}"
                continue

            if ff[0] != "PIN": continue
            assert len(ff) == 5, f"line={line}\nff={ff}"

            # pin tuple: 0:PIN 1:path 2:mod 3:pin 4:dir
            # CONSISTENT: use mux not mem everywhere
            ff[1] = re.sub(r'^(.*[.])mem([^.]+)$' , r'\1mux\2', ff[1])

            pins.append(tuple(ff))
        # for
    # with
    # process last batch of pins
    proc_base_net(cl, pins, xpin, netname)

    # fill in g_bd_nid2bidx: compute ALL bundle indices
    bundles = defaultdict(int)
    for _, nids in g_bd_nids.items():
        bundles[len(nids)] += 1
        nids = sorted(nids, key=lambda nid: g_bd_nid2info[nid][4])
        for i, nid in enumerate(nids, start=0):
            g_bd_nid2bidx[nid] = i

    outcount = defaultdict(int)
    incount = defaultdict(int)
    badedge = 0
    # all nodes created, so enter edges into tiles
    for sink_node, srcs in g_bd_snk2srcs.items():
        if sink_node not in g_bd_nid2info: continue

        for src_node in srcs:
            assert src_node in g_bd_nid2info

            # process edge
            g_bd_edges.add((src_node, sink_node))
            outcount[src_node] += 1
            incount[sink_node] += 1

            # all nodes created, so enter edge into tiles
            #                                        
            (sxl, syl, sxh, syh,    _, stype, sseglen) = g_bd_nid2info[src_node] 
            (dxl, dyl, dxh, dyh, dptc, dtype, dseglen) = g_bd_nid2info[sink_node] 
            if not stype.startswith("CHAN") and not dtype.startswith("CHAN"): continue

            if stype.startswith("CHAN"):
                sidx = g_bd_nid2bidx[src_node]
            else:
                (_, sidx) = g_bd_nid2pkey[src_node]
            if dtype.startswith("CHAN"):
                didx = g_bd_nid2bidx[sink_node]
            else:
                assert dtype == "IPIN"
                didx = xypinside2name(dxl, dyl, dptc, dseglen)
            (dmx, dmy, tsnk, tsrc) = srcsnk2dxynames(
                    sxl, syl, sxh, syh, stype, sseglen, sidx,
                    dxl, dyl, dxh, dyh, dtype, dseglen, didx)
            if dmx < 0:
                badedge += 1
                continue

            # xy --> snk --> set of srcs
            g_bd_conns[(dmx, dmy)][tsnk].add(tsrc)
        # for
    # for

    log(f"\tMade {len(g_bd_nid2info):,d} nodes and {len(g_bd_edges):,d} edges\n")
    # pretty print
    bundles = { sz: bundles[sz] for sz in sorted(bundles) }
    log(f"\tAssigned bundle indices: counts={bundles}\n")

    counts = defaultdict(int)
    for _, bids in g_bd_nids.items():
        counts[len(bids)] += 1
    counts = { c: counts[c] for c in sorted(counts) }
    total = 0
    for c, r in counts.items():
        total += c * r
    log(f"\t{total:,d} muxes: counts={dict(counts)}\n")

    if badedge:
        log(f"\t{badedge:,d} bad edges dropped\n")

    # print tile map
    dumptiles("bd", "a", g_bd_conns)

# def read_base_nets

def main():
    """run it all from top level"""
    parser = argparse.ArgumentParser(
        description="Verify array Verilog against routing graph",
        epilog="Files processed in order listed and any ending .gz are compressed")

    # --read_rr_graph rr_graph_out.stamped.xml
    parser.add_argument('--read_rr_graph',
                        required=True,
                        help="Read .xml routing graph")

    # --read_base_nets k6n8_vpr.xml
    parser.add_argument('--read_base_nets',
                        required=True,
                        help="Read base die netlist")

    # --write_all_tiles
    parser.add_argument('--write_all_tiles',
                        action='store_true',
                        help="Write even non-unique repeated tiles")

    # process and show args
    args = parser.parse_args()
    global g_write_all
    g_write_all = args.write_all_tiles
    log(f"\nInvocation: {sys.argv[0]}\n")
    ml = 0
    for k, v in vars(args).items():
        if v is None: continue
        l = len(k)
        ml = max(ml, l)
    ml += 1
    for k, v in vars(args).items():
        if v is None: continue
        l = len(k)
        log(f"--{k:<{ml}s}{v}\n")
    log( "\n")

    # read the routing graph
    read_rr_graph(args.read_rr_graph)

    # read the basedie nets 
    read_base_nets(args.read_base_nets)

# def main

main()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
