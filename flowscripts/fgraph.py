#!/nfs_home/dhow/bin/python3
"""compute max flow through CB/LR"""

import sys
import re 
#import subprocess
from collections import defaultdict
import time
import math

import networkx as nx
#import numpy as np
import igraph as ig

g_false = False
g_true  = True
g_time  = 0

### some standard routines

def log(text):
    """write to stderr"""
    sys.stderr.write(text)

def out(text):
    """write to stdout"""
    sys.stdout.write(text)
    sys.stdout.flush()

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

def dict_aup_ndn(k):
    """sort with alpha ascending and numeric descending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] = -int(r[i])
    return r

def dict_aup_nup(k):
    """sort with alpha ascending and numeric ascending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    return r

def prg(nums):
    """print range in compressed form is possible"""
    result = ""
    prev = None
    for n in sorted(nums, reverse=True):
        if prev is None:
            prev = (n, n)
            continue
        if prev[1] == n + 1:
            prev = (prev[0], n)
            continue
        if prev is not None:
            if result != "": result += ","
            if prev[0] == prev[1]:
                result += str(prev[0])
            else:
                result += str(prev[0]) + ":" + str(prev[1])
        prev = (n, n)
    # for
    if prev is None: return result
    if result != "": result += ","
    if prev[0] == prev[1]:
        result += str(prev[0])
    else:
        result += str(prev[0]) + ":" + str(prev[1])
    return result
# def

g_adjust = {
    '1B': 0, '1E': 1,
    '4B': 0, '4Q': 1, '4M': 2, '4P': 3, '4E': 4, 
}

g_dir = {
    'E': (1, 0), 'W': (-1, 0), 'N': (0, 1), 'S': (0, -1),
}

def unique(x, y, node):
    """produce unique name from x, y, node=direction/len/tap/bit
       note how trivial this is when you don't do it the crazy VTR way"""
    if node[0] in "OIL":
        return f"X{x}Y{y}{node}"
    else:
        (di, d, ln, pl) = re.match(r'([EWNS])((\d+)[BQMPE])(\d+)$', node).groups()
        (d, (dx, dy)) = (g_adjust[d], g_dir[di])
        return f"X{x - d * dx}Y{y - d * dy}{di}{ln}B{pl}"
# def

