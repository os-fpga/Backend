#!/usr/bin/python3
"""LUTRAM experiments"""

import sys
import re 
from collections import defaultdict

def log(msg):
    """error msg to stderr"""
    sys.stderr.write(msg)
# def log

def dict_aup_nup(k):
    """sort with alpha ascending and numeric ascending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    return r
# def dict_aup_ndn

def dict_aup_ndn(k):
    """sort with alpha ascending and numeric descending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] = -int(r[i])
    return r
# def dict_aup_ndn

def toplogic():
    """create local routing mux file"""

    # 5 0 0 clb         I75             RIGHT  INPUT    47 clb.I30[11]
    xml2mux = {}
    with open("clb.pinmap", encoding='ascii') as ifile:
        for line in ifile:
            ff = re.sub(r'#.*', r'', line).strip().split()
            if len(ff) != 9: continue
            if ff[3] != "clb" or ff[6] != "INPUT": continue
            (muxname, xmlname) = (ff[4], ff[8])
            #log(f"muxname={muxname} xmlname={xmlname}\n")
            xml2mux[xmlname] = muxname
        # for
    # with

    beg2ends = defaultdict(set)
    with open("clb.xml", encoding='ascii') as ifile:
        for line in ifile:
            if "<direct" not in line and "<complete" not in line: continue
            g = re.search(r'\sname="([^"]+)".*\sinput="([^"]+)".*\soutput="([^"]+)"', line)
            if not g: continue
            (name, inp,  out) = g.groups()
            #log(f"name={name} input={inp} output={out}\n")
            if name.startswith("direct_in_"):
                continue
            g = re.match(r'clb_lr.in\[(\d+):(\d+)\]$', out)
            assert g
            (l, r) = g.groups()
            (l, r) = (int(l), int(r))
            assert l == r + 7
            lut_in = r >> 3
            for inpname in inp.split():
                if inpname.startswith("clb_lr.out"):
                    g = re.match(r'clb_lr.out\[(\d+):(\d+)\]$', inpname)
                    assert g
                    (l, r) = g.groups()
                    (l, r) = (int(l), int(r))
                    assert l >= r
                    for b in range(l, r-1, -1):
                        end = f"O{b}"
                        for f in range(8):
                            local = f"L{f}{lut_in}"
                            beg2ends[local].add(end)
                else:
                    assert re.match(r'clb.I\d\d$', inpname), f"\ninpname={inpname}"
                    for b in range(12):
                        xmlname = f"{inpname}[{b}]"
                        muxname = xml2mux[xmlname]
                        for f in range(8):
                            local = f"L{f}{lut_in}"
                            beg2ends[local].add(muxname)
                        # for f
                    # for b
            # for inpname
        # for
    # with

    # add the specials
    for e in range(6):
        for b in range(6):
            beg2ends[f"LS{b}"].add(f"IS{e}")
        # for b
    # for e

    # write it out
    for beg in sorted(beg2ends, key=dict_aup_nup):
        ends = sorted(beg2ends[beg], key=dict_aup_nup)
        print(f"{beg} {' '.join(ends)}")
    # for
# def

toplogic()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
