#!/usr/bin/python3
"""collect and display connections stats"""

import sys
from collections import defaultdict
import re 

g_false = False
g_true = True

def log(m):
    """msg to stderr"""
    sys.stderr.write(m)

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

def toplogic():
    """collect and display connections stats"""

    # bus --> bit
    dbus_set = defaultdict(set)
    sbus_set = defaultdict(set)
    # dbus --> sbus --> set(dbit, sbit)
    driven = defaultdict(lambda: defaultdict(set))
    iname = "clb.mux"
    with open(iname, encoding='ascii') as ifile:
        print(f"Reading {iname}")
        for line in ifile:
            if line[0] == '#': continue
            f = line.split()
            dbus = "??"
            dbit = "??"
            i = -1
            #print(f"LINE: {f}")
            for bit in f:
                if re.match(r'\d', bit): continue
                (bus, n) = re.subn(r'^(O|IS|I|[ENSW]\d+[BQMPE]).*', r'\1', bit)
                if not n: continue

                i += 1
                if not i:
                    #print(f"SINK: {bus} {bit}")
                    dbus = bus
                    dbit = bit
                    dbus_set[dbus].add(dbit)
                    continue

                #print(f"SOURCE: {bus} {bit}")
                sbus = bus
                sbit = bit
                sbus_set[sbus].add(sbit)
                driven[dbus][sbus].add((dbit, sbit))
            # for i, bit
            #print("")
        # for line
    # with

    #
    # print it all out
    #

    print("")
    print("Sources")
    for sbus in sorted(sbus_set, key=dict_aup_ndn):
        print(f"{sbus:<4s} {len(sbus_set[sbus]):2d} {' '.join(sorted(sbus_set[sbus], key=dict_aup_ndn))}")

    print("")
    print("Destinations")
    for dbus in sorted(dbus_set, key=dict_aup_ndn):
        print(f"{dbus:<4s} {len(dbus_set[dbus]):2d} {' '.join(sorted(dbus_set[dbus], key=dict_aup_ndn))}")

    print("")
    print("Summary connection statistics (each pair is connections/sources connections/destinations)")
    print(f"{'s/d':<4s}", end='')
    for dbus in sorted(dbus_set, key=dict_aup_ndn):
        print(f"  {dbus:<4s}  -  ", end='')
    print("")
    for sbus in sorted(sbus_set, key=dict_aup_ndn):
        print(f"{sbus:<4s}", end='')
        for dbus in sorted(dbus_set, key=dict_aup_ndn):
            c = len(driven[dbus][sbus])
            if not c:
                print(f"  {' -    -':9s}", end='')
            else:
                c_over_sources = c / len(sbus_set[sbus])
                c_over_sinks   = c / len(dbus_set[dbus])
                print(f"  {c_over_sources:4.2f} {c_over_sinks:4.2f}", end='')
        # for dbus
        print("")
    # for sbus

    print("")
    print("Wire name key: {direction=East/North/South/West}{length}{tap=Begin/Quarter/Middle/Penultimate/End}{bit}")
# def

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch
# vim: filetype=python
# vim: syntax=on
# vim: fileformat=unix
