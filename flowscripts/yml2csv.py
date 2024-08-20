#!/usr/bin/python3
"""convert yml files to csv"""

import sys
import re 
from collections import defaultdict

def order(vv, minc, maxc):
    """order rows"""
    if minc not in vv or maxc not in vv: return 1e24 if maxc in vv else -1e24
    if vv[minc]: return vv[maxc] / vv[minc]
    if not vv[maxc]: return 1
    return 1 + 1e-24 if vv[maxc] > 0 else 1 - 1e-24

def toplogic():
    """top level"""
    sheet = defaultdict(dict)
    cols = set()
    for i, arg in enumerate(sys.argv, start=0):
        with open(arg, encoding='ascii') as ifile:
            for line in ifile:
                line = re.sub(r'#.*', r'', line.strip())
                if not line: continue
                g = re.search(r'^(\S+):\s*(\S+)$', line)
                if not g: continue
                (n, v) = g.groups()
                if not re.match(r'^[-+]?\d*[.]?\d*[eE]?[-+]?\d*$', v): continue
                sheet[n][i] = float(v)
                cols.add(i)
    nn = sorted(sheet, key=lambda n: (order(sheet[n], min(cols), max(cols)), n))
    for n in nn:
        print(n, end='')
        for i in sorted(cols):
            print(f",{sheet[n][i] if i in sheet[n] else ''}", end='')
        print("")

toplogic()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
