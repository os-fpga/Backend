#!/usr/bin/env python3
#!/usr/bin/python3
"""read in vpr.xml, modify muxes, write it out again"""

import sys
import os
import re
import time
import math
import subprocess
from collections import defaultdict
import hashlib

## pylint: disable-next=import-error,unused-import
#from traceback_with_variables import activate_by_import, default_format

#import json
import uf

## part of traceback_with_variables
#default_format.max_value_str_len = 10000

g_bugs              = False  # generate some bugs to match OpenFPGA
g_chcnt             = 0
g_swid2swname       = {}    # swid ==> swname
g_sgid2sgname       = {}    # sgid ==> sgname
g_sgid2ptcs         = {}    # id ==> [min_ptc, max_ptc]
g_ptc2sgidname      = {}    # ptc ==> (sgid, sgidname)
g_bid2info          = {}    # id ==> { name: , width: , height: , pid2info: {}, n2info: {} }
g_block2bid         = {}    # name ==> id
g_xy2bidwh          = {}    # (x, y) ==> (bid, wo, ho)
g_nid2info          = {}    # nid ==> [xl, yl, xh, yh, p, pin/chan, sgid]
# sgid: pin:side src/snk:-1 chan:sgid
# ONLY for non-CHAN*
g_xyt2nid           = {}    # (xl, yl, ptc) ==> nid
# ONLY for CHAN*
g_xydl2tn           = defaultdict(list)   # (E4, x1, y1, x2, y2) ==> [(ptc, id), ...]
# routeport comes from pattern.mux e.g. I75 I30 IS5 O23
# blockport comes from block_type declaration e.g. io_top[48].f2a_i[0] clb.O0[23] clb.IS0[2]
g_bidport2nn        = {}    # (bid, wo, ho, routeport) ==> (ptc, blockport)
g_edges             = []
g_time              = 0
g_false             = False
g_true              = True
g_base              = ""    # base name for rgraph file
g_routablepins      = {}    # (bid, ptc) --> (wo, ho)
g_routablesides     = defaultdict(set)  # (bid, ptc) --> set(side)

g_sgid2len          = {}    # sgid ==> length
g_len2sgid          = {}    # len# ==> sg id
g_inp2swid          = 0     # sw id for CB mux
g_swid2len          = {}
g_len2swid          = {}

g_xcoords           = []
g_ycoords           = []

g_clb_id            = None  # id for CLB

g_pattern_inputs    = []    # list of pattern inputs in proper order
g_pattern_outputs   = []    # list of pattern outputs in proper order

g_droplist          = []
g_tap2dist          = {
    # S0s staying within tile
    (0, 'B'): 0,
    (0, 'E'): 0,
    # logical length 1
    (1, 'B'): 0,
    #1, 'M'): 0,    # should never happen
    (1, 'E'): 1,
    # logical length 4
    (4, 'B'): 0,
    (4, 'Q'): 1,
    (4, 'M'): 2,
    (4, 'P'): 3,
    (4, 'E'): 4,
}
g_found             = set()
g_notfound          = set()
g_nid2elt           = {}
g_grid_bbox         = []
g_dumps             = False

g_muxes             = [
    # middle rows
    ['clb',      [], ], # 0: middle columns
    ['left',     [], ], # 1: left IOTiles
    ['right',    [], ], # 2: right IOTiles
    # bottom rows
    ['bottom',   [], ], # 3: bottom IOTiles
    ['blcorner', [], ], # 4: bottom left IOTiles
    ['brcorner', [], ], # 5: bottom right IOTiles
    # top rows
    ['top',      [], ], # 6: top IOTiles
    ['tlcorner', [], ], # 7: top left IOTiles
    ['trcorner', [], ], # 8: top right IOTiles
    # other
    ['other',    [], ], # 9: BRAM and DSP
]

g_log = sys.stderr

g_twisted = 0           #  2 or 0: are tracks twisted or not?
g_undriven_ok = False
g_dangling_ok = False
g_drop_iso_nodes = False
g_commercial = False

g_wn = defaultdict(set)

# for tile pattern hashing
g_hash2source = defaultdict(set)    # hash --> file

g_add_layer = True

g_nloop = 0
g_pred2succ = {}
g_succ2pred = {}
g_xyw2succ  = {}

### some standard routines

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
#def gzopenread

def gzopenwrite(path):
    """handle various output file conventions"""
    if not path or path == "-": return sys.stdout
    if not path.endswith(".gz"):
        return open(path, "w", encoding='ascii')
    return subprocess.Popen(
        f"gzip > {path}",
        shell=True, stdin=subprocess.PIPE, universal_newlines=True
    ).stdin
#def gzopenwrite

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
        return "00:00:00"
    thistime -= g_time
    s = int(thistime % 60)
    thistime -= s
    m = int((thistime % 3600) / 60)
    thistime -= m * 60
    h = int(math.floor(thistime / 3600))
    return f"{h:02d}:{m:02d}:{s:02d}"
# def elapsed

###
### read_muxes called from toplogic
###

def read_muxes():
    """read the mux files to stamp"""
    # first mux pattern's I/S/O saved here for comparison
    inpn1 = set()
    inps1 = set()
    out1 = set()
    for k, pair in enumerate(g_muxes, start=0):
        # non-firs mux pattern's I/S/O saved here
        inpn2 = set()
        inps2 = set()
        out2 = set()
        # get mux type and clear out mux list
        (t, m) = pair
        m.clear()
        # where are we saving I/S/O?
        i = inpn2 if k else inpn1
        s = inps2 if k else inps1
        o = out2  if k else out1
        # read the mux file
        muxfile = f"{t}.mux"
        phash = hashlib.sha1()
        with open(muxfile, encoding='ascii') as mfile:
            log(f"{elapsed()} READING {muxfile}\n")
            for line in mfile:
                phash.update(line.encode())
                line = re.sub(r'#.*', r'', line)
                ff = [t for t in line.split() if not t[0].isdigit()]
                if len(ff) < 2: continue
                # record the entire mux
                m.append(ff)

                # save I/S/O, ignore others (wires)
                for f in ff:
                    if f[0] == "O":
                        o.add(f)
                        continue
                    if f[0] != "I": continue
                    if f[1] == "S":
                        s.add(f)
                    else:
                        i.add(f)
                # for f
            # for line
        # with
        hd = phash.hexdigest()
        g_hash2source[hd].add(muxfile)

        if not k: continue
        # compare inputs just collected against first collected
        i = inpn1.symmetric_difference(inpn2)
        if i:
            log(f"Warning: there were normal inputs not in all mux patterns: {i}\n")
        s = inps1.symmetric_difference(inps2)
        if s:
            log(f"Warning: there were special inputs not in all mux patterns: {s}\n")
        o = out1.symmetric_difference(out2)
        if o:
            log(f"Warning: there were outputs not in all mux patterns: {o}\n")
        assert not i and not s and not o

    # for k, pair

    g_pattern_inputs.clear()
    g_pattern_inputs.extend(    sorted(inpn1, key=dict_aup_nup) +
                                sorted(inps1, key=dict_aup_nup))
    g_pattern_outputs.clear()
    g_pattern_outputs.extend(   sorted(out1,  key=dict_aup_nup))
    i = len(g_pattern_inputs)
    o = len(g_pattern_outputs)
    log(f" - Found {i}+{o} routing pattern input+output pins\n")
# def read_muxes

###
### START PYX PARSER
###

def unescape(s):
    """handle pyx escapes"""
    return s.replace(r'\t','\t').replace(r'\\','\\')

def pyx_nop(*_):
    """ignore ?"""
    #pass

def pyx_open(stack, line):
    """handle ("""
    addme = [ line[1:-1], {} ]
    stack[-1].append(addme)
    stack.append(addme)

def pyx_attr_buggy(stack, line):
    """handle A"""
    n, v = line[1:].split(None, 1)
    v = unescape(v)[:-1]
    if re.match(r'\d+$', v):
        v = int(v)
    elif re.match(r'\d*[.]?\d*(e-?\d+)?$', v) and re.match(r'[.]?\d', v):
        v = float(v)
    stack[-1][1][n] = v

def pyx_attr(stack, line):
    """handle A"""
    # must handle 'Aname ' properly (name="" in XML)
    n, v = line[1:-1].split(" ", 1)
    v = unescape(v)
    # NB: these conversionms can be dangerous, if
    # e.g. we want to retain the width of "0000" and not reduce it to 0.
    if re.match(r'-?\d+$', v):
        v = int(v)
    elif re.match(r'-?\d*[.]?\d*(e-?\d+)?$', v) and re.match(r'-?[.]?\d', v):
        v = float(v)
    # save result
    stack[-1][1][n] = v

def pyx_text(stack, line):
    """handle -"""
    stack[-1].append(os.linesep if line[:3] == r'-\n' else unescape(line[1:-1]))

def pyx_close(stack, line):
    """handle )"""
    assert line[1:-1] == stack[-1][0]
    stack.pop()

g_pyx_jump = {
    '?': pyx_nop,
    '(': pyx_open,
    'A': pyx_attr,
    '-': pyx_text,
    ')': pyx_close,
}

###
### END PYX PARSER
###

# these are rewrites of methods in minidom

# was getElementsByTagName
def get_elements_by_tag_name(node, tag, dep):
    """yield nodes whose tag is 'tag'"""
    if not isinstance(node, str):
        if node[0] == tag:
            yield node
        if dep > 0:
            for n in node[2:]:
                yield from get_elements_by_tag_name(n, tag, dep - 1)

# was childNodes
def child_nodes(node):
    """yield children"""
    for n in node[2:]:
        yield n

# was isText (actually isinstance(x, minidom.Text))
def is_text(node):
    """check if node is text"""
    return isinstance(node, str)

# was localName
def local_name(node):
    """return tag of node"""
    return node[0]

def attr2hash(node):
    """return attribute hash table from node"""
    return node[1]

###
### xmltraverse called from toplogic
###

def xmltraverse(xfile, stack):
    """parse PYX file"""

    # parse the PYX file and return routing graph
    lno = 0
    for line in xfile:
        g_pyx_jump[line[0]](stack, line)
        lno += 1
        if not lno % 100000: log(f"  {lno:,d}\r")
    rrg = stack[0][2]

    # here we visit different sections
    log(f"{elapsed()} Visiting graph\n")

    # <channel chan_width_max="180" x_max="180" x_min="180" y_max="180" y_min="180"/>
    for channels in get_elements_by_tag_name(rrg, "channels", 1):
        n = 0
        for channel in child_nodes(channels):
            if is_text(channel) or channel[0] != "channel": continue
            n += 1
            h = channel[1]
            global g_chcnt
            g_chcnt = h['chan_width_max']
        # for channel
        log(f" - Visited {n} <channel>\n")
    # for channels

    # <switches>
    # <switch id="0" name="my_switch" buffered="1">
    for switches in get_elements_by_tag_name(rrg, "switches", 1):
        n = 0
        for switch in child_nodes(switches):
            if is_text(switch) or switch[0] != "switch": continue
            n += 1
            h = switch[1]
            swid = h['id']
            name = h['name']
            g_swid2swname[swid] = name
            if not re.match(r'\D+\d+', name):
                # non-wire seg last in XML is input matrix mux
                global g_inp2swid
                g_inp2swid = swid
        # for switch
        log(f" - Visited {n} <switch>es\n")
    # for switches

    # <segments>
    # <segment id="0" name="L4">
    for segments in get_elements_by_tag_name(rrg, "segments", 1):
        n = 0
        for segment in child_nodes(segments):
            if is_text(segment) or segment[0] != "segment": continue
            n += 1
            h = segment[1]
            sgid = h['id']
            name = h['name']
            g_sgid2sgname[sgid] = name
        # for segment
        log(f" - Visited {n} <segment>s\n")
    # for segments

    # <block_types>
    for block_types in get_elements_by_tag_name(rrg, "block_types", 1):
        n = n2 = 0
        # <block_type id="0" name="io" width="1" height="1">
        for block_type in child_nodes(block_types):
            if is_text(block_type) or block_type[0] != "block_type": continue
            n += 1
            h = block_type[1]
            (bid, name, width, height) = [h[k] for k in ['id', 'name', 'width', 'height']]
            g_bid2info[bid] = {
                'name': name, 'width': width, 'height': height,
                'pid2info': {}, 'n2info': {},
            }
            g_block2bid[name] = bid
            # save ID of "clb"
            if 'clb' in name:
                global g_clb_id
                assert g_clb_id is None
                g_clb_id = bid
            # <pin_class type="input">
            for pin_class in child_nodes(block_type):
                if is_text(pin_class) or pin_class[0] != "pin_class": continue
                h = pin_class[1]
                pin_class_type = h['type']
                # <pin ptc="0">DATIN[0]</pin>
                for pin in child_nodes(pin_class):
                    if is_text(pin) or pin[0] != "pin": continue
                    n2 += 1
                    h = pin[1]
                    ptc = h['ptc']
                    text = None
                    for t in child_nodes(pin):
                        if is_text(t): text = t
                    assert text is not None
                    g_bid2info[bid]['pid2info'][ptc] = [text, pin_class_type]
                    g_bid2info[bid][  'n2info'][text] = [ptc, pin_class_type]
                # for pin
            # for pin_class
        # for block_type
        log(f" - Visited {n} <block_type>s with {n2:,d} <pin>s\n")
    # for block_types

    # <grid>
    for grid in get_elements_by_tag_name(rrg, "grid", 1):
        n = 0
        # <grid_loc x="0" y="0" block_type_id="0" width_offset="0" height_offset="0"/>
        for grid_loc in child_nodes(grid):
            if is_text(grid_loc) or grid_loc[0] != "grid_loc": continue
            n += 1
            h = grid_loc[1]
            # upgrade to later graph type
            if g_add_layer:
                h['layer'] = 0
            (x, y, wo, ho) = [h[k] for k in ['x', 'y', 'width_offset', 'height_offset']]
            # is this difference real?
            if 'block_type' in h:
                bid = h['block_type']
            elif 'block_type_id' in h:
                bid = h['block_type_id']
            else:
                assert 0
            g_xy2bidwh[(x, y)] = (bid, wo, ho)

            # expand grid bbox by real blocks
            if bid == 0: continue
            if not g_grid_bbox:
                g_grid_bbox.clear()
                g_grid_bbox.extend([x, y, x, y])
                continue
            if x < g_grid_bbox[0]: g_grid_bbox[0] = x
            if y < g_grid_bbox[1]: g_grid_bbox[1] = y
            if x > g_grid_bbox[2]: g_grid_bbox[2] = x
            if y > g_grid_bbox[3]: g_grid_bbox[3] = y
        # for grid_loc
        log(f" - Visited {n:,d} <grid_loc>s\n")
    # for grid

    # <rr_nodes> Pass 1 to determine:
    # segment id lengths
    # ptc spans (not explicit in file)
    g_sgid2len.clear()
    for rr_nodes in get_elements_by_tag_name(rrg, "rr_nodes", 1):
        n = 0
        # <node id="0" type="SOURCE" direction="NONE" capacity="1">
        # <node id="1" type="CHANX" direction="INC" capacity="1">
        for node in child_nodes(rr_nodes):
            if is_text(node) or node[0] != "node": continue
            n += 1
            h = node[1]
            loc_h = segment_h = None
            for child in child_nodes(node):
                if is_text(child): continue
                # <loc xlow="0" ylow="0" xhigh="0" yhigh="0" ptc="0"/>
                if child[0] == "loc":
                    loc_h = child[1]
                    # upgrade to later graph type
                    if g_add_layer:
                        loc_h['layer'] = 0
                # <segment segment_id="2"/>
                if child[0] == "segment":
                    segment_h = child[1]
            # for child
            if segment_h is None: continue

            sgid = segment_h['segment_id']
            ln = 1 + (loc_h['xhigh'] - loc_h['xlow']) + (loc_h['yhigh'] - loc_h['ylow'])
            if sgid not in g_sgid2len or ln > g_sgid2len[sgid]: g_sgid2len[sgid] = ln

            ptc = loc_h['ptc']
            if sgid not in g_sgid2ptcs:
                g_sgid2ptcs[sgid] = [ptc, ptc]
            elif ptc < g_sgid2ptcs[sgid][0]:
                g_sgid2ptcs[sgid][0] = ptc
            elif ptc > g_sgid2ptcs[sgid][1]:
                g_sgid2ptcs[sgid][1] = ptc

        # for node
        log(f" - Visited {n:,d} <node>s 1/2\n")
    # for rr_nodes
    # reverse for some uses
    for sgid, ln in g_sgid2len.items():
        g_len2sgid[ln] = sgid
    # build ptc --> sgid/sgname table
    for sgid, vv in g_sgid2ptcs.items():
        for ptc in range(vv[0], vv[1] + 1):
            g_ptc2sgidname[ptc] = (sgid, g_sgid2sgname[sgid])

    # <rr_nodes> Pass 2 to process nodes
    for rr_nodes in get_elements_by_tag_name(rrg, "rr_nodes", 1):
        n = 0
        # <node id="0" type="SOURCE" direction="NONE" capacity="1">
        # <node id="1" type="CHANX" direction="INC" capacity="1">
        for node in child_nodes(rr_nodes):
            if is_text(node) or node[0] != "node": continue
            n += 1
            h = node[1]
            loc_h = None
            sgid = None
            for child in child_nodes(node):
                if is_text(child): continue
                # <loc xlow="0" ylow="0" xhigh="0" yhigh="0" ptc="0"/>
                if child[0] == "loc":
                    loc_h = child[1]
                # <segment segment_id="2"/>
                if child[0] == "segment":
                    sgid = child[1]['segment_id']
            # for child
            assert loc_h
            (nid, ty) = [h[k] for k in ['id', 'type']]
            (xl, yl, xh, yh, ptc) = [loc_h[k] for k in
                                        ['xlow', 'ylow', 'xhigh', 'yhigh', 'ptc']]

            if ty in ('SOURCE', 'OPIN', 'IPIN', 'SINK'):
                if ty.endswith("PIN"):
                    sgid = loc_h['side']
                else:
                    sgid = -1
                g_nid2info[nid] = [xl, yl, xh, yh, ptc, ty, sgid]
                # IPIN and OPIN will use the correct subtile for x, y
                g_xyt2nid[(xl, yl, ptc)] = nid
                continue

            assert sgid is not None
            di = h.get('direction', 'NONE') # doesn't always appear?
            if di.endswith('_DIR'): di = di[:-4]   # INC vs INC_DIR
            t2 = {  'CHANXINC': 'CHANE', 'CHANXDEC': 'CHANW',
                    'CHANYINC': 'CHANN', 'CHANYDEC': 'CHANS',
                }[ty + di]
            dl = f"{t2[-1]}{g_sgid2len[sgid]}" # e.g. E4
            assert (yl == yh) if t2[-1] in 'EW' else (xl == xh)

            g_nid2info[nid] = [xl, yl, xh, yh, ptc, t2, sgid]
            key = (dl, xl, yl, xh, yh)
            pair = (ptc, nid)
            g_xydl2tn[key].append(pair)
            #log(f"Adding to key={key} pair={pair}\n")
        # for node
        log(f" - Visited {n:,d} <node>s 2/2\n")
    # for rr_nodes

    # <rr_edges>
    # save edges and determine switch id lengths
    for rr_edges in get_elements_by_tag_name(rrg, "rr_edges", 1):
        n = 0
        # <edge src_node="0" sink_node="1" switch_id="0"/>
        for edge in child_nodes(rr_edges):
            if is_text(edge) or edge[0] != "edge": continue
            n += 1
            h = edge[1]
            src = h['src_node']
            snk = h['sink_node']
            swid = h['switch_id']
            # we may throw this out, rewrite it, whatever
            g_edges.append([src, snk, swid])
            sgid = g_nid2info[snk][6]
            if sgid in g_sgid2len:
                ln = g_sgid2len[sgid]
                if swid not in g_swid2len or g_swid2len[swid] < ln:
                    g_swid2len[swid] = ln
        # for edge
        log(f" - Visited {n:,d} <edge>s\n")
    # for rr_edges
    # reverse for some uses
    for swid, ln in g_swid2len.items():
        g_len2swid[ln] = swid
