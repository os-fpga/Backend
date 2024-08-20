#!/usr/bin/env python3
"""regenerate repack.xml from .route"""

import os
import sys
import re

def toplogic():
    """main code"""
    assert len(sys.argv) >= 3
    routefilename   = sys.argv[1]   # .route
    repackfilename  = sys.argv[2]   # fpga_repack_constraints.xml
    sys.stderr.write("Reading .route to update repack.xml\n")

    # find globals in .route
    routeglobals = set()
    with open(routefilename, encoding='ascii') as ifile:
        sys.stderr.write(f"Reading {routefilename}\n")
        for line in ifile:
            g = re.match(r'Net \d+ [(](\S+)[)]: global net connecting:', line)
            if not g: continue
            routeglobals.add(g.group(1))
        # for
    # with
    sys.stderr.write(f"Clocks in {routefilename}: {len(routeglobals)}\n")

    # find globals in repack.xml
    repackglobals = {}
    repackclocks = {}
    have_repack = os.path.isfile(repackfilename)
    if have_repack:
        with open(repackfilename, encoding='ascii') as ifile:
            sys.stderr.write(f"Reading {repackfilename}\n")
            for line in ifile:
                g = re.match(
r'\s*<pin_constraint\s+pb_type="([^"]+)"\s+pin="([^"]+\[(\d+)\])"\s+net="([^"]+)"', line)
                if not g: continue

                (pbt, pin, bit, net) = g.groups()
                if net.upper() == "OPEN": continue
                assert pbt in ("clb", "io", "bram", "dsp", )
                assert pin.startswith("clk[")
                bit = int(bit)

                if net in repackglobals:
                    assert repackglobals[net] == bit
                else:
                    repackglobals[net] = bit
                if bit in repackclocks:
                    assert repackclocks[bit] == net
                else:
                    repackclocks[bit] = net
            # for
        # with
        assert len(repackglobals) == len(repackclocks)
    # if
    repackstart = dict(repackglobals)
    sys.stderr.write(f"Clocks in {repackfilename}: {len(repackglobals)}\n")

    # drop clocks in repack.xml not in .route
    dropme = {}
    for net, bit in repackglobals.items():
        if net in routeglobals: continue
        sys.stderr.write(f"Dropping clock that appears in repack.xml but not in .route: Clock {bit} {net}\n")
        dropme[net] = bit
    # for
    for net, bit in dropme.items():
        del repackglobals[net]
        del repackclocks[bit]
    # for

    # clocks not yet used
    numclocks = 16
    unusedclocks = [ c for c in range(numclocks) if c not in repackclocks ]

    # assign any clocks in .route but not in repack.xml
    for net in routeglobals:
        if net in repackglobals: continue
        u = unusedclocks.pop(0)
        sys.stderr.write(f"Adding clock that appears in .route but not in repack.xml: Clock {u} {net}\n")
        repackglobals[net] = u
        repackclocks[u] = net
    # for

    # write new repack.xml file
    if repackstart != repackglobals:
        #repackfilename = "/dev/stdout"  # DEBUG
        with open(repackfilename, "w", encoding='ascii') as ofile:
            sys.stderr.write(f"Writing {repackfilename}\n")
            ofile.write("<repack_design_constraints>\n")
            for pb_type in ("clb", "io", "dsp", "bram", ):
                for bit in range(numclocks):
                    net = repackclocks.get(bit, "OPEN")
                    ofile.write(f'<pin_constraint pb_type="{pb_type}" pin="clk[{bit}]" net="{net}" />\n')
                # for
            # for
            ofile.write("</repack_design_constraints>\n")
        # with
    else:
        sys.stderr.write("No update needed\n")
    # if
# def toplogic

toplogic()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
