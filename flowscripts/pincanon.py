#!/usr/bin/env python3
"""normalize net/pin dump for comparison"""

import sys
import re
from collections import defaultdict

def dict_aup_nup(k):
    """sort with alpha ascending and numeric ascending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    return r

def main():
    """main prog"""
    rows = (sys.argv[1] == "rows")  # each row is a net, with pins listed sorted
    pins = (sys.argv[1] == "pins")  # list pins in same order as "rows", but one/line after PIN
    both = (sys.argv[1] == "both")  # like "pins" but separate pins with NET lines 
    assert both or rows or pins

    net2pins = defaultdict(list)
    net = ""
    with open(sys.argv[2], encoding='ascii') as ifile:
        for line in ifile:
            line = line.strip()
            ff = line.split()
            if not ff: continue
            if ff[0] == "PIN":
                if re.match(r'^(clock_)?mux.*size\d+$', ff[2]) and ff[3] == "out[0]": ff[3] = "out"
                net2pins[net].append(ff[1] + "." + ff[3])
                continue
            if ff[0] == "NET":
                net = re.sub(r'tile_\d+__\d+_[.]n1$', r'0', ff[1])

    net2pins2 = [
        ' '.join(sorted(pinlist, key=dict_aup_nup)) + f" {net}"
        for net, pinlist in net2pins.items() ]

    for row in sorted(net2pins2, key=dict_aup_nup):
        (*pinlist, net) = row.split()
        if rows:
            print(' '.join(pinlist))
            continue
        if both:
            print(f"NET {net}")
        for pin in pinlist:
            print(f"PIN {pin}")

main()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