# def xmltraverse

###
### end xmltraverse
###

###
### debug_dump called from toplogic
###

def debug_dump(filename):
    """dump it all out"""

    if not g_dumps: return

    with open(filename, "w", encoding='ascii') as ofile:
        log(f"{elapsed()} Writing {filename}\n")
        ofile.write("Switches\n")
        for wid in sorted(g_swid2swname):
            ofile.write(f"\t#{wid}\t{g_swid2swname[wid]}\n")
        # for wid

        ofile.write("Segments\n")
        for gid in sorted(g_sgid2sgname):
            ofile.write(f"\t#{gid}\t{g_sgid2sgname[gid]}\n")
        # for gid

        ofile.write("Blocks\n")
        for bid in sorted(g_bid2info):
            (name, width, height, pid2info) = [g_bid2info[bid][k]
                for k in ['name', 'width', 'height', 'pid2info', ]]
            ofile.write(f"\t#{bid}\t{name}\t{width}x{height}\n")
            for pid in sorted(pid2info):
                (txt, ty) = pid2info[pid]
                ofile.write(f"\t\t#{pid}\t{ty}\t{txt}\n")
            # for pid
        # for bid

        ofile.write("Floorplan\n")
        xmax = max( x for (x, _) in g_xy2bidwh )
        ymax = max( y for (_, y) in g_xy2bidwh )
        widest = len(sorted(g_block2bid, key=lambda n: -len(n))[0])
        for y in range(ymax, -1, -1):
            row = []
            for x in range(0, xmax + 1):
                k = (x, y)
                (bid, _, _) = g_xy2bidwh.get(k, -1)
                name =  '?' if bid < 0 else \
                        '!' if bid not in g_bid2info else \
                        g_bid2info[bid]['name']
                name += " " * (widest - len(name))
                row.append(name)
            # for x
            ofile.write(f"\t#{y}\t{' '.join(row)}\n")
        # for y

        ofile.write("Nodes\n")
        for nid in sorted(g_nid2info):
            (x, y, _, _, ptc, ty, sgid) = g_nid2info[nid]
            (bid, _, _) = g_xy2bidwh[(x, y)]
            blk =   '?' if bid < 0 else \
                    '!' if bid not in g_bid2info else \
                    g_bid2info[bid]['name']
            if sgid < 0:
                sgid = ""
            else:
                sgid = g_sgid2sgname[sgid]
            if not ty.startswith("CH") and bid >= 0:
                (txt, di) = g_bid2info[bid]['pid2info'][ptc]
                di = di[0]
                ptc = f"{txt}:{di}"
                ofile.write(f"\t#{nid}\t{x},{y}({blk})\t{ptc}\t{ty}\t{sgid}\n")
            else:
                ofile.write(f"\t#{nid}\t{x},{y}({blk})\t{ptc}\t{ty}\t{sgid}\n")
        # for nid

        ofile.write("Edges\n")
        for e in g_edges:
            (src, snk, swid) = e
            ofile.write(f"\t{src}->{snk}\t{swid}\n")
        # for e
    # with
# def debug_dump

def bbox_update(bbox, x, y):
    """enlarge bbox by new pt"""
    if bbox:
        bbox[0] = min(bbox[0], x)
        bbox[1] = min(bbox[1], y)
        bbox[2] = max(bbox[2], x)
        bbox[3] = max(bbox[3], y)
    else:
        bbox.append(x)
        bbox.append(y)
        bbox.append(x)
        bbox.append(y)
# def bbox_update

def collisions(dom_tree, deduce=False):
    """check nodes for collisions"""
    if deduce:
        log(f"{elapsed()} Checking for collisions and determining tileable\n")
    else:
        log(f"{elapsed()} Checking for collisions\n")

    ok = []
    bad = []
    global g_twisted
    for twist in [0, 2] if deduce else [g_twisted]:

        hxyt2id = defaultdict(set)      # (col, yl, p2) --> set(nid)
        vxyt2id = defaultdict(set)      # (xl, row, p2) --> set(nid)
        rr_nodes = None
        for nodes in get_elements_by_tag_name(dom_tree, "rr_nodes", 2):
            rr_nodes = nodes
        # for nodes
        assert rr_nodes
        invalid = False
        for node in child_nodes(rr_nodes):
            if is_text(node): continue
            assert local_name(node) == 'node'
            hnode = attr2hash(node)
            ty = hnode['type']
            if not ty.startswith('CHAN'):
                continue

            nid = hnode['id']
            hloc = {}
            hseg = {}
            for child in child_nodes(node):
                if is_text(child): continue
                if local_name(child) == 'loc':
                    hloc = attr2hash(child)
                if local_name(child) == "segment":
                    hseg = attr2hash(child)
            # for child
            assert hloc and hseg
            (ptc, xl, xh, yl, yh) = [ hloc[k]
                for k in ['ptc', 'xlow', 'xhigh', 'ylow', 'yhigh'] ]
            sgid = hseg['segment_id']
            (ptcmin, ptcmax) = g_sgid2ptcs[sgid]

            dd = twist if hnode['direction'][:3] == "INC" else -twist
            if ty == "CHANX":
                for i in range(1 + xh - xl):
                    p2 = ptc + i * dd
                    if not ptcmin <= p2 <= ptcmax:
                        invalid = True
                        break
                    hxyt2id[(xl + i, yl, p2)].add(nid)
                # for i
            elif ty == "CHANY":
                for i in range(1 + yh - yl):
                    p2 = ptc + i * dd
                    if not ptcmin <= p2 <= ptcmax:
                        invalid = True
                        break
                    vxyt2id[(xl, yl + i, p2)].add(nid)
                # for i
            else:
                assert 0
            if invalid: break
        # for node
        if invalid: break
        sizes = defaultdict(int)
        for nids in hxyt2id.values():
            sizes[len(nids)] += 1
        for nids in vxyt2id.values():
            sizes[len(nids)] += 1
        del sizes[1]
        bad.append(sum(sizes.values()))
        if not bad[-1]:
            ok.append(twist)
    # for twist
    if len(ok) != 1:
        log(f" - Bad routing graph: {bad} (x,y,p) collisions detected\n")
        sys.exit(1)
    if deduce:
        g_twisted = ok[0]
        tileable = "true" if g_twisted else "false"
        log(f' - Deduced routing graph is tileable="{tileable}"\n')
    else:
        log( " - No (x,y,p) collisions detected\n")
# def

###
### analyze_nodes called from toplogic
### no globals changed
### produce various stats and debugging dumps
###

def analyze_nodes(dom_tree):
    """analyze nodes"""
    log(f"{elapsed()} Analyzing graph\n")

    # DEBUG
    #with open(f"{g_base}.json", "w", encoding='ascii') as jfile:
    #    log(f"{elapsed()} Writing {g_base}.json\n")
    #    json.dump(dom_tree, jfile, indent=1)

    # <rr_nodes>
    # <node capacity="1" id="0" type="SINK">
    #   <loc ptc="0" xhigh="0" xlow="0" yhigh="1" ylow="1"/>
    #   <timing C="0" R="0"/>
    # </node>
    #nvisit = 0
    id2info = {}                    # nid --> (xl, yl, xh, yh, ptc, ty, di, seg)
    hxyt2id = defaultdict(set)      # (col, yl, p2) --> set(nid)
    vxyt2id = defaultdict(set)      # (xl, row, p2) --> set(nid)

    incwires = defaultdict(list)    # (xl, yl, ptc) --> list((ty, di, ln))
    htable = []
    vtable = []
    cxbbox = []                     # [x1, y1, x2, y2]
    cybbox = []                     # [x1, y1, x2, y2]

    for nodes in get_elements_by_tag_name(dom_tree, "rr_nodes", 2):
        #log(f"FOUND rr_nodes\n")
        for node in child_nodes(nodes):
            #log(f"FOUND node: {node}\n")
            #log(f"visit {nvisit} {node}\n")
            #nvisit += 1
            if is_text(node): continue
            #log(f"visit2 {node}\n")
            assert local_name(node) == 'node'
            # <node>
            hnode = attr2hash(node)
            ty = hnode['type']
            nid = hnode['id']

            hloc = {}
            hseg = {}
            for child in child_nodes(node):
                if is_text(child): continue
                #log(f"visit2 {child.local_name} {child}\n")
                if local_name(child) == 'loc':
                    # <loc>
                    hloc = attr2hash(child)
                if local_name(child) == "segment":
                    hseg = attr2hash(child)
            # for child
            assert hloc
            (ptc, xl, xh, yl, yh) = [ hloc[k]
                for k in ['ptc', 'xlow', 'xhigh', 'ylow', 'yhigh'] ]
            ln = 1 + (xh - xl) + (yh - yl)

            if not ty.startswith('CHAN'):
                continue

            # get segment_id and its name
            assert hseg
            sgid = hseg['segment_id']
            sgname = g_sgid2sgname[sgid]
            (ptcmin, ptcmax) = g_sgid2ptcs[sgid]

            di = hnode['direction'][:3]
            id2info[nid] = (xl, yl, xh, yh, ptc, ty, di, sgname)
            # always use xl, yl?
            # ?? <launch> <0> <1> <2> <3>
            # ?? <0> <1> <2> <3> <launch>
            if di == 'INC':
                incwires[(xl, yl, ptc)].append((ty, di, ln))
            elif di == 'DEC':
                # unfortunately always use LL, not driver
                incwires[(xl, yl, ptc)].append((ty, di, ln))
            else:
                assert 0
            if ty == "CHANX":
                sym = ">" if di[:3] == "INC" else "<"
                dd = 1 if di[:3] == "INC" else -1
                bbox_update(cxbbox, xl, yl)
                bbox_update(cxbbox, xh, yh)
                for i in range(1 + xh - xl):
                    p2 = ptc + i * g_twisted * dd
                    assert ptcmin <= p2 <= ptcmax
                    col = xl + i
                    row = g_chcnt * yl + p2
                    ch = f"{sym}{p2}{sym}" if xl == col == xh else \
                         f"{sym}{p2}" if col == xl else \
                         f"{p2}{sym}" if col == xh else \
                         f"{p2}"
                    while len(htable) <= row:
                        htable.append([])
                    while len(htable[row]) <= col:
                        htable[row].append("")
                    #assert htable[row][col] == ""
                    htable[row][col] = ch
                    hxyt2id[(col, yl, p2)].add(nid)
                # for i
            elif ty == "CHANY":
                sym = "^" if di[:3] == "INC" else "v"
                dd = 1 if di[:3] == "INC" else -1
                bbox_update(cybbox, xl, yl)
                bbox_update(cybbox, xh, yh)
                for i in range(1 + yh - yl):
                    p2 = ptc + i * g_twisted * dd
                    assert ptcmin <= p2 <= ptcmax
                    row = yl + i
                    col = g_chcnt * xl + p2
                    ch = f"{sym}{p2}{sym}" if yl == row == yh else \
                         f"{sym}{p2}" if row == yl else \
                         f"{p2}{sym}" if row == yh else \
                         f"{p2}"
                    while len(vtable) <= row:
                        vtable.append([])
                    while len(vtable[row]) <= col:
                        vtable[row].append("")
                    #assert vtable[row][col] == ""
                    vtable[row][col] = ch
                    vxyt2id[(xl, row, p2)].add(nid)
                # for i
            else:
                assert 0
        # for node
    # for nodes

    # a quick-and-dirty dump
    if g_dumps:
        y = 2
        qname = f"{g_base}_y{y}.csv"
        with open(qname, "w", encoding='ascii') as qfile:
            for ptc in range(g_chcnt):
                qfile.write(f"{ptc}")
                for x in range(cxbbox[2] + 1):
                    key = (x, y, ptc)
                    if key in hxyt2id:
                        txt = ':'.join([str(n) for n in sorted(hxyt2id[key])])
                        qfile.write(f",{txt}")
                    else:
                        qfile.write(",")
                qfile.write("\n")

    if g_dumps:
        with open(f"{g_base}.channels", "w", encoding='ascii') as cfile:
            log(f"{elapsed()} Writing {g_base}.channels\n")
            cfile.write("# (x, y, ptc) (CHAN-, INC/DEC, phys_len)...\n\n")
            for triple in sorted(incwires):
                #(x, y, t) = triple
                wires = sorted(incwires[triple])
                cfile.write(f"\t{triple}\t{wires}\n")
            # for triple
        # with cfile

    if g_dumps:
        with open(f"{g_base}.csv", "w", encoding='ascii') as cfile:
            log(f"{elapsed()} Writing {g_base}.csv\n")
            for wh, xtable in [['CHANX', htable], ['CHANY', vtable]]:
                cfile.write(f"'{wh}'\n\n")
                for row in xtable:
                    for col in row:
                        cfile.write(f"'{col}',")
                    # for col
                    cfile.write("\n")
                # for row
                cfile.write("\n")
            # for wh, xtable
        # with cfile

    hcollide = 0
    for triple in hxyt2id:
        hcollide += len(hxyt2id[triple]) - 1
    # for triple
    vcollide = 0
    for triple in vxyt2id:
        vcollide += len(vxyt2id[triple]) - 1
    # for triple

    cdump = g_dumps
    cdump = True
    cname = f"{g_base}.collide"
    with open(cname, "w", encoding='ascii') as cfile:
        if cdump and (hcollide or vcollide):
            if cdump:
                log(f"{elapsed()} Writing {cname}\n")
            for triple in hxyt2id:
                if len(hxyt2id[triple]) < 2: continue
                cfile.write(f"Collision at {triple}:\n")
                for nid2 in hxyt2id[triple]:
                    (xl, yl, xh, yh, ptc, ty, di, sname) = id2info[nid2]
                    cfile.write(
f"\tid={nid2} xl={xl} yl={yl} xh={xh} yh={yh} ptc={ptc} ty={ty} di={di} s={sname}\n")
                # for nid2
            # for triple
            for triple in vxyt2id:
                if len(vxyt2id[triple]) < 2: continue
                cfile.write(f"Collision at {triple}:\n")
                for nid2 in vxyt2id[triple]:
                    (xl, yl, xh, yh, ptc, ty, di, sname) = id2info[nid2]
                    cfile.write(
f"\tid={nid2} xl={xl} yl={yl} xh={xh} yh={yh} ptc={ptc} ty={ty} di={di} s={sname}\n")
                # for nid2
            # for triple
        # if
    # with

    log(
f" - {hcollide} CHANX collisions  {vcollide} CHANY collisions  " +
f"CHANX bbox {cxbbox[0]},{cxbbox[1]} {cxbbox[2]},{cxbbox[3]}  " +
f"CHANY bbox {cybbox[0]},{cybbox[1]} {cybbox[2]},{cybbox[3]}\n")
# def analyze_nodes

###
### find_routable_pins called from toplogic
###
### USE
### g_nid2info      nid --> [xl, yl, xh, yh, ptc, t2, sgid]
### g_xy2bidwh      (x, y) --> (bid, wo, ho)
###
### PRODUCE
### g_routablepins  (bid, sptc) --> (wo, ho)
###

def find_routable_pins(dom_tree):
    """ examine edges to see where we connect CHANX/Y and I/OPIN
        this will allow us to deduce which block pins are connected """
    log(f"{elapsed()} Find which pins are routable\n")
    illegal = 0
    legal = { 'RIGHT', 'TOP', }
    for edges in get_elements_by_tag_name(dom_tree, "rr_edges", 2):
        for edge in child_nodes(edges):
            if is_text(edge): continue
            assert local_name(edge) == 'edge'
            hedge = attr2hash(edge)
            nsrc = hedge['src_node']
            ndst = hedge['sink_node']
            (sxl, syl, sxh, syh, sptc, sty, ssg) = g_nid2info[nsrc]
            (dxl, dyl, dxh, dyh, dptc, dty, dsg) = g_nid2info[ndst]
            # swap CHAN* --> *PIN to *PIN --> CHAN*
            if dty.endswith('PIN'):
                (sxl, syl, sxh, syh, sptc, sty, ssg, dxl, dyl, dxh, dyh, dptc, dty, dsg) = \
                (dxl, dyl, dxh, dyh, dptc, dty, dsg, sxl, syl, sxh, syh, sptc, sty, ssg)
            if not sty.endswith('PIN'): continue
            if not dty.startswith('CHAN'): continue

            # now we know that source PIN connects to CHAN
            assert sxl == sxh and syl == syh
            (bid, wo, ho) = g_xy2bidwh[(sxl, syl)]
            g_routablepins[(bid, sptc)] = (wo, ho)
            g_routablesides[(bid, sptc)].add(ssg)
            if ssg not in legal: illegal += 1
        # for edge
    # for edges
    illegal += len([ss for ss in g_routablesides.values() if len(ss) > 1])
    if illegal:
        log(f" - {illegal} routable pins are not on consistent/legal sides\n")
    else:
        log( " - All routable pins are on consistent/legal sides\n")
