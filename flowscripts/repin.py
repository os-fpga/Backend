#!/usr/bin/env python3
"""replace pins"""

import os
import sys
import re

g_false = False
g_nochange = False

g_maxlen = 0                # will be computed

(g_oldxmin, g_oldymin) = (1, 1)
(g_oldxmax, g_oldymax) = (0, 0)   # will be updated
(g_newxmin, g_newymin) = (1, 1)
(g_newxmax, g_newymax) = (0, 0)   # will be updated

(g_uxmin, g_uxmax) = (0, 0) # will be computed
(g_uymin, g_uymax) = (0, 0) # will be computed

# place one direction (input or output) of pins
def place_pins(per_tile, base, pins, iotiles):
    """place pins following previous order"""
    avail_slots = per_tile * iotiles
    slots_per_pin = avail_slots / len(pins)
    sys.stderr.write(
f"\npins={len(pins):,d} avail_slots={avail_slots:,d} slots_per_pin={slots_per_pin:.2f}\n")

    (cside, cx, cy, cz) = (0, g_newxmin, g_uymax, slots_per_pin / 2)
    sides = [0, 0, 0, 0]
    lines = []
    for pin in pins:

        # legalize cx, cy, cz
        while round(cz) >= per_tile:
            # Step 1: decrease z by one tile
            cz -= per_tile
            # Step 2: increase x,y by one tile
            match cside:
                case 0:
                    cy -= 1
                    if cy < g_uymin:
                        cside = 1
                        cx = g_uxmin
                        cy = g_newymin
                case 1:
                    cx += 1
                    if cx > g_uxmax:
                        cside = 2
                        cx = g_newxmax
                        cy = g_uymin
                case 2:
                    cy += 1
                    if cy > g_uymax:
                        cside = 3
                        cx = g_uxmax
                        cy = g_newymax
                case 3:
                    cx -= 1
                    if cx < g_uxmin:
                        assert 0, f"sides={sides}"
            # match
        # while

        #name side  x  y  z
        (name,   _, _, _, _) = pin
        tabout = "\t" * ((7 + g_maxlen - len(name)) // 8)
        lines.append(f"{name}{tabout}{cx}\t{cy}\t{round(cz) + base}\n")
        sides[cside] += 1

        # move to next coordinate
        cz += slots_per_pin
    # for
    sides = ', '.join([f'{s:,d}' for s in sides])
    sys.stderr.write(f"placed CCW on sides LBRT: {sides}\n")
    return lines
# def place_pins

def pin_sort_func(r):
    """decreasing LEFT -> increasing BOTTOM -> increasing RIGHT -> decreasing TOP"""
    match r[1]:
        case 0:
            return (0, -r[3], -r[4])
        case 1:
            return (1,  r[2],  r[4])
        case 2:
            return (2,  r[3],  r[4])
        case 3:
            return (3, -r[2], -r[4])
    # match
# def pin_sort_func

def toplogic():
    """main code"""
    assert len(sys.argv) >= 5
    #raptorlog       = sys.argv[1]   # original raptor.log file
    inlocfilename   = sys.argv[2]   # original .place or .place.raptor
    varsfilename    = sys.argv[3]   # vars.sh we are now using
    outlocfilename  = sys.argv[4]   # new _pin_loc.place file

    sys.stderr.write("\n\nREPLACING PINS\n")
    #sys.stderr.write(f"\tlog:\t{raptorlog}\n")  # TODO remove
    sys.stderr.write(f"\tplace:\t{inlocfilename}\n")
    sys.stderr.write(f"\tvars:\t{varsfilename}\n")
    sys.stderr.write(f"\tpinloc:\t{outlocfilename}\n\n")

    ## determine device size
    #with open(raptorlog, encoding='ascii') as ifile:
    #    sys.stderr.write(f"Reading {raptorlog}\n")
    #    for line in ifile:
    #        g = re.match(r'FPGA sized to (\d+) x (\d+) ', line)
    #        if not g: continue
    #        (w, h) = g.groups()
    #        (w, h) = (int(w)-2, int(h)-2)
    #        assert w > 0 and h > 0
    #        global g_oldxmax, g_oldymax
    #        if not g_oldxmax:
    #            (g_oldxmax, g_oldymax) = (w, h)
    #        elif g_oldxmax != w or g_oldymax != h:
    #            sys.stderr.write(f"Inconsistent device size in {raptorlog}\n")
    #            sys.exit(1)
    #    # for
    ## with

    global g_maxlen

    # manage older versions of placement file
    # specified .place?
    if inlocfilename.endswith(".place"):
        # there must not be a .raptor
        assert not os.path.isfile(inlocfilename + ".raptor"), f"inlocfilename={inlocfilename}"
        # rename .place to .place.raptor and use latter
        inlocfilenamenew = inlocfilename + ".raptor"
        sys.stderr.write(f"Renaming {inlocfilename} --> {inlocfilenamenew}\n")
        if not g_nochange:
            os.replace(inlocfilename, inlocfilenamenew)
            inlocfilename = inlocfilenamenew
    elif inlocfilename.endswith(".place.raptor"):
        # if .place.raptor doesn't exist and .place does,
        if not os.path.isfile(inlocfilename):
            if os.path.isfile(inlocfilename[:-7]):
                # rename .place to .place.raptor
                sys.stderr.write(f"Renaming {inlocfilename[:-7]} --> {inlocfilename}\n")
                if not g_nochange:
                    os.replace(inlocfilename[:-7], inlocfilename)
                else:
                    inlocfilename = inlocfilename[:-7]
            else:
                # it will fail below
                pass
    else:
        sys.stderr.write(f"Input location file {inlocfilename} must end with .place or .place.raptor\n")
        sys.exit(1)

    ipins = []
    opins = []
    # read the previous (pin) placement file
    with open(inlocfilename, encoding='ascii') as ifile:
        sys.stderr.write(f"Reading {inlocfilename}\n")
        for line in ifile:
            # ignore garbage
            if line.startswith("Netlist_File:"): continue
            if line.startswith("Array size:"):
                ff = line.split()
                (g_oldxmax, g_oldymax) = (int(ff[2]) - 2, int(ff[4]) - 2)
                assert g_oldxmax > 0 and g_oldymax > 0
                continue
            line = line.strip()
            if not line or line[0] == "#": continue
            ff = line.split()
            if len(ff) < 4: continue
            # extract info
            (name, x, y, z, *_) = ff
            (x, y, z) = (int(x), int(y), int(z))
            # ignore objects not in IO ring
            if x not in (g_oldxmin, g_oldxmax) and y not in (g_oldymin, g_oldymax): continue
            # track widest signal name
            if len(name) > g_maxlen: g_maxlen = len(name)
            # check stuff
            assert 1 <= x <= g_oldxmax and 1 <= y <= g_oldymax
            assert 0 <= z < 72
            # which side?
            # 0=L 1=B 2=R 3=T
            side = 0 if x == 1 else 2 if x == g_oldxmax else 1 if y == 1 else 3
            # save the pin
            if name.startswith("out:"):
                #             0     1     2  3  4
                opins.append((name, side, x, y, z))
            else:
                ipins.append((name, side, x, y, z))
        # for
    # with
    assert g_oldxmax > 0 and g_oldymax > 0
    sys.stderr.write(
f"Old = {g_oldxmax}x{g_oldymax} array, {len(ipins):,d} inputs, {len(opins):,d} outputs\n")

    # round up to a tab stop
    minspace = 4
    g_maxlen = (g_maxlen + minspace + 7) & ~7

    # find new size
    global g_newxmax, g_newymax
    with open(varsfilename, encoding='ascii') as ifile:
        sys.stderr.write(f"Reading {varsfilename}\n")
        for line in ifile:
            g = re.search(r' SIZE=(\d+)x(\d+)', line)
            if g: (g_newxmax, g_newymax) = (int(g.group(1)), int(g.group(2)))
        # for
    # with

    # determine how much padding we should have
    # this is # IOTiles next to corners we leave unused
    pad = 1 if g_newxmax <= 10 else 4

    ipins = sorted(ipins, key=pin_sort_func)
    opins = sorted(opins, key=pin_sort_func)

    # compute numbers describing USABLE IOTiles
    width = g_newxmax - 2 - pad * 2
    height = g_newymax - 2 - pad * 2
    global g_uxmin, g_uxmax, g_uymin, g_uymax
    (g_uxmin, g_uxmax) = (g_newxmin + 1 + pad, g_newxmax - 1 - pad)
    (g_uymin, g_uymax) = (g_newymin + 1 + pad, g_newymax - 1 - pad)
    iotiles = 2 * width + 2 * height

    sys.stderr.write(
f"New = {g_newxmax}x{g_newymax} array, " +
f"usable: pad={pad}, width={width}, height={height}, iotiles={iotiles}, " +
f"uxmin={g_uxmin}, uxmax={g_uxmax}, uymin={g_uymin}, uymax={g_uymax}\n")

    # get old contents if any
    have_old = os.path.isfile(outlocfilename)
    oldlines = []
    if have_old:
        with open(outlocfilename, encoding='ascii') as ifile:
            sys.stderr.write(f"Reading {outlocfilename}\n")
            for line in ifile:
                oldlines.append(line)
    else:
        sys.stderr.write(f"No pre-existing {outlocfilename}\n")

    # generate new contents
    newlines  = place_pins(24,  0, ipins, iotiles)
    newlines += place_pins(48, 24, opins, iotiles)

    # update if changed
    if oldlines != newlines:
        if have_old and not os.path.isfile(outlocfilename + ".bak"):
            sys.stderr.write(f"Renaming {outlocfilename} --> {outlocfilename}.bak\n")
            if not g_nochange:
                os.replace(outlocfilename, outlocfilename + ".bak")
        outlocfilename0 = "/dev/null" if g_nochange else outlocfilename
        with open(outlocfilename0, "w", encoding='ascii') as ofile:
            sys.stderr.write(f"Writing {outlocfilename}\n")
            for line in newlines:
                ofile.write(line)
        sys.stderr.write(f"\nWrote {len(newlines):,d} pins\n")
    else:
        sys.stderr.write(f"\nUnchanged: {outlocfilename}\n")

# def toplogic

toplogic()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
