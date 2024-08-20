#!/nfs_home/dhow/bin/python3
"""read in fpga_top.v, add loops to chanx/y instance ports, write it out again"""

import sys
#import os
import re
from collections import defaultdict

g_root = "/nfs_project/castor/castor_release"
g_rel = "v1.6.225"
g_dev = "FPGA10x8_gemini_compact_latch_pnr"
g_default = "{g_root}/{g_rel}/k6n8_TSMC16nm_7.5T/{g_dev}/_run_dir/SRCphys/fpga_top.remapped.v"
g_default = "fpga_top.remapped.v"

g_wirere = r'(cbx|cby|sb)_\d+__\d+__\d+_chan[xy]_(left|right|top|bottom)_out*;$'
g_portre = (
    r'[.]((chan[xy])_((?:right|top|bottom|left)_(?:in|out))(\d*))[(]' +
    r'((?:cbx|cby|sb)_\d+__\d+__\d+_chan[xy]_(?:right|left|top|bottom)_out)[)](,?)$'
)

def toplogic():
    """process the file"""

    # arguments
    loopw = 8
    setloopw = False
    nparg = 0
    filename = g_default
    for arg in sys.argv[1:]:
        if arg == "-l":
            setloopw = True
            continue
        if setloopw:
            loopw = int(arg)
            setloopw = False
            continue
        if not nparg:
            nparg += 1
            filename = arg
            continue
        sys.stderr.write("Extra argument: {arg}\n")
        sys.exit(1)
    # for

    # information from first pass
    bl = set()
    br = set()
    xset = set()
    yset = set()
    xtriples = set()
    nsunused = 0
    with open(filename, encoding='ascii') as ifile:
        sys.stderr.write(f"Reading {filename}\n")
        for line in ifile:
            ff = line.split()
            # deduce channel bus width
            if len(ff) == 3 and ff[0] == "wire":
                if not re.match(g_wirere, ff[2]): continue
                g = re.match(r'\[(\d+):(\d+)\]$', ff[1])
                assert g
                bl.add(int(g.group(1)))
                br.add(int(g.group(2)))
                continue
            # if
            # deduce coordinates
            if len(ff) == 3 and ff[0] != "module" and ff[2] == "(":
                g = re.match(r'tile_(\d+)__(\d+)_$', ff[1])
                if not g: continue
                xset.add(int(g.group(1)))
                yset.add(int(g.group(2)))
                if "_bram_" in ff[0] or "_dsp" in ff[0]:
                    xtriples.add(int(g.group(1)))
                    if "_io_tile" not in ff[0]:
                        nsunused += 4
                continue
            # if
        # for
    # with
    # always saw same indices
    assert len(bl) == 1
    assert len(br) == 1
    bl = list(bl)[0]
    br = list(br)[0]
    # exactly one is zero
    assert (not bl) != (not br)
    if bl:
        subbus = f"[{bl-loopw}:0]"
    else:
        subbus = f"[0:{br-loopw}]"
    sys.stderr.write(f"{len(xset)}x{len(yset)} coordinates; channels declared [{bl}:{br}]; {subbus} not looped\n")
    xset.add(0)
    yset.add(0)

    # transform on second pass
    sd2vector = {
        'left_in':      'e',
        'right_out':    'e',
        'right_in':     'w',
        'left_out':     'w',
        'bottom_in':    'n',
        'top_out':      'n',
        'top_in':       's',
        'bottom_out':   's',
    }
    firstwire = True
    brl = "{"
    brr = "}"
    xset = sorted(xset)
    yset = sorted(yset, reverse=True)
    (iotile, x, y) = (False, None, None)
    loops = {}
    loop2n = defaultdict(int)
    startinst = False
    inst = 0
    instchanged = 0
    portchanged = 0
    with open(filename, encoding='ascii') as ifile:
        sys.stderr.write(f"Reading {filename}\n")
        for line in ifile:
            ff = line.split()
            # insert new wire declarations
            if firstwire and len(ff) == 3 and ff[0] == "wire":
                firstwire = False
                for x in xset:
                    for y in yset:
                        for vector in ['e', 'w', 'n', 's', ]:
                            loop = f"{vector}loop_{x}__{y}_"
                            print(f"wire [0:{loopw - 1}] {loop};")
                            loops[loop] = (x, y)
                continue
            # deduce coordinates
            if len(ff) == 3 and ff[0] != "module" and ff[2] == "(":
                g = re.match(r'tile_(\d+)__(\d+)_$', ff[1])
                if not g: continue
                iotile = "_io_tile" in ff[0]
                x = int(g.group(1))
                y = int(g.group(2))
                startinst = True
                inst += 1
            # if
            # adjust channel bus connections
            if len(ff) == 1 and (g := re.match(g_portre, ff[0])):
                (port, chan, sd, which, bus, comma) = g.groups()
                vector = sd2vector[sd]
                which = int(which) if which else 0
                if iotile:
                    which = -which
                (x0, y0) = (x, y)
                if chan == "chany":
                    x0 += which
                else:
                    y0 += which
                loop = f"{vector}loop_{x0}__{y0}_"
                assert loop in loops
                assert loops[loop] == (x0, y0)
                loop2n[loop] += 1
                assert loop2n[loop] <= 2
                if bl:
                    bus = f"{brl}{loop},{bus}{subbus}{brr}"
                else:
                    bus = f"{brl}{bus}{subbus},{loop}{brr}"
                fw = re.split(r'(\s+)', line)
                assert len(fw) > 2 and fw[2].startswith(".chan")
                fw[2] = f".{port}({bus}){comma}"
                line = ''.join(fw)
                portchanged += 1
                if startinst:
                    startinst = False
                    instchanged += 1
            # if
            # print the line
            print(line[:-1])
        # for
    # with

    triple_missed = 0
    for loop in loops:
        if loop in loop2n: continue
        g = re.match(r'([ewns])loop_(\d+)__(\d+)_$', loop)
        assert g
        (vector, x, y) = g.groups()
        (x, y) = (int(x), int(y))
        if vector in "ew" and x == 0: continue
        if vector in "ns" and y == 0: continue
        #  north/south        bram/dsp col      interior row        not bot in bram/dsp
        if vector in "ns" and x in xtriples and 1 < y < yset[0] and (y - 2) % 3:
            triple_missed += 1
            continue
        sys.stderr.write(f"Declared but unused: {loop}\n")
    # for
    sys.stderr.write(f"{triple_missed} loops declared but unused on BRAM/DSP ({nsunused} expected)\n")

    sys.stderr.write(f"{inst} instances seen: {portchanged} ports looped on {instchanged} instances\n")
    sys.stderr.write("Done\n")
# def

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: ignorecase filetype=python autoindent hlsearch syntax=on fileformat=unix