# def find_routable_pins

###
### create_pinmaps called from toplogic
###
### USE
### g_routablepins      (bid, sptc) --> (wo, ho)
### g_bid2info          bid --> 'pid2info' --> ptc --> [text, 'input'|'output']
### g_bid2info          bid -->   'n2info' --> txt --> [ptc,  'input'|'output']
### g_pattern_inputs    [inputs] from .mux
### g_pattern_outputs   [outputs] from .mux
###
### PRODUCE
### g_bidport2nn        (bid, wo, ho, routeport) --> (ptc, blockport) or None
###

def create_pinmaps():
    """align pins on blocks between pattern names and block pin namess"""

    # all pins used in the routing graph edges
    # (bid, wo, ho) ==> [(name, ptc), ...]
    bidoo2ipins = defaultdict(list) # inputs
    bidoo2opins = defaultdict(list) # outputs
    # for every block and pin_id
    for (bid, ptc) in g_routablepins:
        # get port name on block and direction
        (blockportname, di) = g_bid2info[bid]['pid2info'][ptc]
        # which subtile is it in
        (wo, ho) = g_routablepins[(bid, ptc)]
        # add to correct table by direction
        assert di[0] in "IO"
        b2p = bidoo2ipins if di[0] == "I" else bidoo2opins
        b2p[(bid, wo, ho)].append((blockportname, ptc))
        #log(f"bid={bid} ptc={ptc} blockportname={blockportname} di={di} wo={wo} ho={ho}\n")
    # for bid, ptc

    # canonically reorder inputs then outputs
    for w, b2p in [[1, bidoo2ipins], [0, bidoo2opins]]:
        # at each subtile
        for triple in b2p:
            # by name (x[0]) inside grid_loc
            b2p[triple] = sorted(b2p[triple], key=lambda x: dict_aup_nup(x[0]))
        # for triple
    # for b2p

    # for each port direction
    consumed = set()
    for w, b2p in [[0, bidoo2opins], [1, bidoo2ipins]]:
        # for each block type X subtile
        for (bid, wo, ho) in b2p:
            io_tile = g_bid2info[bid]['name'].startswith("io")
            # all pins that routing pattern might request by direction
            # these are sorted by normal/special then dict_aup_nup
            routeports = g_pattern_inputs if w else g_pattern_outputs
            # these are sorted by dict_aup_nup
            blockpairs = b2p[(bid, wo, ho)]
            # SERIOUS HACK TO DEAL WITH HOW FORCED TO IMPLEMENT IOTile
            # It has 48 real inputs and 24 real outputs
            # Provided in 72 pairs which are half fake
            # OLD: need to connect 47..0 only inputs and 71..48 only outputs
            # do the latter by reversing the lists
            # NEW: need to connect 71..24 only inputs and 23..0 only outputs
            # do the former by reversing the INPUT lists
            if w == 1 and     io_tile:  # "1" (NEW) was "0" (OLD)
                #routeports = routeports[::-1]  # for OLD
                routeports = sorted(routeports, key=dict_aup_ndn)
                blockpairs = blockpairs[::-1]
            # SERIOUS HACK TO DEAL WITH INTERDIGITATED BUS INPUT PORTS
            # ON TILES USING LOCAL ROUTING
            # we must reverse the numbers on each thing to sort
            if w == 1 and not io_tile:
                blockpairs = sorted(blockpairs, key=lambda x: dict_aup_nuprev(x[0]))
            # create mapping routeport -> blockport
            for i, routeport in enumerate(routeports, start=0):
                if i < len(blockpairs):
                    (blockport, ptc) = blockpairs[i]
                    consumed.add((bid, ptc))
                    g_bidport2nn[(bid, wo, ho, routeport)] = (ptc, blockport)
                else:
                    g_bidport2nn[(bid, wo, ho, routeport)] = None
            # for i, routeport
        # for b, w, h
    # for w, b2p

    # write out pinmap we deduced
    with open(f"{g_base}.pinmap", "w", encoding='ascii') as pfile:
        log(f"{elapsed()} Writing {g_base}.pinmap\n")

        # check for pins appearing in routing graph that didn't get pattern ports
        unassigned = defaultdict(int)
        for (bid, ptc) in sorted(g_routablepins):
            if (bid, ptc) in consumed: continue
            bn = g_bid2info[bid]['name']
            (bpn, di) = g_bid2info[bid]['pid2info'][ptc]
            pfile.write(
f"Warning: block={bn}#{bid} {di} port={bpn}#{ptc} not assigned to routing port\n")
            unassigned[(bid, bn)] += 1
        # for b, p
        for bid, bn in unassigned:
            log(
f"Warning: block {bn}#{bid} has {unassigned[(bid, bn)]} unassigned ports (see .pinmap)\n")

        pfile.write("id x y BLOCK       ROUTEPORT       SIDE   DIR    BLOCKPORT\n\n")
        for (bid, wo, ho, routeport) in sorted(g_bidport2nn,
                key=lambda k: [k[0], k[1], -k[2], dict_aup_ndn(k[3])]):
            # don't dump out EMPTY
            if not bid: continue
            # translations
            blkname = g_bid2info[bid]['name']
            pair = g_bidport2nn[(bid, wo, ho, routeport)]
            if pair:
                (ptc, blockport) = pair
                (blockportname, di) = g_bid2info[bid]['pid2info'][ptc]
                assert blockport == blockportname
                side = list(g_routablesides[(bid, ptc)])[0]
                pair = f"{di:<6s} {ptc:4d} {blockport}"
            else:
                side = "-"
                pair = "-      -    -"
            pfile.write(
f"{bid:2d} {wo} {ho} {blkname:<11s} {routeport:<15s} {side:<6s} {pair}\n")
        # for
    # with
# def create_pinmaps

###
### Use
###
### g_xy2bidwh          (x, y) --> (bid, wo, ho)
### g_bidport2nn        (bid, wo, ho, routeport) --> (ptc, blockport) or None
### g_xyt2nid           (xl, yl, ptc) --> nid
### g_tap2dist          (ln, tap) --> dist
### g_grid_bbox         [x1, y1, x2, y2]
### g_xydl2tn           (dl, xl, yl, xh, yh) --> list((ptc, nid))
###

def wire2node0(name, x, y):
    """translate short names to node id #s"""
    #log(f"wire2node0: A name={name} x={x} y={y}\n")

    # pin lookup
    if name[0] in 'IO':

        # interesting block at x,y?
        (bid, wo, ho) = g_xy2bidwh.get((x, y), (0, 0, 0))
        #log(f"wire2node0: B bid={bid} wo={wo} ho={ho}\n")
        if bid == 0:
            #log(f"wire2node0: B2 returning -1\n")
            return -1

        # do we have a mapping?
        key = (bid, wo, ho, name)
        # this should not happen
        if key not in g_bidport2nn:
            log(f"WARNING can't find {x} {y} {name}\n")
            return -1
        nn = g_bidport2nn[key]
        #log(f"wire2node0: C key={key} nn={nn}\n")
        if not nn:
            #log(f"wire2node0: C2 returning -1\n")
            return -1

        (ptc, _) = nn
        #log(f"wire2node0: D ptc={ptc}\n")
        rv = g_xyt2nid.get((x, y, ptc), -1)
        #if rv < 0:
        #    log(f"wire2node0: D2 returning -1\n")
        #else:
        #    log(f"wire2node0: D3 SUCCESS {rv} <== {name} {x} {y}\n")
        return rv

    # wire segment lookup
    g = re.match(r'([ENWS])(\d+)([BQMPE])(\d+)$', name)
    assert g, f"name={name}"
    (di, ln, tap, b) = g.groups()
    (ln, b) = (int(ln), int(b))
    d = g_tap2dist[(ln, tap)]   # +ve, E = length, M = l/2, B = 0

    # NB: adjust for physical wires being longer than logical wires
    if not g_commercial:
        ln += 1
    #log(f"wire2node0: E di={di} ln={ln} tap={tap} b={b} d={d}\n")

    # deduce full wire extent
    (xl, yl, xh, yh) = (x, y, x, y)
    shift = 0   # original
    shift = 1   # try this
    if di == 'E':
        xl -= d
        xh += ln - d
        xh -= 1
        xl += shift
        xh += shift
    if di == 'N':
        yl -= d
        yh += ln - d
        yh -= 1
        yl += shift
        yh += shift
    shift = 0   # final update
    if di == 'W':
        xl -= ln - d
        xh += d
        xl += 1
        xl -= shift
        xh -= shift
    if di == 'S':
        yl -= ln - d
        yh += d
        yl += 1
        yl -= shift
        yh -= shift

    # clip to fabric grid
    if xl < g_grid_bbox[0]: xl = g_grid_bbox[0]
    if yl < g_grid_bbox[1]: yl = g_grid_bbox[1]
    if xh > g_grid_bbox[2]: xh = g_grid_bbox[2]
    if yh > g_grid_bbox[3]: yh = g_grid_bbox[3]

    dl = f"{di}{ln}"
    key = (dl, xl, yl, xh, yh)
    #log(f"wire2node0: F xl={xl} yl={yl} xh={xh} yh={yh} dl={dl} key={key}\n")
    if key not in g_xydl2tn:
        #log(f"wire2node0: G returning -1\n")
        return -1

    # find node # [0] is ptc, [1] is node#; list of pairs is sorted
    pairs = g_xydl2tn[key]
    if b >= len(pairs):
        #log(f"wire2node0: H returning -1\n")
        return -1
    rv = pairs[b][1]
    #log(f"wire2node0: H2 SUCCESS {rv} <== {name} {x} {y}\n")
    return rv
# def wire2node0

def wire2node(name, x, y, quiet=False):
    """wrapper to track stats"""
    n = wire2node0(name, x, y)
    if n < 0:
        g_notfound.add((name, x, y))
    else:
        g_found.add((name, x, y))
        # translation tracking
        if not quiet:
            g_wn[n].add((x,y,name))
    return n
# def wire2node

###
### verify_wires_in_xml called from toplogic
###

def verify_wires_in_xml():
    """finding all possible wires"""
    log(f"{elapsed()} Verifying all wires can be looked up\n")
    f = 0
    nodes = defaultdict(set)
    calls = defaultdict(int)
    edges = defaultdict(int)
    b_edges = defaultdict(int)
    for y in g_ycoords:
        for x in g_xcoords:

            # wires
            for l in '14':
                for e in 'BME':
                    if l == '1' and e == 'M': continue
                    for b in range(16):
                        for d in 'ENSW':
                            # INC_DIR has no B at the max coordinate
                            if e == 'B':
                                if d == 'E' and x == g_xcoords[-1]: continue
                                if d == 'N' and y == g_ycoords[-1]: continue
                            w = d + l + e + str(b)
                            i = wire2node(w, x, y, True)
                            calls[e] += 1
                            if i >= 0:
                                f += 1
                                nodes[e].add(i)
                                continue
                            # ignore certain errors
                            if e == 'E':
                                if d == 'E' and x == g_xcoords[0]:
                                    edges[d] += 1
                                    continue
                                if d == 'W' and x == g_xcoords[-1]:
                                    edges[d] += 1
                                    continue
                                if d == 'N' and y == g_ycoords[0]:
                                    edges[d] += 1
                                    continue
                                if d == 'S' and y == g_ycoords[-1]:
                                    edges[d] += 1
                                    continue
                            if e == 'B':
                                if d == 'E' and x == g_xcoords[-1]:
                                    b_edges[d] += 1
                                    continue
                                if d == 'W' and x == g_xcoords[0]:
                                    b_edges[d] += 1
                                    continue
                                if d == 'N' and y == g_ycoords[-1]:
                                    b_edges[d] += 1
                                    continue
                                if d == 'S' and y == g_ycoords[0]:
                                    b_edges[d] += 1
                                    continue
                            log(
f"Warning: wire not found: {w} x={x} y={y}\n")
                        # for d
                    # for b
                # for e
            # for l
        # for x
    # for y

    for e in 'BME':
        miss = calls[e] - len(nodes[e])
        miss = f" (missing {miss:,d})" if miss else ""
        log(
f" - Looked up {len(nodes[e]):,d} wire {e}-points in {calls[e]:,d} requests{miss}\n")
        if e == 'E' and edges:
            total = sum(list(edges.values()))
            edges = [f"{edges[n]:,d}{n}" for n in sorted(edges)]
            edges = '+'.join(edges)
            log(
f" - {total:,d}={edges} requests failed entering array only in phantom cols/rows\n")
        if e == 'B' and b_edges:
            total = sum(list(b_edges.values()))
            b_edges = [f"{b_edges[n]:,d}{n}" for n in sorted(b_edges)]
            b_edges = '+'.join(b_edges)
            log(
f" - {total:,d}={b_edges} requests failed exiting array only from phantom cols/rows\n")
    # for e

    hist = defaultdict(int)
    # g_xydl2tn           (dl, xl, yl, xh, yh) --> list((ptc, nid))
    for key in g_xydl2tn:
        hist[len(g_xydl2tn[key])] += 1
    hist = ' '.join([f"s{c:,d}#{hist[c]:,d}" for c in sorted(hist, key=lambda c: -hist[c])])
    log(f" - Track histogram: {hist}\n")
# def verify_wires_in_xml

def fix_chan_rc(rrg):
    """set C=0 R=9 in all CHAN nodes"""

    log(f"{elapsed()} Correcting CHAN RC values\n")

    for nodes in get_elements_by_tag_name(rrg, "rr_nodes", 1):
        for node in child_nodes(nodes):
            if is_text(node): continue
            assert local_name(node) == 'node'
            hnode = attr2hash(node)
            ty = hnode['type']
            if not ty.startswith('CHAN'): continue
            for child in child_nodes(node):
                if is_text(child): continue
                if local_name(child) != "timing": continue
                # overwrite RC values
                htiming = attr2hash(child)
                htiming["C"] = "0"
                htiming["R"] = "9"
            # for child
        # for node
    # for nodes
# def

###
### check_connectivity called from toplogic
###

