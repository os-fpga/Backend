#!/usr/bin/env python3
"""read all files and then generate bitstream"""

import sys
import re

place = {}
with open("top.place", encoding='ascii') as ifile:
    sys.stderr.write(f"Opened top.place\n")
    for line in ifile:
        ff = line.split()
        if len(ff) != 6: continue
        (what, x, y, _, _, _) = ff
        place[what] = (x, y)
        #print(f"SAVED <{what}> {x} {y}")
    # for
# with

modes = {}
with open("top.net", encoding='ascii') as ifile:
    sys.stderr.write(f"Opened top.net\n")
    (x, y) = ("0", "0")
    for line in ifile:
	    # <block name="$auto$maccmap.cc:240:synth$284.C[24]" instance="clb[9]" mode="default">
        if line.startswith("\t<block ") and ' instance="clb' in line:
            name = re.sub(r'^.* name="([^""]*)" .*$', r'\1', line)
            if name and name[-1] == "\n": name = name[:-1]
            if name not in place:
                print(f"NOT FOUND: name=<{name}> line={line}")
                continue
            (x, y) = place[name]
        # <block name="$auto$maccmap.cc:240:synth$284.C[27]" instance="fle[3]" mode="arithmetic">
        if line.startswith("\t\t\t<block ") and ' instance="fle' in line:
            g = re.search(r' instance="fle\[(\d+)\]".* mode="([^""]*)', line)
            if not g: continue
            (f, mode) = g.groups()
            #print(f"({x},{y},{f}) ==> {mode}")
            modes[(x, y, f)] = mode
    # for
# with

with open("dq", encoding='ascii') as ifile:
    sys.stderr.write(f"Opened dq\n")
    seen = set()
    for line in ifile:
        key = line[4:line.rindex('.')]
        if key in seen: continue
        seen.add(key)
        g =     re.search(r'_clb_(\d+)__(\d+)_.*__fle_(\d+)[.].*ff_bypass_(\d+)[.].*_ff_bypass_([DQ])', line)
        if not g:
            g = re.search(r'_clb_(\d+)__(\d+)_.*__fle_(\d+)[.].*ff_bypass_(\d+)_([DQ])', line)
        if not g: continue
        (x, y, f, b, p) = g.groups()
        mode = modes[(x, y, f)]
        print(f"({x},{y},{f}) b={b},p={p},mode={mode}")
    # for
# with


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
