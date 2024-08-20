#!/nfs_home/dhow/bin/python
#!/usr/bin/python3
"""read in vpr.xml, modify muxes, write it out again"""

import sys
import os
import re
import subprocess
from collections import defaultdict

# pylint: disable-next=import-error,unused-import
from traceback_with_variables import activate_by_import, default_format

import json

# part of traceback_with_variables
default_format.max_value_str_len = 10000

g_chcnt             = 0
g_swid2swname       = {}    # swid ==> swname
g_sgid2sgname       = {}    # sgid ==> sgname
g_sgid2ptcs         = {}    # id ==> [min_ptc, max_ptc]
g_bid2info          = {}    # id ==> { name: , width: , height: , pid2info: {}, n2info: {} }
g_block2bid         = {}    # name ==> id
g_xy2bidwh          = {}    # (x, y) ==> (bid, wo, ho)
g_nid2info          = {}    # nid ==> [xl, yl, xh, yh, ptc, SINK/CHANE, sgid] sgid < 0 for pins
# ONLY for non-CHAN*
g_xyt2nid           = {}    # (xl, yl, ptc) ==> nid
# ONLY for CHAN*
g_xydl2tn           = defaultdict(list)   # (E4, x1, y1, x2, y2) ==> [(ptc, id), ...]
# routeport comes from pattern.mux
# blockport comes from block_type declaration
g_bidport2nn        = {}    # (bid, wo, ho, routeport) ==> (ptc, blockport)
g_edges             = []
g_false             = False
g_true              = True

g_sgid2len          = {}    # sgid ==> length
g_len2sgid          = {}    # len# ==> sg id

g_clb_id            = None  # id for CLB

g_grid_bbox         = []

g_ignore_ipins = False
g_ignore_opins = False

g_false = False

g_rrg_src2dst = defaultdict(set)
g_rrg_dst2src = defaultdict(set)
g_gsb_src2dst = defaultdict(set)
g_gsb_dst2src = defaultdict(set)

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

g_fno = 0

def read_xml(filename):
    """read an XML file"""
    # figure out how to convert input file to PYX
    xmlstarlet = "/usr/bin/xmlstarlet"
    if   filename.endswith(".pyx.gz"):
        command = f"gunzip -c {filename}"
    elif filename.endswith(".pyx"):
        command = f"cat {filename}"
    elif filename.endswith(".xml.gz"):
        command = f"gunzip -c {filename} | {xmlstarlet} pyx"
    elif filename.endswith(".xml"):
        command = f"{xmlstarlet} pyx < {filename}"
    else:
        log(f"Unrecognized input file {filename}\n")
        sys.exit(1)

    # read rrgraph: traverse tree and extract info
    dom_tree = ['document', {}]
    stack = [dom_tree]
    global g_fno
    g_fno += 1
    with subprocess.Popen(command,
            shell=True, stdout=subprocess.PIPE, universal_newlines=True).stdout as xfile:
        log(f"  {g_fno} {filename}              \r")
        # parse the PYX file and return result
        lno = 0
        for line in xfile:
            g_pyx_jump[line[0]](stack, line)
            lno += 1
            if not lno % 100000: log(f"  {lno:,d} {filename}             \r")
        # for
    # with

    return stack[0][2]
# def

def xmltraverse(rrg):
    """parse PYX file"""

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
        # for switch
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
    # for block_types

    # <grid>
    for grid in get_elements_by_tag_name(rrg, "grid", 1):
        n = 0
        # <grid_loc x="0" y="0" block_type_id="0" width_offset="0" height_offset="0"/>
        for grid_loc in child_nodes(grid):
            if is_text(grid_loc) or grid_loc[0] != "grid_loc": continue
            n += 1
            h = grid_loc[1]
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
    # for grid

    # <rr_nodes> Pass 1 to determine:
    # segment id lengths
    # ptc spans (not explicit in file)
    g_sgid2len.clear()
    for rr_nodes in get_elements_by_tag_name(rrg, "rr_nodes", 1):
        n = nr = np = 0
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
                # <segment segment_id="2"/>
                if child[0] == "segment":
                    segment_h = child[1]
            # for child
            if h['type'].endswith('PIN'):
                np += 1
                if loc_h['side'] != 'RIGHT': nr += 1
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
    # for rr_nodes
    # reverse for some uses
    for sgid, ln in g_sgid2len.items():
        g_len2sgid[ln] = sgid

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
        # for node
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
        # for edge
    # for rr_edges
# def xmltraverse

def getinfo(nid):
    """get printable info"""
    (xl, yl, xh, yh, ptc, ty, sgid) = g_nid2info[nid]
    if ty.startswith('CHAN'):
        sgid = g_sgid2sgname[sgid]
    elif ty.endswith('PIN'):
        (bid, _, _) = g_xy2bidwh[(xl, yl)]
        # block name is always in the pin name
        #blkname = g_bid2info[bid]['name']
        (text, pin_class_type) = g_bid2info[bid]['pid2info'][ptc]
        sgid = f"{sgid}:{pin_class_type}:{text}"
    else:
        assert 0
    return (xl, yl, xh, yh, ptc, ty, sgid)

