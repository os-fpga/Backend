#!/nfs_home/dhow/bin/python3
"""compare information in Verilog to that in GSBs"""

import sys
import os 
import re 
#import subprocess
from collections import defaultdict

## pylint: disable-next=import-error,unused-import
#from traceback_with_variables import activate_by_import, default_format

#import json

## part of traceback_with_variables
#default_format.max_value_str_len = 10000

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

def dict_aup_nup(k):
    """sort with alpha ascending and numeric ascending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    return r
# def dict_aup_nup

# fastest way to scan large files: leading keys passed to re.match
# value starting with ">" means start of block of lines
g_leads = {
    'VPR was run with the following command-line:':
        '>vpr',
    'Circuit name: ':
        'ckt',
    'Resource usage...':
        '>resource',
    'BB estimate of min-dist (placement) wire length: ':
        'bb',
    'Placement estimated critical path delay (least slack): ':
        'fmax',
    'Placement estimated setup Total Negative Slack (sTNS): ':
        'tns',
    'Final critical path delay (least slack): ':
        'rfmax',
    'Final setup Total Negative Slack (sTNS): ':
        'rtns',
    'Successfully routed after ':
        'iter',
    'Router Stats: total_nets_routed: ':
        'rops',
    'Total number of overused nodes: ':
        'over',
    'VPR succeeded':
        'succeed',
    'The entire flow of VPR took ':
        'secs',
}
g_pattern = ""  # filled in below
# how to exit a capture block
g_cexit = {
    ">vpr": "\n",       # blank line
    ">resource": "\n",  # blank line
}
g_source = {}   # design --> source

def init_tables():
    """initialize data needed for speed"""
    global g_pattern

    # ensure tags unique (1:1)
    assert len(g_leads) == len(set(g_leads.values()))
    xlate = {
        ' ': r'\s',
        '.': r'[.]',
        '(': r'[(]',
        ')': r'[)]',
    }
    g_pattern = '|'.join([''.join([xlate.get(c, c) for c in lead]) for lead in g_leads])
    #log(f"g_pattern = {g_pattern}\n")
    captures = {c for c in g_leads.values() if c[0] == ">"}
    #log(f"captures={captures} g_cexit={set(g_cexit)}\n")
    assert captures == set(g_cexit)

    # note which designs are from which source
    # these are pulled from INSIDE the design
    # des --> des3
    # or1200 --> or1200_top
    for d in """
            alu4 apex2 apex4 bigkey clma des3 diffeq dsip elliptic ex1010 ex5p
            frisc misex3 pdc s298 s38417 s38584 seq spla tseng
            """.strip().split():
        g_source[d] = "MCNC"
    for d in """
            bgm blob_merge boundtop ch_intrinsics diffeq1 diffeq2 LU32PEEng
            LU8PEEng mcml mkDelayWorker32B mkPktMerge mkSMAdapter4B or1200_top raygentop
            sha stereovision0 stereovision1 stereovision2 stereovision3
            """.strip().split():
        g_source[d] = "VTR"
# def

def scanlog(extract, filename):
    """all work goes here"""

    # does file exist?
    if not os.path.isfile(filename):
        return

    # non-standard data
    runtype = ".missing."
    design = ".missing."
    success = False
    secs = 0
    # standard data
    data = {}

    with open(filename, encoding='ascii') as ifile:
        log(f"Reading {filename}\n")

        # @@@ THESE THREE LINES MUST BE FAST
        for line in ifile:
            g = re.match(g_pattern, line)
            if not g: continue

            # @@@ remaining code executed only on interesting lines

            tag = g_leads[g.group(0)]
            # check for multi-line "captures"
            if tag[0] == '>':
                # init anything we maintain across multiple lines
                netlist = False
                while 1:
                    # note we consume more lines here
                    line = next(ifile)
                    if line.startswith(g_cexit[tag]):
                        break
                    ff = line.strip().split()
                    # handle captures
                    match tag:
                        case ">vpr":
                            if "/vpr " in line:
                                runtypes = { "--pack", "--place", "--route", }
                                ff = [f for f in ff if f in runtypes]
                                runtype = ff[-1][2:]
                            continue
                        case ">resource":
                            if ff[0] == "Netlist":
                                netlist = True
                                continue
                            if ff[0] == "Architecture":
                                netlist = False
                                continue
                            if not netlist: continue
                            if ' '.join(ff[1:4]) != "blocks of type:": continue
                            if '_' in ff[4]: continue
                            if runtype != "pack": continue
                            #print(f"design {design} {runtype} {ff[4]} {ff[0]}")
                            extract[design][(runtype, ff[4])] = int(ff[0])
                            continue
                    # match
                    assert 0
                # while
                continue
            # if

            ff = line.strip().split()
            match tag:
                case 'ckt':
                    design = ff[2]
                    if design.endswith("_post_synth"):
                        design = design[:-len("_post_synth")]
                    continue
                case 'succeed':
                    success = True
                    continue
                case 'secs':
                    secs = float(ff[6])
                    continue
                #'BB estimate of min-dist (placement) wire length: ': 'bb',
                case 'bb':
                    data[tag] = ff[-1]
                    continue
                #'Placement estimated critical path delay (least slack): ':  'fmax',
                #'Final critical path delay (least slack): 3.67501 ns, Fmax: 272.108 MHz
                case 'fmax' | 'rfmax':
                    data[tag] = float(ff[-2])
                    continue
                #'Placement estimated setup Total Negative Slack (sTNS): ':  'tns',
                #'Final setup Total Negative Slack (sTNS): -5.94264 ns
                case 'tns' | 'rtns':
                    data[tag] = float(ff[-2])
                    continue
                # Successfully routed after 16 routing iterations.
                case 'iter':
                    data[tag] = ff[3]
                    continue
                #Router Stats: t_n_r: 2 t_c_r: 1 t_h_p: 2 t_h_p: 4 ...
                case 'rops':
                    data[tag] = int(ff[7]) + int(ff[9])
                    continue
                #'Total number of overused nodes: ':
                case 'over':
                    data[tag] = ff[-1]
                    continue
            # match
            assert 0
        # for

        for tag in sorted(data):
            #print(f"design {design} {runtype} {tag} {data[tag]}")
            extract[design][(runtype, tag)] = data[tag]
        if success:
            #print(f"design {design} {runtype} time {secs}")
            extract[design][(runtype, 'time')] = secs
        else:
            #print(f"design {design} {runtype} time 0")
            extract[design][(runtype, 'time')] = 0
    # with

# def

def toplogic():
    """run all scans"""
    init_tables()
    # collect the data
    extract = defaultdict(dict)
    if len(sys.argv) <= 1:
        # for debugging
        #or d in [ 'alu4', 'ex5p', 'bgm', ]:
        for d in [ 'apex2', 'tseng', ]:
            filename = f"../Testcases/{d}/route_stats/stamp_gsafe_false/packing_vpr_stdout.log"
            scanlog(extract, filename)
            filename = f"../Testcases/{d}/route_stats/stamp_gsafe_false/placement_vpr_stdout.log"
            scanlog(extract, filename)
            filename = f"../Testcases/{d}/route_stats/stamp_gsafe_false/routing_vpr_stdout.log"
            scanlog(extract, filename)
        # for d
    else:
        # intended use case
        for arg in sys.argv[1:]:
            scanlog(extract, arg)

    # what's the set of keys?
    keys = set()
    for d in extract:
        keys.update(extract[d])

    # dump out a header
    print(','.join(["design", "source"] + ['.'.join(k) for k in sorted(keys)]))

    # dump out the table by increasing packed CLB count, with pack failures last
    for d in sorted(extract, key=lambda d: extract[d].get(('pack', 'clb'), 999999)):
        print(f"{d},{g_source.get(d,'other')}", end='')
        for key in sorted(keys):
            if key in extract[d]:
                print(f",{extract[d][key]}", end='')
            else:
                print(",", end='')
        # for
        print("")
    # for

# def

toplogic()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: syntax=on fileformat=unix autoindent hlsearch
