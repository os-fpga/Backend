#!/usr/bin/python3
"""create local routing.
   due to extreme flow and router inadequacies,
   this pattern is absurdly simple.
   in particular, it was defined before flat routing.
   redo from scratch after flow stabilizes."""

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

def write_json(jname, dst2src):
    """write accumulated connections to JSON file"""
    # prep for JSON
    table = []
    for dst in sorted(dst2src, key=dict_aup_ndn):
        ins = sorted(dst2src[dst], key=dict_aup_nup)
        row = []
        for i in ins:
            g = re.match(r'(\S+)\[(\S+)\]$', i)
            assert g
            row.append([g.group(1), g.group(2)])
        table.insert(0, row)
    # for dst

    # dump to JSON
    with open(jname, "w", encoding='ascii') as jfile:
        log(f"Writing {jname}\n")
        sep = "[\n"
        for row in table:
            rowtxt = ",".join([f'["{i[0]}","{i[1]}"]' for i in row])
            jfile.write(f"{sep}[{rowtxt}]")
            sep = ",\n"
        # for row
        jfile.write("\n]\n")
    # with
# def

def toplogic():
    """generate crossbars for normal local routing connections"""

    dst2src = defaultdict(set)

    for di in range(6):
        for df in range(8):
            dst = f"F{df}I{di}"
            # crossbar over evens + crossbar over odds, interdigitated
            # even crossbar: choose 2 of 0,2,4: 0,2 0,4 2,4
            # odd  crossbar: choose 2 of 1,3,5: 1,3 1,5 3,5
            #
            # objectives in this table (in re-arranging pairs above):
            # source numbers reaching neither 4 nor 5 (2 fastest sinks) can't be adjacent
            #   0 and 3 are not adjacent
            # dist=0 should always be possible
            # d=5 should have smallest max dist
            # i think this leads to a unique solution
            #
            # should have been an ILP but small enough to solve manually
            # not sure if router can really use all this but doesn't hurt
            #
            # we get:
            # 8 0's + 8 4's ==> 8 0's (even crossbar)
            # 8 1's + 8 3's ==> 8 1's (odd  crossbar)
            # 8 0's + 8 2's ==> 8 2's (even crossbar)
            # 8 3's + 8 5's ==> 8 3's (odd  crossbar)
            # 8 2's + 8 4's ==> 8 4's (even crossbar)
            # 8 1's + 8 5's ==> 8 5's (odd  crossbar)
            # all these can be routed with only local CAPACITY information
            for si in [ [0, 4],         # di=0 dist=0,2
                        [1, 3],         # di=1 dist=0,4
                        [0, 2],         # di=2 dist=0,2
                        [3, 5],         # di=3 dist=0,2
                        [2, 4],         # di=4 dist=0,4
                        [1, 5], ][di]:  # di=5 dist=0,2
                for sf in range(8):
                    src = f"I{sf}[{si}]"
                    dst2src[dst].add(src)
                # for sf
            # for si
        # for df
    # for di
    # loop counts tell us there are 6*8*2*8 = 768 input pins
    # loop counts tell us there are 6*8 = 48 muxes
    # therefore 768/48 = 16 inputs per mux

    # Rev4: simpler pattern replaces previous.
    # also includes transposition doing later in emit_lr.
    if g_false:
        dst2src.clear()
        for di in range(6):                 # dest input
            for df in range(8):             # dest FLE
                dst = f"F{df}I{di}"

                # I2[15:0] drives in5 and in2
                # I1[15:0] drives in4 and in1
                # I0[15:0] drives in3 and in0

                for sb in range(16):        # source bit
                    sp = di % 3             # source port
                    src = f"I{sp}[{sb}]"
                    dst2src[dst].add(src)
                # for sb
            # for df
        # for di
    # if

    # Rev5: simpler pattern replaces previous.
    # also includes transposition doing later in emit_lr.
    if g_true:
        dst2src.clear()
        for di in range(6):                 # dest input
            for df in range(8):             # dest FLE
                dst = f"F{df}I{di}"

                # T I0[11:0] drives in0/2/4
                # T I1[11:0] drives in1/3/5
                # R I2[11:0] drives in1/2/5
                # R I3[11:0] drives in0/3/4

                for sp in [ [0, 3],         # in0 from I0/I3
                            [1, 2],         # in1 from I1/I2
                            [0, 2],         # in2 from I0/I2
                            [1, 3],         # in3 from I1/I3
                            [0, 3],         # in4 from I0/I3
                            [1, 2],][di]:   # in5 from I1/I2

                    for sb in range(12):        # source bit
                        src = f"I{sp}[{sb}]"
                        dst2src[dst].add(src)
                    # for sb
                # for sp
            # for df
        # for di
        write_json("other.json", dst2src)

        # add feedback connections for CLB since arch XML can't express them
        for di in range(0, 2):      # dest input (only 0/1 of 6)
            for df in range(8):     # dest FLE
                dst = f"F{df}I{di}"

                for sf in range(8): # source FLE owning O6
                    src = f"O[{sf * 3 + 2}]"
                    dst2src[dst].add(src)
                # for sf
            # for df
        # for di
        write_json("clb.json", dst2src)
    # if

# def

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch
# vim: filetype=python
# vim: syntax=on
# vim: fileformat=unix
