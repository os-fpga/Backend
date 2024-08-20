#!/usr/bin/env python3
"""read (1) dq and (2) top.csv, show wrong words"""

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

pattern = sys.argv[1] if len(sys.argv) > 1 else ""
badpaths = defaultdict(dict)    # bad_path --> index --> good_bit
badbits = 0
with open("dq", encoding='ascii') as ifile:
    sys.stderr.write(f"Reading dq\n")
    for line in ifile:
        g = re.match(r'! (\d),(.*).mem_out\[(\d+)\]$', line)
        if not g: continue
        (good, path, index) = g.groups()
        if pattern and pattern not in path: continue
        badpaths[path][int(index)] = good
        badbits += 1
    # for
# with

latest = defaultdict(dict)  # bad_path --> index --> curr_bit
allbits = 0
allpaths = defaultdict(set)
with open("top.csv", encoding='ascii') as ifile:
    sys.stderr.write(f"Reading top.csv\n")
    for line in ifile:
        g = re.match(r'(\d),(.*).mem_out\[(\d+)\]$', line)
        if not g: continue
        (curr, path, index) = g.groups()
        index = int(index)
        allpaths[path].add(index)
        allbits += 1
        if path not in badpaths: continue
        latest[path][index] = curr
# with

flip = { "0": "1", "1": "0", }
for path in sorted(badpaths, key=dict_aup_ndn):
    good = ""; ngood = 0
    curr = ""; ncurr = 0
    mark = ""
    w = max(allpaths[path]) + 1
    for b in range(w):
        if b in badpaths[path]:
            curr0 = badpaths[path][b]
            good0 = flip[curr0]
            curr += curr0
            good += good0
            mark += "*"
        else:
            curr0 = latest[path][b]
            good0 = curr0
            curr += curr0
            good += curr0
            mark += " "
        if curr0 == "1": ncurr += 1
        if good0 == "1": ngood += 1
    print(f"PATH {path}")
    print(f"     {good} want {ngood} ON")
    print(f"     {curr} have {ncurr} ON")
    print(f"     {mark}")
    print("")
# for
#print(f"BAD LUTS = {len(badpaths):,d}/{len(allpaths):,d}, BAD BITS = {badbits:,d}/{allbits:,d}")

# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
