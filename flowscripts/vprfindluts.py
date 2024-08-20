#!/usr/bin/env python3
"""read (1) dq and (2) top.csv, show wrong LUTs"""

import sys
import re
from collections import defaultdict

def dict_aup_ndn(k):
    """sort with alpha ascending and numeric descending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] = -int(r[i])
    return r
# def dict_aup_ndn

badpaths = defaultdict(dict)
badbits = 0
#single = defaultdict(list)
with open("dq", encoding='ascii') as ifile:
    sys.stderr.write(f"Reading dq\n")
    for line in ifile:
        #if not re.search(r'_15__26_.*__fle_0.*frac_lut6', line): continue
        #print(line, end='')
        g = re.match(r'! (\d),(fpga_top.grid_clb_(\d+)__(\d+)_.*__fle_(\d+).*frac_lut6.*).mem_out\[(\d+)\]$', line)
        if not g: continue
        (good, path, x, y, f, index) = g.groups()
        #single[int(index)].append(good)
        badpaths[path][int(index)] = good
        badbits += 1
    # for
# with

latest = defaultdict(dict)
path2xyf = {}
allbits = 0
allluts = set()
with open("top.csv", encoding='ascii') as ifile:
    sys.stderr.write(f"Reading top.csv\n")
    for line in ifile:
        #if not re.search(r'_15__26_.*__fle_0.*frac_lut6', line): continue
        #print(line, end='')
        g = re.match(r'(\d),(fpga_top.grid_clb_(\d+)__(\d+)_.*__fle_(\d+).*frac_lut6.*).mem_out\[(\d+)\]$', line)
        if not g: continue
        (curr, path, x, y, f, index) = g.groups()
        #single[int(index)].append(curr)
        allluts.add(path)
        allbits += 1
        if path not in badpaths: continue
        latest[path][int(index)] = curr
        if path not in path2xyf:
            path2xyf[path] = (x, y, f)
            #print(f"SAVED ({x},{y},{f}) ==> {path}")
        else:
            assert path2xyf[path] == (x, y, f)
# with

#reordered = {}
#for i in sorted(single):
#    reordered[i] = single[i]
#print(reordered)

lno = 0
xy2clbnet = {}
clbnet2xy = {}
with open("top.place", encoding='ascii') as ifile:
    sys.stderr.write(f"Reading top.place\n")
    for line in ifile:
        lno += 1
        if lno <= 2: continue
        f = line.split()
        if len(f) != 6: continue
        if f[0][0] == "#": continue
        (name, x, y, _, _, _) = f
        xy2clbnet[(x, y)] = name
        clbnet2xy[name] = (x, y)

netkeys = { 'fast6[0]', 'ble6[0]', 'ble5[0]', 'ble5[1]', 'adder[0]', }
xyf2inst2net = defaultdict(dict)
with open("top.net", encoding='ascii') as ifile:
    sys.stderr.write(f"Reading top.net\n")
    for line in ifile:
        g = re.match(r'\t*<block\s+(.*?)/?>$', line)
        if not g: continue
        values = {}
        body = g.group(1)
        for f in body.split():
            g = re.match(r'(\w+)="([^"]*)"$', f)
            assert g, f"f={f}"
            (k, v) = g.groups()
            values[k] = v
        # for
        assert "instance" in values, f"values={values}"
        inst = values["instance"]
        if inst == "clb_lr[0]":
            clbnet = values["name"]
            (savex, savey) = clbnet2xy[clbnet]
            #print(f"Recognized CLB ({savex},{savey}) {clbnet}")
            continue
        g = re.match(r'fle\[(\d+)\]$', inst)
        if g:
            savef = g.group(1)
            #print(f"\tRecognized FLE {savef}")
            continue
        if inst in netkeys:
            outnet = values["name"]
            #print(f"\t\tRecognized OUT {inst} {outnet}")
            xyf2inst2net[(savex, savey, savef)][inst] = outnet
            continue
    # for
# with

flip = { "0": "1", "1": "0", }
functions = {
    "10" * 16: 0,
    "1100" * 8: 1,
    "11110000" * 4: 2,
    "1111111100000000" * 2: 3,
    "11111111111111110000000000000000": 4,
    "00000000000000000000000000000000": 'F',
    "11111111111111111111111111111111": 'T',
}

omitted = 0
for path in sorted(badpaths, key=dict_aup_ndn):
    good = ""; ngood = 0
    curr = ""; ncurr = 0
    mark = ""
    ndiff = 0
    for b in range(64):
        if b == 32:
            curr += "_"
            good += "_"
            mark += "_"
        if b in badpaths[path]:         # from dq
            good0 = badpaths[path][b]
            curr0 = flip[good0]
            curr += curr0
            good += good0
            mark += "*"
            ndiff += 1
            assert path in latest
            assert b ^ 32 in latest[path]
            curro = latest[path][b ^ 32]
            if curr0 == curro:
                mark = mark[:-1] + "+"
        else:
            curr0 = latest[path][b]     # from top.csv
            good0 = curr0
            curr += curr0
            good += curr0
            mark += " "
        if curr0 == "1": ncurr += 1
        if good0 == "1": ngood += 1
    # for

    (x, y, f) = path2xyf[path]
    insts = xyf2inst2net[(x, y, f)]

    dontcare = True
    for inst in insts:
        if not inst.startswith("ble5"):
            dontcare = False
            break
        outnet = insts[inst]
        if outnet != "open":
            if inst == "ble5[0]" and mark[:32] != " " * 32:
                dontcare = False
                break
            if inst == "ble5[1]" and mark[33:] != " " * 32:
                dontcare = False
                break
    if dontcare:
        omitted += 1
        continue

    clbnet = xy2clbnet[(x, y)]
    print(f"FLE {path}")
    print(f"LOC ({x},{y},{f}) {clbnet}")
    for inst in insts:
        outnet = insts[inst]
        print(f"OUT {inst} {outnet}")
    wl = functions.get(good[:32], "?")
    wu = functions.get(good[33:], "?")
    hl = functions.get(curr[:32], "?")
    hu = functions.get(curr[33:], "?")
    print(f"    {good} want {ngood:2d} ON {wl} / {wu}")
    print(f"    {curr} have {ncurr:2d} ON {hl} / {hu}")
    print(f"    {mark} diff {ndiff:2d}")
    print("")
    #assert  ngood == 0 and ncurr in (16, 32) or \
    #        ngood * 2 == ncurr or \
    #        ngood + 16 == ncurr, \
    #        f"ngood = {ngood}, ncurr = {ncurr}"
# for
print(f"BAD LUTS = {len(badpaths):,d}/{len(allluts):,d}, BAD BITS = {badbits:,d}/{allbits:,d}")
if omitted: print(f"Omitted {omitted} LUTs due to only don't-care BLE5 differences")

# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