def check_connectivity(dom_tree, phase):
    """graph/connectivity checks"""

    log(f"{elapsed()} Checking graph connectivity\n")

    # collect all nodes in rrgraph
    allnodes = uf.uf()
    g_nid2elt.clear()
    for nodes in get_elements_by_tag_name(dom_tree, "rr_nodes", 2):
        for node in child_nodes(nodes):
            if is_text(node): continue
            assert local_name(node) == 'node'
            hnode = attr2hash(node)
            nid = hnode['id']
            assert nid >= 0
            allnodes.add(nid)
            g_nid2elt[nid] = node
        # for node
    # for nodes

    # put nodes in same union when connected by an edge
    ecount = 0
    for edges in get_elements_by_tag_name(dom_tree, "rr_edges", 2):
        for edge in child_nodes(edges):
            if is_text(edge): continue
            assert local_name(edge) == 'edge'
            hedge = attr2hash(edge)
            nsrc = hedge['src_node']
            ndst = hedge['sink_node']
            allnodes.union(nsrc, ndst)
            ecount += 1
        # for edge
    # for edges

    # collect all nodes into disjoint sets formed by edges
    roots = defaultdict(set)
    for nid in allnodes.keys():
        assert nid >= 0
        r = allnodes.find(nid)
        assert r >= 0
        roots[r].add(nid)

    k = len(allnodes.keys())
    log(
f" - {len(roots):,d} connected components over {k:,d} nodes and {ecount:,d} edges\n")


    dumped = 0
    write_cc = True
    if write_cc:
        cname = f"{g_base}.cc{phase}"
    else:
        cname = "/dev/stdout"
    with open(cname, "w", encoding='ascii') as cfile:
        if write_cc:
            log(f"{elapsed()} Writing {cname}\n")
        opens = 0
        weird = 0
        pairs = 0
        quads = 0
        wires = 0
        junks = 0
        for root in sorted(roots, key=lambda r: -len(roots[r])):
            bbox = []
            for n in roots[root]:
                (x, y, xh, yh, _, _, _) = g_nid2info[n]
                if not bbox:
                    bbox = [x, y, xh, yh]
                else:
                    if x  < bbox[0]: bbox[0] = x
                    if y  < bbox[1]: bbox[1] = y
                    if xh > bbox[2]: bbox[2] = xh
                    if yh > bbox[3]: bbox[3] = yh
            # for n
            if g_false and dumped < 6:
                log(f"\t{len(roots[root])} nodes, bbox={bbox}\n")
            dumped += 1
            # don't dump largest component
            if dumped == 1: continue
            # dump this component
            order = { 'SOUR': 0, 'OPIN': 1, 'CHAN': 2, 'IPIN': 3, 'SINK': 4, }

            # fill these in during the next block
            whatcounts = defaultdict(int)
            routable = 0

            for n, nid in enumerate(
                    sorted( roots[root],
                            key=lambda nid: (order[g_nid2info[nid][5][:4]], nid)),
                    start=0):
                # dump nid
                (xl, yl, xh, yh, ptc, what, sgid) = g_nid2info[nid]
                whatcounts[what[:4]] += 1
                if what[:4] != "CHAN":
                    # SOURCE and SINK are not necessarily points, I/OPIN must be
                    assert what in ('SOURCE', 'SINK') or xl == xh and yl == yh, f"""
({xl},{yl},{xh},{yh}) #{ptc} {what} {sgid}
"""
                    (bid, _, _) = g_xy2bidwh[(xl, yl)]
                    if what[1:] == "PIN":
                        if (bid, ptc) in g_routablepins: routable += 1
            # for nid

            # classify this component
            mask = sum( 1 << order[w] for w in whatcounts )
            if routable:
                if mask in (3, 24) and len(roots[root]) == 2:
                    # SOURCE --> OPIN or IPIN --> SINK, but routable
                    opens += 1
                    sort = "nc_rports"
                else:
                    weird += 1
                    sort = "weird"
            elif mask in (3, 24) and len(roots[root]) == 2:
                # SOURCE --> OPIN or IPIN --> SINK
                pairs += 1
                sort = "nc_hports"
            elif mask == 27 and len(roots[root]) == 4:
                # SOURCE --> OPIN --> IPIN --> SINK
                quads += 1
                sort = "chained_hports"
            elif mask == 4 and len(roots[root]) == 1:
                # CHAN* by itself
                if bbox[2] < g_grid_bbox[0] or g_grid_bbox[2] < bbox[0] or \
                    bbox[3] < g_grid_bbox[1] or g_grid_bbox[3] < bbox[1]:
                    # outside usable area: junk
                    junks += 1
                    sort = "nc_rjunks"
                else:
                    # intersecting usable area
                    wires += 1
                    sort = "nc_rwires"
            else:
                weird += 1
                sort = "weird"

            if not write_cc: continue

            cfile.write(
f"Component ({sort}) with {len(roots[root])} nodes covering {bbox}:\n")
            for n, nid in enumerate(
                    sorted( roots[root],
                            key=lambda nid: (order[g_nid2info[nid][5][:4]], nid)),
                    start=0):
                # dump nid
                (xl, yl, xh, yh, ptc, what, sgid) = g_nid2info[nid]
                if what[:4] == "CHAN":
                    # or    g_swid2swname?
                    which = g_sgid2sgname[sgid]
                    cfile.write(
f" {n}\t#{nid} ({xl},{yl} {xh},{yh}) ptc#{ptc} {what} {which}\n")
                else:
                    # SOURCE and SINK are not necessarily points, I/OPIN must be
                    assert what in ('SOURCE', 'SINK') or xl == xh and yl == yh, f"""
({xl},{yl},{xh},{yh}) #{ptc} {what} {sgid}
"""
                    (bid, _, _) = g_xy2bidwh[(xl, yl)]
                    h = g_bid2info[bid]
                    blockname = h['name']
                    if what[1:] == "PIN":
                        (blockportname, di) = g_bid2info[bid]['pid2info'][ptc]
                        port = f"{blockportname}#{ptc}({di})"
                    else:
                        port = f"n/a#{ptc}"
                    cfile.write(f" {n}\t#{nid} ({xl},{yl}) {blockname}#{bid} {port} {what}\n")
            # for nid
        # for root
    # with

    counts = defaultdict(int)
    for root in roots:
        counts[len(roots[root])] += 1
    hist = [f"s{c:,d}#{counts[c]:,d}" for c in sorted(counts, key=lambda c: -counts[c])]
    if g_true or weird:
        log(f" - Component histogram: {' '.join(hist)}\n")
    log(
f" - {opens:,d} N/C route ports, " +
f"{pairs:,d} N/C hidden ports, " +
f"{quads:,d} chained hidden ports, " +
f"and {weird:,d} weird components\n")

    singletons = [r for r in roots if len(roots[r]) == 1]
    node_order = {  'CHANE': 0, 'CHANW': 1, 'CHANN': 2, 'CHANS': 3,
                    'SOURCE': 4, 'OPIN': 5, 'IPIN': 6, 'SINK': 7, }
    if singletons:
        gb = g_grid_bbox
        counts = { 'edge': defaultdict(int), 'core': defaultdict(int), }
        sums = defaultdict(int)
        for r in singletons:
            (xl, yl, xh, yh, _, ty, _) = g_nid2info[r]
            where = "edge" if xh < gb[0] or gb[2] < xl or yh < gb[1] or gb[3] < yl else "core"
            counts[where][ty] += 1
            sums[where] += 1
        if len(sums) > 1:
            for where in sums:
                other = { 'edge': 'core', 'core': 'edge', }[where]
                for ty in counts[where]:
                    counts[other][ty] += 0
                for ty in counts[other]:
                    counts[where][ty] += 0
        for where in sorted(sums):
            cc = [  f"{ty}#{counts[where][ty]:,d}"
                    for ty in sorted(counts[where], key=lambda k: node_order[k])]
            log(f" - {sums[where]:,d} N/C {where} nodes ({' '.join(cc)})\n")

    # final sanity check for what drives IPINs
    sinks = { 'IPIN', 'SINK', }
    drivers = defaultdict(int)
    chanflags = defaultdict(int)
    for edges in get_elements_by_tag_name(dom_tree, "rr_edges", 2):
        for edge in child_nodes(edges):
            if is_text(edge): continue
            assert local_name(edge) == 'edge'
            hedge = attr2hash(edge)

            nsrc = hedge['src_node']
            tsrc = g_nid2info[nsrc][5]
            ndst = hedge['sink_node']
            tdst = g_nid2info[ndst][5]

            #if tsrc.startswith('CHAN'):
            if tsrc != 'SOURCE':
                chanflags[nsrc] |= 1
            #if tdst.startswith('CHAN'):
            if tdst != 'SINK':
                chanflags[ndst] |= 2

            if tdst not in sinks: continue
            drivers[(tsrc, tdst)] += 1
        # for edge
    # for edges
    txt = ' '.join([f"{p[0]}=>{p[1]}#{drivers[p]:,d}" for p in sorted(drivers)])
    log(f" - Edges to ipins/sinks: {txt}\n")

    undriven = len([n for n in chanflags if chanflags[n] == 1])
    dangling = len([n for n in chanflags if chanflags[n] == 2])
    if undriven or dangling:
        counts = defaultdict(int)
        for n in chanflags:
            if chanflags[n] == 3: continue
            counts[g_nid2info[n][5]] += 1
        counts.pop('SOURCE', None)
        counts.pop('SINK', None)
        counts = ' '.join([f"{t}#{counts[t]:,d}" for t in sorted(counts)])
        bname = f"{g_base}.badchan"
        croutable = 0
        eroutable = 0
        with open(bname, "w", encoding='ascii') as bfile:
            log(f"{elapsed()} Writing {bname}\n")
            for nid in sorted(chanflags, key=lambda n: g_nid2info[n]):
                if chanflags[nid] == 3: continue
                prob = ['undriven', 'dangling', ][chanflags[nid] - 1]
                info = g_nid2info[nid]
                if info[5].endswith('PIN'):
                    (x, y, _, _, ptc, _, _) = info
                    (bid, _, _) = g_xy2bidwh[(x, y)]
                    # report only _routable_ undriven/dangling pins
                    if (bid, ptc) not in g_routablepins: continue
                    if g_xcoords[0] < x < g_xcoords[-1] and g_ycoords[0] < y < g_ycoords[-1]:
                        croutable += 1
                    else:
                        eroutable += 1
                    blockname = g_bid2info[bid]['name']
                    (blockportname, di) = g_bid2info[bid]['pid2info'][ptc]
                    extra = f" {blockname}:{blockportname}:{di}"
                else:
                    extra = ""
                bfile.write(f"{nid}\t{info}{extra}\t{prob}\n")
            # for nid
        # with
        log(
f" - {undriven:,d} undriven and {dangling:,d} dangling nodes ({counts}) " +
f"including {croutable:,d}+{eroutable:,d} core+edge routable\n")
    # if
# def check_connectivity

###
### build_loops called from toplogic
###

def build_loops():
    """build pred/succ info looping support"""

    # write out predsucc file
    (xmin, ymin, xmax, ymax) = (g_xcoords[0], g_ycoords[0], g_xcoords[-1], g_ycoords[-1])
    keep = g_chcnt - g_nloop * 2
    # collect candidates for predsucc
    west = defaultdict(set)
    south = defaultdict(set)
    for nid, info in g_nid2info.items():
        #xl  yl  xh  yh  p  t  sg
        (xl, yl, xh, yh, p, t, _ ) = info
        if t not in ("CHANW", "CHANS"): continue
        if p < keep: continue
        assert xl == xh and yl == yh
        if t == "CHANW":
            west[(xl, yl)].add(nid)
        if xl == xmax and t == "CHANS":
            south[(xl, yl)].add(nid)
    # double check we have what we need, sort by ptc
    for collected in [west, south]:
        for xy in collected:
            assert len(collected[xy]) == g_nloop
            collected[xy] = sorted(collected[xy], key=lambda nid: g_nid2info[nid][4])
    # write out actual predsucc
    g_pred2succ.clear()
    g_succ2pred.clear()
    g_xyw2succ.clear()
    filename = f"{g_base}.predsucc"
    with open(filename, "w", encoding='ascii') as ofile:
        log(f"{elapsed()} Writing {filename}\n")
        for x in range(xmin, xmax):     # xmax excluded
            for y in range(ymin, ymax + 1):
                for w in range(g_nloop):
                    pred = west[(x + 1, y)][w]
                    succ = west[(x    , y)][w]
                    ofile.write(f"W {pred} --> {succ} {x} {y} {w} \n")
                    assert pred not in g_pred2succ
                    assert succ not in g_succ2pred
                    key = (x, y, w)
                    assert key not in g_xyw2succ
                    g_pred2succ[pred] = succ
                    g_succ2pred[succ] = pred
                    g_xyw2succ[key] = succ
                # for
            # for
        # for
        for x in range(xmax, xmax + 1): # just xmax
            for y in range(ymin, ymax): # ymax excluded
                for w in range(g_nloop):
                    pred = south[(x, y + 1)][w]
                    succ = south[(x, y    )][w]
                    ofile.write(f"S {pred} --> {succ} {x} {y} {w} \n")
                    assert pred not in g_pred2succ
                    assert succ not in g_succ2pred
                    key = (x, y, w)
                    assert key not in g_xyw2succ
                    g_pred2succ[pred] = succ
                    g_succ2pred[succ] = pred
                    g_xyw2succ[key] = succ
                # for
            # for
        # for
    # with
# def

def xend(taps, t):
    """choose tap letter for wire end driving into mux"""
    return taps[t] if 0 <= t < len(taps) else "?"

def xbeg(taps, t):
    """choose tap letter for wire beg driven from mux"""
    return taps[0] if t < 0 else taps[t] if t < len(taps) else "?"

###
### write_muxes write out .mux files based on what's in rgraph
###

def write_muxes(dom_tree, root, us):
    """write .mux files to 'root' (mxi or mxo)"""

    # FIXME
    if g_commercial: return

    log(f"{elapsed()} Writing .mux files\n")

    # build reversed tables only this routine uses
    # g_bidport2nn: (bid, wo, ho, routeport) ==> (ptc, blockport)
    bidwohop2rp = {}    # (bid, wo, ho, ptc) --> routeport
    for quad, pair in g_bidport2nn.items():
        if not pair: continue
        (bid, wo, ho, routeport) = quad
        (ptc, _) = pair                     # ptc, blockport
        bidwohop2rp[(bid, wo, ho, ptc)] = routeport
    # for
    # g_xydl2tn: (dl, xl, yl, xh, yh) --> list((ptc, nid))
    xydl2t2w = defaultdict(dict)    # (dl, xl, yl, xh, yh) --> ptc --> w
    for xydl, pairs in g_xydl2tn.items():
        for i, pair in enumerate(pairs, start=0):
            (ptc, _) = pair
            xydl2t2w[xydl][ptc] = i
    # for

    # look through L0s: build maps
    l0map = {}
    us = 1 if us else 0
    if us:
        incheck = defaultdict(set)
        for edges in get_elements_by_tag_name(dom_tree, "rr_edges", 2):
            for edge in child_nodes(edges):
                if is_text(edge): continue
                assert local_name(edge) == 'edge'
                hedge = attr2hash(edge)
                src_nid = hedge['src_node']
                dst_nid = hedge['sink_node']
                (dxl, dyl, dxh, dyh, dp, dt, dsg) = g_nid2info[dst_nid]
                if dt.startswith('CHAN') and g_sgid2len[dsg] == 1:
                    incheck[dst_nid].add(src_nid)
                    l0map[dst_nid] = src_nid
                # if
            # for edge
        # for edges
        # check that L0s don't have any in-convergence (i.e. they are wires)
        bad = 0
        for src_nids in incheck.values():
            if len(src_nids) > 1: bad += 1
        if bad:
            log(f" - ERROR: {bad:,d} L0s have >1 input\n")

    # record ALL connections by tile with mux
    xmin = g_xcoords[ 0]
    ymin = g_ycoords[ 0]
    xmax = g_xcoords[-1]
    ymax = g_ycoords[-1]
    conns = defaultdict(lambda: defaultdict(set))
    wm = defaultdict(set)
    pathcounts = defaultdict(set)
    for edges in get_elements_by_tag_name(dom_tree, "rr_edges", 2):
        for edge in child_nodes(edges):
            if is_text(edge): continue
            assert local_name(edge) == 'edge'
            hedge = attr2hash(edge)
            src_nid = hedge['src_node']
            dst_nid = hedge['sink_node']

            # map L0s in edge to up/downstream
            smap = False
            seen = set()
            while src_nid in l0map:
                src_nid = l0map[src_nid]
                smap = True
                if src_nid in seen:
                    log(f" - ERROR: L0 cycle on node {src_nid}\n")
                    break
                seen.add(src_nid)
            seen = set()
            while dst_nid in l0map:
                dst_nid = l0map[dst_nid]
                if dst_nid in seen:
                    log(f" - ERROR: L0 cycle on node {dst_nid}\n")
                    break
                seen.add(dst_nid)
            if src_nid == dst_nid: continue

            #0    1    2    3    4   5   6
            #                    PTC TYP SEG
            (sxl, syl, sxh, syh, sp, st, ssg) = g_nid2info[src_nid]
            (dxl, dyl, dxh, dyh, dp, dt, dsg) = g_nid2info[dst_nid]

            # compute intersection point containing mux
            match (st, dt):
                # OPIN --> IPIN
                case ('OPIN', 'IPIN') if smap:  (mx, my) = (sxl, syl)
                # OPIN --> CHAN
                case ('OPIN', 'CHANE'):     (mx, my) = (sxl, syl)
                case ('OPIN', 'CHANN'):     (mx, my) = (sxl, syl)
                case ('OPIN', 'CHANW'):     (mx, my) = (sxl, syl)
                case ('OPIN', 'CHANS'):     (mx, my) = (sxl, syl)
                # CHANX --> CHANY
                case ('CHANE', 'CHANN'):    (mx, my) = (dxl, syl)
                case ('CHANE', 'CHANS'):    (mx, my) = (dxl, syl)
                case ('CHANW', 'CHANN'):    (mx, my) = (dxl, syl)
                case ('CHANW', 'CHANS'):    (mx, my) = (dxl, syl)
                # CHANY --> CHANX
                case ('CHANN', 'CHANE'):    (mx, my) = (sxl, dyl)
                case ('CHANN', 'CHANW'):    (mx, my) = (sxl, dyl)
                case ('CHANS', 'CHANE'):    (mx, my) = (sxl, dyl)
                case ('CHANS', 'CHANW'):    (mx, my) = (sxl, dyl)
                # stitch (max almost certainly not needed)
                case ('CHANE', 'CHANE'):    (mx, my) = (max(dxl-1,xmin), dyl)
                case ('CHANN', 'CHANN'):    (mx, my) = (dxl, max(dyl-1,ymin))
                case ('CHANW', 'CHANW'):    (mx, my) = (dxh, dyh)
                case ('CHANS', 'CHANS'):    (mx, my) = (dxh, dyh)
                # reversal (max almost certainly not needed)
                case ('CHANE', 'CHANW'):    (mx, my) = (dxh, dyh)
                case ('CHANN', 'CHANS'):    (mx, my) = (dxh, dyh)
                case ('CHANW', 'CHANE'):    (mx, my) = (max(dxl-1,xmin), dyl)
                case ('CHANS', 'CHANN'):    (mx, my) = (dxl, max(dyl-1,ymin))
                # CHAN --> IPIN
                case ('CHANE', 'IPIN'):     (mx, my) = (dxl, dyl)
                case ('CHANN', 'IPIN'):     (mx, my) = (dxl, dyl)
                case ('CHANW', 'IPIN'):     (mx, my) = (dxl, dyl)
                case ('CHANS', 'IPIN'):     (mx, my) = (dxl, dyl)
                # other edges we ignore
                case _:                     continue
            # match
            if us:
                assert xmin <= mx <= xmax and ymin <= my <= ymax
            else:
                assert xmin - 1 <= mx <= xmax and ymin - 1 <= my <= ymax

            # compute source/end name
            # ex/ey is where upstream driving mux would be
            (ux, uy) = (mx, my)
            match st:
                case 'OPIN':
                    (bid, wo, ho) = g_xy2bidwh[(mx, my)]
                    end = bidwohop2rp.get((bid, wo, ho, sp), "O?")
                case 'CHANE':
                    ln = g_sgid2len[ssg]
                    dl = f"{st[-1]}{ln}"
                    ex = sxh - ln if sxl == xmin else sxl - 1
                    tp = xend("?QMPE", mx - ex) if ln > 2 else "E"
                    w = xydl2t2w.get((dl, sxl, syl, sxh, syh), {}).get(sp, 999)
                    end = f"{st[-1]}{ln-us}{tp}{w}"
                    ux = max(sxl - 1, 1)
                case 'CHANN':
                    ln = g_sgid2len[ssg]
                    dl = f"{st[-1]}{ln}"
                    ey = syh - ln if syl == ymin else syl - 1
                    tp = xend("?QMPE", my - ey) if ln > 2 else "E"
                    w = xydl2t2w.get((dl, sxl, syl, sxh, syh), {}).get(sp, 999)
                    end = f"{st[-1]}{ln-us}{tp}{w}"
                    uy = max(syl - 1, 1)
                case 'CHANW':
                    ln = g_sgid2len[ssg]
                    dl = f"{st[-1]}{ln}"
                    ex = sxl + ln - 1 if sxh == xmax else sxh
                    tp = xend("BQMPE", ex - mx) if ln > 2 else "E"
                    w = xydl2t2w.get((dl, sxl, syl, sxh, syh), {}).get(sp, 999)
                    end = f"{st[-1]}{ln-us}{tp}{w}"
                    ux = sxh
                case 'CHANS':
                    ln = g_sgid2len[ssg]
                    dl = f"{st[-1]}{ln}"
                    ey = syl + ln - 1 if syh == ymax else syh
                    tp = xend("BQMPE", ey - my) if ln > 2 else "E"
                    w = xydl2t2w.get((dl, sxl, syl, sxh, syh), {}).get(sp, 999)
                    end = f"{st[-1]}{ln-us}{tp}{w}"
                    uy = syh
            # match
            assert end[0] in "OENWS"

            # compute sink/beg name
            # mx/my should play no role here
            match dt:
                case 'IPIN':
                    (bid, wo, ho) = g_xy2bidwh[(mx, my)]
                    beg = bidwohop2rp.get((bid, wo, ho, dp), "I?")
                case 'CHANE':
                    ln = g_sgid2len[dsg]
                    dl = f"{dt[-1]}{ln}"
                    tp = xbeg("BQMP", xmin - (dxh - ln)) if ln > 2 else "B"
                    w = xydl2t2w.get((dl, dxl, dyl, dxh, dyh), {}).get(dp, 999)
                    beg = f"{dt[-1]}{ln-us}{tp}{w}"
                case 'CHANN':
                    ln = g_sgid2len[dsg]
                    dl = f"{dt[-1]}{ln}"
                    tp = xbeg("BQMP", ymin - (dyh - ln)) if ln > 2 else "B"
                    w = xydl2t2w.get((dl, dxl, dyl, dxh, dyh), {}).get(dp, 999)
                    beg = f"{dt[-1]}{ln-us}{tp}{w}"
                case 'CHANW':
                    ln = g_sgid2len[dsg]
                    dl = f"{dt[-1]}{ln}"
                    tp = xbeg("BQMP", dxl + (ln - 1) - xmax) if ln > 2 else "B"
                    w = xydl2t2w.get((dl, dxl, dyl, dxh, dyh), {}).get(dp, 999)
                    beg = f"{dt[-1]}{ln-us}{tp}{w}"
                case 'CHANS':
                    ln = g_sgid2len[dsg]
                    dl = f"{dt[-1]}{ln}"
                    tp = xbeg("BQMP", dyl + (ln - 1) - ymax) if ln > 2 else "B"
                    w = xydl2t2w.get((dl, dxl, dyl, dxh, dyh), {}).get(dp, 999)
                    beg = f"{dt[-1]}{ln-us}{tp}{w}"
            # match
            assert beg[0] in "IENWS"

            # save connection
            wm[src_nid].add((mx,my,end))
            wm[dst_nid].add((mx,my,beg))

            conns[(mx, my)][beg].add(end)

            # keep track of the path through this input pin
            dx = 1 if ux < mx else -1 if ux > mx else 0
            dy = 1 if uy < my else -1 if uy > my else 0
            path = [end]
            for _ in range(1 + abs(ux - mx) + abs(uy - my)):
                (bid, wo, ho) = g_xy2bidwh[(ux, uy)]
                tile = f"{g_bid2info[bid]['name']}{wo + ho}"
                path.append(tile)
                ux += dx
                uy += dy
            # for
            path.append(beg)
            path = '.'.join(path)
            pathcounts[path].add((src_nid, dst_nid))
        # for edge
    # for edges

    # here we will write out the unique edges we saw
    paths = sum(len(v) for v in pathcounts.values())
    unique = len(pathcounts)
    log(f" - Saw {unique:,d} unique delay arcs in {paths:,d} sinks\n")
    filename = f"{g_base}.delays"
    with open(filename, "w", encoding='ascii') as ofile:
        for path in sorted(pathcounts, key=dict_aup_nup):
            ptxt = '\n'.join(f"{s} {d}" for s, d in pathcounts[path])
            ofile.write(f"{path}\n{ptxt}\n")
        # for
    # with

    # write out all tiles' patterns (canonically)
    xmade = set()
    order = { l: i for i, l in enumerate("IEWNSO", start=0) }
    hash2xy = defaultdict(set)  # hash --> set((x,y)) then list((x,y))
    xy2file = {}                # (x,y) --> file
    for mxy in sorted(conns):
        (mx, my) = mxy
        path = f"{root}/x{mx}"
        if path not in xmade:
            os.makedirs(path, exist_ok=True)
            xmade.add(path)
        filename = f"{path}/mux_{mx}__{my}_.mux"
        phash = hashlib.sha1()
        with open(filename, "w", encoding='ascii') as ofile:
            for beg in sorted(conns[mxy]     , key=lambda k: [order[k[0]], dict_aup_nup(k)]):
                ends = sorted(conns[mxy][beg], key=lambda k: [order[k[0]], dict_aup_nup(k)])
                line = f"{beg} {' '.join(ends)}\n"
                ofile.write(line)
                phash.update(line.encode())
            # for
        # with
        hd = phash.hexdigest()
        hash2xy[hd].add((mx, my))
        xy2file[(mx, my)] = filename
    # for
    for hd, xys in hash2xy.items():
        hash2xy[hd] = sorted(xys)

    # write out debugging file for nid translations
    if us:
        badnid = 0
        badname = 0
        filename = f"{g_base}.xlate"
        with open(filename, "w", encoding='ascii') as ofile:
            for nid in sorted(set(g_wn) | set(wm)):
                (xl, yl, xh, yh, p, t, sg) = g_nid2info[nid]
                if   nid not in g_wn:
                    tag = "+"
                    badnid += 1
                elif nid not in wm:
                    if nid in l0map:
                        # technically, only when l0map leads to reported edge
                        tag = " "
                    else:
                        tag = "-"
                else:
                    tag = " "
                ofile.write(f"{tag}{nid} ({xl},{yl},{xh},{yh},{p},{t},{sg})\n")
                for triple in sorted(g_wn.get(nid, set()) | wm.get(nid, set())):
                    if   nid not in g_wn or triple not in g_wn[nid]:
                        tag = "+"
                        badname += 1
                    elif nid not in   wm or triple not in   wm[nid]:
                        if nid in l0map:
                            # same as above
                            tag = " "
                        else:
                            tag = "-"
                    else:
                        tag = " "
                    (x, y, name) = triple
                    ofile.write(f"{tag}\t{x},{y},{name}\n")
                # for triple
            # for nid
        # with
        if badnid or badname:
            log(f" - ERROR: {badnid:,d} nodes and {badname:,d} names newly appeared\n")
        # if
    # if

    # write out tile pattern map

    # prepare other tables for writing
    h2l = {}
    l2h = {}
    for i, h in enumerate(sorted(hash2xy, key=lambda h: hash2xy[h])):
        l = chr(ord('A') + i) if i < 26 else chr(ord('a') + i - 26)
        h2l[h] = l
        l2h[l] = h

    xy2l = {}
    l2f = {}
    l2n = {}
    for h in hash2xy:
        l = h2l[h]
        for xy in hash2xy[h]:
            xy2l[xy] = l
        l2f[l] = xy2file[hash2xy[h][0]]
        l2n[l] = len(hash2xy[h])

    # write out the map
    filename = f"{g_base}.tilemap"
    (xmin, ymin, xmax, ymax) = (g_xcoords[0], g_ycoords[0], g_xcoords[-1], g_ycoords[-1])
    with open(filename, "w", encoding='ascii') as ofile:
        ofile.write("+")
        for x in range(xmin, xmax + 1):
            ofile.write(f"{x%10}")
        # for
        ofile.write("+\n")
        for y in range(ymax, ymin - 1, -1):
            ofile.write(f"{y%10}")
            for x in range(xmin, xmax + 1):
                l = xy2l[(x, y)]
                ofile.write(f"{l}")
            # for
            ofile.write(f"{y%10}\n")
        # for
        ofile.write("+")
        for x in range(xmin, xmax + 1):
            ofile.write(f"{x%10}")
        # for
        ofile.write("+\n")

        w = max(len(v) for v in l2f.values())
        for i, l in enumerate(sorted(l2f), start=1):
            o = ' '.join(list(g_hash2source[l2h[l]]))
            ofile.write(f"{l} {i:2d} {l2n[l]:4d} {l2f[l]:<{w}s} {o}\n")
        # for
    # with