def toplogic():
    """create graph and compute max flow"""

    # make graphs
    #dg = nx.DiGraph()   # directed
    #ug = nx.Graph()     # undirected

    # read in the routing pattern definition
    muxdb = defaultdict(list)
    maxlen = 0  # M = max wire length seen
    for tile in ["clb", "other", "left", ]:
        isclb = tile == "clb"
        for filename in ["lr.mux", f"{tile}.mux", ]:
            # no local routing on IOTiles
            if tile not in ('clb', 'other') and filename == "lr.mux": continue
            with open(filename, encoding='ascii') as mfile:
                log(f"Reading {filename}\n")
                for line in mfile:
                    line = re.sub(r'#.*', r'', line).strip()
                    ff = line.split()
                    if not ff: continue
                    islocal = ff[0][0] == "L"
                    # we drop outputs driving non-CLB locals
                    muxdb[tile].append([m for m in ff if
                        not (m[0] == "O" and islocal and not isclb) ])
                    for m in ff:
                        g = re.match(r'[NSWE](\d+)', m)
                        if not g: continue
                        l = int(g.group(1))
                        maxlen = max(maxlen, l)
                    # for m
                # for line
            # with
        # for filename
        ins = sum(len(m) - 1 for m in muxdb[tile])
        log(f"Read {len(muxdb[tile]):,d} muxes with {ins:,d} inputs\n")
    # for tile
    muxes = muxdb["other"]  # just check this once
    muxes = muxdb["clb"]
    muxesleft = muxdb["left"]

    # coordinates
    # arrangement (for maxlen=4):
    # 0 1 2 3 IO 5 6 7 8 MID 10 11 12 13 IO 15 16 17 18
    # hence 4 * maxlen + IO + MID + IO
    # coordinates [0, size) are defined
    # we populate the square [maxlen, size - maxlen)^2
    size = 4 * maxlen + 3
    half = maxlen >> 1
    #ize = 3 * maxlen + 3
    lower = (2 * maxlen + 1, 2 * maxlen + 1 + half) # FIXME not yet used
    upper = (3 * maxlen + 2 - half, 3 * maxlen + 2) # FIXME not yet used
    every = (0, maxlen + size)                      # FIXME not yet used
    middle = size >> 1
    log(f"Setting maxlen={maxlen} half={half} size={size} middle={middle}\n")

    # create all nodes
    nodes = set()
    m = 0
    dist = {}
    for x in range(maxlen, size - maxlen):
        for y in range(maxlen, size - maxlen):
            thismux = muxesleft if (x == 1 and y == middle) else muxes
            for mux in thismux:
                m += 1
                for tap in mux:
                    node = unique(x, y, tap)
                    nodes.add(node)
                    (pd, n) = re.subn(r'X\d+Y\d+[ENWS](\d+).\d+$', r'\1', node)
                    dist[node] = int(pd) if n else 0
                # for tap
            # for mux
        # for y
    # for x
    out(f"Created node table from {m:,d} muxes\n")

    # create maps syms <=> indices
    sym2idx = {}
    idx2sym = {}
    for i, n in enumerate(sorted(nodes, key=dict_aup_nup), start=0):
        sym2idx[n] = i
        idx2sym[i] = n

    # double check and create all vertices
    v = len(nodes)
    assert v == len(sym2idx)
    assert v == len(idx2sym)

    # add edges
    e = 0
    edges = set()
    src2snks = defaultdict(set)
    snk2srcs = defaultdict(set)
    for x in range(maxlen, size - maxlen):
        for y in range(maxlen, size - maxlen):
            thismux = muxesleft if (x == maxlen and y == middle) else muxes
            for mux in thismux:
                (snk, *srcs) = mux
                snk = unique(x, y, snk)
                snk_idx = sym2idx[snk]
                for src in srcs:
                    src = unique(x, y, src)
                    src_idx = sym2idx[src]
                    edges.add((src_idx, snk_idx))
                    src2snks[src].add(snk)
                    snk2srcs[snk].add(src)
                    #nx.set_edge_attributes(dg, name='capacity', values={(src, snk): 1.0})
                    #nx.set_edge_attributes(ug, name='capacity', values={(src, snk): 1.0})
                    e += 1
                # for src
            # for mux
        # for y
    # for x

    # now make the graph
    #ug = ig.Graph(v, list(edges), directed=False)
    dg = ig.Graph(v, list(edges), directed=True)
    log(f"Built fabric graphs with {len(nodes):,d} nodes and {e:,d} edges\n")

    # graph properties

    # biconnected check
    #log(f"Check graph properties\n")
    comps = dg.biconnected_components(return_articulation_points=False)
    if len(comps) == 1:
        log("Graph is biconnected\n")
    else:
        log("Graph is NOT biconnected\n")
    #log(f"{elapsed()} Done checking graph properties\n")

    for mode in range(4):
    #or mode in range(1, 2):

        # build source and dest node sets
        mode0 = mode & 1
        onodes = set()
        inodes = set()
        oaccepted = set()
        iaccepted = set()
        for n in nodes:
            g = re.match(r'X(\d+)Y(\d+)([OIL])', n)
            if not g: continue
            (x, y, di) = g.groups()
            (x, y) = (int(x), int(y))
            string = "OL" if mode < 2 else "OI"
            if di == string[mode0]:
                if abs(y - middle) > maxlen: continue
                if mode < 2:
                    if abs(x - middle) > maxlen: continue
                else:
                    if not maxlen < x <= maxlen + maxlen: continue
                if not mode0:
                    onodes.add(n)
                    oaccepted.add((x, y))
                else:
                    inodes.add(n)
                    iaccepted.add((x, y))
            string = "LO" if mode < 2 else "IO"
            if di == string[mode0]:
                if y != middle: continue
                if mode < 2:
                    if x != middle: continue
                else:
                    if x != maxlen: continue
                if not mode0:
                    inodes.add(n)
                    iaccepted.add((x, y))
                else:
                    onodes.add(n)
                    oaccepted.add((x, y))
        no = len(onodes)
        ni = len(inodes)
        wantprod = no * ni
        xs1 = { x for x, y in oaccepted }
        ys1 = { y for x, y in oaccepted }
        xs2 = { x for x, y in iaccepted }
        ys2 = { y for x, y in iaccepted }
        log(f"\nCase {mode} " +
            f"({prg(xs1)},{prg(ys1)}) ==> ({prg(xs2)},{prg(ys2)}) " +
            f"onodes={no:,d} * inodes={ni:,d} = {wantprod:,d}\n")

        # run alg #1
        # pairs = nx.all_pairs_dijkstra(dg, weight='capacity')    # 23 minutes
        # run alg #2
        # pairs = dict(nx.all_pairs_shortest_path(dg))    # 8 minutes
        # run alg #3
        # pairs = dict(nx.all_pairs_dijkstra_path(dg))    # 18 minutes
        # run alg #4
        # pairs = dict(nx.all_pairs_bellman_ford_path(dg))    # 38 minutes

        # find shortest paths from onodes to inodes
        # UNBELEIVABLE: had to write by hand since packages' versions ALL SUCK
        # (no ability to limit to nodes of interest, or stop when those all reached)
        #out(f"{elapsed()} START\n")
        paths = defaultdict(dict)
        pathlen = defaultdict(dict)
        omissed = defaultdict(int)
        imissed = defaultdict(int)
        for n0 in onodes:
            n0base = re.sub(r'^X\d+Y\d+(.*)$', r'\1', n0)
            # reached: node --> distance
            reached = { n0 : (0, 0) }
            # buckets to explore: distance --> set(nodes)
            buckets = defaultdict(set)
            buckets[(0, 0)] = set(reached)
            # nodes we will need to reach
            need = set(inodes)
            # keep track of backward path
            bwd = {}
            # while we have buckets to explore and nodes we need to reach
            while buckets and need:
                # get nodes at next distance
                mind = min(list(buckets))
                bucket = buckets[mind]
                del buckets[mind]

                # move through nodes to explore at distance "mind"
                for src in bucket:
                    # move through sinks driven by this node
                    for snk in src2snks[src]:
                        # skip if already reached
                        if snk in reached: continue
                        # determine distance to snk
                        pd = dist[snk]
                        assert pd >= 0
                        d = (mind[0] + pd, mind[1] + 1)
                        # add node to explore at correct distance
                        buckets[d].add(snk)
                        # say we've already reached it so we don't double-enter
                        reached[snk] = d
                        # no longer in needed-to-reach list
                        need.discard(snk)
                        # keep backwards link to make path later
                        bwd[snk] = src
                        # done if nothing left to reach
                        if not need: break
                    # for snk
                    # done if nothing left to reach
                    if not need: break
                # for src
                # done if nothing left to reach
                if not need: break
            # while
            # over each endpoint we were trying to reach from n0
            for n in inodes:
                path = []   # default: no path if not reached
                if n in bwd:
                    t = n
                    # trace backwards saving each node including end
                    while t != n0:
                        path.append(t)
                        t = bwd[t]
                    # while
                    # remember to save beginning
                    path.append(t)
                    # save path for this onode --> inode pair
                    paths[n0][n] = path[::-1]
                    pathlen[n0][n] = reached[n]
                else:
                    # save path for this onode --> inode pair
                    paths[n0][n] = []
                    pathlen[n0][n] = (float("inf"), float("inf"))
                    omissed[n0base] += 1
                    nbase = re.sub(r'^X\d+Y\d+(.*)$', r'\1', n)
                    imissed[nbase ] += 1
                    (xo, yo) = re.match(r'X(\d+)Y(\d+)', n0).groups()
                    (xi, yi) = re.match(r'X(\d+)Y(\d+)', n ).groups()
                    (xd, yd) = (int(xi) - int(xo), int(yi) - int(yo))
                    if mode == 2 and nbase.startswith("IS"): continue
                    log(f"No path: ({xd:+d},{yd:+d}) {n0} ==> {n}\n")
            # for
        # for
        #out(f"{elapsed()} END1\n")

        # display result
        counts = defaultdict(int)
        verbose = False
        for onode in sorted(onodes, key=dict_aup_nup):
            if onode not in paths:
                if verbose: out(f"{onode} ==> NOTHING\n")
                counts[(-1, -1)] += 1
                continue
            oipaths = paths[onode]
            for inode in sorted(inodes, key=dict_aup_nup):
                if inode in oipaths:
                    ln = pathlen[onode][inode]
                    txtpath = ' '.join(oipaths[inode])
                    if verbose: out(f"{onode} ==> {inode} : {ln} {txtpath}\n")
                    counts[ln] += 1
                else:
                    if verbose: out(f"{onode} ==> {inode} : NOTHING\n")
                    counts[(-1, -1)] += 1
        tot = 0
        for ln in sorted(counts):
            tot += (c := counts[ln])
            out(f"{ln} #{c:,d}\n")
        if wantprod != tot:
            out(f"TOTAL #{tot:,d} MISMATCH\n")
        if omissed or imissed:
            log("Sometimes unreachable:\n")
        for miss in [omissed, imissed]:
            if not miss: continue
            txt = ' '.join([f"{k}#{miss[k]:,d}" for k in sorted(miss, key=dict_aup_nup)])
            log(f"{txt}\n")
        # for miss
    # for mode

    # FIXME
    return

    # flows in four directions
    for sn, tn, ox, oy, ix, iy in [
            [ "SL", "TR", lower, every, upper, every, ],
            [ "SR", "TL", upper, every, lower, every, ],
            [ "SB", "TT", every, lower, every, upper, ],
            [ "ST", "TB", every, upper, every, lower, ], ]:
        dg.add_nodes_from([sn, tn])
        s = 0
        t = 0
        six = set()
        siy = set()
        sox = set()
        soy = set()
        for n in nodes:
            g = re.match(r'X(\d+)Y(\d+)([OL])\d+$', n)  # add S? for specials
            if not g: continue
            (x, y, ol) = g.groups()
            (x, y) = (int(x), int(y))
            match ol:
                case 'O':
                    if not ox[0] <= x < ox[1]: continue
                    if not oy[0] <= y < oy[1]: continue
                    dg.add_edges_from([(sn, n)])
                    nx.set_edge_attributes(dg, name='capacity', values={(sn, n): 1.0})
                    sox.add(x)
                    soy.add(y)
                    s += 1
                case 'L':
                    if not ix[0] <= x < ix[1]: continue
                    if not iy[0] <= y < iy[1]: continue
                    dg.add_edges_from([(n, tn)])
                    nx.set_edge_attributes(dg, name='capacity', values={(n, tn): 1.0})
                    six.add(x)
                    siy.add(y)
                    t += 1
            # match
        # for
        # compute flow
        maxflow = nx.maximum_flow_value(dg, sn, tn)
        out(f"{sn[-1]}==>{tn[-1]} flow={maxflow} s={s:,d} t={t:,d} " +
            f"ox={prg(sox)} oy={prg(soy)} ix={prg(six)} iy={prg(siy)}\n")
    # for

# def

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: ignorecase filetype=python autoindent hlsearch syntax=on fileformat=unix