def show_edge(edge):
    """print out edge info"""
    for di, nid in [["src", edge[0]], ["dst", edge[1]]]:
        (xl, yl, xh, yh, ptc, ty, sgid) = getinfo(nid)
        log(f" {di}: nid={nid} {ty} {xl} {yl} {xh} {yh} ptc={ptc} {sgid}")
    # for
    log("\n")
    # show all sources to sink
    for src in sorted(g_rrg_dst2src[edge[1]] | g_gsb_dst2src[edge[1]]):
        rrgf = "RRG" if src in g_rrg_dst2src[edge[1]] else "---"
        gsbf = "GSB" if src in g_gsb_dst2src[edge[1]] else "---"
        log(f"up\t{src} --> {edge[1]}: {rrgf} {gsbf}")
        for di, nid in [["src", src], ["dst", edge[1]]]:
            (xl, yl, xh, yh, ptc, ty, sgid) = getinfo(nid)
            log(f" {di}: nid={nid} {ty} {xl} {yl} {xh} {yh} ptc={ptc} {sgid}")
        log("\n")
    # show all sinks from source
    for dst in sorted(g_rrg_src2dst[edge[0]] | g_gsb_src2dst[edge[0]]):
        rrgf = "RRG" if dst in g_rrg_src2dst[edge[0]] else "---"
        gsbf = "GSB" if dst in g_gsb_src2dst[edge[0]] else "---"
        log(f"dn\t{edge[0]} --> {dst}: {rrgf} {gsbf}")
        for di, nid in [["src", edge[0]], ["dst", dst]]:
            (xl, yl, xh, yh, ptc, ty, sgid) = getinfo(nid)
            log(f" {di}: nid={nid} {ty} {xl} {yl} {xh} {yh} ptc={ptc} {sgid}")
        log("\n")
# def

def toplogic():
    """top code"""

    assert len(sys.argv) > 2

    global g_ignore_ipins, g_ignore_opins 
    while len(sys.argv) > 1 and sys.argv[1][0] == '-':
        for c in sys.argv[1][1:]:
            if c == 'i':
                g_ignore_ipins = True
                continue
            if c == 'o':
                g_ignore_opins = True
                continue
            sys.stderr.write(f"Unrecognized option: {sys.argv[1]}\n")
            sys.exit(1)
        # for c
        del sys.argv[1]
    # while

    # rrgraph is first
    rrg = read_xml(sys.argv[1])

    # GSBs follow
    gsbs = []
    for a in sys.argv[2:]:
        g = read_xml(a)
        if g_false:
            a2 = re.sub(r'.*/', r'', a)
            with open(a2 + ".json", "w", encoding='ascii') as jfile:
                json.dump(g, jfile, indent=1)
        if g[0] != "gsbs":
            # reading GSBs one at a time
            gsbs.append(g)
        else:
            # reading all GSBs in one file inside <gsbs>
            gsbs.extend(i for i in g[2:] if not is_text(i))

    log(f"Read {1+len(gsbs)} file( equivalent)s                                  \n")

    # build rrgraph data structures
    xmltraverse(rrg)

    # collect RRG edges
    rrg_edges = set()
    for rr_edges in get_elements_by_tag_name(rrg, "rr_edges", 1):
        # <edge src_node="0" sink_node="1" switch_id="0"/>
        for edge in child_nodes(rr_edges):
            if is_text(edge) or edge[0] != "edge": continue
            h = edge[1]
            snid = h['src_node']
            dnid = h['sink_node']
            st = g_nid2info[snid][5]
            dt = g_nid2info[dnid][5]
            if not st.startswith("CHAN") and not dt.startswith("CHAN"): continue
            if g_ignore_ipins and dt == "IPIN": continue
            if g_ignore_opins and st == "OPIN": continue
            rrg_edges.add((snid, dnid))
            g_rrg_src2dst[snid].add(dnid)
            g_rrg_dst2src[dnid].add(snid)
        # for edge
    # for rr_edges

    # collect GSB edges
    gsb_edges = set()
    for g in gsbs:
        if is_text(g): continue
        assert g[0] in ("rr_cb", "rr_sb")
        for dst in child_nodes(g):
            if is_text(dst): continue
            assert dst[0] in ('IPIN', 'CHANX', 'CHANY')
            if g_ignore_ipins and dst[0] == "IPIN": continue
            dnid = dst[1]['node_id']
            for src in child_nodes(dst):
                if is_text(src): continue
                assert src[0] == "driver_node"
                if g_ignore_opins and src[1]['type'] == "OPIN": continue
                snid = src[1]['node_id']
                if snid != dnid:
                    gsb_edges.add((snid, dnid))
                    g_gsb_src2dst[snid].add(dnid)
                    g_gsb_dst2src[dnid].add(snid)

    n1 = 0
    for edge in rrg_edges:
        (snid, dnid) = edge
        if edge in gsb_edges: continue
        n1 += 1
        log(f"{n1}: {edge} in RRG but not in GSBs")
        show_edge(edge)

    n2 = 0
    for edge in gsb_edges:
        (snid, dnid) = edge
        if edge in rrg_edges: continue
        n2 += 1
        log(f"{n2}: {edge} in GSBs but not in RRG")
        show_edge(edge)

    log(f"{len(rrg_edges)} edges in RRG and {len(gsb_edges)} edges in GSBs\n")
    log(f"{n1} edges in RRG but not in GSBs and {n2} edges in GSBs but not in RRG\n")

# def toplogic

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: ignorecase filetype=python autoindent hlsearch syntax=on fileformat=unix