# def write_muxes

###
### edit_xml called from toplogic
###

def edit_xml(dom_tree):
    """change rr_edges"""

    log(f"{elapsed()} Editing graph\n")

    #
    # Step 1. Throw out any edge involving CHAN*
    #
    # Note: we later add O->I, but they don't need to be thrown out here
    # since VPR can't generate them.
    #

    # find old rr_edges node
    old_edges = None
    for ee in get_elements_by_tag_name(dom_tree, "rr_edges", 2):
        old_edges = ee
    # for ee
    assert old_edges

    # move through all rr_edge nodes
    moves = []
    drop = 0
    for edge in child_nodes(old_edges):
        if is_text(edge): continue
        h = attr2hash(edge)
        src_nid = h['src_node']
        dst_nid = h['sink_node']
        # we will replace any edge that connects a routable wire to anything
        if g_nid2info[src_nid][5].startswith("CHAN") or \
            g_nid2info[dst_nid][5].startswith("CHAN"):
            drop += 1
        else:
            # we will move this edge to new result
            moves.append(edge)
    # for edge

    # get rid of ALL edges
    del old_edges[2:]

    # move from old_edges to new_edges
    old_edges.extend(moves)
    save = len(moves)

    edges = old_edges

    #
    # Step 2. Add new edges
    #

    # INFO
    beg_unfound = set()             # beg nodes we couldn't find
    end_unfound = set()             # end nodes we couldn't find
    added = 0                       # total mux inupts connected (ignoring S0s)
    size = defaultdict(set)         # track width of muxes
    load = defaultdict(set)         # track load of muxes
    # NB this is for S0s
    swid0 = str(g_len2swid[1 + 0])  # switch_id for L0
    fakemapn0 = {}                  # e.g. E1E7 --> N0E19, O23 --> N0E23
    fakemaps0 = {}                  # e.g. E1E7 --> S0E19, O23 --> S0E23
    fakemape0 = {}
    fakemapw0 = {}  # TODO these can be anonymous now inside the next structure
    alledges = set()                # e.g. set((id_of(E1E7), id_of(S0E19)))
    choose = {  'N': [fakemapn0, "N", 0, 1],
                'E': [fakemape0, "E", 1, 0],
                'S': [fakemaps0, "S", 0, 0],
                'W': [fakemapw0, "W", 0, 0], }

    write_unfound = True
    if write_unfound:
        uname = f"{g_base}.unfound"
    else:
        uname = "/dev/stdout"
    with open(uname, "w", encoding='ascii') as ufile:

        if write_unfound:
            log(f"{elapsed()} Writing {uname}\n")
            ufile.write("# SINK: SOURCES...\n\n")
            for t, m in g_muxes:
                for f in m:
                    ufile.write(f"Pattern ({t}): {f}\n")
            ufile.write("\n# Not found: pattern_node x y\n\n")

        # stamp mux edges at each x,y coordinate
        clipped = 0
        uses = 0
        for y in g_ycoords:
            yidx     = 3 * (1 if y == g_ycoords[0] else 2 if y == g_ycoords[-1] else 0)
            for x in g_xcoords:
                xidx = 1 * (1 if x == g_xcoords[0] else 2 if x == g_xcoords[-1] else 0)
                # 0 will be center and all others will be edges or corners
                idx = yidx + xidx

                # change from 0 (clb) to 9 (non-clb) based on floorplan
                if not idx and g_xy2bidwh[(x, y)][0] != g_clb_id: idx = 9

                m = g_muxes[idx][1]
                for f in m:
                    (beg, *ends) = f
                    bnid0 = wire2node(beg, x, y)
                    if bnid0 < 0:
                        key = (beg, x, y)
                        if write_unfound and key not in beg_unfound:
                            ufile.write(f"Not found: BEG {beg} @{x},{y}\n")
                        beg_unfound.add(key)
                        continue

                    # figure out what type of switch
                    if beg[0] == 'I':
                        swids = str(g_inp2swid)
                    elif beg[0] in 'ENWS':
                        # NB we step up the length
                        if g_commercial:
                            swids = str(g_len2swid[    int(beg[1])])
                        else:
                            swids = str(g_len2swid[1 + int(beg[1])])
                    else:
                        assert 0

                    for end in ends:

                        bnid = bnid0
                        # find the end node which drives the beg mux
                        enid = wire2node(end, x, y)
                        if enid < 0:
                            key = (end, x, y)
                            if write_unfound and key not in end_unfound:
                                ufile.write(f"Not found: END {end} @{x},{y}\n")
                            end_unfound.add(key)
                            continue

                        # Decision: interpose a "fake" connection or two to avoid
                        # OpenFPGA choking on deviation from VPR generation rules?

                        (fcase1, fcase2) = ("", "")
                        if not g_commercial:
                            #
                            # ####
                            # Case: illegal (Output --> Input) path : add N0/E0 & maybe W0/S0
                            # ####
                            #
                            if   end[0] == 'O' and beg[0] == 'I':
                                (ssg, dsg) = (g_nid2info[enid][6], g_nid2info[bnid][6])
                                if   ssg == 'RIGHT':    fcase1 = "N"
                                elif ssg == 'TOP':      fcase1 = "E"
                                else:                   assert 0, \
    f"end={end} enid={enid} info={g_nid2info[enid]}"
                                if ssg != dsg:
                                    if   dsg == 'RIGHT':    fcase1 += "S"
                                    elif dsg == 'TOP':      fcase1 += "W"
                                    else:                   assert 0, \
    f"beg={beg} bnid={bnid} info={g_nid2info[bnid]}"
                            #
                            # ####
                            # Case: illegal (Output --> (clipped)SB) path : add S0 or W0
                            # Case: illegal (Output --> SB) path : add N0 or E0
                            # ####
                            #
                            elif end[0] == 'O':
                                ssg = g_nid2info[enid][6]
                                (dxl, dyl, _, _, _, _, _) = g_nid2info[bnid]
                                # clipped subcases
                                if   beg[0] == 'E' and x == dxl:
                                    assert x == 1
                                    clipped += 1
                                    if ssg == 'RIGHT':  fcase1 = "NW"   # W-->beg=E is 180
                                    elif ssg == 'TOP':  fcase1 = ""
                                    else:               assert 0, \
    f"end={end} enid={enid} info={g_nid2info[enid]}"
                                elif beg[0] == 'N' and y == dyl:
                                    assert y == 1
                                    clipped += 1
                                    if ssg == 'RIGHT':  fcase1 = ""
                                    elif ssg == 'TOP':  fcase1 = "ES"   # S-->beg=N is 180
                                    else:               assert 0, \
    f"end={end} enid={enid} info={g_nid2info[enid]}"
                                # remaining subcases
                                elif ssg == 'RIGHT':
                                    if beg[0] != 'S':   fcase1 = "N"
                                elif ssg == 'TOP':
                                    if beg[0] != 'W':   fcase1 = "E"
                                else:                   assert 0, \
    f"end={end} enid={enid} info={g_nid2info[enid]}"
                            #
                            # ####
                            # Case: illegal (SB --> Input) path : add S0 or W0
                            # ####
                            #
                            elif beg[0] == 'I':
                                dsg = g_nid2info[bnid][6]
                                if   dsg == 'RIGHT':
                                    if end[0] not in 'NS':  fcase1 = "S"
                                elif dsg == 'TOP':
                                    if end[0] not in 'EW':  fcase1 = "W"
                                else:                       assert 0, \
    f"beg={beg} bnid={bnid} info={g_nid2info[bnid]}"
                                assert end[2] != 'B'    # FIXME
                            #
                            # ####
                            # Case: illegal (SB --> (clipped)SB) path : add S0 or W0
                            # ####
                            #
                            else:
                                # all subcases are clipped subcases
                                (dxl, dyl, _, _, _, _, _) = g_nid2info[bnid]
                                if   beg[0] == 'E' and x == dxl:
                                    assert x == 1
                                    clipped += 1
                                    fcase1 = "W"                        # W-->beg=E is 180
                                elif beg[0] == 'N' and y == dyl:
                                    assert y == 1
                                    clipped += 1
                                    fcase1 = "S"                        # S-->beg=N is 180

                        #log(f"{end} --> {beg} fake={fake}\n")

                        # Execute: add L0(s) if needed

                        # replace upstream enid with new interposed n0/e0
                        # happens when upstream is OPIN so want swid0
                        for fd in fcase1:
                            uses += 1
                            assert fd in "NESW"
                            (fm, _, xd, yd) = choose[fd]
                            # map to L0s must be same across tiles
                            mid = fm.get(end, f"{fd}0E{len(fm)}")
                            fm[end] = mid
                            assert len(fm) <= 208  # FIXME
                            # node is tile-specific
                            (x2, y2) = (x - xd, y - yd)
                            fnid = wire2node(mid, x2, y2)
                            if fnid < 0:
                                key = (mid, x2, y2)
                                if write_unfound and key not in end_unfound:
                                    ufile.write(f"Not found: MID {mid} @{x2},{y2}\n")
                                end_unfound.add(key)
                                continue
                            # create the *swid0* "mux1" if not created already
                            if (enid, fnid) not in alledges:
                                edges.append(["edge",
                                    {   'src_node':     enid,
                                        'sink_node':    fnid,
                                        'switch_id':    int(swid0), }, ])
                                alledges.add((enid, fnid))
                            enid = fnid
                            end = mid   # if both cases active

                        # replace downstream bnid with new interposed s0/w0
                        # happens when downstream is IPIN but still want swids
                        if fcase2 != "":
                            uses += 1
                            assert fcase2 in "SW"
                            (fm, fd, _, _) = choose[fcase2]
                            # map to L0s must be same across tiles
                            mid = fm.get(end, f"{fd}0E{len(fm)}")
                            fm[end] = mid
                            assert len(fm) <= 208  # FIXME
                            # node is tile-specific
                            fnid = wire2node(mid, x, y)
                            if fnid < 0:
                                key = (mid, x, y)
                                if write_unfound and key not in end_unfound:
                                    ufile.write(f"Not found: MID {mid} @{x},{y}\n")
                                end_unfound.add(key)
                                continue
                            # create the *swids* "mux1" if not created already
                            if (fnid, bnid) not in alledges:
                                edges.append(["edge",
                                    {   'src_node':     fnid,
                                        'sink_node':    bnid,
                                        'switch_id':    int(swids), }, ])
                                alledges.add((fnid, bnid))
                            bnid = fnid
                            swid = swid0    # preceding is now swid0
                        else:
                            swid = swids    # default behavior

                        # NORMAL connection always

                        # EDGE: enid --> bnid
                        if (enid, bnid) not in alledges:
                            edges.append(["edge",
                                {   'src_node':     enid,
                                    'sink_node':    bnid,
                                    'switch_id':    int(swid), }, ])
                            alledges.add((enid, bnid))

                        # possible things we just did:
                        # legal connection with no alteration
                        #   enid >-swids-> bnid
                        # OPIN illegally driving SB
                        #   enid/OPIN >-swid0-> fake1 >-swids-> bnid
                        # SB illegally driving IPIN
                        #   enid >-swid0-> fake2 >-swids-> bnid/IPIN
                        # OPIN illegally driving IPIN, same block side
                        #   enid/OPIN >-swid0-> fake1 >-swids-> bnid/IPIN
                        # OPIN illegally driving IPIN, different block side
                        #   enid/OPIN >-swid0-> fake1 >-swid0-> fake2 >-swids-> bnid/IPIN
                        # we insert 0, 1, or 2 additional nodes: fake1 and/or fake2.
                        # (if OPIN/IPIN don't appear, the node is neither)

                        added += 1
                        # don't track S0s in mux stats since later eaten
                        size[bnid].add(enid)
                        load[enid].add(bnid)

                    # for end
                # for f
            # for x
        # for y
    # with
    if clipped:
        log(f" - {clipped:,d} jumper uses to clipped INC_DIRs\n")
    log(
f" - Edges: {drop:,d} dropped, {save:,d} saved, {added:,d} added, {uses:,d} jumper uses\n")

    # dump out the fakemaps
    fakename = f"{g_base}.jumpmap"
    with open(fakename, "w", encoding='ascii') as ffile:
        log(f"{elapsed()} Writing {fakename}\n")
        for fm in "NSEW":
            (fakemap, _, _, _) = choose[fm]
            for end in sorted(fakemap, key=dict_aup_ndn):
                mid = fakemap[end]
                ffile.write(f"{end}\t{mid}\n")

    # report on routing ports not used by blocks not needing them
    if beg_unfound or end_unfound:
        b = len(beg_unfound)
        e = len(end_unfound)
        log(
f" - Some blocks don't use all routing connections: beg={b:,d} end={e:,d}\n")

    # here we remove edges with invalid nids. These are mistakes I need to remove.
    startsize = len(edges)
    keep_edges = []
    for edge in child_nodes(edges):
        if is_text(edge):
            continue
        h = attr2hash(edge)
        src_nid = h['src_node']
        dst_nid = h['sink_node']
        if src_nid in g_nid2info and dst_nid in g_nid2info:
            keep_edges.append(edge)
    del edges[2:]
    edges.extend(keep_edges)
    endsize = len(edges)
    if startsize != endsize:
        log(f"Removed {startsize - endsize} edges with invalid NIDs\n")

    # here we remove duplicate edges.
    # these shouldn't appear so I dump them out as well.
    startsize = len(edges)
    # determiine repetition count of all edges
    edge_reps = defaultdict(int)
    for edge in child_nodes(edges):
        if is_text(edge):
            continue
        h = attr2hash(edge)
        src_nid = h['src_node']
        dst_nid = h['sink_node']
        key = (src_nid, dst_nid)
        edge_reps[key] += 1
    # for
    # report unusual counts
    for key in edge_reps:
        if edge_reps[key] <= 1: continue
        d = edge_reps[key]
        log(f"Removing duplicates of this edge ({d} duplicates)\n")
        for nid in key:
            if nid in g_nid2info:
                log(f" - {g_nid2info[nid]}\n")
            else:
                log(f" - nid={nid}\n")
    # for
    # remove duplicates
    keep_edges = []
    for edge in child_nodes(edges):
        if is_text(edge):
            continue
        h = attr2hash(edge)
        src_nid = h['src_node']
        dst_nid = h['sink_node']
        key = (src_nid, dst_nid)
        edge_reps[key] -= 1
        assert edge_reps[key] >= 0
        if edge_reps[key] == 0:
            keep_edges.append(edge)
    del edges[2:]
    edges.extend(keep_edges)
    endsize = len(edges)
    if startsize != endsize:
        log(f"Started with {startsize} edges and ended with {endsize} edges\n")

    # check for undriven and/or dangling nodes
    odegree = defaultdict(int)  # sources
    idegree = defaultdict(int)  # sinks
    src2dst = defaultdict(set)  # source --> sinks
    dst2src = defaultdict(set)  # sink --> sources
    for edge in child_nodes(edges):
        if is_text(edge): continue
        h = attr2hash(edge)
        src_nid = h['src_node']
        dst_nid = h['sink_node']
        odegree[src_nid] += 1
        idegree[dst_nid] += 1
        src2dst[src_nid].add(dst_nid)
        dst2src[dst_nid].add(src_nid)
    undriven = [nid for nid in odegree if nid not in idegree and
        g_nid2info[nid][5].startswith('CHAN')]   # sources
    dangling = [nid for nid in idegree if nid not in odegree and
        g_nid2info[nid][5].startswith('CHAN')]   # sinks
    nu0 = len(undriven)
    nd0 = len(dangling)
    nu = nd = 0
    if g_undriven_ok: undriven = []
    if g_dangling_ok: dangling = []
    remove_edge = defaultdict(int)
    while undriven or dangling:
        if undriven:
            src_nid = undriven.pop()
            for dst_nid in src2dst[src_nid]:
                remove_edge[(src_nid, dst_nid)] = 0
                idegree[dst_nid] -= 1
                assert idegree[dst_nid] >= 0
                if idegree[dst_nid]: continue
                if not g_nid2info[dst_nid][5].startswith('CHAN'): continue
                undriven.append(dst_nid)
                nu += 1
            # for dst_nid
        if dangling:
            dst_nid = dangling.pop()
            for src_nid in dst2src[dst_nid]:
                remove_edge[(src_nid, dst_nid)] = 0
                odegree[src_nid] -= 1
                assert odegree[src_nid] >= 0
                if odegree[src_nid]: continue
                if not g_nid2info[src_nid][5].startswith('CHAN'): continue
                dangling.append(src_nid)
                nd += 1
            # for src_nid
    # while
    keep_edges = []
    e0 = tx = rm = ke = 0
    se = len(edges)
    for edge in child_nodes(edges):
        e0 += 1
        if is_text(edge):
            tx += 1
            continue
        h = attr2hash(edge)
        src_nid = h['src_node']
        dst_nid = h['sink_node']
        if (src_nid, dst_nid) in remove_edge:
            remove_edge[(src_nid, dst_nid)] += 1
            rm += 1
            continue
        keep_edges.append(edge)
        ke += 1
    # for
    del edges[2:]
    edges.extend(keep_edges)
    rmhist = defaultdict(int)
    for e in remove_edge:
        rmhist[remove_edge[e]] += 1
    log(
f" - Cut {rm:,d} edges to disconnect " +
f"{nu0:,d}+{nu:,d} undriven and {nd0:,d}+{nd:,d} dangling CHAN\n")
    if g_false: log(
f" - DEBUG: tx={tx} rm={rm} ke={ke} e0={e0} len(e)={len(edges)} " +
f"se={se} len(r_e)={len(remove_edge)} rmhist={rmhist}\n")
    assert tx + rm + ke == e0
    assert len(edges) == 2 + ke
    assert rm == len(remove_edge)

    hist = defaultdict(int)
    for v in idegree.values():
        if v: hist[v] += 1
    hist = ' '.join([f"s{c:,d}#{hist[c]:,d}" for c in sorted(hist, key=lambda c:       c )])
    log(f" - Mux size histogram: {hist}\n")

    hist = defaultdict(int)
    for v in odegree.values():
        if v: hist[v] += 1
    hist = ' '.join([f"s{c:,d}#{hist[c]:,d}" for c in sorted(hist, key=lambda c:       c )])
    log(f" - Mux load histogram: {hist}\n")

    # drop CHANX/CHANY that have no edges
    if g_drop_iso_nodes:

        # determine all nodes touched by edges
        touched = set()
        for edge in child_nodes(edges):
            if is_text(edge): continue
            h = attr2hash(edge)
            touched.add(h['src_node'])
            touched.add(h['sink_node'])
        # for

        # crunch away untouched CHANX/Y
        oldnodes = sorted(g_nid2info)
        assert oldnodes[0] == 0 and oldnodes[-1] == len(oldnodes) - 1
        nextid = 0
        old2new = {}
        removed = set()
        for nid in oldnodes:
            if g_nid2info[nid][5].startswith("CHAN") and nid not in touched:
                removed.add(nid)
                continue
            old2new[nid] = nextid
            g_nid2info[nextid] = g_nid2info[nid]
            nextid += 1
        # for

        # empty out end of list
        rm = len(oldnodes) - nextid
        while nextid < len(oldnodes):
            del g_nid2info[nextid]
            nextid += 1
        # while

        # update other tables

        # update rr_nodes section
        prevnodes = None
        for nn in get_elements_by_tag_name(dom_tree, "rr_nodes", 2):
            prevnodes = nn
        assert prevnodes
        keepnodes = []
        for node in child_nodes(prevnodes):
            if is_text(node) or node[0] != "node": continue
            nid = node[1]['id']
            assert (nid in removed) != (nid in old2new)
            if nid in old2new:
                node[1]['id'] = old2new[nid]
                keepnodes.append(node)
        # for node
        del prevnodes[2:]
        prevnodes.extend(keepnodes)

        # update g_xyt2nid
        for xyt, nid in g_xyt2nid.items():
            g_xyt2nid[xyt] = old2new[nid]
        # for

        # update g_xydl2tn
        for xydl, pairs in g_xydl2tn.items():
            g_xydl2tn[xydl] = [(p[0], old2new[p[1]]) for p in pairs if p[1] not in removed]
        # for

        # update rr_edges section
        for edge in child_nodes(edges):
            if is_text(edge): continue
            h = attr2hash(edge)
            h['src_node' ] = old2new[h['src_node' ]]
            h['sink_node'] = old2new[h['sink_node']]
        # for
        log(f" - Removed {rm:,d} isolated CHANX/Y nodes\n")
    # if
