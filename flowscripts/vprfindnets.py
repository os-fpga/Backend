#!/usr/bin/env python3
"""read (1) .route (2) dq (3) muxdump, show misrouted nets"""

import sys
import re
from collections import defaultdict

node2net = {}
net2nodes = defaultdict(int)
allnets = 0
node2prev = {}
with open("top.route", encoding='ascii') as ifile:
    sys.stderr.write(f"Reading top.route\n")
    prev = ""
    for line in ifile:
        f = line.split()
        if not f: continue
        if f[0] == "Net":
            if len(f) != 3: continue
            allnets += 1
            netname = f[2][1:-1]
            prev = ""
            continue
        if f[0] == "Node:":
            curr = f[1]
            if curr not in node2net:
                net2nodes[netname] += 1
                if prev != "":
                    assert curr not in node2prev
                    node2prev[curr] = prev
            node2net[curr] = netname
            prev = curr
            continue
    # for
# with

badpaths = defaultdict(dict)
badbits = 0
with open("dq", encoding='ascii') as ifile:
    sys.stderr.write(f"Reading dq\n")
    for line in ifile:
        g = re.match(r'! (\d),(fpga_top.sb_\d+__\d+_.mem_\w+_track_\d+).mem_out\[(\d+)\]$', line)
        if not g: continue
        (good, path, index) = g.groups()
        badpaths[path][int(index)] = good
        badbits += 1
    # for
# with

latest = defaultdict(dict)
allbits = 0
allpaths = set()
with open("top.csv", encoding='ascii') as ifile:
    sys.stderr.write(f"Reading top.csv\n")
    for line in ifile:
        g = re.match(r'(\d),(fpga_top.sb_\d+__\d+_.mem_\w+_track_\d+).mem_out\[(\d+)\]$', line)
        if not g: continue
        (curr, path, index) = g.groups()
        allpaths.add(path)
        allbits += 1
        if path not in badpaths: continue
        latest[path][int(index)] = curr
# with

muxes = defaultdict(list)
with open("muxdump", encoding='ascii') as ifile:
    sys.stderr.write(f"Reading muxdump\n")
    for line in ifile:
        (path, snk, sources) = re.match(r'^(\S+) :: (\d+) <==\s*(.+)', line).groups()
        if path not in badpaths: continue
        net = node2net[snk]
        curr = ""
        good = ""
        for idx in sorted(latest[path]):
            curr0 = latest[path][idx]
            curr += curr0
            good += badpaths[path].get(idx, curr0)
        assert len(curr) == len(good)
        assert curr != good
        srclist = re.sub(r'[\[,\]]', r' ', sources).split()
        n = len([s for s in srclist if node2net.get(s, " ") == net])
        w = len(srclist)
        src = node2prev[snk]
        i = srclist.index(src)
        srclist = [f"*{s}" if node2net.get(s, " ") == net else s for s in srclist]
        srclist = f"[{', '.join(srclist)}]"
        muxes[net].append(f"{good} ==> {curr} PATH = {path} SINK = {snk} SOURCES = #{w} {srclist}#{n} {src}#{i}")
    # for
# with

badnets = 0
badmuxes = 0
for net in sorted(muxes):
    print(f"NET = {net} NODES = {net2nodes[net]}")
    for line in muxes[net]:
        print(f"    {line}")
        badmuxes += 1
    # for
    badnets += 1
# for
print(f"BAD NETS = {badnets:,d}/{allnets:,d}, BAD MUXES = {badmuxes:,d}/{len(allpaths):,d}, BAD BITS = {badbits:,d}/{allbits:,d}")


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
