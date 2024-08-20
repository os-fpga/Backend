#!/usr/bin/env python3
#!/nfs_home/dhow/bin/python3
#!/usr/bin/python3
"""compute how mux select values work"""

import sys
import re
from collections import defaultdict

def dict_aup_nup(k):
    """sort with alpha ascending and numeric ascending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    return r
# def dict_aup_nup

def compute(sym, vals, functions, depth=0):
    #print(f"{' ' * depth}Entering compute({sym})")
    if sym in vals:
        val = vals[sym]
        if val in ("0", "1"): return val
        if re.match(r'in\[\d+\]$', val): return val
        if sym == "sram": return compute(val, vals, functions, depth+1)
        assert 0, f"sym={sym}"
    if sym in functions:
        inputs = functions[sym]
        if len(inputs) == 1: return compute(inputs[0], vals, functions, depth+1)
        if len(inputs) == 3:
            s = compute(inputs[0], vals, functions, depth+1)
            assert s in ("0", "1")
            return compute(inputs[1 if s == "1" else 2], vals, functions, depth+1)
        assert 0, f"sym={sym} inputs={inputs}"
    assert 0, f"sym={sym}"
# def

def toplogic():
    with open(sys.argv[1], encoding='ascii') as ifile:
        contents = ifile.read()

    contents = re.sub(r'//[^\n]*\n', ' ', contents)
    contents = re.sub('endmodule', 'endmodule;', contents)
    contents = re.sub(r'\n', r' ', contents)
    contents = re.sub(r'[(),.]', r' ', contents)
    contents = re.sub(r'\[\s*(\d+)\s*:\s*(\d+)\s*\]', r'[\1:\2]', contents)
    contents = re.sub(r'\s+', r' ', contents)

    skipme = False
    lines = defaultdict(set)
    namemax = 0
    for stmt in contents.split(';'):
        f = stmt.lower().split()
        if not f: continue
        if f[0] == "module":
            name = stmt.split()[1]
            width = {}
            functions = {}
            vals = {}
            #print(f"\nStarting {name}")
            skipme = name == "frac_lut6_mux"
            continue
        if skipme: continue
        if f[0] == "input":
            assert len(f) == 3, f"f={f}"
            (a, b) = re.match(r'\[(\d+):(\d+)\]$', f[1]).groups()
            (a, b) = (int(a), int(b))
            var = f[2]
            width[var] = 1 + abs(a - b)
            for i in range(min(a, b), max(a, b)+1):
                vari = f"{var}[{i}]"
                vals[vari] = vari
            continue
        if f[0] == "output":
            assert len(f) == 3, f"f={f}"
            outp = f[2]
            continue
        if f[0] != "endmodule":
            del f[0:2]
            assert not 1 & len(f), f"f = {f}"
            conns = dict(zip(f[0::2], f[1::2]))
            if   len(conns) == 2:
                functions[conns['z']] = [ conns['a'] ]
            elif len(conns) == 4:
                functions[conns['z']] = [ conns['s'], conns['d1'], conns['d0'] ]
            else:
                assert 0, f"f={f} conns={conns}"
            continue
        # now compute the function and print it out
        wsel = width["sram"]
        vals["sram"] = vals["sram[0]"]
        want2sels = defaultdict(set)
        for sel in range(1 << wsel):
            for b in range(wsel):
                vari = f"sram[{b}]"
                vals[vari] = str(1 & (sel >> b))
            z = compute(outp, vals, functions)
            # for b
            #print(f"{name} sel={sel} out={z}")
            want2sels[z].add(str(sel))
        # for sel
        line = name + " = { "
        if len(name) > namemax: namemax = len(name)
        rev = max(want2sels[min(want2sels)]) > min(want2sels[max(want2sels)])
        for want in sorted(want2sels, key=dict_aup_nup):
            nwant = int(re.sub(r'^in\[(\d+)\]$', r'\1', want))
            sels = sorted(want2sels[want], key=dict_aup_nup, reverse=rev)
            sels = [int(s) for s in sels]
            line += f"{nwant}:{sels}, "
        line += "}"
        sz = int(re.sub(r'^\w+_size(\d+) = .*$', r'\1', line))
        lines[sz].add(line)
    # for stmt

    namemax += 1
    for sz in sorted(lines):
        for muxes in sorted(lines[sz], key=dict_aup_nup):
            f = muxes.split('=')
            print(f"{f[0]:{namemax}s}={f[1]}")
# def toplogic

toplogic()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