# def edit_xml

def check_rules(dom_tree, us=False, commercial=False):
    """check current graph against VPR/OpenFPGA rules"""
    log(f"{elapsed()} Checking graph against VPR/OpenFPGA rules\n")
    intsd = { ('IPIN', 'SINK'), ('SOURCE', 'OPIN') }
    internal = wire = inp = badinp = nout = badout = badother = 0

    if not us:
        # discovered strange VPR behavior
        ql = 1; qh = 1   # DEC_DIR into turn: allowed tap points shift
    else:
        ql = 0; qh = 0

    # in our pattern, edge (io) INC_DIR driven from same coordinate after turn
    ei = us
    ei = False  # we fixed this
    # in our pattern, IOs flip around some wires
    ef = us
    ef = False  # we fixed this; ran this way with genb/g_safe
    ef = True   # now needed and not directly illegal; OpenFPGA takes it
    # in our pattern, L0s (S0s and W0s) inserted & could drive opposite direction
    r0 = us     # MUST STAY

    # 1 to allow INC_DIR to stitch one beyond the end
    ids = 1
    ids = 1

    if commercial:
        sgid0 = -1  # never recognize L0s since don't exist
        ql = 1; qh = 1   # DEC_DIR into turn: allowed tap points shift
        cm = 1
        ei = True
    else:
        sgid0 = int(g_len2sgid[1 + 0])    # segment id for L0
        cm = 0
    bb =  g_grid_bbox   # xl, yl, xh, yh

    # SB --> IPIN; ax=0 E/W ax=1 N/S
    if commercial:
        chan_ipin_ok = {
            # src      dest          ax d  s  w
            ('CHANE', 'TOP'):       (0, 0, 0, 0),
            ('CHANE', 'RIGHT'):     (0, 0, 0, 1),
            ('CHANW', 'TOP'):       (0, 0, 1, 2),
            ('CHANW', 'RIGHT'):     (0, 0, 1, 3),
            ('CHANN', 'TOP'):       (1, 0, 0, 4),
            ('CHANN', 'RIGHT'):     (1, 0, 0, 5),
            ('CHANS', 'TOP'):       (1, 0, 1, 6),
            ('CHANS', 'RIGHT'):     (1, 0, 1, 7),
        }
    else:
        chan_ipin_ok = {
            # src      dest         ax  d  s  w
            ('CHANE', 'TOP'):       (0, 0, 0, 0),
            ('CHANE', 'BOTTOM'):    (0, 1, 0, 1),
            ('CHANW', 'TOP'):       (0, 0, 0, 2),
            ('CHANW', 'BOTTOM'):    (0, 1, 0, 3),
            ('CHANN', 'RIGHT'):     (1, 0, 0, 4),
            ('CHANN', 'LEFT'):      (1, 1, 0, 5),
            ('CHANS', 'RIGHT'):     (1, 0, 0, 6),
            ('CHANS', 'LEFT'):      (1, 1, 0, 7),
        }

    # OPIN --> SB; di=0 INC_DIR di=1 DEC_DIR
    if commercial:
        opin_chan_ok = {
            # src        dest       di  dx  dy  w
            ('RIGHT',   'CHANE'):   (0, -1,  0, 0),
            ('RIGHT',   'CHANW'):   (1,  0,  0, 1),
            ('RIGHT',   'CHANN'):   (0,  0, -1, 2),
            ('RIGHT',   'CHANS'):   (1,  0,  0, 3),
            ('TOP',     'CHANE'):   (0, -1,  0, 4),
            ('TOP',     'CHANW'):   (1,  0,  0, 5),
            ('TOP',     'CHANN'):   (0,  0, -1, 6),
            ('TOP',     'CHANS'):   (1,  0,  0, 7),
        }
    else:
        opin_chan_ok = {
            # src        dest       di  dx  dy  w
            ('RIGHT',   'CHANN'):   (0,  0,  0, 0),
            ('RIGHT',   'CHANS'):   (1,  0,  0, 1),
            ('TOP',     'CHANE'):   (0,  0,  0, 2),
            ('TOP',     'CHANW'):   (1,  0,  0, 3),
            ('LEFT',    'CHANN'):   (0,  1,  0, 4),
            ('LEFT',    'CHANS'):   (1,  1,  0, 5),
            ('BOTTOM',  'CHANE'):   (0,  0,  1, 6),
            ('BOTTOM',  'CHANW'):   (1,  0,  1, 7),
        }

    for rr_edges in get_elements_by_tag_name(dom_tree, "rr_edges", 2):
        for edge in child_nodes(rr_edges):
            if is_text(edge) or edge[0] != "edge": continue
            h = attr2hash(edge)
            src_nid = h['src_node']
            dst_nid = h['sink_node']
            (sxl, syl, sxh, syh, _, st, ssg) = g_nid2info[src_nid]
            (dxl, dyl, dxh, dyh, _, dt, dsg) = g_nid2info[dst_nid]

            # connections we don't really use
            if (st, dt) in intsd:
                internal += 1
                continue
            # SB --> IPIN
            if dt == "IPIN":
                if st == "OPIN":
                    # OPIN --> IPIN ok if orthogonal distance at most 1
                    if abs(sxl - dxl) + abs(syl - dyl) <= 1:
                        inp += 1
                    else:
                        badinp += 1
                    continue
                (ax, d, s, w) = chan_ipin_ok.get((st, dsg), (2, 0, 0, 88))
                if   ax == 0 and syl + d == dyl and sxl <= dxl + s <= sxh:
                    inp += 1
                elif ax == 1 and sxl + d == dxl and syl <= dyl + s <= syh:
                    inp += 1
                else:
                    badinp += 1
                    # this happens if there are TOP pins there shouldn't be
                    if  g_false and \
                        st == "CHANN" and sxl == sxh and syl == syh and dsg == "TOP":
                        continue
                    log(f"Warning: unrecognized CHAN-->IPIN connection ({w}):\n")
                    log(f"\t(sxl, syl, sxh, syh, sptc, st, ssg) = {g_nid2info[src_nid]}\n")
                    log(f"\t(dxl, dyl, dxh, dyh, dptc, dt, dsg) = {g_nid2info[dst_nid]}\n")
                continue
            # OPIN --> SB
            if st == "OPIN":
                (di, dx, dy, w) = opin_chan_ok.get((ssg, dt), (2, 0, 0, 88))
                if commercial:
                    if (sxl == 1 and dxl == 1 and dt == "CHANE" or
                        syl == 1 and dyl == 1 and dt == "CHANN"):
                        dx = 0 ; dy = 0
                if   di == 0 and sxl == dxl + dx and syl == dyl + dy:
                    nout += 1
                elif di == 1 and sxh == dxh + dx and syh == dyh + dy:
                    nout += 1
                else:
                    badout += 1
                    # this happens if there are TOP pins there shouldn't be
                    if  g_false and \
                        dt == "CHANN" and dxl == dxh and dyl == dyh and ssg == "TOP":
                        continue
                    log(f"Warning: unrecognized OPIN-->CHAN connection ({w}):\n")
                    log(f"\t(sxl, syl, sxh, syh, sptc, st, ssg) = {g_nid2info[src_nid]}\n")
                    log(f"\t(dxl, dyl, dxh, dyh, dptc, dt, dsg) = {g_nid2info[dst_nid]}\n")
                continue
            # SB --> SB
            if st.startswith("CHAN") and dt.startswith("CHAN"):
                chan_chan_ok = {
                    ('CHANE', 'CHANN'): 0,
                    ('CHANW', 'CHANN'): 1,
                    ('CHANE', 'CHANS'): 2,
                    ('CHANW', 'CHANS'): 3,

                    ('CHANN', 'CHANE'): 4,
                    ('CHANS', 'CHANE'): 5,
                    ('CHANN', 'CHANW'): 6,
                    ('CHANS', 'CHANW'): 7,

                    ('CHANE', 'CHANE'): 8,
                    ('CHANW', 'CHANW'): 9,
                    ('CHANN', 'CHANN'): 10,
                    ('CHANS', 'CHANS'): 11,

                    ('CHANE', 'CHANW'): 12,
                    ('CHANW', 'CHANE'): 13,
                    ('CHANN', 'CHANS'): 14,
                    ('CHANS', 'CHANN'): 15,
                }
                check = chan_chan_ok.get((st, dt), 88)
                if   check == 0:
                    # CHANE => CHANN
                    if sxl <= dxl <= sxh and syl + 1 == dyl:
                        wire += 1; continue
                    if sxl <= dxl <= sxh and syl     == dyl and ei and syl == bb[1]:
                        wire += 1; continue
                elif check == 1:
                    # CHANW => CHANN
                    if sxl - ql <= dxl <= sxh - qh and syl + 1 == dyl:
                        wire += 1; continue
                    if sxl - ql <= dxl <= sxh - qh and syl     == dyl and ei and syl == bb[1]:
                        wire += 1; continue
                elif check == 2:
                    # CHANE => CHANS
                    if sxl <= dxl <= sxh and syl     == dyh:
                        wire += 1; continue
                elif check == 3:
                    # CHANW => CHANS
                    if sxl - ql <= dxl <= sxh - qh and syl     == dyh:
                        wire += 1; continue
                elif check == 4:
                    # CHANN => CHANE
                    if sxl + 1 == dxl and syl <= dyl <= syh:
                        wire += 1; continue
                    if sxl     == dxl and syl <= dyl <= syh and ei and sxl == bb[0]:
                        wire += 1; continue
                elif check == 5:
                    # CHANS => CHANE
                    if sxl + 1 == dxl and syl - ql <= dyl <= syh - qh:
                        wire += 1; continue
                    if sxl     == dxl and syl - ql <= dyl <= syh - qh and ei and sxl == bb[0]:
                        wire += 1; continue
                elif check == 6:
                    # CHANN => CHANW
                    if sxl     == dxh and syl <= dyl <= syh:
                        wire += 1; continue
                elif check == 7:
                    # CHANS => CHANW
                    if sxl     == dxh and syl - ql <= dyl <= syh - qh:
                        wire += 1; continue
                elif check == 8:
                    # CHANE => CHANE
                    # pylint: disable-next=chained-comparison
                    if sxl < dxl and sxh + ids >= dxl and syl == dyl:
                        wire += 1; continue
                elif check == 9:
                    # CHANW => CHANW
                    if sxh > dxh and sxl <= dxh + 1 and syl == dyl:
                        wire += 1; continue
                elif check == 10:
                    # CHANN => CHANN
                    # pylint: disable-next=chained-comparison
                    if syl < dyl and syh + ids >= dyl and sxl == dxl:
                        wire += 1; continue
                elif check == 11:
                    # CHANS => CHANS
                    if syh > dyh and syl <= dyh + 1 and sxl == dxl:
                        wire += 1; continue
                elif check == 12:
                    # CHANE => CHANW
                    if ef and sxh == bb[2] and dxh == bb[2] and syl == dyl:
                        wire += 1; continue
                elif check == 13:
                    # CHANW => CHANE
                    if ef and sxl == bb[0] + cm and dxl == bb[0] and syl == dyl:
                        wire += 1; continue
                    if r0  and ssg == sgid0 and dsg != sgid0 and syl == dyl and sxl+1 == dxl:
                        wire += 1; continue
                elif check == 14:
                    # CHANN => CHANS
                    if ef and syh == bb[3] and dyh == bb[3] and sxl == dxl:
                        wire += 1; continue
                elif check == 15:
                    # CHANS => CHANN
                    if ef and syl == bb[1] + cm and dyl == bb[1] and sxl == dxl:
                        wire += 1; continue
                    if r0  and ssg == sgid0 and dsg != sgid0 and sxl == dxl and syl+1 == dyl:
                        wire += 1; continue
            else:
                # it's not CHAN* --> CHAN*
                assert 0
            # not previously recognized
            log(f"Warning: unrecognized CHAN-->CHAN connection ({check}):\n")
            log(f"\t(sxl, syl, sxh, syh, sptc, st, ssg) = {g_nid2info[src_nid]}\n")
            log(f"\t(dxl, dyl, dxh, dyh, dptc, dt, dsg) = {g_nid2info[dst_nid]}\n")
            badother += 1
        # for edge
    # for rr_edges
    log(f" - {badinp:,d}I+{badout:,d}O+{badother:,d}W bad connections, " +
        f"{inp:,d}I+{nout:,d}O+{wire:,d}W good connections, {internal:,d} internal\n")
    #sys.exit(1)
