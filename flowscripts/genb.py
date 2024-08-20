#!/usr/bin/python3
"""create a simple pattern for other work.
   due to extreme flow and router inadequacies,
   this pattern is absurdly simple.
   redo from scratch after flow stabilizes."""

import sys
#import os 
from collections import defaultdict
import re 

g_false = False
g_true = True
g_safe = False  # keep this "False" now that the flow works

### some standard routines

def log(text):
    """write to stderr"""
    sys.stderr.write(text)
# def log

def out(text):
    """write to stdout"""
    sys.stdout.write(text)
    sys.stdout.flush()
# def out

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

# reverse direction
g_enter2leave = {'E': 'W', 'W': 'E', 'N': 'S', 'S': 'N', }

# -1 = turn left; 0 = straight; 1 = turn right
# angle = n*90
g_dir2dir2turn = {
    # source        -- dest --
    'E': {'E': 0, 'N':-1,         'S': 1, },
    'N': {'E': 1, 'N': 0, 'W':-1,         },
    'W': {        'N': 1, 'W': 0, 'S':-1, },
    'S': {'E':-1,         'W': 1, 'S': 0, },
    # actually, -1 & 1 reversed, but OK as long as consistent
    # dest          -- source --
}

def generate_grid(ofile, enters):
    """generate muxes for one routing grid"""

    # enter == direction(s) that enter IOTile from fabric
    # leave == direction(s) that leave IOTile to   fabric
    enter = enters[0]
    clb = enter == 'C'  # this has 2 effects
    clb = False         # FIXME make CLBs look like non-CLBs
    iotile = enter not in 'CO'
    if iotile: leave = g_enter2leave[enter]
    beg2ends = defaultdict(set)

    # Steps 1 & 2: SB to SB, outputs to SB

    # first iterate over the wires to whose driving muxes we are spec'ing inputs
    for ln in [1, 4, ]:
        for bdi in ['E', 'N', 'W', 'S', ]:
            for bb in range(2*8):
                f = bb >> 1

                # name of mux driving into = BEG of new wire
                beg = f"{bdi}{ln}B{bb}"

                # For IOTiles, w e use the driver driving L1 outside an IOTile
                # to instead drive an L4 back into the array,
                # from its penultimate tap.
                if iotile and not g_safe:
                    if beg.startswith(f"{enter}1B"):
                        beg = f"{leave}4P{bb}"
                # TODO: <THIS> should appy to anything in enters not just first

                # Step 1: SB to SB

                for edi, turn in g_dir2dir2turn[bdi].items():

                    # 2 src: if straight, source from both lengths
                    if not turn:
                        end = f"{edi}1E{bb}"
                        beg2ends[beg].add(end)
                        end = f"{edi}4E{bb}"
                        beg2ends[beg].add(end)
                        continue

                    # 2 src: if turning, source from same length
                    # (there are two turns)
                    end = f"{edi}{ln}E{bb}"
                    beg2ends[beg].add(end)

                    # 1 src: depends on len

                    # if turning len=1, alternate turn+twist from len=1
                    if ln == 1 and (bb & 1) == max(turn, 0):
                        # it's important this twist to the immediate neighbor,
                        # since we have lots of distance=2 skipping elsewhere.
                        eb = bb + turn
                        eb = (eb + 2*2*8) % (2*8)
                        end = f"{edi}1E{eb}"
                        beg2ends[beg].add(end)

                    # if turning len=4, alternate turn w/o twist from len=1
                    if ln == 4 and (bb & 1) == max(turn, 0):
                        end = f"{edi}1E{bb}"
                        beg2ends[beg].add(end)

                # for edi, turn

                # Step 2: outputs to SB 0=reg0 1=reg1 2=O6

                # ln in {1, 4}, bdi in ENWS, bb in 0-15 (f in 0-7), computed "beg"

                # special overdesign for now in IOTile output connections
                # TODO: merge design with <THAT>
                if iotile and (g_safe or g_true):
                    for op in range(24 >> 1):
                        end = op * 2
                        # evens->evens and odds->odds, so +/- 1 twist sufficient
                        if bb & 1: end += 1
                        end = f"O{end}"
                        beg2ends[beg].add(end)
                    # for op
                    continue
          
                # 3 src: all 3 outputs for that plane, or same but dispersed
                # even bb receive all 3 outputs in same plane
                # odd  bb receives 3 outputs with plane dispersion
                for op in range(3):
                    end = f * 3 + op
                    if bb & 1:
                        if bb & 2:
                            end -= 3
                        else:
                            end += 3
                        assert 0 <= end <= 24
                    end = f"O{end}"
                    beg2ends[beg].add(end)
                # for op
            # for bb
        # for bdi
    # for ln

    # Step 3 SB to inputs
    #
    # remember this of routing downstream from CBs
    # %5 reaches all in3,in5
    # %4 reaches all in0,in4
    # %3 reaches all in1,in3
    # %2 reaches all in2,in4
    # %1 reaches all in1,in5
    # %0 reaches all in0,in2
    # where %n means N % 6 == n in CB#N or If[n]

    # Therefore here's how to pack 6 signals s0--s5 across 4 input pins each:
    # %5         s2! s3  s4  s5  width=4
    # %4     s1! s2  s3  s4      width=4
    # %3 s0! s1  s2  s3          width=4
    # %2 s0  s1  s2          s5! width=4
    # %1 s0  s1          s4! s5  width=4
    # %0 s0          s3! s4  s5  width=4
    # FO  4   4   4   4   4   4 
    #
    # verify:
    # s5 reaches in0  in1  in22 in3  in4  in55
    # s4 reaches in00 in1  in2  in3  in4  in55
    # s3 reaches in00 in1  in2  in33 in4  in5
    # s2 reaches in0  in1  in2  in33 in44 in5
    # s1 reaches in0  in11 in2  in3  in44 in5
    # s0 reaches in0  in11 in22 in3  in4  in5
    #
    # verify w/o "!" connections (6 signals across 3 input pins each):
    # s5 reaches in0  in1  in2  in3       in55
    # s4 reaches in00      in2  in3  in4  in5 
    # s3 reaches in0  in1       in33 in4  in5 
    # s2 reaches in0  in1  in2  in3  in44     
    # s1 reaches      in11 in2  in3  in4  in5 
    # s0 reaches in0  in1  in22      in4  in5 
    #
    # Those 6 signals are even&odd of 1E, 4M, 4E
    # even&odd since 16 wires/len+dir but only 8 LUTs

    if g_false: # was "not iotile"
        # CB for CLB and non-IOTile other (BRAM + DSP)
        others = [[1, 'E', 0], [4, 'M', 1], [4, 'E', 2], ]
        # CLB effect #1: span smaller since inputs permutable
        span = 3 if clb else 4
        for f in range(8):
            for edi in 'ENWS':
                for o in range(2):
                    eb = f * 2 + o
                    for eln, pt, w in others:
                        start = w * 2 + o
                        for n in range(span):
                            # broaden this? CHECK
                            if g_false:
                                # old: rotation within FLE
                                f2 = f
                                nn = (n + start) % 6
                            else:
                                # new: rotation within CLB
                                nn = (f * 6 + start + n) % 48
                                f2 = nn // 6
                                nn %= 6
                            end = f"{edi}{eln}{pt}{eb}"
                            beg = f"I{f2}{nn}"
                            beg2ends[beg].add(end)
                        # for n
                    # for eln, pt, w
                # for o
            # for edi
        # for f
    elif not iotile:

        for i in range(6):
            for f in range(8):
                n = f * 6 + i

                beg = f"I{f}{i}"
                edi = "ENWS"[(n // 2) % 4]

                # length 1
                start = 0; pt = "E"
                if n & 1: start += 2
                if n & 8: start += 1
                if 16 <= n < 32: start ^= 2
                for b in range(start, 16, 4):
                    end = f"{edi}1{pt}{b}"
                    beg2ends[beg].add(end)
                # for b

                # length 4
                start = 0; pt = "E"
                if n & 1: start += 1
                if not n & 8: pt = "M"
                if 16 <= n < 32: start ^= 1
                for b in range(start, 16, 2):
                    end = f"{edi}4{pt}{b}"
                    beg2ends[beg].add(end)
                # for b
            # for f
        # for i

    else:
        # CB is completely different for IOTile
        for f in range(8):
            for nn in range(6):
                beg = f"I{f}{nn}"
                for b in range(16):
                    # skip if LSBs unequal? CHECK
                    # requiring 1 bit  to match --> 64:1 CBs <NOW>
                    # requiring 2 bits to match --> 32:1 CBs
                    if (nn & 1) != (b & 1): continue
                    for e in ['1E', '4Q', '4M', '4P', ]:
                        for en in enters:
                            end = f"{en}{e}{b}"
                            beg2ends[beg].add(end)
                        # for en
                    # for e
                # for b
            # for nn
        # for f

    # Step 4 for CLB: outputs to inputs 0=reg0 1=reg1 2=O6
    #
    # Options:
    #    a b ccccc
    # %5 2 2   1 2
    # %4 2 1   1 2
    # %3 1 0 0 1  
    # %2 1 2 0 1  
    # %1 0 1 0   2
    # %0 0 0 0   2
    # option b means 0 can't connect to 4 or 5 (fastest) --> pick a
    #
    # verify:
    # 0 --> in0 in1 in2         in5
    # 1 -->     in1 in2 in3 in4
    # 2 --> in0         in3 in4 in5

    # CLB effect #2: add direct output-->input connections
    if clb:
        span = 2    # original
        span = 4    # full coverage
        for f in range(8):
            for o in range(3):
                start = o * 2
                for n in range(span):
                    # broadening this not all that important
                    nn = (n + start) % 6
                    end = f"O{f * 3 + o}"
                    beg = f"I{f}{nn}"
                    beg2ends[beg].add(end)
            # for o
        # for f

    # Step 5: SB to special inputs
    for bb in range(6):
        beg = f"IS{bb}"

        eln = 1
        pt = 'E'

        # stretch each IS span across more planes
        for ii, edi in enumerate('ENWS', start=0):
            for eb in range(3*bb-2, 3*bb+2+2, 2):
            #or eb in range(3*bb-1, 3*bb+1+1, 2):
                # spread around which planes are doubly-loaded
                # (very minor effect)
                if bb < 3:
                    eb += ii & 1
                else:
                    eb -= ii & 1
                eb = (eb + 2*2*8) % (2*8)
                # record
                end = f"{edi}{eln}{pt}{eb}"
                beg2ends[beg].add(end)
            # for eb
        # for edi
    # for bb

    # For IOTiles
    if iotile:
        # we remove any use of {leave}1E# or {leave}4E#
        for beg in beg2ends:
            for en in enters:
                le = g_enter2leave[en]
                kill = { end for end in beg2ends[beg] if end[0] == le and end[2] == 'E' }
                beg2ends[beg].difference_update(kill)
    if iotile and not g_safe:
        # we add drivers for L4s coming in
        for en in enters:
            le = g_enter2leave[en]
            for pt in 'PMQ':
                # the primary direction for P was already used to drive outputs
                if pt == 'P' and en == enter: continue
                # cover all 16 bits
                for b in range(16):
                    # we'll be driving this out
                    beg = f"{le}4{pt}{b}"
                    pt2master = { 'P': '4P', 'M': '1B', 'Q': '4P', }
                    # we want to copy the output set from this
                    mbeg = f"{leave}{pt2master[pt]}{b}"
                    # these are the outputs from the source master
                    # TODO: we should take advantage of copies to narrow <THAT>
                    ends = { end for end in beg2ends[mbeg] if end[0] == 'O' }
                    # add those outputs to the wire that would otherwise dangle
                    beg2ends[beg].update(ends)
        # remove nonsensical wire drivers
        kill = set()
        for beg in beg2ends:
            # enters == entering into IOTile from fabric
            if beg[2] != 'B': continue
            if beg[0] in enters and beg[1] == '4':
                kill.add(beg)
            if len(enters) == 2 and beg[0] == enters[1] and beg[1] == '1':
                kill.add(beg)
            # TODO: length=1 case needed since didn't generalize <THIS> over BOTH enters
        for beg in kill:
            del beg2ends[beg]

    ##
    ## check the pattern (IOTiles)
    ##
    if enters not in ('C', 'O'):
        # enter into IOTile from fabric
        emissing = set()
        bmissing = set()
        end2begs = defaultdict(set)
        for beg in beg2ends:
            for end in beg2ends[beg]:
                end2begs[end].add(beg)
            # for end
        # for beg
        for en in enters:
            # leave from IOTile into fabric
            le = g_enter2leave[en]
            for b in range(16):
                for tap in "BQMP":
                    beg = f"{le}4{tap}{b}"
                    if beg not in beg2ends:
                        bmissing.add(beg)
                # for tap
                for tap in "QMPE":
                    end = f"{en}4{tap}{b}"
                    if end not in end2begs:
                        emissing.add(end)
                # for tap
            # for b
        # for en
        if emissing:
            log(f"{enters}: missing ends: {' '.join(sorted(emissing))}\n")
        if bmissing:
            log(f"{enters}: missing begs: {' '.join(sorted(bmissing))}\n")
    # if

    #
    # write it out
    #

    # this is meant to be canonical and amenable to hash signatures
    order = { l: i for i, l in enumerate("IEWNSO", start=0) }
    for beg in sorted(beg2ends     , key=lambda k: [order[k[0]], dict_aup_nup(k)]):
        ends = sorted(beg2ends[beg], key=lambda k: [order[k[0]], dict_aup_nup(k)])
        ofile.write(f"{beg} {' '.join(ends)}\n")
    # for
    if not g_false: return
    # WE SKIP THE REST WHICH INCLUDES STATS TRASHING SHA1SUMs

    # figure out column widths
    # figure out loading, including midpoint taps
    colw = defaultdict(int)
    loads = defaultdict(int)
    for beg in beg2ends:
        colw2 = defaultdict(int)
        for end in beg2ends[beg]:
            g = re.match(r'(O|(?:[EWNS]\d+[EPMQ]))', end)
            assert g, f"end={end}"
            colw2[g.group(1)] += 1
            end = re.sub(r'(\d)[EM](\d)', r'\1B\2', end)
            loads[end] += 1
        for g in colw2:
            if g not in colw or colw2[g] > colw[g]: colw[g] = colw2[g]

    # calc histograms
    lhist = defaultdict(int)
    whist = defaultdict(int)
    for beg in beg2ends:
        if beg[0] not in 'EWNSI': continue
        nends = len(beg2ends[beg])
        w = f"{nends}W"
        whist[w] += 1
        l = f"{loads.get(beg, 0)}L"
        if beg[0] not in 'IO':
            lhist[l] += 1

    # write out mux lines
    gs = sorted(colw, key=dict_aup_ndn)
    ends = []
    for wh2 in 'EWNSO':
        for gg in gs:
            if gg[0] != wh2: continue
            ends += [f"{gg:<5s}"] * colw[gg]
    headt = f"# mux conns   {' '.join(ends)}\n"

    for which in 'EWNSI':
        ofile.write(headt)
        for beg in sorted(beg2ends, key=dict_aup_ndn):
            if beg[0] != which: continue
            alle = sorted(beg2ends[beg], key=dict_aup_ndn)
            # break into groups
            groups = {g : [] for g in colw}
            for end in alle:
                g = re.match(r'(O|(?:[EWNS]\d+[EPMQ]))', end)
                assert g and g.group(1) in colw, f"end={end}"
                groups[g.group(1)].append(end)
            gs = sorted(groups, key=dict_aup_ndn)
            ends = []
            for wh2 in 'EWNSO':
                for gg in gs:
                    if gg[0] != wh2: continue
                    #q = f"{colw[gg]}{gg}{len(groups[gg])}"
                    s = [""] * (colw[gg] - len(groups[gg]))
                    ends += groups[gg] + s
            w = f"{len(alle)}W"
            l = f"{loads.get(beg, 0)}L"
            tends = [f"{end:<5s}" for end in ends]
            ofile.write(f"{beg:<5s} {w:>3s} {l:>3s} {' '.join(tends)}\n")
        ofile.write('\f\n')

    # these are self-describing

    ofile.write("# block output loading\n")
    for beg in sorted(loads, key=dict_aup_ndn):
        if not beg.startswith('O'): continue
        l = f"{loads[beg]}L"
        #lhist[l] += 1
        ofile.write(f"# {beg}\t\t{l:>3s}\n")

    ofile.write("\n# mux width histogram\n")
    (wtot, ntot) = (0, 0)
    for w in sorted(whist, key=dict_aup_ndn):
        n = whist[w]
        ofile.write(f"# {w:>3s}\t{n:3d}\n")
        wtot += int(w[:-1]) * n
        ntot += n
    ofile.write(
f"# average width = {wtot/ntot:.2f} over {wtot} inputs on {ntot} SB/CB muxes\n")

    ofile.write("\n# signal load histogram\n")
    (ltot, ntot) = (0, 0)
    for l in sorted(lhist, key=dict_aup_ndn):
        n = lhist[l]
        ofile.write(f"# {l:>3s}\t{n:3d}\n")
        ltot += int(l[:-1]) * n
        ntot += n
    ofile.write(f"# average load = {ltot/ntot:.2f} over {ltot} loads on {ntot} SB muxes\n")

# def generate_grid

def toplogic():
    """ask for all the patterns"""
    patterns = [
        # enters file
        ('C',  'clb'),
        ('O',  'other'),
        ('E',  'right'),
        ('W',  'left'),
        ('N',  'top'),
        ('S',  'bottom'),
        ('EN', 'trcorner'),
        ('WN', 'tlcorner'),
        ('ES', 'brcorner'),
        ('WS', 'blcorner'),
    ]
    for (enters, base) in patterns:
        muxfile = f"{base}.mux"
        with open(muxfile, "w", encoding='ascii') as ofile:
            log(f"Writing {muxfile}\n")
            generate_grid(ofile, enters)
        # with
    # for
# def toplogic

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch
# vim: filetype=python
# vim: syntax=on
# vim: fileformat=unix