# def check_rules

def write_gsbs(rrg):
    """write out all GSBs for all coordinates"""

    log(f"{elapsed()} Writing GSB files\n")
    nfile = set()

    intsd = { ('IPIN', 'SINK'), ('SOURCE', 'OPIN') }

    # these serve as sinks
    chanx = defaultdict(set)    # (x, y, ptc) ==> snid
    chany = defaultdict(set)    # (x, y, ptc) ==> snid

    # these serve as sources
    # (x, y) ==> (ty, ptc) ==> {(snid, dnid, swid)}
    muxes = defaultdict(lambda: defaultdict(set))
    # meaning: at x,y, there's a ty launching from ptc driven from edges {(snid, dnid, swid)}
    # if NOT for that (ty, ptc), then you have a feedthrough

    opin_displace = {
        # CHANS y = 0, CHANN y = 1
        # CHANW x = 0, CHANE x = 1
        'RIGHT':        {'CHANN': (0, 1), 'CHANS': (0, 0), },
        'TOP':          {'CHANE': (1, 0), 'CHANW': (0, 0), },
        'LEFT':         {'CHANN': (1, 1), 'CHANS': (1, 0), },
        'BOTTOM':       {'CHANE': (1, 1), 'CHANW': (0, 1), },
    }

    ipins = defaultdict(set)
    opins = defaultdict(lambda: defaultdict(set))
    # (x, y) --> sgid --> dt --> {snids}
    #   becomes
    # (x, y) --> sgid --> dt --> snid --> idx
    opin2index = defaultdict(lambda: defaultdict(set))
    opin2info = {}
    for rr_edges in get_elements_by_tag_name(rrg, "rr_edges", 1):
        for edge in child_nodes(rr_edges):
            if is_text(edge) or edge[0] != "edge": continue
            h = attr2hash(edge)
            snid = h['src_node']
            dnid = h['sink_node']
            swid = h['switch_id']
            # don't need sgid
            (sxl, syl, sxh, syh, sptc, st, sgid) = g_nid2info[snid]
            (dxl, dyl, dxh, dyh, dptc, dt, dgid) = g_nid2info[dnid]

            # Case 1: ignore edges inside tiles
            if (st, dt) in intsd: continue

            # Case 2: CHAN* --> IPIN
            if dt == 'IPIN':
                if st == 'OPIN': continue
                assert st.startswith('CHAN')

                # BOTTOM cbx.Y = clb.Y - 1
                # LEFT   cby.X = clb.X - 1
                if dgid == "BOTTOM":
                    dyl -= 1
                if dgid == "LEFT":
                    dxl -= 1

                ipins[(dxl, dyl)].add((snid, dnid, swid))
                continue

            # Case 3: OPIN --> CHAN*
            if st == 'OPIN':
                # ignore special nonroutable chained connections
                if dt == 'IPIN': continue
                assert sgid in opin_displace
                assert dt.startswith('CHAN')
                assert dt in opin_displace[sgid]
                (x, y) = opin_displace[sgid][dt]
                (x, y) = (sxl - x, syl - y) # convert pin coord to SB coord
                opins[(x, y)][dnid].add((snid, dnid, swid))
                # @@
                #opin2index[(x, y)][sgid].add(snid)
                opin2index[(sxl, syl)][sgid].add(snid)

                # save info we will need later
                if snid in opin2info: continue
                (x, y, _, _, ptc, _, _) = g_nid2info[snid]
                (bid, wo, ho) = g_xy2bidwh[(x, y)]
                (pname, _) = g_bid2info[bid]['pid2info'][ptc]
                parts = pname.split('.')
                assert 1 <= len(parts) <= 2
                subtile = 0
                if len(parts) == 2:
                    (mod, pname) = parts
                    g = re.search(r'\[(\d+)\]$', mod)
                    if g:
                        subtile = int(g.group(1))
                else:
                    pname = parts[0]
                pname = re.sub(r'\W', '_', pname)
                opin2info[snid] = (wo, ho, subtile, pname)
            else:
                # Case 4: CHAN* --> CHAN*
                assert st.startswith('CHAN') and dt.startswith('CHAN')

            # Case 3/4: common code

            # FIND WHERE dst MUX LIVES
            if dt == 'CHANE':
                (x, y) = (dxl - 1, dyl)
                ptc = dptc
            if dt == 'CHANN':
                (x, y) = (dxl, dyl - 1)
                ptc = dptc
            if dt == 'CHANW':
                (x, y) = (dxh, dyh)
                dptc += ((dxl - dxh) + (dyl - dyh)) * g_twisted
                ptc = dptc
            if dt == 'CHANS':
                (x, y) = (dxh, dyh)
                dptc += ((dxl - dxh) + (dyl - dyh)) * g_twisted
                ptc = dptc
            muxes[(x, y)][(dt, ptc)].add((snid, dnid, swid))

            # Case 4: CHAN* --> CHAN*
            # mark channels this flows through
            # used ONLY for feedthrough generation
            if st == 'CHANE':
                for i in range(1 + sxh - sxl):
                    x = i + sxl - 1
                    ptc = sptc + (i + 1) * g_twisted
                    key3 = (x, syl, ptc)
                    chanx[key3].add(snid)
                    #log(f"Added chan at {key3}\n")
                # for
            if st == 'CHANW':
                for i in range(1 + sxh - sxl):
                    x = i + sxl
                    ptc = sptc - i * g_twisted  # MINUS
                    key3 = (x, syl, ptc)
                    chanx[key3].add(snid)
                    #log(f"Added chan at {key3}\n")
                # for
            if st == 'CHANN':
                for i in range(1 + syh - syl):
                    y = i + syl - 1
                    ptc = sptc + (i + 1) * g_twisted
                    key3 = (sxl, y, ptc)
                    chany[key3].add(snid)
                    #log(f"Added chan at {key3}\n")
                # for
            if st == 'CHANS':
                for i in range(1 + syh - syl):
                    y = i + syl
                    ptc = sptc - i * g_twisted  # MINUS
                    key3 = (sxl, y, ptc)
                    chany[key3].add(snid)
                    #log(f"Added chan at {key3}\n")
                # for
            # if
        # for edge
    # for rr_edges

    # convert sets of nids to a nid --> index mapping
    # also produce nid --> info database
    for key1 in opin2index:
        for sgid in opin2index[key1]:
            opin2index[key1][sgid] = { snid : i
                for i, snid in enumerate(sorted(opin2index[key1][sgid]), start=0) }
    # NB: index #s are specific to a GSB & side so we cannot share them.
    # should this be ALL pins on that side/chan, or just those connected?
    # now doing the latter. perhaps no difference in "proper" networks.
    # NB: with *all* pins on RIGHT, this seems to work. However, if pins are on
    # multiple sides, more complex sorting is needed. We will print in correct order,
    # but numeric indices will be wrong.

    #pin2order1 = { 'BOTTOM': 1, 'TOP': 0, 'LEFT': 1, 'RIGHT': 0, }
    #pin2order2 = { 'CHANE': 1, 'CHANW': 0, 'CHANN': 1, 'CHANS': 0, }
    # pin[6] x chan[5] --> order
    pinside2chan2order = {
        'TOP':      {   'CHANE':    0,  'CHANW':    1, },   # top
        'BOTTOM':   {   'CHANE':    1,  'CHANW':    0, },   # bottom
        'RIGHT':    {   'CHANN':    0,  'CHANS':    1, },   # right
        'LEFT':     {   'CHANN':    1,  'CHANS':    0, },   # left
    }

    # we need to sort opins for printing order
    for key1 in opins:
        for dnid in sorted(opins[key1]):
            opins[key1][dnid] = sorted(opins[key1][dnid],
                key=lambda p: ( pinside2chan2order  [ g_nid2info[p[0]][6] ]
                                                    [ g_nid2info[p[1]][5] ],
                                p[0] ) )

    # double-check no collisions in channels
    good = True
    for key in chanx:
        if len(chanx[key]) == 1: continue
        log(f"key = {key}  chanx[key] = {chanx[key]}\n")
        good = False
    for key in chany:
        if len(chany[key]) == 1: continue
        log(f"key = {key}  chany[key] = {chany[key]}\n")
        good = False
    assert good

    # double check no axis/ptc collision between muxes
    good = True
    n = 0
    for key1 in muxes:
        (x, y) = key1
        for key2 in muxes[key1]:
            (ty, ptc) = key2 
            dnids = { dnid for _, dnid, _ in muxes[key1][key2] }
            if len(dnids) <= 1: continue

            #if g_true: continue
            edges = sorted(muxes[key1][key2], key=lambda e: (e[1], e[0]))
            log(f"{n}: x,y={x},{y} ty={ty} ptc={ptc} muxes={edges}\n")
            for snid, dnid, swid in edges:
                (sxl, syl, sxh, syh, sptc, sty, ssgid) = g_nid2info[snid]
                (dxl, dyl, dxh, dyh, dptc, dty, dsgid) = g_nid2info[dnid]
                log(f"\tnid={snid} {sxl} {syl} {sxh} {syh} ptc={ptc} {sty} sgid={ssgid} " +
f"==> nid={dnid} {dxl} {dyl} {dxh} {dyh} ptc={dptc} {dty} sgid={dsgid}\n")
            n += 1
            good = False
        # for ty, ptc
    # for x,y
    assert good

    dirname = "gsb_stamp_dir"
    os.makedirs(dirname, mode=0o777, exist_ok=True)  # mkdir lacks exist_ok

    # ensure there are CBX/CBY files where ever there's an SB
    for key in muxes:
        if not key[0] or not key[1]: continue
        if len(ipins[key]) == 0: ipins[key].clear()

    # write out CBX/CBX and generate pin indices
    xy2xdnids = {}
    xy2ydnids = {}
    xset = set()
    yset = set()
    for x, y in ipins:
        xset.add(x)
        yset.add(y)
        # assign indices to pins and group CB inputs by dest
        cbx = defaultdict(set)
        cby = defaultdict(set)
        xdnids = set()
        ydnids = set()
        xydnids = defaultdict(set)  # set will change to list
        for snid, dnid, swid in ipins[(x, y)]:
            sty = g_nid2info[snid][5]
            if sty[-1] in 'EW':
                cbx[dnid].add(snid)
            else:
                cby[dnid].add(snid)
            dside = g_nid2info[dnid][6]
            xydnids[dside].add(dnid)
        for dnid in cbx:
            cbx[dnid] = sorted(cbx[dnid], key=lambda snid: g_nid2info[snid][4])
        for dnid in cby:
            cby[dnid] = sorted(cby[dnid], key=lambda snid: g_nid2info[snid][4])
        for dside in xydnids:
            xydnids[dside] = {
                dnid : i for i, dnid in enumerate(sorted(xydnids[dside]), start=0) }

        pinside2cbside = {
            'RIGHT':    'left',
            'TOP':      'bottom',
            'LEFT':     'right',
            'BOTTOM':   'top',
        }

        xfilename = f"{dirname}/cbx_{x}__{y}_.xml"
        yfilename = f"{dirname}/cby_{x}__{y}_.xml"
        with open(xfilename, "w", encoding='ascii') as xfile, \
             open(yfilename, "w", encoding='ascii') as yfile:
            nfile.add(xfilename)
            nfile.add(yfilename)
            for wfile, cb, chan, dirs in [
                [xfile, cbx, "CHANX",
                    [ ['BOTTOM', "left" ], ['TOP',   "left" ] ]     # I'll bet lowercase flips
                ],
                [yfile, cby, "CHANY",
                    [ ['LEFT',   "top"  ], ['RIGHT', "top"  ] ]     # ditto
                ], ]:
                wfile.write(f'<rr_cb x="{x}" y="{y}" num_sides="4">\n')
                for pinside, chside in dirs:
                    dnids = xydnids[pinside]
                    for dnid in sorted(dnids, key=lambda dnid: dnids[dnid]):
                        sz = len(cb[dnid])
                        if not sz: continue
                        (dxl, dyl, _, _, _, _, dside) = g_nid2info[dnid]

                        # HACK
                        cbside = pinside2cbside[dside]

                        wfile.write(
f'\t<IPIN side="{cbside}" index="{dnids[dnid]}" node_id="{dnid}" mux_size="{sz}">\n')
                        for snid in cb[dnid]:
                            (sxl, syl, _, _, index, st, sgid) = g_nid2info[snid]
                            # update index by distance for twisted case
                            if chan == "CHANX":
                                d = dxl - sxl
                            else:
                                d = dyl - syl
                            if st[-1] in 'WS':
                                d = -d
                            index += d * g_twisted
                            wfile.write(
f'\t\t<driver_node type="{chan}" side="{chside}" ' +
f'node_id="{snid}" index="{index}" segment_id="{sgid}"/>\n')
                        # for snid
                        wfile.write('\t</IPIN>\n')
                    # for dnid
                # for xx
                wfile.write('</rr_cb>\n')
            # for wfile
        # with

        # save for pin lookup when writing sb
        xy2xdnids[(x, y)] = xdnids
        xy2ydnids[(x, y)] = ydnids
    # for x, y

    # write out empty SBs and CBs
    xmax = max(xset)
    ymax = max(yset)
    ymaxp1 = ymax + 1
    for x, y  in {(x, 0) for x in xset} | {(0, y) for y in yset} | {(0, 0)}:
        # SBs: may be overwritten later
        sfilename = f"{dirname}/sb_{x}__{y}_.xml"
        with open(sfilename, "w", encoding='ascii') as sfile:
            nfile.add(sfilename)
            sfile.write(f'<rr_sb x="{x}" y="{y}" num_sides="4">\n')
            sfile.write('</rr_sb>\n')
        # with
        # CBXs
        xfilename = f"{dirname}/cbx_{x}__{y}_.xml"
        with open(xfilename, "w", encoding='ascii') as xfile:
            nfile.add(xfilename)
            xfile.write(f'<rr_cb x="{x}" y="{y}" num_sides="4">\n')
            xfile.write('</rr_cb>\n')
        # with
        # CBYs
        if y == 0: y = ymaxp1
        yfilename = f"{dirname}/cby_{x}__{y}_.xml"
        with open(yfilename, "w", encoding='ascii') as yfile:
            nfile.add(yfilename)
            yfile.write(f'<rr_cb x="{x}" y="{y}" num_sides="4">\n')
            yfile.write('</rr_cb>\n')
        # with
    # for

    # check for tracks not visited
    e = defaultdict(int)
    #or x in {0} | xset:
    for x in       xset:
        for y in yset:
            for p in range(g_chcnt):
                key = (x, y, p)
                if key in chanx: continue
                #log(f"Empty CHANX track: {x} {y} {p}\n")
                e[g_ptc2sgidname[p][1]] += 1
    if e: log(f" - {sum(e.values()):,d} untouched CHANX track units: {dict(e)}\n")
    e = defaultdict(int)
    #or y in {0} | yset:
    for y in       yset:
        for x in xset:
            for p in range(g_chcnt):
                key = (x, y, p)
                if key in chany: continue
                #log(f"Empty CHANY track: {x} {y} {p}\n")
                e[g_ptc2sgidname[p][1]] += 1
    if e: log(f" - {sum(e.values()):,d} untouched CHANY track units: {dict(e)}\n")

    portnames = { 'CHANE': 'chanx', 'CHANN': 'chany', 'CHANW': 'chanx', 'CHANS': 'chany', }
    missfeed = defaultdict(int)
    for key1 in muxes:
        (x, y) = key1
        sfilename = f"{dirname}/sb_{x}__{y}_.xml"
        with open(sfilename, "w", encoding='ascii') as sfile:
            nfile.add(sfilename)
            sfile.write(f'<rr_sb x="{x}" y="{y}" num_sides="4">\n')
            
            # redesign this data structure
            smuxes = defaultdict(lambda: defaultdict(set))
            for key2 in muxes[key1]:
                (ty, ptc) = key2
                for snid, dnid, swid in muxes[key1][key2]:
                    assert g_nid2info[dnid][5] == ty
                    #assert g_nid2info[dnid][4] == ptc  # FIXME
                    smuxes[ty][ptc].add((snid, dnid, swid))

            for dchan, chanmap, dty, exitside in [
                ["CHANY", chany, "CHANN", "top"],
                ["CHANX", chanx, "CHANE", "right"],
                ["CHANY", chany, "CHANS", "bottom"],
                ["CHANX", chanx, "CHANW", "left"]]:
                mymuxes = smuxes[dty]
                first = 1 if dty[-1] in "SW" else 0
                for ptc in range(first, g_chcnt, 2):
                    # do we have a mux?
                    if ptc in mymuxes:
                        # set of edges
                        myedges = mymuxes[ptc]
                        dnids = { dnid for _, dnid, _ in myedges }
                        assert len(dnids) == 1
                        dnid = list(dnids)[0]
                        (dxl, dyl, dxh, dyh, dptc, dty2, dsgid) = g_nid2info[dnid]

                        # for each triple, we compute what its adjusted source ptc is
                        # so we can sort by it
                        quads = []
                        for snid, dnid, swid in myedges:
                            (sxl, syl, sxh, syh, sptc, sty, ssgid) = g_nid2info[snid]
                            #@@
                            if sty == "OPIN": continue
                            # adjust sptc for twisting
                            if   sty[-1] in "EW" and dchan == "CHANY":
                                if sty[-1] == 'E':
                                    sptc += (    dxl - sxl) * g_twisted
                                else:
                                    sptc -= (1 + dxl - sxl) * g_twisted
                            elif sty[-1] in "NS" and dchan == "CHANX":
                                if sty[-1] == 'N':
                                    sptc += (dyl - syl    ) * g_twisted
                                else:
                                    sptc += (syl - dyl - 1) * g_twisted
                            elif sty[-1] in 'EN':
                                sptc += ((dxl - sxl) + (dyl - syl) - 1) * g_twisted
                            else:
                                sptc += ((sxl - dxh) + (syl - dyh) - 1) * g_twisted
                            quads.append((sptc, snid, dnid, swid))
                        quads = sorted(quads)

                        sgname = g_sgid2sgname[dsgid]
                        sz = len(myedges)
                        sz = len(quads) #@@
                        # increase count by # OPINs driving this node
                        sz += len(opins[(x, y)][dnid])
                        pname = f"{dchan.lower()}_{exitside}_out"
                        sfile.write(
f'\t<{dchan} side="{exitside}" index="{ptc}" node_id="{dnid}" segment_id="{dsgid}" ' +
f'segment_name="{sgname}" mux_size="{sz}" sb_module_pin_name="{pname}">\n')

                        enter2axis = {
                            'top':      'top_bottom',
                            'right':    'right_left',
                            'bottom':   'bottom_top',
                            'left':     'left_right',
                        }

                        # pin[6] x chan[5] --> edge
                        pinside2chan2edge = {
                            'RIGHT':    {   'CHANN':    'right',    'CHANS':    'right', },
                            'TOP':      {   'CHANE':    'top',      'CHANW':    'top', },
                            'LEFT':     {   'CHANN':    'left',     'CHANS':    'left', },
                            'BOTTOM':   {   'CHANE':    'bottom',   'CHANW':    'bottom', },
                        }

                        for sty, schan, enterside in [
                            ["CHANS", 'CHANY', "top"],
                            ["CHANW", 'CHANX', "right"],
                            ["CHANN", 'CHANY', "bottom"],
                            ["CHANE", 'CHANX', "left"]]:

                            # output pin sources
                            for snid, _, swid in opins[(x, y)][dnid]:
                                (sxl, syl, sxh, syh, sptc, sty2, ssgid) = g_nid2info[snid]
                                if enterside != exitside: continue
                                s = exitside
                                idx = opin2index[(sxl, syl)][ssgid][snid]
                                g = pinside2chan2edge[ssgid][dty2]
                                a = enter2axis[enterside]
                                (w, h, t, p) = opin2info[snid]
                                sfile.write(
f'\t\t<driver_node type="OPIN" side="{s}" index="{idx}" node_id="{snid}" grid_side="{g}" ' +
f'sb_module_pin_name="{a}_grid_{g}_width_{w}_height_{h}_subtile_{t}__pin_{p}"/>\n')
                            # for

                            # mux sources
                            for sptc2, snid, dnid, swid in quads:
                                (sxl, syl, sxh, syh, sptc, sty2, ssgid) = g_nid2info[snid]
                                if sty2 != sty:
                                    continue
                                # get remainder of info
                                sgid = g_nid2info[snid][6]
                                sgname = g_sgid2sgname[sgid]
                                pname = f"{portnames[g_nid2info[snid][5]]}_{enterside}_in"
                                sfile.write(
f'\t\t<driver_node type="{schan}" side="{enterside}" index="{sptc2}" node_id="{snid}" ' +
f'segment_id="{sgid}" segment_name="{sgname}" sb_module_pin_name="{pname}"/>\n')
                            # for snid, dnid, swid

                        # for sty, enterside
                        sfile.write(f'\t</{dchan}>\n')

                    # otherwise we have a feedthrough
                    else:
                        if not ptc & 1:
                            if x == xmax and dty == "CHANE": continue
                            if y == ymax and dty == "CHANN": continue
                        key3 = (x, y, ptc)

                        if key3 not in chanmap or not chanmap[key3]:
                            if ptc & 1:
                                if x == 0 and exitside == "left": continue
                                if y == 0 and exitside == "bottom": continue
                            #log(f" - Missing feedthough: {key1} {exitside} ptc={ptc}\n")
                            missfeed[g_ptc2sgidname[ptc][1]] += 1
                            continue

                        nid = list(chanmap[key3])[0]
                        sgid = g_nid2info[nid][6]
                        sname = g_sgid2sgname[sgid]

                        sz = 0
                        pname = f"{dchan.lower()}_{exitside}_out"
                        sfile.write(
f'\t<{dchan} side="{exitside}" index="{ptc}" node_id="{nid}" ' +
f'segment_id="{sgid}" segment_name="{sname}" mux_size="{sz}" sb_module_pin_name="{pname}">\n')
                        enterside = {
                            'right': 'left',
                            'top': 'bottom',
                            'left': 'right',
                            'bottom': 'top', }[exitside]
                        sptc = ptc - g_twisted

                        pname = f"{dchan.lower()}_{enterside}_in"
                        sfile.write(
f'\t\t<driver_node type="{dchan}" side="{enterside}" index="{sptc}" node_id="{nid}" ' +
f'segment_id="{sgid}" segment_name="{sname}" sb_module_pin_name="{pname}"/>\n')
                        sfile.write(f'\t</{dchan}>\n')
                    # if-else
                # for ptc
            sfile.write('</rr_sb>\n')
        # with
    # for x, y
    log(f" - Missing {sum(missfeed.values()):,d} feedthroughs: {dict(missfeed)}\n")
    log(f"{elapsed()} Wrote {len(nfile):,d} GSB files\n")
# def write_gsbs

def write_json():
    """write out various internal tables in JSON"""
    filename = f"{g_base}.json"
    lbr = '{'
    rbr = '}'
    with open(filename, "w", encoding='ascii') as ofile:
        log(f"{elapsed()} Writing {filename}\n")
        ofile.write("{\n\n")

        ofile.write(f'"channels": {g_chcnt},\n')

        ofile.write('"switches": {\n')
        for i, swid in enumerate(g_swid2swname, start=-len(g_swid2swname)):
            end = "" if i == -1 else ","
            ofile.write(f' "{swid}": "{g_swid2swname[swid]}"{end}\n')
        # for
        ofile.write('},\n')

        ofile.write('"segments": {\n')
        for i, sgid in enumerate(g_sgid2sgname, start=-len(g_sgid2sgname)):
            end = "" if i == -1 else ","
            ofile.write(f' "{sgid}": ["{g_sgid2sgname[sgid]}", {g_sgid2len[sgid]}]{end}\n')
        # for
        ofile.write('},\n')

        ofile.write('"block_types": {\n')
        for i, bid in enumerate(g_bid2info, start=-len(g_bid2info)):
            (name, width, height, pid2i) = [g_bid2info[bid][k]
                    for k in ('name', 'width', 'height', 'pid2info')]
            ofile.write(f' "{bid}": ["{name}", {width}, {height}, {lbr}\n')
            for j, ptc in enumerate(pid2i, start=-len(pid2i)):
                end = "" if j == -1 else ","
                (text, pin_class_type) = pid2i[ptc]
                ofile.write(f'  "{ptc}": ["{text}", "{pin_class_type}"]{end}\n')
            # for
            end = "" if i == -1 else ","
            ofile.write(f' {rbr}]{end}\n')
        # for
        ofile.write('},\n')

        ofile.write('"grid": [\n')
        for i, xy in enumerate(sorted(g_xy2bidwh), start=-len(g_xy2bidwh)):
            (bid, wo, ho) = g_xy2bidwh[xy]
            end = "" if i == -1 else ","
            ofile.write(f' [{xy[0]}, {xy[1]}, {bid}, {wo}, {ho}]{end}\n')
        # for
        ofile.write('],\n')

        ofile.write(f'"rr_nodes": {lbr}\n')
        for i, nid in enumerate(g_nid2info, start=-len(g_nid2info)):
            end = "" if i == -1 else ","
            (xl, yl, xh, yh, p, pinchan, sgid) = g_nid2info[nid]
            if pinchan.endswith("PIN"): sgid = f'"{sgid}"'
            ofile.write(
                f' "{nid}": [{xl}, {yl}, {xh}, {yh}, {p}, "{pinchan}", {sgid}]{end}\n')
        # for
        ofile.write("}\n")

        ofile.write("\n}\n")
    # with
# def write_json

# PYX is trivial!

def pyxdump(pfile, n):
    """write out PYX"""
    if isinstance(n, str):
        if n == "\n":
            pfile.write('-\\n\n')
        else:
            pfile.write(f'-{n}\n')
    else:
        tag = n[0]
        pfile.write(f"({tag}\n")
        for k in sorted(n[1]):
            pfile.write(f"A{k} {n[1][k]}\n")
        for n2 in n[2:]:
            pyxdump(pfile, n2)
        pfile.write(f"){tag}\n")
# def pyxdump

########################
#######          #######
####### toplogic #######
#######          #######
########################

def toplogic():
    """top code"""

    # collect options
    global g_twisted, g_dangling_ok, g_undriven_ok, g_commercial
    global g_drop_iso_nodes, g_add_layer, g_nloop
    gsbs = False
    edit = True
    g_twisted = 0
    g_nloop = 0
    while len(sys.argv) > 1 and sys.argv[1][0] == '-':
        for c in sys.argv[1][1:]:
            if c.isdigit():
                if not g_nloop and c != '0':
                    log(" - Enabling looping\n")
                g_nloop = g_nloop * 10 + int(c)
                continue
            if c == "c":
                if not g_commercial:
                    log(" - Enabling commercial routing\n")
                g_commercial = True
                continue
            if c == "g":
                if not gsbs:
                    log(" - Enabling GSB writing\n")
                gsbs = True
                continue
            if c == "i":
                if edit:
                    log(" - Disabling stamping\n")
                edit = False
                continue
            # ?auto-deduce from routing graph: maybe in analyze_nodes?
            if c == "t":
                if not g_twisted:
                    log(" - Assuming tileable graph\n")
                g_twisted = 2
                continue
            if c == "l":
                if g_add_layer:
                    log(' - Disabling forcing layer="0"\n')
                g_add_layer = False
                continue
            if c == "d":
                if not g_dangling_ok:
                    log(" - Disabling removal of dangling edges\n")
                g_dangling_ok = True
                continue
            if c == "u":
                if not g_undriven_ok:
                    log(" - Disabling removal of undriven edges\n")
                g_undriven_ok = True
                continue
            if c == "s":
                if not g_drop_iso_nodes:
                    log(" - Removing singleton CHANX/Y nodes\n")
                g_drop_iso_nodes = True
                continue
            log(f"Error: unrecognized option: {sys.argv[1]}\n")
            sys.exit(1)
        # for c
        del sys.argv[1]
    # while

    # determine input filename and design base name
    # figure out how to convert input file to PYX
    global g_base
    assert len(sys.argv) == 2
    xmlstarlet = "/usr/bin/xmlstarlet"
    filename = sys.argv[1]
    if   filename.endswith(".pyx.gz"):
        g_base = filename[:-7]
        command = f"gunzip -c {filename}"
    elif filename.endswith(".pyx"):
        g_base = filename[:-4]
        command = f"cat {filename}"
    elif filename.endswith(".xml.gz"):
        g_base = filename[:-7]
        command = f"gunzip -c {filename} | {xmlstarlet} pyx | tee {g_base}.pyx"
    elif filename.endswith(".xml"):
        g_base = filename[:-4]
        command = f"{xmlstarlet} pyx < {filename} | tee {g_base}.pyx"
    else:
        log(f"Unrecognized input file {filename}\n")
        sys.exit(1)
    gz = filename.endswith(".gz")
    log(f"Stamping graph {g_base} from {filename}\n")

    logfilename = f"{g_base}.log"
    global g_log
    # pylint: disable-next=consider-using-with
    g_log = open(logfilename, "w", encoding='ascii')
    log(f"{elapsed()} Writing {logfilename}\n")

    if g_true or edit:
        log("\n")
        read_muxes()

    # read rrgraph: traverse tree and extract info
    with subprocess.Popen(command,
            shell=True, stdout=subprocess.PIPE, universal_newlines=True).stdout as xfile:
        log(f"\n{elapsed()} READING {filename}\n")
        dom_tree = ['document', {}]
        # 2nd arg is a stack, with last item being one modified
        xmltraverse(xfile, [dom_tree])
    assert dom_tree[2][0] == "rr_graph"
    rrg = dom_tree[2]

    ptcmsg = ' '.join(
        [f"{s}:{g_sgid2sgname[s]}={g_sgid2ptcs[s]}" for s in sorted(g_sgid2ptcs)])
    g = g_grid_bbox
    log(
f" - Channels = {g_chcnt}  Grid bbox {g[0]},{g[1]} {g[2]},{g[3]}  {ptcmsg}\n")
    global g_dumps
    if g[2] - g[0] + g[3] - g[1] < 40:
        # set to True for debug dumps on small devices
        g_dumps = True
        g_dumps = False
    else:
        g_dumps = False

    assert g_clb_id is not None
    # compute bbox
    global g_xcoords, g_ycoords
    g_xcoords = sorted({ x for (x, y) in g_xy2bidwh if g_xy2bidwh[(x, y)][0] != 0 })
    g_ycoords = sorted({ y for (x, y) in g_xy2bidwh if g_xy2bidwh[(x, y)][0] != 0 })

    # sort g_xydl2tn lists
    if g_dumps:
        wname = f"{g_base}.wires"
    else:
        wname = "/dev/stdout"
    with open(wname, "w", encoding='ascii') as wfile:
        if g_dumps:
            log(f"{elapsed()} Writing {wname}\n")
            wfile.write("# (x, y, dir+len) : #items [wire_ptc, wire_nid]...\n\n")
        for t in sorted(g_xydl2tn):
            tns = sorted(g_xydl2tn[t])
            g_xydl2tn[t] = tns
            if g_dumps:
                wfile.write(f"{t} : #{len(tns)} {tns}\n\n")
        # for t
    # with

    collisions(dom_tree, True)

    debug_dump(f"{g_base}.dump")

    analyze_nodes(dom_tree)

    if g_true or edit:
        find_routable_pins(dom_tree)

        # process block ports after we know which ones appear in <node ...>
        create_pinmaps()

        if edit: verify_wires_in_xml()

    check_connectivity(dom_tree, "o")   # original
    check_rules(dom_tree, us=False, commercial=False)           # check VPR/OpenFPGA rules

    # build pred/succ tables
    if g_nloop:
        build_loops()

    # write out .mux
    if not edit:
        write_muxes(dom_tree, "mxi", False)

    if edit:
        fix_chan_rc(rrg)
        edit_xml(dom_tree)
        log(
f" - Connected {len(g_found):,d} nodes and left {len(g_notfound):,d} unconnected\n")
        for nf in sorted(g_notfound, key=lambda k: [dict_aup_ndn(k[0]), k[1], k[2]]):
            g_log.write(f"Left unconnected: {nf} (not found by wire2node)\n")

        check_connectivity(dom_tree, "u")   # updated
        check_rules(dom_tree, us=True, commercial=g_commercial) # check VPR/OpenFPGA rules

        collisions(dom_tree)

    # write out GSBs
    if gsbs:
        write_gsbs(rrg)
    # write out .mux
    if edit:
        write_muxes(dom_tree, "mxo", True)

    # write out tables
    write_json()

    # write out new result
    versions = ["stamped", "looped", ] if edit else []
    for version in versions:
        xmllint = "/usr/bin/xmllint"
        command = f"{xmlstarlet} p2x | {xmllint} --format -"
        xmlfile = f"{g_base}.{version}.xml"
        if gz:
            command += " | gzip"
            xmlfile += ".gz"
        command += f" > {xmlfile}"

        with subprocess.Popen(command,
                shell=True, stdin=subprocess.PIPE, universal_newlines=True).stdin as pfile:
            log(f"{elapsed()} Writing {xmlfile}\n")
            pyxdump(pfile, rrg)
        # with

        if version == "looped": break

        # create loops
        for edges in get_elements_by_tag_name(dom_tree, "rr_edges", 2):
            for edge in child_nodes(edges):
                if is_text(edge): continue
                assert local_name(edge) == 'edge'
                hedge = attr2hash(edge)
                src_nid = hedge['src_node']
                dst_nid = hedge['sink_node']
                if src_nid in g_pred2succ:
                    hedge['src_node'] = g_pred2succ[src_nid]
            # for
        # for
    # while

    log(f"{elapsed()} Exiting\n")
# def toplogic

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: ignorecase filetype=python autoindent hlsearch syntax=on fileformat=unix
