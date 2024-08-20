#!/usr/bin/env python3
#!/nfs_home/dhow/bin/python3
#!/usr/bin/python3
"""read all files and then generate bitstream"""

import sys
import os
import logging
import re
import time
import math
import subprocess
from collections import defaultdict
import json
import itertools
import hashlib
import argparse

### pylint: disable-next=import-error,unused-import
##from traceback_with_variables import activate_by_import, default_format
##
### part of traceback_with_variables
##default_format.max_value_str_len = 10000

from arch_lib.vpr_dm import VprDataModel
from arch_lib.arch_dm import ArchDataModel
import uf
import pack_dm

### WARNING ### WARNING ### WARNING ###
###
### The comment "FIXME CONFIG" marks
### about two dozen places that should
### be made more configurable.
###
### WARNING ### WARNING ### WARNING ###

g_logger = logging.getLogger('main')

g_config = {}   # path --> value
g_routablepins = {}

g_false = False
g_true = True

g_proc_rotate_dump = False

g_xys2lutpath = {}  # (x, y, s) ==> path

g_commercial = 0
g_default_clock = "11111111111111111111111111111111"

# to what extent should we dump non-default mux settings?
g_mux_set_dump_sb = 1
g_mux_set_dump_cb = 2
g_mux_set_dump_lb = 4
g_mux_set_dump = 0

# design-driven mux settings written to this file
g_write_set_muxes = sys.stdout
g_frag2grouped = {}

g_black = ""
g_red = ""
g_purple = ""
g_green = ""

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

def gzopenread(path):
    """handle various input file conventions"""
    if not path or path == "-": return sys.stdin
    if not path.endswith(".gz"):
        return open(path, encoding='ascii')
    return subprocess.Popen(
        f"gunzip -c {path}",
        shell=True, stdout=subprocess.PIPE, universal_newlines=True
    ).stdout
# def gzopenread

def gzopenwrite(path):
    """handle various output file conventions"""
    if not path or path == "-": return sys.stdout
    if not path.endswith(".gz"):
        return open(path, "w", encoding='ascii')
    return subprocess.Popen(
        f"gzip > {path}",
        shell=True, stdin=subprocess.PIPE, universal_newlines=True
    ).stdin
# def gzopenwrite

def dict_aup_ndn(k):
    """sort with alpha ascending and numeric descending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] = -int(r[i])
    return r
# def dict_aup_ndn

def dict_aup_nup(k):
    """sort with alpha ascending and numeric ascending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    return r
# def dict_aup_nup

def dict_aup_nuprev(k):
    """sort with alpha ascending and numeric ascending & reversed"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    for i in range(1, len(r)//2, 2):
        (r[i], r[-1-i]) = (r[-1-i], r[i])
    return r
# def dict_aup_nuprev

g_time = 0

def elapsed():
    """return elapsed time"""
    global g_time
    thistime = time.time()
    if not g_time:
        g_time = thistime
        return "0:00:00"
    thistime -= g_time
    s = int(thistime % 60)
    thistime -= s
    m = int((thistime % 3600) / 60)
    thistime -= m * 60
    h = int(math.floor(thistime / 3600))
    return f"{h:01d}:{m:02d}:{s:02d}"
# def elapsed

def plural(n, notone="s", one=""):
    """return plural ending"""
    return one if n == 1 else notone
# def plural

## dump_vpr_xml.py

def showelt(elt):
    """standard format"""
    txt = ' '.join((elt.text or '').split())
    return f'{elt.elem.tag} {elt.attribs} "{txt}"'
# def showelt

def expand(word, info, implied=True):
    """expand expression"""

    g = re.match(r'(\w+)\[(\d+):(\d+)\]$', word)
    if g:
        (base, left, right) = g.groups()
        (left, right) = (int(left), int(right))
        if left <= right:
            for b in range(left, right + 1):
                val = f"{base}[{b}]"
                yield val
            return
        #for b in range(left, right - 1, -1):
        #    val = f"{base}[{b}]"
        #    #print(f"ABOUT TO YIELD {val}")
        #    yield val
        # WOW, this is how OpenFPGA reads it!!!
        for b in range(right, left + 1):
            val = f"{base}[{b}]"
            #print(f"ABOUT TO YIELD {val}")
            yield val
        return
    g = re.match(r'(\w+)\[(\d+)\]$', word)
    if g or not implied:
        yield word
        return
    (n, _) = info[word]
    for b in range(n):
        val = f"{word}[{b}]"
        yield val
    return
# def expand

g_muxinfo = defaultdict(list)

# all pb_types encountered in tree. traverse later.
g_pbtypes = []

# table of discovered LUTs
g_lutcirc2size = {} # name --> (inpsize, ramsize)

# table of discovered mode words
g_circ2port2modeinfo = {}   # circname --> port --> (size, "sram", "RS_LATCH")

# base --> list( (name, cmname, modebits) )
g_base2pathcircbits = defaultdict(list)

# name --> (cmname, modebits)
g_name2circbits = {}

# name --> capacity
g_capacity = {}

# (blk, port) --> width *IF* equivalent="full"
g_blkeqport2width = {}

# (portname, bit) --> netname
g_portb2net = {}

# (netname, x, y, blkeqport) --> list( (argument tuples to make_conn) )
g_save_for_reroute = defaultdict(list)
g_dump_ppfu = False

g_inps_synced = 0

# dotted_path --> num_pb
g_dotted2npb = {}

# logname --> (physpbname, modebits, idx_offset)
g_physmapping = {}

def path2text(path):
    """convert VPR.xml pb_type/mode path to dotted text path
       path elements do NOT include subscripts"""
    textpath = ""
    for p in path:
        if p.elem.tag == "mode":
            textpath += f"[{p.elem.attrib.get('name')}]"
            continue
        if p.elem.tag == "pb_type":
            if textpath: textpath += "."
            textpath += p.elem.attrib.get("name")
            continue
    return textpath
# def path2text

g_muxseen = defaultdict(set)        # FIXME fix generation so we don't need this filterning

# this shouldn't have pb_type indices (don't change anything)
g_inter_info = defaultdict(dict)    # pbpath -> conn name -> = [tag, {input: #}, width]
# inputs SHOULD have pb_type and port indices

g_dump_reps = False
g_node2pin = defaultdict(set)   # direct_input --> { direct_output }
g_out2inphash = {}              # mux_output --> { mux_input --> idx }
g_blockclock = set()            # set of (block, pin)

# called on "complete" "direct" "mux" and "pb_type"
def showmuxetc(path, obj, pb_info, businfo):
    """show physical muxes"""

    assert obj.elem.tag in ("direct", "mux", "complete", "pb_type"), f"tag = {obj.elem.tag}"

    # is there a physical or mode-free path to this?
    phys = False
    mode = False
    for p in path:
        if p.elem.tag != "mode": continue
        mode = True
        name = p.elem.attrib.get("name")
        disable_packing = p.elem.attrib.get("disable_packing")
        assert (name == "physical") == (disable_packing == "true")
        if name == "physical" and disable_packing == "true":
            phys = True
            break
    # for

    textpath = path2text(path)

    # remember for reading .net later
    if obj.elem.tag != "pb_type":

        # build list of inputs
        # compute width of all inputs
        inplist = []
        inptext = obj.elem.attrib.get("input")
        for inp in inptext.split():
            assert '.' in inp
            (pbs, ports) = inp.split('.')
            pb = re.sub(r'\[.*$', '', pbs)
            lpb = list(expand(pbs, pb_info, implied=False))
            lpt = list(expand(ports, businfo[pb]))
            for pb2 in lpb:
                for port2 in lpt:
                    inplist.append(f"{pb2}.{port2}")
                # for
            # for
        # for
        #print(f"EXPANDED\n\tinptext={inptext} ==>\n\tinplist={inplist}")
        w = len(inplist)
        b = int(math.ceil(math.log2(w)))
        # save info
        name = obj.elem.attrib.get("name")
        inphash = {}
        for i, inp in enumerate(inplist, start=0):
            inphash[inp] = i
        assert name not in g_inter_info[textpath]
        g_inter_info[textpath][name] = [obj.elem.tag, inphash, len(inplist)]
        #g_logger.info(f"INTER g_interhash_info['{textpath}']['{name}'] = {v}")
    # if

    # track pb_type's
    if obj.elem.tag == "pb_type":
        g_pbtypes.append([list(path), obj])
        np = int(obj.elem.attrib.get("num_pb") or 1)
        if np != 1:
            if g_dump_reps:
                print(f"{np} num_pb in pb_type {textpath}")
            g_dotted2npb[textpath] = np
        return
    # if

    # ignore direct
    #if obj.elem.tag == "direct":
    #    return

    # ignore things inside non-physical modes
    if mode and not phys:
        return

    # track mux/complete --> mux's
    # remember b, w describe INPUT width
    if obj.elem.tag in ("mux", "complete", "direct"):

        muxmode = obj.elem.tag != "direct"

        # step through all outputs
        outlist = []
        outtext = obj.elem.attrib.get("output")
        for outp in outtext.split():
            assert '.' in outp
            (pbs, ports) = outp.split('.')
            for pb in expand(pbs, pb_info, muxmode):
                pb0 = re.sub(r'\[.*$', '', pb)
                port0 = re.sub(r'\[.*$', '', ports)
                isclock = businfo[pb0][port0][1] == "clock"
                for port in expand(ports, businfo[pb0]):
                    pin = f"{textpath}:{pb}.{port}"
                    # FIXME CONFIG we are assuming tile <=> pb_type here
                    block = re.sub(r'\W.*$', '', pin)

                    outlist.append(f"{pb0}.{port}")
                    if muxmode:
                        # standard case 
                        # FIXME this filtering doesn't work?
                        if (pin, b, w) in g_muxseen[block]: continue
                        g_muxseen[block].add((pin, b, w))
                        g_muxinfo[block].append((pin, b, w))
                        if isclock:
                            g_blockclock.add((block, pin))
                            #log(f"\tMARKING block={block} pin={pin}\n")
                        #g_logger.info(f"MUX (vpr): {pin} <-- {b} selects for {w} inputs")

                # for port
            # for pb
        # for outp
        #print(f"EXPANDED\n\touttext={outtext} ==>\n\toutlist={outlist}")

        # track output wiring only at top level
        if '.' in textpath: return
        if not muxmode:
            # final processing on "direct" output tracking
            # must line up
            assert w == len(outlist)
            for inp, outp in zip(inplist, outlist):
                g_node2pin[inp].add(outp)
        else:
            for outp in outlist:
                assert outp not in g_out2inphash
                g_out2inphash[outp] = inphash

        return
    # if

    assert 0
# def showmuxetc

g_block2wiring = {}

# pininfo unused here
def pb_mode_inter(pbm, indent, path, pb_info, businfo, _):
    """visit interconnect"""

    # pb_info:  name    --> (num_pb, "pb_type")
    # businfo:  name    --> (num_pins, clock|input|output)
    # pininfo:  name[#] --> clock|input|output

    global g_node2pin, g_out2inphash
    g_node2pin.clear()
    g_out2inphash.clear()

    # a pb_type's or mode's interconnect
    for intc in pbm.interconnects:
        g_logger.info(f"{indent}Found {showelt(intc)}")

        indent += " "
        for complete in intc.completes:
            g_logger.info(f"{indent}Found {showelt(complete)}")
            showmuxetc(path, complete, pb_info, businfo)

        for direct in intc.directs:
            g_logger.info(f"{indent}Found {showelt(direct)}")
            showmuxetc(path, direct, pb_info, businfo)

        for mux in intc.muxs:
            g_logger.info(f"{indent}Found {showelt(mux)}")
            showmuxetc(path, mux, pb_info, businfo)

            indent += " "
            for dc in mux.delay_constants:
                g_logger.info(f"{indent}Found {showelt(dc)}")
            indent = indent[:-1]
        indent = indent[:-1]
    # for intc

    # save top-level wiring interpretation for after reading in other stuff
    if len(path) != 1: return
    block = path[0].elem.attrib.get("name")
    #block = re.sub(r'\W.*$', '', block)
    g_block2wiring[block] = [g_node2pin, g_out2inphash]
    # re-assign to avoid sharing/over-write
    g_node2pin = defaultdict(set)   
    g_out2inphash = {}              

# def pb_mode_inter

def fillinfo(businfo, pininfo, port, which):
    """fill in information about port"""
    name = port.elem.attrib.get("name")
    npin = int(port.elem.attrib.get("num_pins"))
    businfo[name] = (npin, which)
    for b in range(npin):
        pininfo[f"{name}[{b}]"] = which
    # for
# def fillinfo

g_dump_pb_mode_tree = False

def walk_pb_type(pb_type, depth, path, pb_info, businfo, pininfo):
    """ recursive routine to walk through pb_type """

    name = pb_type.elem.attrib.get("name")
    npb = int(pb_type.elem.attrib.get("num_pb") or 1)
    pb_info[name] = (npb, "pb_type")
    businfo[name] = {}
    pininfo[name] = {}

    # pb_info:  name    --> num_pb
    # businfo:  name    --> (num_pins, clock|input|output)
    # pininfo:  name[#] --> clock|input|output

    path.append(pb_type)
    indent = " " * depth
    if g_dump_pb_mode_tree:
        print(f"{indent}PB_TYPE {path2text(path)} #{npb}")

    g_logger.info(f"{indent}Found {showelt(pb_type)}")

    # visit (and record) this pb_type in the tree
    showmuxetc(path, pb_type, pb_info, businfo)

    # first dump local attributes
    indent += " "
    for clock in pb_type.clocks:
        g_logger.info(f"{indent}Found {showelt(clock)}")
        fillinfo(businfo[name], pininfo[name], clock, "clock")

    for inp in pb_type.inputs:
        g_logger.info(f"{indent}Found {showelt(inp)}")
        fillinfo(businfo[name], pininfo[name], inp, "input")

    for output in pb_type.outputs:
        g_logger.info(f"{indent}Found {showelt(output)}")
        fillinfo(businfo[name], pininfo[name], output, "output")

    for setup in pb_type.T_setups:
        g_logger.info(f"{indent}Found {showelt(setup)}")

    for hold in pb_type.T_holds:
        g_logger.info(f"{indent}Found {showelt(hold)}")

    for ctq in pb_type.T_clock_to_Qs:
        g_logger.info(f"{indent}Found {showelt(ctq)}")

    for dm in pb_type.delay_matrixs:
        g_logger.info(f"{indent}Found {showelt(dm)}")

    for dc in pb_type.delay_constants:
        g_logger.info(f"{indent}Found {showelt(dc)}")

    for mode in pb_type.modes:
        g_logger.info(f"{indent}Found {showelt(mode)}")
        path.append(mode)

        indent += " "
        if g_dump_pb_mode_tree:
            print(f"{indent}MODE {path2text(path)} #{0}")

        for pbt in mode.pb_types:
            walk_pb_type(pbt, len(indent), path, pb_info, businfo, pininfo)

        # a mode's interconnect
        pb_mode_inter(mode, indent, path, pb_info, businfo, pininfo)

        indent = indent[:-1]
        path.pop()
    # for mode

    for pbt in pb_type.pb_types:
        walk_pb_type(pbt, len(indent), path, pb_info, businfo, pininfo)

    # a pb_type's interconnect
    pb_mode_inter(pb_type, indent, path, pb_info, businfo, pininfo)

    # handle special pb_type's
    #if name in g_lutcirc2size:
    #    (inpsize, ramsize) = g_lutcirc2size[name]

    indent = indent[:-1]

    path.pop()
# def walk_pb_type

def read_vpr_xml(xmlfile):
    """read in vpr_xml and crawl the info"""
    indent = ""

    #
    # Process data
    #

    # read file specified
    log(f"{elapsed()} Reading {xmlfile}\n")
    g_vpr_xml = VprDataModel(xmlfile, validate=False)

    # walk through a typical architecture XML
    indent += " "
    for mods in g_vpr_xml.modelss:
        g_logger.info(f"{indent}Found {showelt(mods)}")
        indent += " "
        for mod in mods.models:

            g_logger.info(f"{indent}Found {showelt(mod)}")
            indent += " "
            for inp in mod.input_portss:
                g_logger.info(f"{indent}Found {showelt(inp)}")

                indent += " "
                for port in inp.ports:
                    g_logger.info(f"{indent}Found {showelt(port)}")
                indent = indent[:-1]

            for outp in mod.output_portss:
                g_logger.info(f"{indent}Found {showelt(outp)}")

                indent += " "
                for port in outp.ports:
                    g_logger.info(f"{indent}Found {showelt(port)}")
                indent = indent[:-1]
            indent = indent[:-1]
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for tiletop in g_vpr_xml.tiless:
        g_logger.info(f"{indent}Found {showelt(tiletop)}")

        indent += " "
        for tile in tiletop.tiles:
            g_logger.info(f"{indent}Found {showelt(tile)}")

            indent += " "
            for sub_tile in tile.sub_tiles:
                g_logger.info(f"{indent}Found {showelt(sub_tile)}")

                # this won't in general be true once we start using sub_tile
                (tt, st) = (tile.elem.attrib.get('name'), sub_tile.elem.attrib.get('name'))
                #print(f"tt={tt} st={st}")
                # FIXME CONFIG currently tile and sub_tile must have same names
                assert tt == st, f"tile '{tt}' != sub_tile '{st}'"

                g_capacity[tt] = c = int(sub_tile.elem.attrib.get("capacity") or 1)
                if g_dump_reps and c != 1:
                    print(f"{c} capacity in sub_tile {tt}")

                indent += " "
                for equivsite in sub_tile.equivalent_sitess:
                    g_logger.info(f"{indent}Found {showelt(equivsite)}")

                    indent += " "
                    for site in equivsite.sites:
                        g_logger.info(f"{indent}Found {showelt(site)}")
                    indent = indent[:-1]

                for clock in sub_tile.clocks:
                    g_logger.info(f"{indent}Found {showelt(clock)}")

                for inp in sub_tile.inputs:
                    g_logger.info(f"{indent}Found {showelt(inp)}")
                    if inp.attribs.get('equivalent', '') == "full":
                        inp_name = inp.attribs['name']
                        inp_num_pins = inp.attribs['num_pins']
                        g_blkeqport2width[(tt, inp_name)] = int(inp_num_pins)
                        if g_dump_ppfu:
                            print(
f"pb_pin_fixup1: tt={tt} inp_name={inp_name} inp_num_pins={inp_num_pins}")

                for output in sub_tile.outputs:
                    g_logger.info(f"{indent}Found {showelt(output)}")

                for fc in sub_tile.fcs:
                    g_logger.info(f"{indent}Found {showelt(fc)}")

                    indent += " "
                    for fc_override in fc.fc_overrides:
                        g_logger.info(f"{indent}Found {showelt(fc_override)}")
                    indent = indent[:-1]

                for pinloc in sub_tile.pinlocationss:
                    g_logger.info(f"{indent}Found {showelt(pinloc)}")

                    indent += " "
                    for loc in pinloc.locs:
                        g_logger.info(f"{indent}Found {showelt(loc)}")
                    indent = indent[:-1]
                indent = indent[:-1]
            indent = indent[:-1]
            # finish sub-tile
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for dev in g_vpr_xml.devices:
        g_logger.info(f"{indent}Found {showelt(dev)}")

        indent += " "
        for sizing in dev.sizings:
            g_logger.info(f"{indent}Found {showelt(sizing)}")

        for area in dev.areas:
            g_logger.info(f"{indent}Found {showelt(area)}")

        for cwd in dev.chan_width_distrs:
            g_logger.info(f"{indent}Found {showelt(cwd)}")

            indent += " "
            for x in cwd.xs:
                g_logger.info(f"{indent}Found {showelt(x)}")

            for y in cwd.ys:
                g_logger.info(f"{indent}Found {showelt(y)}")
            indent = indent[:-1]

        for sb in dev.switch_blocks:
            g_logger.info(f"{indent}Found {showelt(sb)}")

        for cb in dev.connection_blocks:
            g_logger.info(f"{indent}Found {showelt(cb)}")
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for swlist in g_vpr_xml.switchlists:
        g_logger.info(f"{indent}Found {showelt(swlist)}")

        indent += " "
        for switch in swlist.switchs:
            g_logger.info(f"{indent}Found {showelt(switch)}")
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for seglist in g_vpr_xml.segmentlists:
        g_logger.info(f"{indent}Found {showelt(seglist)}")

        indent += " "
        for segment in seglist.segments:
            g_logger.info(f"{indent}Found {showelt(segment)}")

            indent += " "
            for mux in segment.muxs:
                g_logger.info(f"{indent}Found {showelt(mux)}")

            for sb in segment.sbs:
                g_logger.info(f"{indent}Found {showelt(sb)}")
            indent = indent[:-1]
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for dirlist in g_vpr_xml.directlists:
        g_logger.info(f"{indent}Found {showelt(dirlist)}")

        indent += " "
        for direct in dirlist.directs:
            g_logger.info(f"{indent}Found {showelt(direct)}")
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for cblist in g_vpr_xml.complexblocklists:
        g_logger.info(f"{indent}Found {showelt(cblist)}")

        indent += " "
        for pbt in cblist.pb_types:
            path = []
            pb_info = {}
            businfo = {}
            pininfo = {}
            walk_pb_type(pbt, len(indent), path, pb_info, businfo, pininfo)
            assert not path
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for layout in g_vpr_xml.layouts:
        g_logger.info(f"{indent}Found {showelt(layout)}")

        indent += " "
        for fl in layout.fixed_layouts:
            g_logger.info(f"{indent}Found {showelt(fl)}")

            indent += " "
            for row in fl.rows:
                g_logger.info(f"{indent}Found {showelt(row)}")

            for col in fl.cols:
                g_logger.info(f"{indent}Found {showelt(col)}")

            for corner in fl.cornerss:
                g_logger.info(f"{indent}Found {showelt(corner)}")

            for fill in fl.fills:
                g_logger.info(f"{indent}Found {showelt(fill)}")

            for region in fl.regions:
                g_logger.info(f"{indent}Found {showelt(region)}")
            indent = indent[:-1]
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent
# def read_vpr_xml

# table of discovered LUTs
g_lutcirc2size = {} # name --> (inpsize, ramsize)
# table of discovered mode words
# circname --> port --> (size, "sram", "RS_LATCH")
g_circ2port2modeinfo = defaultdict(dict)
g_circmod_is_clock = {}
g_pbinter2cm = defaultdict(dict)

g_dump_anno = False

## dump_arch_xml.py

def read_open_xml(xmlfile):
    """read in openfpga_xml and crawl the info"""
    indent = ""

    # read file specified
    log(f"{elapsed()} Reading {xmlfile}\n")
    g_arch_xml = ArchDataModel(xmlfile, validate=False)

    # walk through a typical architecture XML
    indent += " "
    for techlib in g_arch_xml.technology_librarys:
        g_logger.info(f"{indent}Found {showelt(techlib)}")

        indent += " "
        for devlib in techlib.device_librarys:
            g_logger.info(f"{indent}Found {showelt(devlib)}")

            indent += " "
            for devmod in devlib.device_models:
                g_logger.info(f"{indent}Found {showelt(devmod)}")

                indent += " "
                for lib in devmod.libs:
                    g_logger.info(f"{indent}Found {showelt(lib)}")

                for design in devmod.designs:
                    g_logger.info(f"{indent}Found {showelt(design)}")

                for xtr in devmod.pmoss:
                    g_logger.info(f"{indent}Found {showelt(xtr)}")

                for xtr in devmod.nmoss:
                    g_logger.info(f"{indent}Found {showelt(xtr)}")
                indent = indent[:-1]
            indent = indent[:-1]
        indent = indent[:-1]

        indent += " "
        for varlib in techlib.variation_librarys:
            g_logger.info(f"{indent}Found {showelt(varlib)}")

            indent += " "
            for variation in varlib.variations:
                g_logger.info(f"{indent}Found {showelt(variation)}")
            indent = indent[:-1]
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for circlib in g_arch_xml.circuit_librarys:
        g_logger.info(f"{indent}Found {showelt(circlib)}")

        indent += " "
        for circmod in circlib.circuit_models:
            g_logger.info(f"{indent}Found {showelt(circmod)}")
            circname = circmod.elem.attrib.get("name")
            typp = circmod.elem.attrib.get("type")
            inpsize = 0
            ramsize = 0

            if typp == "mux" and "clock" in circname:
                if  circmod.elem.attrib.get("is_default") == "false" and \
                    "clock" in circmod.elem.attrib.get("prefix"):
                    g_circmod_is_clock[circname] = True

            indent += " "
            for destech in circmod.design_technologys:
                g_logger.info(f"{indent}Found {showelt(destech)}")

            for port in circmod.ports:
                g_logger.info(f"{indent}Found {showelt(port)}")
                # look for mode "ports"
                if port.elem.attrib.get("mode_select") == "true":
                    upname = circmod.elem.attrib.get("name")
                    porttype = port.elem.attrib.get("type")
                    portprefix = port.elem.attrib.get("prefix")
                    portsize = port.elem.attrib.get("size")
                    portcmname = port.elem.attrib.get("circuit_model_name")
                    g_circ2port2modeinfo[upname][portprefix] = (
                        int(portsize), porttype, portcmname)
                    #if porttype == "sram" and portprefix == "mode"
                size = int(port.elem.attrib.get("size"))
                if port.elem.attrib.get("type") == "input":
                    inpsize = size
                if port.elem.attrib.get("type") == "sram":
                    ramsize = size

            for delaymatrix in circmod.delay_matrixs:
                g_logger.info(f"{indent}Found {showelt(delaymatrix)}")

            for inbuf in circmod.input_buffers:
                g_logger.info(f"{indent}Found {showelt(inbuf)}")

            for outbuf in circmod.output_buffers:
                g_logger.info(f"{indent}Found {showelt(outbuf)}")

            for pglogic in circmod.pass_gate_logics:
                g_logger.info(f"{indent}Found {showelt(pglogic)}")

            # do we have enough for this to be a LUT?
            if typp == "lut" and inpsize and ramsize:
                assert 1 << inpsize == ramsize
                g_lutcirc2size[circname] = (inpsize, ramsize)
                #g_logger.info(
                #f"FOUND LUT '{circname}' : inputsize={inpsize} ramsize={ramsize}")

            indent = indent[:-1]
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for config_proto in g_arch_xml.configuration_protocols:
        g_logger.info(f"{indent}Found {showelt(config_proto)}")

        indent += " "
        for org in config_proto.organizations:
            g_logger.info(f"{indent}Found {showelt(org)}")

            indent += " "
            for bl in org.bls:
                g_logger.info(f"{indent}Found {showelt(bl)}")

            for bl in org.wls:
                g_logger.info(f"{indent}Found {showelt(bl)}")
            indent = indent[:-1]
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for conblock in g_arch_xml.connection_blocks:
        g_logger.info(f"{indent}Found {showelt(conblock)}")

        indent += " "
        for switch in conblock.switchs:
            g_logger.info(f"{indent}Found {showelt(switch)}")
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for swblock in g_arch_xml.switch_blocks:
        g_logger.info(f"{indent}Found {showelt(swblock)}")

        indent += " "
        for switch in swblock.switchs:
            g_logger.info(f"{indent}Found {showelt(switch)}")
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for rseg in g_arch_xml.routing_segments:
        g_logger.info(f"{indent}Found {showelt(rseg)}")

        indent += " "
        for seg in rseg.segments:
            g_logger.info(f"{indent}Found {showelt(seg)}")
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for dcon in g_arch_xml.direct_connections:
        g_logger.info(f"{indent}Found {showelt(dcon)}")

        indent += " "
        for direct in dcon.directs:
            g_logger.info(f"{indent}Found {showelt(direct)}")
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    for tileann in g_arch_xml.tile_annotationss:
        g_logger.info(f"{indent}Found {showelt(tileann)}")

        indent += " "
        for globport in tileann.global_ports:
            g_logger.info(f"{indent}Found {showelt(globport)}")

            indent += " "
            for tile in globport.tiles:
                g_logger.info(f"{indent}Found {showelt(tile)}")
            indent = indent[:-1]
        indent = indent[:-1]
    indent = indent[:-1]
    assert not indent

    indent += " "
    words = 0
    pb_type_keys = {
        "name",
        "physical_pb_type_name",
        "circuit_model_name",
        "physical_mode_name",
        "physical_pb_type_index_factor",
        "physical_pb_type_index_offset",
        "mode_bits",
    }
    if g_dump_anno: print("")
    for pta in g_arch_xml.pb_type_annotationss:
        g_logger.info(f"{indent}Found {showelt(pta)}")

        indent += " "
        for pbt in pta.pb_types:
            g_logger.info(f"{indent}Found {showelt(pbt)}")

            # check for unsupported keys
            attrib = pbt.elem.attrib
            for attr in attrib:
                assert attr in pb_type_keys, f"attr={attr}"
            # get all supported keys
            logname = attrib.get("name")
            # always required
            assert logname is not None

            # can have none of these, or one of these. multiples not allowed
            physpbname = attrib.get("physical_pb_type_name")
            cmname = attrib.get("circuit_model_name")
            physmodename = attrib.get("physical_mode_name")
            assert 1 >= sum(
                1 if x is not None else 0 for x in (physpbname, cmname, physmodename))

            idx_factor = attrib.get("physical_pb_type_index_factor")
            idx_offset = attrib.get("physical_pb_type_index_offset")
            # only legal for physical_pb_type_name. not required
            assert idx_factor is None and idx_offset is None or physpbname is not None

            # the purpose of physical_pb_type_index_offset appears to be:
            # provide missing index to physical mode when logical mode has fewer indices

            modebits = attrib.get("mode_bits")
            # only legal for physical_b_type_name or circuit_model_name
            # not required in either case
            assert modebits is None or physpbname is not None or cmname is not None

            # also set mode bits from logical --> physical mapping
            if   physpbname:
                g_physmapping[logname] = (physpbname, modebits, idx_offset)
                if g_dump_anno:
                    print(f"pb_type {logname} physical ==> {physpbname} ({modebits})")
            elif cmname and cmname in g_circ2port2modeinfo and modebits not in (None, ""):
                # FIXME CONFIG next line seems to make assumptions
                base = re.sub(r'\W.*$', '', logname)
                g_base2pathcircbits[base].append((logname, cmname, modebits))
                g_name2circbits[logname] = (cmname, modebits)
                words += 1
                if g_dump_anno:
                    print(f"pb_type {logname} circuit_model ==> {cmname} ({modebits})")
            elif physmodename:
                assert physmodename == "physical", \
                    f"You must change the physical_mode_name for {logname} to 'physical'"
                if g_dump_anno:
                    print(f"pb_type {logname} physical_mode_name ==> {physmodename}")
            else:
                # just declare a pb_type so we can attach port or conn info to it below
                if g_dump_anno:
                    print(f"pb_type {logname}")

            indent += " "
            for interconn in pbt.interconnects:
                g_logger.info(f"{indent}Found {showelt(interconn)}")
                # check we have required attributes
                attrib = interconn.elem.attrib
                for attr in attrib:
                    assert attr in ("name", "circuit_model_name")
                assert len(attrib) == 2
                intername = attrib.get('name')
                circname = attrib.get('circuit_model_name')
                g_pbinter2cm[logname][intername] = circname
                if g_dump_anno:
                    print(f"  name={intername} circuit_model_name={circname}")

            for port in pbt.ports:
                g_logger.info(f"{indent}Found {showelt(port)}")
                # check we have required attributes
                attrib = port.elem.attrib
                for attr in attrib:
                    assert attr in (    "name",
                                        "physical_mode_port",
                                        "physical_mode_pin_rotate_offset"), \
                            f"attr={attr} attrib={attrib}"
                assert len(attrib) in (2, 3)
                assert "name" in attrib
                assert "physical_mode_port" in attrib
                rot = attrib.get('physical_mode_pin_rotate_offset')
                if rot is not None and g_dump_anno:
                    print(
f"  name={attrib.get('name')} " +
f"physical_mode_port={attrib.get('physical_mode_port')} " +
f"rotate={rot}")
            indent = indent[:-1]

        indent = indent[:-1]
    if g_dump_anno: print("")
    indent = indent[:-1]
    assert not indent

    log(f" - {words} mode word types and default values declared\n")
# def read_open_xml

# put this in an external file later
# NB: LHS strings are decimal mux input indices
# NB: RHS strings are binary, encoded mux selects, LSB-left-to-MSB-right
# both of these should be changed to signal->interconnect form seen in packer output
g_anno_file = '''

# for a logical mode:
#   SET physical mux settings (first field lacks ":", odd # fields)
# for a logical mux:
#   MAP logical mux setting to physical mux setting (first field has ":", even # fields)

# fast_lut6: set
clb.clb_lr.fle[fast_lut6]                               \
    clb.clb_lr.fle[physical].fabric.ff_bypass[0]:D[0]   1 \
    clb.clb_lr.fle[physical].fabric.ff_bypass[1]:D[0]   1

# n1_lut6: set
clb.clb_lr.fle[n1_lut6]                                 \
    clb.clb_lr.fle[physical].fabric.ff_bypass[0]:Q[0]   1 \
    clb.clb_lr.fle[physical].fabric.ff_bypass[1]:Q[0]   0
clb.clb_lr.fle[n1_lut6]                                 \
    clb.clb_lr.fle[physical].fabric.ff_bypass[0]:D[0]   1 \
    clb.clb_lr.fle[physical].fabric.ff_bypass[1]:D[0]   1

# arithmetic: set then map
clb.clb_lr.fle[arithmetic]                              \
    clb.clb_lr.fle[physical].fabric.ff_bypass[0]:D[0]   1 \
    clb.clb_lr.fle[physical].fabric.ff_bypass[1]:D[0]   1

clb.clb_lr.fle[arithmetic].adder:out[0]                 0 1 \
clb.clb_lr.fle[physical].fabric.ff_bypass[0]:Q[0]       0 1

# n2_lut5: set then map
clb.clb_lr.fle[n2_lut5]                                 \
    clb.clb_lr.fle[physical].fabric.ff_bypass[0]:D[0]   1 \
    clb.clb_lr.fle[physical].fabric.ff_bypass[1]:D[0]   1

clb.clb_lr.fle[n2_lut5].ble5.ff[DFFRE].DFFRE            \
    clb.clb_lr.fle[physical].fabric.ff_bypass:D[0]      0

clb.clb_lr.fle[n2_lut5].ble5.ff[DFFNRE].DFFNRE          \
    clb.clb_lr.fle[physical].fabric.ff_bypass:D[0]      0

clb.clb_lr.fle[n2_lut5].ble5:out[0]                     0 1 \
clb.clb_lr.fle[physical].fabric.ff_bypass:Q[0]          1 0

clb.clb_lr.fle[n2_lut5].ble5:out[0]                     0 1 \
clb.clb_lr.fle[physical].fabric.ff_bypass:D[0]          0 1

# IO also has mapped muxes
io[io_input].io_input:a2f_o[0]                          0 1 \
io[physical].iopad:a2f_o[0]                             1 0

io[io_output].io_output.outpad:outpad[0]                0 1 \
io[physical].iopad.pad:outpad[0]                        1 0

'''

g_anno_mappings = defaultdict(set)
g_anno_settings = defaultdict(set)

g_read_anno_dump = False

def read_anno():
    """populate linkages between logical and physical modes"""
    # OpenFPGA deduces these over and over, but we just specify them once.

    # read through lines in specification
    specs = 0
    mappings = 0
    settings = 0
    # collapse line continuations
    contents = re.sub(r'\\\s*\n', r' ', g_anno_file)
    for line in re.findall(r'.*\n', contents):
        # filter good lines
        line = re.sub(r'\s+', r' ', line)   # add .strip()
        if line[0] == '#': continue
        ff = line.split()
        if not ff: continue
        specs += 1

        # convert to standard format in "six"
        six = []
        if ':' in ff[0]:
            assert not 1 & len(ff)
            m = len(ff) >> 1
            (logpath, logport) = ff[0].split(':')
            (phypath, phyport) = ff[m].split(':')
            for i, f in enumerate(ff[1:m], start=1):
                p =  ff[i+m]
                assert f.isdecimal() and p.isdecimal()
                six.append((logpath, logport, f, phypath, phyport, p))
            # for
        else:
            assert     1 & len(ff)
            (logpath, logport) = (ff[0], "-")
            for f, ix in zip(ff[1::2], ff[2::2]):
                assert ix.isdecimal()
                (phypath, phyport) = f.split(':')
                six.append((logpath, logport, "-1", phypath, phyport, ix))
            # for

        # expand and record
        if g_read_anno_dump:
            print(f"Processing line {line[:-1]}")
        for logpath, logport, log_idx, phypath, phyport, phy_idx in six:

            (logbase, _) = re.split(r'[\[._]', logpath, 1)
            (phybase, _) = re.split(r'[\[._]', phypath, 1)
            assert logbase == phybase, f"logbase={logbase} phybase={phybase}"
            ( logical_reps,  logical_curr) = replicas(logbase, logpath, 0)
            (physical_reps, physical_curr) = replicas(phybase, phypath, 0, keep_idx=True)
            assert len(logical_reps) == len(physical_reps)

            # nasty trick we should fix
            logical_reps  = [(mx, ix-2, pp) for (mx, ix, pp) in  logical_reps]
            physical_reps = [(mx, ix-2, pp) for (mx, ix, pp) in physical_reps]

            if g_read_anno_dump:
                print(f"logical {logpath} {logport} {log_idx}")
                print(f"physical {phypath} {phyport} {phy_idx}")
                print(f"logical_reps={logical_reps} logical_curr={logical_curr}")
                print(f"physical_reps={physical_reps} physical_curr={physical_curr}")

            while 1:
                logpath0 = pb_add_subs(logpath,  logical_reps,  logical_curr)
                phypath0 = pb_add_subs(phypath, physical_reps, physical_curr)

                if g_read_anno_dump:
                    print(
f"logpath {logpath0} {logport} {log_idx} ==> phypath {phypath0} {phyport} {phy_idx}")
                # RECORD IT HERE: log_idx < 0 ==> mode selecting settings, else map settings
                if log_idx == "-1":
                    assert logport == "-"
                    key = logpath0
                    setting = (phypath0, phyport, phy_idx)
                    if key in g_anno_settings and setting in g_anno_settings[key]:
                        print(f"DUPLICATE setting key={key}")
                    else:
                        settings += 1
                        g_anno_settings[key].add(setting)
                else:
                    key = f"{logpath0}:{logport}:{log_idx}"
                    mapping = (phypath0, phyport, phy_idx)
                    if key in g_anno_mappings and mapping in g_anno_mappings[key]:
                        print(f"DUPLICATE mapping key={key}")
                    else:
                        mappings += 1
                        g_anno_mappings[key].add(mapping)

                logmore = nextrep( logical_curr,  logical_reps)
                phymore = nextrep(physical_curr, physical_reps)
                assert logmore == phymore
                if not logmore: break
            # while 1
        # for logpath...
    # for line
    log(
f" - {settings:,d} pb_type mux settings and {mappings:,d} pb_type mux mappings " +
f"from {specs:,d} side annotations\n")
# def read_anno

### START PYX PARSER

def unescape(s):
    """handle pyx escapes"""
    return s.replace(r'\t','\t').replace(r'\\','\\')
# def unescape

def pyx_nop(*_):
    """ignore ?"""
    #pass
# def pyx_nop

def pyx_open(stack, line):
    """handle ("""
    addme = [ line[1:-1], {} ]
    stack[-1].append(addme)
    stack.append(addme)
# def pyx_open

def pyx_attr_buggy(stack, line):
    """handle A"""
    n, v = line[1:].split(None, 1)
    v = unescape(v)[:-1]
    if re.match(r'\d+$', v):
        v = int(v)
    elif re.match(r'\d*[.]?\d*(e-?\d+)?$', v) and re.match(r'[.]?\d', v):
        v = float(v)
    stack[-1][1][n] = v
# def pyx_attr_buggy

def pyx_attr(stack, line):
    """handle A"""
    # must handle 'Aname ' properly (name="" in XML)
    n, v = line[1:-1].split(" ", 1)
    v = unescape(v)
    # NB: these conversionms can be dangerous, if
    # e.g. we want to retain the width of "0000" and not reduce it to 0.
    if re.match(r'-?\d+$', v):
        v = int(v)
    elif re.match(r'-?\d*[.]?\d*(e-?\d+)?$', v) and re.match(r'-?[.]?\d', v):
        v = float(v)
    # save result
    stack[-1][1][n] = v
# def pyx_attr

def pyx_text(stack, line):
    """handle -"""
    stack[-1].append(os.linesep if line[:3] == r'-\n' else unescape(line[1:-1]))
# def pyx_text

def pyx_close(stack, line):
    """handle )"""
    assert line[1:-1] == stack[-1][0]
    stack.pop()
# def pyx_close

g_pyx_jump = {
    '?': pyx_nop,
    '(': pyx_open,
    'A': pyx_attr,
    '-': pyx_text,
    ')': pyx_close,
}

def pyxparse(xfile, stack):
    """parse PYX file"""
    # parse the PYX file and return routing graph
    lno = 0
    for line in xfile:
        g_pyx_jump[line[0]](stack, line)
        lno += 1
        if not lno % 100000: log(f"  {lno:,d}\r")
    log(f" - {lno:,d} pyx lines\n")
    return stack[0][2]
# def pyxparse

### END PYX PARSER

g_chcnt             = 0
g_swid2swname       = {}    # swid ==> swname
g_sgid2sgname       = {}    # sgid ==> sgname
g_sgid2ptcs         = {}    # id ==> [min_ptc, max_ptc]
g_ptc2sgidname      = {}    # ptc ==> (sgid, sgidname)
# g_bid2info[bid]['pid2info'][ptc] = [text, pin_class_type]
# g_bid2info[bid][  'n2info'][text] = [ptc, pin_class_type]
# bid is a block id int
# text is clb.I00[0] or io_right.<something>
# ptc is an int
# pin_class_type is "INPUT" or "OUTPUT"
g_bid2info          = {}    # id ==> { name: , width: , height: , pid2info: {}, n2info: {} }
g_block2bid         = {}    # name ==> id
# translate x,y (int) to bid (int), wo, and ho
g_xy2bidwh          = {}    # (x, y) ==> (bid, wo, ho)

g_nid2info          = {}    # nid ==> [xl, yl, xh, yh, p, pin/chan, sgid]
# sgid: pin:side src/snk:-1 chan:sgid
# ONLY for non-CHAN*
g_xyt2nid           = {}    # (xl, yl, ptc) ==> nid
# ONLY for CHAN*
g_xydl2tn           = defaultdict(list)   # (E4, x1, y1, x2, y2) ==> [(ptc, id), ...]
g_edges             = []

g_sgid2len          = {}    # sgid ==> length
g_len2sgid          = {}    # len# ==> sg id
g_inp2swid          = 0     # sw id for CB mux
g_swid2len          = {}
g_len2swid          = {}

g_clb_id            = None  # id for CLB
g_grid_bbox         = []
g_add_layer = True

# these are rewrites of methods in minidom

# was getElementsByTagName
def get_elements_by_tag_name(node, tag, dep):
    """yield nodes whose tag is 'tag'"""
    if not isinstance(node, str):
        if node[0] == tag:
            yield node
        if dep > 0:
            for n in node[2:]:
                yield from get_elements_by_tag_name(n, tag, dep - 1)
# def get_elements_by_tag_name

# was childNodes
def child_nodes(node):
    """yield children"""
    for n in node[2:]:
        yield n
# def child_nodes

# was isText (actually isinstance(x, minidom.Text))
def is_text(node):
    """check if node is text"""
    return isinstance(node, str)
# def is_text

# was localName
def local_name(node):
    """return tag of node"""
    return node[0]
# def local_name

def attr2hash(node):
    """return attribute hash table from node"""
    return node[1]
# def attr2hash

def nodexy2ptc(src, x, y):
    """given a node 'src', return ptc it would have at (x, y)"""
    # xh, xy, sgid unused
    (xl, yl, _, _, ptc, ty, _) = g_nid2info[src] 
    if ty[:4] != "CHAN": return ptc
    # NB may want to adjust 0 coordinate if g_commercial some day
    # not needed as long as everybody does the same thing
    match ty:
        case "CHANE":
            return ptc + 2 * (x - xl)
        case "CHANN":
            return ptc + 2 * (y - yl)
        case "CHANW":
            return ptc - 2 * (x - xl)
        case "CHANS":
            return ptc - 2 * (y - yl)
    assert 0
# def nodexy2ptc

g_idx2sel_memo = defaultdict(dict) 

# this handles the way muxes are designed by OpenFPGA
def idx2sel(sz, idx):
    """ we want input in[idx] : return mem value to select it"""

    if sz not in g_idx2sel_memo:
        power2 = sz
        pairs = 0
        while power2 & (power2 - 1):
            power2 -= 1
            pairs += 1
        assert not power2 & (power2 - 1)
        assert sz == pairs + power2
        if pairs:
            for p in range(pairs):
                g_idx2sel_memo[sz][p * 2 + 0] = (power2 - 1 - p) * 2 + 1
                g_idx2sel_memo[sz][p * 2 + 1] = (power2 - 1 - p) * 2
            # for
            for ii in range(pairs * 2, sz):
                g_idx2sel_memo[sz][ii] = (sz - 1 - ii) * 2 + 0
            # for
        else:
            for ii in range(sz):
                g_idx2sel_memo[sz][ii] = sz - 1 - ii
            # for
        #print(f"mux size {sz} selectors = {g_idx2sel_memo[sz]}")
        
    assert idx in g_idx2sel_memo[sz], f"sz={sz} idx={idx}"
    return g_idx2sel_memo[sz][idx]
# def idx2sel

def sort_sb_inputs(s, sx, sy):
    """sort function for inputs to SB muxes"""
    #f1 = True ; tf3 = False; tf5 = False; tf7 = True   #1 BAD (but less so)
    #f1 = False; tf3 = True ; tf5 = True ; tf7 = False  #2 BAD
    #f1 = True ; tf3 = False; tf5 = True ; tf7 = False  #3 not possible
    #f1 = False; tf3 = True ; tf5 = False; tf7 = True   #4 not possible
    #f1 = True ; tf3 = True ; tf5 = False; tf7 = False  #5 BAD
    tf1 = False; tf3 = False; tf5 = True ; tf7 = True   #6 OK
    opin = {("RIGHT", tf1): 1, ("TOP", tf3): 3, ("RIGHT", tf5): 5, ("TOP", tf7): 7, }
    chan = {"CHANS":        0, "CHANW":      2, "CHANN":        4, "CHANE":      6, }
    # handle OPIN
    if g_nid2info[s][5] == "OPIN":
        side = g_nid2info[s][6]
        me = sx == g_nid2info[s][0] and sy == g_nid2info[s][1]
        return (opin[(side, me)],       g_nid2info[s][4]     )
    # handle wire
    return     (chan[g_nid2info[s][5]], nodexy2ptc(s, sx, sy))
# def sort_sb_inputs

# ln: length of wires for this node's segment_id
# mi: index of node in list with matching ax, dc, p0, xl, xh, yl, yh sorted by node id
# ax: axis: 0=CHANX node, 1=CHANY node
# dc: decrement: 0=INC_DIR node, 1=DEC_DIR node
# p0: lowest ptc number assigned to this node's segment_id
# xl, xh, yl, yh: coordinates of this CHANX/CHANY node
def tileptc(ln, mi, ax, dc, p0, xl, xh, yl, yh):
    """compute tileable ptc from other info"""
    (lo, hi, mx) = (yl, yh, g_grid_bbox[3]) if ax else (xl, xh, g_grid_bbox[2]) 
    ptc = p0 + mi * 2 * ln

    reps = 1 if lo > 1 else ln
    if reps <= 1:
        if dc: ptc += 2 * ln - 1
    else:
        rep = min(lo + ln - 1, mx) - hi
        if dc: rep = reps - 1 - rep
        ptc += rep * 2 + dc

    return ptc
# def tileptc

g_route_setting = defaultdict(dict) # snk --> src --> (path, value, i)
g_nroute = 0
g_bidptc2ipin = {}  # (bid, ptc) --> i (pin number on pin's side)

### xmltraverse called from read_graph_xml
def xmltraverse(rrg, muxfile):
    """parse PYX file"""

    # here we visit different sections
    #log(" Visiting graph\n")

    # <channel chan_width_max="180" x_max="180" x_min="180" y_max="180" y_min="180"/>
    for channels in get_elements_by_tag_name(rrg, "channels", 1):
        n = 0
        for channel in child_nodes(channels):
            if is_text(channel) or channel[0] != "channel": continue
            n += 1
            h = channel[1]
            global g_chcnt
            g_chcnt = h['chan_width_max']
        # for channel
        #log(f" - Visited {n} <channel>\n")
    # for channels

    # <switches>
    # <switch id="0" name="my_switch" buffered="1">
    for switches in get_elements_by_tag_name(rrg, "switches", 1):
        n = 0
        for switch in child_nodes(switches):
            if is_text(switch) or switch[0] != "switch": continue
            n += 1
            h = switch[1]
            swid = h['id']
            name = h['name']
            g_swid2swname[swid] = name
            if not re.match(r'\D+\d+', name):
                # non-wire seg last in XML is input matrix mux
                global g_inp2swid
                g_inp2swid = swid
        # for switch
        #log(f" - Visited {n} <switch>es\n")
    # for switches

    # <segments>
    # <segment id="0" name="L4">
    for segments in get_elements_by_tag_name(rrg, "segments", 1):
        n = 0
        for segment in child_nodes(segments):
            if is_text(segment) or segment[0] != "segment": continue
            n += 1
            h = segment[1]
            sgid = h['id']
            name = h['name']
            g_sgid2sgname[sgid] = name
        # for segment
        #log(f" - Visited {n} <segment>s\n")
    # for segments

    # <block_types>
    for block_types in get_elements_by_tag_name(rrg, "block_types", 1):
        n = n2 = 0
        # <block_type id="0" name="io" width="1" height="1">
        for block_type in child_nodes(block_types):
            if is_text(block_type) or block_type[0] != "block_type": continue
            n += 1
            h = block_type[1]
            (bid, name, width, height) = [h[k] for k in ['id', 'name', 'width', 'height']]
            g_bid2info[bid] = {
                'name': name, 'width': width, 'height': height,
                'pid2info': {}, 'n2info': {},
            }
            g_block2bid[name] = bid
            # save ID of "clb"
            if 'clb' in name:
                global g_clb_id
                assert g_clb_id is None
                g_clb_id = bid
            # <pin_class type="input">
            for pin_class in child_nodes(block_type):
                if is_text(pin_class) or pin_class[0] != "pin_class": continue
                h = pin_class[1]
                pin_class_type = h['type']
                # <pin ptc="0">DATIN[0]</pin>
                for pin in child_nodes(pin_class):
                    if is_text(pin) or pin[0] != "pin": continue
                    n2 += 1
                    h = pin[1]
                    ptc = h['ptc']
                    # tolerate OpenFPGA tileable='true' needless verbosity
                    if not isinstance(ptc, int): ptc = int(ptc.split(',')[0])
                    text = None
                    for t in child_nodes(pin):
                        if is_text(t): text = t
                    assert text is not None
                    g_bid2info[bid]['pid2info'][ptc] = [text, pin_class_type]
                    g_bid2info[bid][  'n2info'][text] = [ptc, pin_class_type]
                # for pin
            # for pin_class
        # for block_type
        #log(f" - Visited {n} <block_type>s with {n2:,d} <pin>s\n")
    # for block_types

    # <grid>
    for grid in get_elements_by_tag_name(rrg, "grid", 1):
        n = 0
        # <grid_loc x="0" y="0" block_type_id="0" width_offset="0" height_offset="0"/>
        for grid_loc in child_nodes(grid):
            if is_text(grid_loc) or grid_loc[0] != "grid_loc": continue
            n += 1
            h = grid_loc[1]
            # upgrade to later graph type
            if g_add_layer:
                h['layer'] = 0
            (x, y, wo, ho) = [h[k] for k in ['x', 'y', 'width_offset', 'height_offset']]
            # is this difference real?
            if 'block_type' in h:
                bid = h['block_type']
            elif 'block_type_id' in h:
                bid = h['block_type_id']
            else:
                assert 0
            g_xy2bidwh[(x, y)] = (bid, wo, ho)

            # expand grid bbox by real blocks
            if bid == 0: continue
            if not g_grid_bbox:
                g_grid_bbox.clear()
                g_grid_bbox.extend([x, y, x, y])
                continue
            if x < g_grid_bbox[0]: g_grid_bbox[0] = x
            if y < g_grid_bbox[1]: g_grid_bbox[1] = y
            if x > g_grid_bbox[2]: g_grid_bbox[2] = x
            if y > g_grid_bbox[3]: g_grid_bbox[3] = y
        # for grid_loc
        #log(f" - Visited {n:,d} <grid_loc>s\n")
    # for grid

    # <rr_nodes> Pass 1 to determine:
    # segment id lengths
    # ptc spans (not explicit in file)
    g_sgid2len.clear()
    for rr_nodes in get_elements_by_tag_name(rrg, "rr_nodes", 1):
        n = 0
        # <node id="0" type="SOURCE" direction="NONE" capacity="1">
        # <node id="1" type="CHANX" direction="INC" capacity="1">
        for node in child_nodes(rr_nodes):
            if is_text(node) or node[0] != "node": continue
            n += 1
            h = node[1]
            loc_h = segment_h = None
            for child in child_nodes(node):
                if is_text(child): continue
                # <loc xlow="0" ylow="0" xhigh="0" yhigh="0" ptc="0"/>
                if child[0] == "loc":
                    loc_h = child[1]
                    # upgrade to later graph type
                    if g_add_layer:
                        loc_h['layer'] = 0
                # <segment segment_id="2"/>
                if child[0] == "segment":
                    segment_h = child[1]
            # for child
            if segment_h is None: continue

            sgid = segment_h['segment_id']
            ln = 1 + (loc_h['xhigh'] - loc_h['xlow']) + (loc_h['yhigh'] - loc_h['ylow'])
            if sgid not in g_sgid2len or ln > g_sgid2len[sgid]: g_sgid2len[sgid] = ln

            ptc = loc_h['ptc']
            # tolerate OpenFPGA tileable='true' needless verbosity
            if not isinstance(ptc, int): ptc = int(ptc.split(',')[0])
            if sgid not in g_sgid2ptcs:
                g_sgid2ptcs[sgid] = [ptc, ptc]
            elif ptc < g_sgid2ptcs[sgid][0]:
                g_sgid2ptcs[sgid][0] = ptc
            elif ptc > g_sgid2ptcs[sgid][1]:
                g_sgid2ptcs[sgid][1] = ptc

        # for node
        #log(f" - Visited {n:,d} <node>s 1/2\n")
    # for rr_nodes
    # reverse for some uses
    for sgid, ln in g_sgid2len.items():
        assert ln <= min(g_grid_bbox[2], g_grid_bbox[3]), (
            f"Segment {sgid} longer than min array dimension")
        g_len2sgid[ln] = sgid
    # we should also prohibit holes in the routing
    # build ptc --> sgid/sgname table
    for sgid, vv in g_sgid2ptcs.items():
        for ptc in range(vv[0], vv[1] + 1):
            g_ptc2sgidname[ptc] = (sgid, g_sgid2sgname[sgid])

    # <rr_nodes> Pass 2 to process nodes
    wclass2ids = defaultdict(set)
    for rr_nodes in get_elements_by_tag_name(rrg, "rr_nodes", 1):
        n = 0
        # <node id="0" type="SOURCE" direction="NONE" capacity="1">
        # <node id="1" type="CHANX" direction="INC" capacity="1">
        for node in child_nodes(rr_nodes):
            if is_text(node) or node[0] != "node": continue
            n += 1
            h = node[1]
            loc_h = None
            sgid = None
            for child in child_nodes(node):
                if is_text(child): continue
                # <loc xlow="0" ylow="0" xhigh="0" yhigh="0" ptc="0"/>
                if child[0] == "loc":
                    loc_h = child[1]
                # <segment segment_id="2"/>
                if child[0] == "segment":
                    sgid = child[1]['segment_id']
            # for child
            assert loc_h
            (nid, ty) = [h[k] for k in ['id', 'type']]
            (xl, yl, xh, yh, ptc) = [loc_h[k] for k in
                                        ['xlow', 'ylow', 'xhigh', 'yhigh', 'ptc']]
            # tolerate OpenFPGA tileable='true' needless verbosity
            if not isinstance(ptc, int): ptc = int(ptc.split(',')[0])

            if ty in ('SOURCE', 'OPIN', 'IPIN', 'SINK'):
                if ty.endswith("PIN"):
                    sgid = loc_h['side']
                else:
                    sgid = -1
                g_nid2info[nid] = [xl, yl, xh, yh, ptc, ty, sgid]
                # IPIN and OPIN will use the correct subtile for x, y
                g_xyt2nid[(xl, yl, ptc)] = nid
                continue

            assert sgid is not None
            di = h.get('direction', 'NONE') # doesn't always appear?
            if di.endswith('_DIR'): di = di[:-4]   # INC vs INC_DIR
            t2 = {  'CHANXINC': 'CHANE', 'CHANXDEC': 'CHANW',
                    'CHANYINC': 'CHANN', 'CHANYDEC': 'CHANS',
                }[ty + di]
            dl = f"{t2[-1]}{g_sgid2len[sgid]}" # e.g. E4
            assert (yl == yh) if t2[-1] in 'EW' else (xl == xh)

            g_nid2info[nid] = [xl, yl, xh, yh, ptc, t2, sgid]
            key = (dl, xl, yl, xh, yh)
            pair = (ptc, nid)
            g_xydl2tn[key].append(pair)
            #log(f"Adding to key={key} pair={pair}\n")
            key = (t2, sgid, xl, yl, xh, yh)
            wclass2ids[key].add(nid)
        # for node
        #log(f" - Visited {n:,d} <node>s 2/2\n")
    # for rr_nodes

    # Pass 3. Normalize PTCs to conform with OpenFPGA "bus index" use
    # This means we no longer require tileable="true"
    t2axdc = { 'CHANE': (0, 0), 'CHANN': (1, 0), 'CHANW': (0, 1), 'CHANS': (1, 1), }
    counts = defaultdict(int)
    for key, nids in wclass2ids.items():
        (t2, sgid, xl, yl, xh, yh) = key
        ln = g_sgid2len[sgid]
        (ax, dc) = t2axdc[t2]
        p0 = g_sgid2ptcs[sgid][0]
        for mi, nid in enumerate(sorted(nids), start=0):
            (xl, yl, xh, yh, ptc0, _, _) = g_nid2info[nid] 
            ptc = tileptc(ln, mi, ax, dc, p0, xl, xh, yl, yh)
            counts[ptc == ptc0] += 1
            g_nid2info[nid][4] = ptc
        # for
    # for
    (diff, same) = (counts[False], counts[True])
    frac = 100.0 * diff / (same + diff)
    if frac >   0.0: frac = max(frac,  1.0)
    if frac < 100.0: frac = min(frac, 99.0)
    log(f" - CHAN node ptc numbers {frac:.0f}% altered\n")
    wclass2ids.clear()

    # <rr_edges>
    # save edges and determine switch id lengths
    for rr_edges in get_elements_by_tag_name(rrg, "rr_edges", 1):
        n = 0
        # <edge src_node="0" sink_node="1" switch_id="0"/>
        for edge in child_nodes(rr_edges):
            if is_text(edge) or edge[0] != "edge": continue
            n += 1
            h = edge[1]
            src = h['src_node']
            snk = h['sink_node']
            swid = h['switch_id']
            # we may throw this out, rewrite it, whatever
            g_edges.append([src, snk, swid])
            sgid = g_nid2info[snk][6]
            if sgid in g_sgid2len:
                ln = g_sgid2len[sgid]
                if swid not in g_swid2len or g_swid2len[swid] < ln:
                    g_swid2len[swid] = ln
        # for edge
        #log(f" - Visited {n:,d} <edge>s\n")
    # for rr_edges
    # reverse for some uses
    for swid, ln in g_swid2len.items():
        g_len2swid[ln] = swid

    ### find routable.pins
    ###
    ### USE
    ### g_nid2info      nid --> [xl, yl, xh, yh, ptc, t2, sgid]
    ### g_xy2bidwh      (x, y) --> (bid, wo, ho)
    ###
    ### PRODUCE
    ### g_routablepins  (bid, sptc) --> (wo, ho)
    ###
    ### examine edges to see where we connect CHANX/Y and I/OPIN
    ### this will allow us to deduce which block pins are connected
    illegal = 0
    legal = { 'RIGHT', 'TOP', }
    ipins = defaultdict(set)
    for edges in get_elements_by_tag_name(rrg, "rr_edges", 1):
        for edge in child_nodes(edges):
            if is_text(edge): continue
            assert local_name(edge) == 'edge'
            hedge = attr2hash(edge)
            nsrc = hedge['src_node']
            ndst = hedge['sink_node']
            (sxl, syl, sxh, syh, sptc, sty, ssg) = g_nid2info[nsrc]
            (dxl, dyl, dxh, dyh, dptc, dty, dsg) = g_nid2info[ndst]
            # swap CHAN* --> *PIN to *PIN --> CHAN*
            if dty.endswith('PIN'):
                (sxl, syl, sxh, syh, sptc, sty, ssg, dxl, dyl, dxh, dyh, dptc, dty, dsg) = \
                (dxl, dyl, dxh, dyh, dptc, dty, dsg, sxl, syl, sxh, syh, sptc, sty, ssg)
            if not sty.endswith('PIN'): continue
            if not dty.startswith('CHAN'): continue

            # now we know that source PIN connects to CHAN
            assert sxl == sxh and syl == syh
            (bid, wo, ho) = g_xy2bidwh[(sxl, syl)]
            g_routablepins[(bid, sptc)] = (wo, ho)
            if ssg not in legal:
                illegal += 1
            elif sty == "IPIN":
                ipins[(bid, wo, ho, ssg)].add(sptc)
        # for edge
    # for edges

    if illegal:
        log(f" - {illegal} routable pins are not on legal sides\n")
    else:
        pass
        #log( " - All routable pins are on legal sides\n")

    # compute ipin #s for each block side
    g_bidptc2ipin.clear()
    for bid, wo, ho, side in sorted(ipins):
        for i, ptc in enumerate(sorted(ipins[(bid, wo, ho, side)]), start=0):
            assert (bid, ptc) not in g_bidptc2ipin
            g_bidptc2ipin[(bid, ptc)] = i
        # for
        name = g_bid2info[bid]['name']
        #print( f"Block {name}/{bid}/({wo},{ho}) side {side} " +
        #       f"had {len(ipins[(bid, wo, ho, side)])} pins")
    # for

    # create config bits for routing
    # Step 1: group inputs into muxes
    want = { ("OPIN", "CHAN"), ("CHAN", "CHAN"), ("CHAN", "IPIN"), }
    # when we add OPIN --> IPIN, ignore those with only 1 input (don't want carry chains)
    receiving = defaultdict(set)
    for (src, snk, swid) in g_edges:
        if (g_nid2info[src][5][:4], g_nid2info[snk][5][:4]) not in want: continue
        receiving[snk].add(src)
    # for
    # Step 2: create edge --> setting map and set all config bits to 1
    chan = {"CHANS":        0, "CHANW":      2, "CHANN":        4, "CHANE":      6, }
    with open(muxfile or "/dev/null", "w", encoding='ascii') as mfile:
        for snk in receiving:
            (xl, yl, xh, yh, ptc, pinchan, sgid) = g_nid2info[snk]
            if pinchan == "IPIN":
                cbxy = {'TOP': 'cbx', 'BOTTOM': 'cbx', 'RIGHT': 'cby', 'LEFT': 'cby', }[sgid]
                (bid, wo1, ho1) = g_xy2bidwh[(xl, yl)]
                if (bid, ptc) not in g_routablepins: continue
                (wo2, ho2) = g_routablepins[(bid, ptc)]
                assert wo1 == wo2 and ho1 == ho2
                npin = g_bidptc2ipin[(bid, ptc)]
                path = f"fpga_top.{cbxy}_{xl}__{yl}_.mem_{sgid.lower()}_ipin_{npin}"
                sources = sorted(   receiving[snk],
                    key=lambda s: ( 0 if g_nid2info[s][5] == "OPIN" else 1,
                                    nodexy2ptc(s, xl, yl),
                                    chan.get(g_nid2info[s][5], -1) ) )
                # NB WARNING FIXME
                # OpenFPGA is buggy & can't handle matching PTCs (all but last is lost).
                # We add the same CHAN order used in sort_sb_inputs() as 3rd key.
            else:
                match pinchan:
                    case "CHANE":
                        (sx, sy, side) = (xl - 1,   yl,     "right" )
                        # see write_cb_sb for doc of this code
                        if sx < g_commercial:
                            sx += 1
                            ptc += 2
                    case "CHANN":
                        (sx, sy, side) = (xl,       yl - 1, "top"   )
                        # see write_cb_sb for doc of this code
                        if sy < g_commercial:
                            sy += 1
                            ptc += 2
                    case "CHANW":
                        (sx, sy, side) = (xh,       yh,     "left"  )
                        ptc -= 2 * (xh - xl          )
                    case "CHANS":
                        (sx, sy, side) = (xh,       yh,     "bottom")
                        ptc -= 2 * (          yh - yl)
                    case _:         continue
                if g_commercial:
                    assert sx > 0 and sy > 0
                path = f"fpga_top.sb_{sx}__{sy}_.mem_{side}_track_{ptc}"
                #ources = sorted(   receiving[snk],
                #   key=lambda s: ( 0 if g_nid2info[s][5] == "OPIN" else 1,
                #                   nodexy2ptc(s, sx, sy)))
                sources = sorted(receiving[snk], key=lambda s: sort_sb_inputs(s, sx, sy))
                #if snk == 101817:
                #if snk == 1118781:
                #    print(f"SB DEBUG: snk = {snk} sources = {sources}")
                #    for s in sources:
                #        print(
                #           f"\tsource = {s} g_nid2info[s] = {g_nid2info[s]} " +
                #           f"sort_sb_inputs({s}, {sx}, {sy}) = {sort_sb_inputs(s, sx, sy)}" )
                mfile.write(f"{path} :: {snk} <== {sources}\n")

            s = len(receiving[snk])
            #if s == 1: continue
            w = int(math.ceil(math.log2(s)))
            m = w - 1
            for i, src in enumerate(sources, start=0):
                # convert normal index to OpenFPGA index
                ii = idx2sel(s, i)
                # save it
                if s < 2:
                    value = ""
                else:
                    value = ''.join("1" if ii & (1 << (m - b)) else "0" for b in range(w))
                edge = (src, snk)
                setting = (path, value, i)
                g_route_setting[snk][src] = setting
                #print(f"Set {edge} to {setting}\n")
            # for
            if s == 1: continue
            # set all config bits to 1
            for b in range(w):
                # create the config bit location
                newpath = path + f".mem_out[{b}]"
                assert newpath not in g_config
                #print(f"SET {newpath} {1}")
                g_config[newpath] = "1"
            global g_nroute
            g_nroute += w
        # for
    # with
# def xmltraverse

### read_graph_xml
def read_graph_xml(xmlfile, muxfile, serfile):
    """read the routing graph"""

    # figure out path from input file format
    xmlstarlet = "/usr/bin/xmlstarlet"
    xml = True
    base = ""
    if   xmlfile.endswith(".json.gz"):
        xml = False
        command = f"gunzip -c {xmlfile}"
    elif xmlfile.endswith(".json"):
        xml = False
        command = ""
    elif xmlfile.endswith(".pyx.gz"):
        base = xmlfile[:-7]
        command = f"gunzip -c {xmlfile}"
    elif xmlfile.endswith(".pyx"):
        base = xmlfile[:-4]
        command = f"cat {xmlfile}"  # TODO command = ""
    elif xmlfile.endswith(".xml.gz"):
        base = xmlfile[:-7]
        sl = base.rfind('/') + 1
        command = f"gunzip -c {xmlfile} | {xmlstarlet} pyx | tee {base[sl:]}.pyx"
    elif xmlfile.endswith(".xml"):
        base = xmlfile[:-4]
        sl = base.rfind('/') + 1
        command = f"{xmlstarlet} pyx < {xmlfile} | tee {base[sl:]}.pyx"
    else:
        log(f"Unrecognized input file {xmlfile}\n")
        sys.exit(1)

    # read rrgraph: traverse tree and extract info
    with subprocess.Popen(
            command,
            shell=True,
            stdout=subprocess.PIPE,
            universal_newlines=True).stdout if command else open(
            xmlfile, encoding='ascii') as xfile:
        log(f"{elapsed()} Reading {xmlfile}\n")
        if xml:
            dom_tree = ['document', {}]
            stack = [dom_tree]
            # 2nd arg is a stack, with last item being one modified
            rrg = pyxparse(xfile, stack)
            assert len(dom_tree) >= 3 and dom_tree[2][0] == "rr_graph"
        else:
            rrg = json.load(xfile)
    # with

    # process what we read
    xmltraverse(rrg, muxfile)

    # serialize to json file for later quicker re-read
    if serfile:
        with gzopenwrite(serfile) as ofile:
            log(f"{elapsed()} Writing {serfile}\n")
            json.dump(rrg, ofile, indent=None, separators=(',', ':'), sort_keys=True)

    return rrg
# def read_graph_xml

g_outidx2ptc = defaultdict(dict)
g_outbit2ptc = defaultdict(dict)

def prep_top_wiring():
    """how are block pins connected at the tops of blocks"""

    # g_node2pin = defaultdict(set)   # direct_input --> { direct_output }
    # g_out2inphash = {}              # mux_output --> { mux_input --> idx }
    for block, pair in g_block2wiring.items():

        # HACK currently no top wiring that matters in the IOTiles
        # FIXME CONFIG
        if block.startswith("io"): continue

        (node2pin, out2inphash) = pair
        bid = g_block2bid[block]
        binfo = g_bid2info[bid]
        n2info = binfo['n2info']

        # process "direct"s which in effect make new names for OPINs
        node2optc = {}
        for inp, outs in node2pin.items():
            ptcs = set()
            for outp in outs:
                if outp not in n2info: continue
                (ptc, pct) = n2info[outp]
                assert pct == "OUTPUT"
                ptcs.add(ptc)
            # for
            if ptcs: node2optc[inp] = min(ptcs)
        # for
        #print(f"TOP WIRING: block={block} node2optc={node2optc}")

        #print(f"TOP WIRING: block={block} out2inphash={out2inphash}")
        for outkey, inphash in out2inphash.items():
            #  in: clb_lr.in[44]
            # out: mem_clb_lr_0_in_44
            # FIXME CONFIG lots of name assumptions here
            outtail = re.sub(r'^(\w+)_lr\.(\w+)\[(\d+)\]$', r'mem_\1_lr_0_\2_\3', outkey)
            w = len(inphash)
            for inp, i in inphash.items():
                if inp in node2optc:
                    ptc = node2optc[inp]
                elif inp in n2info:
                    (ptc, pct) = n2info[inp]
                    assert pct == "INPUT"
                else:
                    print(
f"TOP WIRING ANALYSIS FAILURE: block={block} inp={inp} outkey={outkey}")
                    continue
                val = idx2sel(w, i)
                n = int(math.ceil(math.log2(w)))
                bit = val2bit(n, val)
                g_outidx2ptc[outtail][i  ] = ptc
                g_outbit2ptc[outtail][bit] = ptc
            # for
        # for

    # for
# def prep_top_wiring

#
# write_cb_sb: support routines
#

def compress(bus, indices):
    """produce compressed references to bus bits"""
    ranges = []
    for i in indices:
        if not ranges or ranges[-1][1] + 1 != i:
            ranges.append((i, i))
        else:
            ranges[-1] = (ranges[-1][0], i)
    refs = [f"{bus}[{i[0]}:{i[1]}]" if i[0] != i[1] else f"{bus}[{i[0]}]" for i in ranges]
    return "{" + ', '.join(refs) + "}" if len(refs) != 1 else refs[0]
# def compress

def regbus(busses, conns):
    """register busses' indices in 'busses'"""
    # no whitespace
    conns = re.sub(r'\s', r'', conns)
    # get rid of {} and ,
    conns = re.sub(r'[{,}]+', r' ', conns)
    # process each
    for conn in conns.split():
        g = re.match(r'(\w+)\[(\d+)(?::(\d+))?]$', conn)
        if not g:
            busses[conn].add(0)
            continue
        (bus, l, r) = g.groups()
        busses[bus].add(int(l))
        if r is None: continue
        busses[bus].add(int(r))
    # for
# def regbus

g_chan2inport = {
    'CHANE': 'chanx_left_in',
    'CHANN': 'chany_bottom_in',
    'CHANW': 'chanx_right_in',
    'CHANS': 'chany_top_in',
}

# case 1: used by RIGHT/TOP with VPR pattern
# case 2: used by RIGHT/TOP with stamped pattern
# CASE 3: both
g_diff2group = {
    ('RIGHT', 'CHANE', 0, 0):  'bottom_left_grid_right_width_0_height_0_subtile_0_', # case 2
    ('RIGHT', 'CHANE', 0, 1):     'top_left_grid_right_width_E_height_0_subtile_0_',
    ('RIGHT', 'CHANE', 1, 0): 'right_bottom_grid_right_width_0_height_0_subtile_0_',
    ('RIGHT', 'CHANE', 1, 1): 'right_bottom_grid_right_width_F_height_0_subtile_0_',

    ('RIGHT', 'CHANN', 0, 0):  'bottom_left_grid_right_width_0_height_0_subtile_0_', # case 2
    ('RIGHT', 'CHANN', 0, 1):     'top_left_grid_right_width_0_height_0_subtile_0_', # case 1
    ('RIGHT', 'CHANN', 1, 0): 'right_bottom_grid_right_width_N_height_0_subtile_0_',
    ('RIGHT', 'CHANN', 1, 1): 'right_bottom_grid_right_width_O_height_0_subtile_0_',

    ('RIGHT', 'CHANW', 0, 0):  'bottom_left_grid_right_width_0_height_0_subtile_0_', # case 2
    ('RIGHT', 'CHANW', 0, 1):     'top_left_grid_right_width_U_height_0_subtile_0_',
    ('RIGHT', 'CHANW', 1, 0): 'right_bottom_grid_right_width_V_height_0_subtile_0_',
    ('RIGHT', 'CHANW', 1, 1): 'right_bottom_grid_right_width_W_height_0_subtile_0_',

    ('RIGHT', 'CHANS', 0, 0):  'bottom_left_grid_right_width_0_height_0_subtile_0_', # CASE 3
    ('RIGHT', 'CHANS', 0, 1):     'top_left_grid_right_width_R_height_0_subtile_0_',
    ('RIGHT', 'CHANS', 1, 0): 'right_bottom_grid_right_width_S_height_0_subtile_0_',
    ('RIGHT', 'CHANS', 1, 1): 'right_bottom_grid_right_width_T_height_0_subtile_0_',

    ('TOP'  , 'CHANE', 0, 0):    'left_bottom_grid_top_width_0_height_0_subtile_0_', # case 2
    ('TOP'  , 'CHANE', 0, 1):       'top_left_grid_top_width_E_height_0_subtile_0_',
    ('TOP'  , 'CHANE', 1, 0):   'right_bottom_grid_top_width_0_height_0_subtile_0_', # case 1
    ('TOP'  , 'CHANE', 1, 1):   'right_bottom_grid_top_width_F_height_0_subtile_0_',

    ('TOP'  , 'CHANN', 0, 0):    'left_bottom_grid_top_width_0_height_0_subtile_0_', # case 2
    ('TOP'  , 'CHANN', 0, 1):       'top_left_grid_top_width_0_height_0_subtile_0_',
    ('TOP'  , 'CHANN', 1, 0):   'right_bottom_grid_top_width_N_height_0_subtile_0_',
    ('TOP'  , 'CHANN', 1, 1):   'right_bottom_grid_top_width_O_height_0_subtile_0_',

    ('TOP'  , 'CHANW', 0, 0):    'left_bottom_grid_top_width_0_height_0_subtile_0_', # CASE 3
    ('TOP'  , 'CHANW', 0, 1):       'top_left_grid_top_width_U_height_0_subtile_0_',
    ('TOP'  , 'CHANW', 1, 0):   'right_bottom_grid_top_width_V_height_0_subtile_0_',
    ('TOP'  , 'CHANW', 1, 1):   'right_bottom_grid_top_width_W_height_0_subtile_0_',

    ('TOP'  , 'CHANS', 0, 0):    'left_bottom_grid_top_width_0_height_0_subtile_0_', # case 2
    ('TOP'  , 'CHANS', 0, 1):       'top_left_grid_top_width_R_height_0_subtile_0_',
    ('TOP'  , 'CHANS', 1, 0):   'right_bottom_grid_top_width_S_height_0_subtile_0_',
    ('TOP'  , 'CHANS', 1, 1):   'right_bottom_grid_top_width_T_height_0_subtile_0_',
}

def add_src(sx, sy, dpinchan, srcs, src, twist):
    """convert 'src' to appropriate text form and add to 'srcs'"""
    #sxl  syl  sxh  syh  sptc  spinchan  ssgid
    (sxl, syl, _  , _  , sptc, spinchan, ssgid) = g_nid2info[src]
    if spinchan == "OPIN":
        # Case A: handle an OPIN
        #bid  wo  ho
        (bid, _, _) = g_xy2bidwh[(sxl, syl)]
        info = g_bid2info[bid]['pid2info'][sptc] 
        #text  pin_class_type
        (text, _) = info
        (blk, text) = text.split('.')
        # pull out slot number for IO. FIXME CONFIG conditionalize on capacity?
        g = re.match(r'\w+\[(\d+)\]$', blk)
        st = g.group(1) if g else "0"
        text = re.sub(r'[^\w]', r'_', text)
        (dx, dy) = (sxl - sx, syl - sy)
        group = g_diff2group[(ssgid, dpinchan, dx, dy)]
        group = re.sub(r'_subtile_0_', fr'_subtile_{st}_', group)
        srcs.append(f"{group}_pin_{text}")
        return
    # if

    # Case B: handle CHAN
    port = g_chan2inport[spinchan]
    ptc2 = nodexy2ptc(src, sx, sy) >> 1
    if spinchan[-1] in 'WS': ptc2 -= twist
    # determine if we add fully new reference or extend previous
    prev = srcs[-1] if srcs else ""
    g = re.match(fr'({port}\[(?:\d+:)?)(\d+)\]$', prev)
    if not g or int(g.group(2)) + 1 != ptc2:
        srcs.append(f"{port}[{ptc2}]")
        return
    # if
    (lead, last) = g.groups()
    if lead[-1] == ":":
        srcs[-1] = f"{lead}{ptc2}]"
    else:
        srcs[-1] = f"{lead}{last}:{ptc2}]"
# def add_src

def write_cb_sb(dirpath):
    """figure out routing blocks and write them out"""

    log(f"{elapsed()} Generate routing Verilog\n")
    os.makedirs(dirpath, mode=0o777, exist_ok=True)

    # break into separate tiles
    wxy2sinks = defaultdict(lambda: defaultdict(set))

    side2n = {'top':0, 'right':1, 'bottom':2, 'left':3, }
    n2side = dict(zip(side2n.values(), side2n.keys()))
    for snk, inps in g_route_setting.items():
        (xl, yl, xh, yh, ptc, pinchan, sgid) = g_nid2info[snk]
        match pinchan:
            case "IPIN":
                (bid, wo1, ho1) = g_xy2bidwh[(xl, yl)]
                if (bid, ptc) not in g_routablepins: continue
                (wo2, ho2) = g_routablepins[(bid, ptc)]
                assert wo1 == wo2 and ho1 == ho2
                assert xl == xh and yl == yh
                (sx, sy) = (xl, yl)
                side = sgid.lower()
                n = g_bidptc2ipin[(bid, ptc)]
                block = {'TOP': 'cbx', 'BOTTOM': 'cbx', 'RIGHT': 'cby', 'LEFT': 'cby', }[sgid]
            # in next two cases, change a 0 coordinate to 1 if g_commercial
            case "CHANE":
                (sx, sy, side) = (xl - 1,   yl,     "right" )
                if sx < g_commercial:
                    sx += 1
                    ptc += 2
            case "CHANN":
                (sx, sy, side) = (xl,       yl - 1, "top"   )
                if sy < g_commercial:
                    sy += 1
                    ptc += 2
            case "CHANW":
                (sx, sy, side) = (xh,       yh,     "left"  )
                # ptc is for xl, yl
                # we want for xh, yh (driver location)
                # ptc decreases as you move from xl, yl to xh, yh
                ptc -= 2 * (xh - xl          )
            case "CHANS":
                (sx, sy, side) = (xh,       yh,     "bottom")
                # ditto
                ptc -= 2 * (          yh - yl)
            case _:         continue
        if pinchan != "IPIN":
            n = ptc
            block = "sb"
        side = side2n[side]
        wxy2sinks[(block, sx, sy)][(sgid, -len(inps))].add((side, n, snk))
    # for

    lbr = '{'
    rbr = '}'
    snk2blwl = {}

    chan2outport = {
        'CHANE': 'chanx_right_out',
        'CHANN': 'chany_top_out',
        'CHANW': 'chanx_left_out',
        'CHANS': 'chany_bottom_out',
    }
    
    xs = [wxy[1] for wxy in wxy2sinks]
    ys = [wxy[2] for wxy in wxy2sinks]
    (xmin, xmax, ymin, ymax) = (min(xs), max(xs), min(ys), max(ys))

    # add cbx/cby "through" cells
    if ymin == 0:
        for sx in range(xmin + 1, xmax + 1):
            wxy2sinks[("cbx", sx, ymin)] = defaultdict(set)
    if xmin == 0:
        for sy in range(ymin + 1, ymax + 1):
            wxy2sinks[("cby", xmin, sy)] = defaultdict(set)

    hcw = g_chcnt >> 1
    hash2list = defaultdict(list)
    for triple, sgidni2snks in wxy2sinks.items():
        (block, sx, sy) = triple
        (bid, _, _) = g_xy2bidwh[(sx, sy)]
        what = "track" if block == "sb" else "ipin"
        master = f"{block}_{sx}__{sy}_"

        # TRICKY
        if block == "sb":
            ft_twist = 1
            src_twist = 1
        else:
            ft_twist = 0
            if g_commercial:
                src_twist = 1
            else:
                src_twist = 0

        # 1. enumerate muxes and count needed bits
        assign_order = []
        bits = 0
        for double, snks in sgidni2snks.items():
            (sgid, minps) = double
            slen = g_sgid2len.get(sgid, "CB")
            for side, n, snk in snks:
                assign_order.append((side, slen, n, snk))
                inps = len(g_route_setting[snk])
                bits += int(math.ceil(math.log2(inps)))
            # for
        # for
        # 2. determine subarray size
        sq = int(math.ceil(math.sqrt(bits)))
        # 3. assign bl/wl indices to each bit
        bl = 0; wl = 0
        for side, slen, n, snk in sorted(assign_order):
            inps = len(g_route_setting[snk])
            b = int(math.ceil(math.log2(inps)))
            bli = []; wli = []
            for _ in range(b):
                bli.append(bl)
                wli.append(wl)
                bl += 1
                if bl >= sq:
                    bl = 0
                    wl += 1
            # for
            bli = compress("bl", bli)
            wli = compress("wl", wli)
            snk2blwl[snk] = (bli, wli)
        # for

        # 4. write out muxes then mems in OpenFPGA order
        ramlines = {}
        busses = defaultdict(set)   # bus --> set of int (indices)

        # we will save lines for the body here
        # later we will assemble "leads" and "tails".
        lines = []

        outseen = set()
        for double in sorted(sgidni2snks):
            (sgid, minps) = double
            slen = g_sgid2len.get(sgid, "CB")
            inps = -minps
            bits = int(math.ceil(math.log2(inps)))
            bmax = bits - 1
            snks = sgidni2snks[double]
            for mxm in ["mux", "mem"]:
                nb = 0
                for side, n, snk in sorted(snks):

                    #dxl  dyl  dxh  dyh                  dsgid 
                    (_  , _  , _  , _  , dptc, dpinchan, _    ) = g_nid2info[snk]
                    assert inps == len(g_route_setting[snk])

                    # check for "singleton" mux which becomes an assign stmt
                    if len(g_route_setting[snk]) == 1:
                        if mxm == "mem": continue
                        srcs = []
                        src = list(g_route_setting[snk])[0]
                        add_src(sx, sy, dpinchan, srcs, src, src_twist)
                        assert len(srcs) == 1
                        regbus(busses, srcs[0])
                        port = chan2outport.get(dpinchan, "CB")
                        b = n >> 1
                        outp = f"{port}[{b}]"
                        outseen.add(outp)
                        lines.append(f"assign {outp} = {srcs[0]};\n")
                        regbus(busses, outp)
                        continue
                    # if

                    # FIXME CONFIG these names should come from XML
                    if block == "sb":
                        mod = f"mux_tree_tapbuf_L{slen}SB_size{inps}"
                    else:
                        mod = f"mux_tree_tapbuf_size{inps}"
                    rbus = f"{mod}_{nb}_sram"
                    # 1-bit "busses" don't get subscripts
                    sub = f"[0:{bmax}]" if bmax else ""
                    pos = rbus
                    neg = pos + "_inv"

                    # first time through, declare ram bus
                    if mxm == "mux":
                        # 1-bit "busses" don't get declarations
                        if sub:
                            ramlines[pos] = f"wire {sub} {pos};\n"
                            ramlines[neg] = f"wire {sub} {neg};\n"
                    else:
                        # otherwise add mem to module
                        mod += "_mem"

                    tside = n2side[side]
                    use = f"{mxm}_{tside}_{what}_{n}"
                    lines.append(f"\t{mod} {use}")
                    finish = ' ('

                    #dumpit = (  1 <= sx <= 9 and sy == 0 and
                    #            mxm == "mux" and
                    #            tside == "top" and what == "track" and n in (158, 62))
                    dumpit = False

                    if mxm == "mux":
                        srcs = []
                        # g_route_setting[snk][src] --> (path, value, i)
                        for src in sorted(  g_route_setting[snk],
                                            key=lambda k: g_route_setting[snk][k][2] ):
                            add_src(sx, sy, dpinchan, srcs, src, src_twist)
                            if not dumpit: continue
                            (qxl, qyl, qxh, qyh, qptc, qpinchan, qsgid) = g_nid2info[src]
                            print(
f"{snk} ({sx},{sy}) <== {src} ({qxl},{qyl},{qxh},{qyh}) {qptc} {qpinchan} {qsgid}")
                        # for
                        inputs = f"{lbr}{', '.join(srcs)}{rbr}"
                        regbus(busses, inputs)
                        lines.append(f"{finish}\n\t\t.in({inputs})")
                        finish = ','
                    else:
                        (bli, wli) = snk2blwl[snk] 
                        lines.append(f"{finish}\n\t\t.bl({bli})")
                        regbus(busses, bli)
                        finish = ','
                        lines.append(f"{finish}\n\t\t.wl({wli})")
                        regbus(busses, wli)

                    # don't declare these local connections to regbus
                    # FIXME CONFIG these names should come from XML
                    ports = ["sram",    "sram_inv"] if mxm == "mux" else \
                            ["mem_out", "mem_outb"]
                    for port, bus in zip(ports, [f"{pos}{sub}", f"{neg}{sub}"]):
                        lines.append(f"{finish}\n\t\t.{port}({bus})")
                        finish = ','

                    if mxm == "mux":
                        b = n >> 1
                        if dpinchan == "IPIN":
                            (text, _) = g_bid2info[bid]['pid2info'][dptc]
                            (blk, text) = text.split('.')
                            # pull out slot. FIXME CONFIG conditionalize on capacity.
                            g = re.match(r'\w+\[(\d+)\]$', blk)
                            st = g.group(1) if g else "0"
                            text = re.sub(r'[^\w]', r'_', text)
                            if block == "cbx":
                                outp = (
                    f"bottom_grid_top_width_0_height_0_subtile_{st}__pin_{text}")
                            else:
                                outp = (
                    f"left_grid_right_width_0_height_0_subtile_{st}__pin_{text}")
                        else:
                            port = chan2outport.get(dpinchan, "CB")
                            outp = f"{port}[{b}]"
                        outseen.add(outp)
                        lines.append(f"{finish}\n\t\t.out({outp})")
                        regbus(busses, outp)

                    lines.append( ");\n\n")
                    nb += 1
                # for side, n, snk
            # for mxm
        # for double

        # determine half channel width
        # and ensure output busses are correct
        dirs = ['x_left', 'x_right', 'y_bottom', 'y_top']
        #hcw = set()
        #for d in dirs:
        #    hcw.update(busses[f"chan{d}_in"])
        #if hcw:
        #    hcw = 1 + max(hcw)
        #else:
        #    # FIXME
        #    hcw = 80
        # FIXME
        assert hcw == 80, f"hcw = {hcw}"
        for d in dirs:
            for bus in [ f"chan{d}_in", f"chan{d}_out" ]:
                if bus in busses: busses[bus].update([0, hcw - 1])

        # write it all out
        leads = []
        # lines has already been filled
        tails = []

        # headers and module declaration
        leads.append( "`timescale 1ns / 1ps\n")
        leads.append( "\n")
        leads.append( "`default_nettype wire\n")
        leads.append( "\n")

        module_line_index = len(leads)  # remember which item has module name
        leads.append(f"module {master}")
        finish = '('

        # determine port order
        if block == "sb":
            porder = [
                'chany_top_in', 'top', 'chanx_right_in', 'right',
                'chany_bottom_in', 'bottom', 'chanx_left_in', 'left',
                ' ',
                'chany_top_out', 'chanx_right_out',
                'chany_bottom_out', 'chanx_left_out',
            ]
        else:
            porder = [
                'chany_bottom_in', 'chanx_left_in', 
                'chany_top_in', 'chanx_right_in', 
                ' ',
                'chany_bottom_out', 'chanx_left_out',
                'chany_top_out', 'chanx_right_out',
                'bottom', 'left',
            ]
        porder = { v: i for i, v in enumerate(porder, start=0) }
        misc = porder[' ']
        ordered = sorted([  (   porder.get(b, porder.get(b.split('_')[0], misc)),
                                dict_aup_nup(b),
                                b)
                            for b in busses ])

        # which channel ports are present?
        # NB: this later filtered by "busses", so these are candidates
        havechanport = set()
        if block != "cby" or g_commercial:
            if sx > xmin:
                havechanport.update(['chanx_left_in', 'chanx_left_out'])
            if sx < xmax or block == "cbx":
                havechanport.update(['chanx_right_in', 'chanx_right_out'])
        if block != "cbx" or g_commercial:
            if sy > ymin:
                havechanport.update(['chany_bottom_in', 'chany_bottom_out'])
            if sy < ymax or block == "cby":
                havechanport.update(['chany_top_in', 'chany_top_out'])

        # port declarators in module statement
        portdeclared = set()
        for _, _, port in ordered:
            if port not in busses: continue
            if port.startswith("chan") and port not in havechanport: continue
            portdeclared.add(port)
            leads.append(f"{finish}{port}")
            finish = ",\n                "
        leads.append(");\n")

        # port declarations inside module body
        for _, _, port in ordered:
            if port not in busses: continue
            if port.startswith("chan") and port not in havechanport: continue
            busses[port].add(0)
            (l, r) = (min(busses[port]), max(busses[port]))
            pdir = "output" if port.endswith("_out") else "input"
            if block != "sb":
                if '__pin_' in port: pdir = "output"
            leads.append(f"{pdir} [{l}:{r}] {port};\n")

        leads.append("\n")

        # declare config --> mux busses
        for wire in sorted(ramlines):
            leads.append(ramlines[wire])

        # connect feed-throughs
        leads.append("\n")
        needblank = False
        for (lhs, rhs) in [
            ['chany_bottom_out',    'chany_top_in',     ],
            ['chanx_left_out',      'chanx_right_in',   ],
            ['chany_top_out',       'chany_bottom_in',  ],
            ['chanx_right_out',     'chanx_left_in',    ], ]:
            #f lhs not in havechanport or rhs not in havechanport: continue
            if lhs not in portdeclared or rhs not in portdeclared: continue
            for b in range(hcw):
                left = f"{lhs}[{b}]"
                if left in outseen: continue
                right = f"{rhs}[{b - ft_twist}]"
                #if left not in portdeclared or right not in portdeclared:
                #    continue
                leads.append(f"\tassign {left} = {right};\n")
                needblank = True
            # for
        # for
        if needblank: leads.append("\n")

        # endmodule and ending comments
        tails.append( "endmodule\n")
        tails.append( "\n")
        tails.append( "`default_nettype none\n")

        # compute hashcode
        rtlhash = hashlib.sha1()
        for i, group in enumerate([leads, lines, tails], start=0):
            for j, line in enumerate(group, start=0):
                if i == 0 and j == module_line_index: continue
                rtlhash.update(line.encode(encoding='ascii'))
        hd = rtlhash.hexdigest()

        # add this routing block to its (hashed) equivalence class
        hash2list[hd].append(master)

        # write out routing block if first in equivalence class
        if len(hash2list[hd]) > 1:
            continue
        filename = f"{dirpath}/{master}.v"
        with open(filename, "w", encoding='ascii') as ofile:
            # write leads, lines (core instantiations), and tails
            for group in [leads, lines, tails]:
                for line in group:
                    ofile.write(line)
        # with
        #print(f"hd={hd} master={master}")

    # for
    hashes = sorted(hash2list, key=lambda h: dict_aup_nup(hash2list[h][0]))
    filename = f"{dirpath}/rmasters.csv"
    with open(filename, "w", encoding='ascii') as ofile:
        for h in hashes:
            ofile.write(','.join(hash2list[h]) + "\n")
        # for
    # with

    blocks = sum(len(hash2list[h]) for h in hash2list)
    masters = len(hash2list)
    log(
f"{elapsed()} Finished routing Verilog ({masters:,d} masters over {blocks:,d} instances)\n")
# def write_cb_sb

def expand_pattern(pattern):
    """expand blif-style LHS patterns"""
    idx = pattern.find('-')
    if idx < 0:
        yield pattern
        return
    (l, r) = (pattern[0:idx], pattern[idx+1:])
    yield from expand_pattern(l + "0" + r)
    yield from expand_pattern(l + "1" + r)
    return
# def expand_pattern

g_d2bits = {'0': '0000', '1': '0001', '2': '0010', '3': '0011',
            '4': '0100', '5': '0101', '6': '0110', '7': '0111',
            '8': '1000', '9': '1001', 'a': '1010', 'b': '1011',
            'c': '1100', 'd': '1101', 'e': '1110', 'f': '1111', }

def val2bits(value):
    """convert Verilog value to bitstring"""
    g = re.match(r"(\d+)'([hHoObB])(\w+)$", value)
    assert g
    (w, radix, digits) = g.groups()
    w = int(w)
    # expand to binary
    chunk = {'h':4, 'o':3, 'b':1, }[radix.lower()]
    bits = ''.join(g_d2bits[d.lower()][4-chunk:] for d in digits)
    # ensure always w bits
    if len(bits) < w:
        bits = "0" * (w - len(bits)) + bits
    else:
        bits = bits[-w:]
    return bits
# def val2bits

g_out2lut = {}
g_out2lut_used = {}

g_dump_luts = False

def strip_complex(contents):
    """remove comments, strings, attributes, newlines"""
    # form tokens respecting comments, strings, attributes, others
    tokens = re.findall(r'''
        [/][*](?:[^*]|[*][^/])*[*][/]   |   # /* --- */
        [/][/][^\n]*                    |   # // ---
        ["](?:[^"\\]|\\.)*["]           |   # " --- "
        [(][*]                          |   # (*
        [*][)]                          |   # *)
        \n                              |   # newline
        .                                   # anything else
    ''', contents, re.VERBOSE)
    # replace comments, strings, and newlines with spaces
    comments = { "/*", "//", }
    tokens = [" " if t[:2] in comments or t[0] in '"\n' else t for t in tokens]
    # reassemble and remove attributes (* --- *)
    return re.sub(r'[(][*]([^*]|[*][^)])*[*][)]', r' ', ''.join(tokens))
# def strip_complex

# new version reading from _post_route.v
# WE ARE NOT USING THIS VERSION
def read_design_post_route_v(vfile):
    """read LUT contents from Verilog"""
    with open(vfile, encoding='ascii') as ifile:
        log(f"{elapsed()} Reading {vfile}\n")
        contents = ifile.read()

    # simplify
    contents = strip_complex(contents)

    # step through statements
    for stmt in contents.split(';'):
        # look for structural instances with parameters
        # master #( .name (value) [, name(value) ]* ) instance ( ...
        g = re.match(
            r'''\s* ([\w$]+|\\\S+) \s* \#
                \s* [(] \s* ((?:[^()\\]|\\S+|[(][^()]*[)]|\s+?)*?) \s* [)]
                \s* ([\w$]+|\\\S+) \s* [(]''',
            stmt, re.VERBOSE)
        if not g: continue

        (master, params, instance) = g.groups()
        if master[0] == "\\": master = master[1:]
        params = params.strip()
        if instance[0] == "\\": instance = instance[1:]

        pdict = {}
        for param in params.split(','):
            if param == "": continue
            g = re.match(r'\s*[.]\s*([\w$]+|\\\S+) \s* [(] \s* (.*?) \s* [)] \s* $',
                param, re.VERBOSE)
            assert g, f"param={param} stmt={stmt}"
            (name, value) = g.groups()
            value = re.sub(r'\s+', r'', value)
            if "'" in value:
                value = val2bits(value)
            elif value.isdecimal():
                value = int(value)
            else:
                assert 0, f"name={name} value={value} stmt={stmt}"
            pdict[name] = value
        # for

        if   re.match(r'LUT_K', master) and 'K' in pdict and 'LUT_MASK' in pdict:
            assert instance[:4] == "lut_", f"instance={instance}"
            k = pdict['K']
            value = pdict['LUT_MASK']
            assert 1 << k == len(value), f"k={k} value={value}"
            output = instance[4:]
            assert output not in g_out2lut
            g_out2lut[output] = value
            if g_dump_luts:
                print(f"LUT k={k} value={value} output={output} inst={instance}")
            g_logger.info(f"LUT {output} <== {value}")
        # FIXME CONFIG too many magic strings in the following
        elif re.match(r'RS_TDP', master) and 'MODE_BITS' in pdict:
            assert instance[:10] == "RS_TDP36K_", f"instance={instance}"
            output = instance[10:]
            # FIXME
            #SAVE[output] = pdict['MODE_BITS']
        elif re.match(r'RS_DSP', master) and 'MODE_BITS' in pdict:
            assert instance[:12] == "RS_DSP_MULT_", f"instance={instance}"
            output = instance[12:]
            # FIXME
            #SAVE[output] = pdict['MODE_BITS']
        else:
            continue
    # for

    nl = len(g_out2lut)
    log(f" - {nl:,d} LUT{plural(nl)} specified\n")
# def read_design_post_route_v

# old version reading from _post_synth.v
# THIS IS THE VERSION WE ARE USING
def read_design_post_synth_v(vfile):
    """read LUT contents from Verilog"""
    with open(vfile, encoding='ascii') as ifile:
        log(f"{elapsed()} Reading {vfile}\n")
        contents = ifile.read()

    # simplify
    contents = strip_complex(contents)

    # step through statements
    for stmt in contents.split(';'):
        # LUT2 #( .INIT_VALUE(4'h1) ) \out:c ( .Y(c), .A({ a, b }) )
        g = re.match(
            r'''\s* LUT(\d) \s*                                                     # k
                \# \s* [(] \s* [.] \s* INIT_VALUE \s* [(] ([^()]*) [)] \s* [)] \s*  # mask
                (\\\S+ | [\w$]+) \s*                                                # instance
                [(] .* [.] \s* Y \s* [(] ([^()]*) [)] .* [)] \s* $''',              # output
            stmt, re.VERBOSE)
        if not g: continue

        #print(f"STATEMENT g={g} stmt={stmt}")
        # found a LUT instance: pull out k, INIT_VALUE, and output net
        (k, value, inst, output) = g.groups()  # don't use instance name (yet?)
        k = int(k)
        value = re.sub(r'\s+', '', value)
        output = re.sub(r'\s+', '', output) # strips and changes A [0] ==> A[0]
        if output[0] == '\\' and not output[1].isdigit(): output = output[1:]

        bits = val2bits(value)
        assert 1 << k == len(bits)

        assert output not in g_out2lut
        g_out2lut[output] = bits
        if g_dump_luts:
            print(f"LUT k={k} value={bits} inst={inst} output={output}")
        g_logger.info(f"LUT {output} <== {bits}")
    # for

    nl = len(g_out2lut)
    log(f" - {nl:,d} LUT{plural(nl)} specified\n")

# def read_design_post_synth_v

# what we are trying to extract
# LUT6 #(
#   .INIT_VALUE(64'h000a0c0000000000)
# ) _6155_ (
#   .Y(_4568_[15]),
#   .A({    \sramcont.state [1:0],
#           \sramcont.state [2],
#           \sramcont.state [3],
#           \resultwriteinst.dataout [15],
#           rgdatain[15] })
# );

# NEVER SUCCESSFULLY USED
def read_design_blif(bfile):
    """read LUTs from blif"""
    # now read post-VPR blif
    with open(bfile, encoding='ascii') as ifile:
        log(f"{elapsed()} Reading {bfile}\n")
        contents = ifile.read()

    # remove backslashes
    contents = re.sub(r'\\\n', r' ', contents)
    # remove comments
    contents = re.sub(r'^\s*#.*\n', r'', contents)
    contents = re.sub(r'\s#.*\n', r'', contents)
    # remove uninteresting commands
    contents = re.sub(r'^[.](param|subckt|model|inputs|outputs|end)\s.*\n', r'', contents)

    # pull out LUT info
    out2ins = {}
    out2lut = defaultdict(list)
    out2bits = {}
    outnet = " ? "
    for line in contents.split('\n'):
        if line.startswith(".names "):
            f = line.split()
            outnet = f[-1]
            out2ins[outnet] = f[1:-1]
            continue
        if len(line) == 0:
            outnet = " ? "
            continue
        if line[0] in "-01":
            out2lut[outnet].append(line)
    assert " ? " not in out2lut
    assert len(out2ins) == len(out2lut)
    flip = { "0": "1", "1": "0", }
    for outnet in out2lut:
        line0 = out2lut[outnet][0]
        lw = len(line0)
        w = lw - 2
        b = 1 << w
        val = [ flip[line0[-1]] ] * b
        for line in out2lut[outnet]:
            assert len(line) == lw
            assert line[-2] == " "
            newval = line[-1]
            assert newval in "01"
            for ins in expand_pattern(line[:-2]):
                mx = len(ins) - 1
                b = 0
                for i, c in enumerate(ins, start=0):
                    if c == "1": b += 1 << (mx - i)
                # for
                val[b] = newval
            # for
        bits = ''.join(val)
        out2bits[outnet] = bits
    # for

    log(f"{len(out2bits):,d} LUTs specified\n")

    # handle aliases in blif
    alias = uf.uf()
    for outnet, bits in out2bits.items():
        if bits != "01": continue
        ins = out2ins[outnet]
        assert len(ins) == 1
        inp = ins[0]
        alias.add(outnet)
        alias.add(inp)
        alias.union(outnet, inp)
    r2k = defaultdict(set)
    for k in alias.keys():
        r = alias.find(k)
        r2k[r].add(k)
    vouts = set(g_out2lut)
    overlaps = 0
    for r in r2k:
        if vouts & r2k[r]: overlaps += 1
    # for

    # check against what we learned from INIT_VALUE
    matched = 0
    mismatched = 0
    for outnet in set(g_out2lut) & set(out2bits):
        if g_out2lut[outnet] == out2bits[outnet]:
            matched += 1
            continue
        print(f"MISMATCH: two v={g_out2lut[outnet]} blif={out2bits[outnet]} outnet={outnet}")
        mismatched += 1
    # for

    only = {'v': 0, 'blif': 0, }
    for outnet in set(g_out2lut) ^ set(out2bits):
        (which, bits) = (   (   "v", g_out2lut[outnet]) if outnet in g_out2lut else
                            ("blif",  out2bits[outnet]) )
        print(f"MISMATCH: one {which}={bits} outnet={outnet}")
        only[which] += 1
    # for
    print(
f"SUMMARY: matched={matched} mismatched={mismatched} " +
f"only v={only['v']} only blif={only['blif']} " +
f"blif roots={len(r2k)} overlaps={overlaps}" )
# def read_design_blif

def bpath2text(block_path, drop_default=False, drop_idx=False):
    """convert block path (from .net) into dotted text
       returned path omits subscripts if drop_idx=True"""
    text = ""
    # not using block nor name
    for _, _, binst, bmode in block_path:
        if text: text += "."
        # drop # the packer adds (paths don't depend on these)
        if drop_idx:
            binst = re.sub(r'\[\d+\]$', '', binst)
        if not bmode or drop_default and bmode == "default":
            bmode = ""
        else:
            bmode = f"[{bmode}]"
        text += f"{binst}{bmode}"
    # for
    return text
# def bpath2text

# this is the clean way to do it, and should transition to using this
def pb_add_subs(path, dups, curr):
    """add subscripts into a dotted path"""

    # FIXME client should supply i2c
    # convert dups/curr into something more direct
    i2c = {}
    for c, d in zip(curr, dups):
        # don't need limit nor dotted here
        (_, i, _) = d
        i2c[i] = c
    # for

    path = path.split('.')
    for i, p in enumerate(path, start=0):
        if not re.search(r'\[\d+\]', p):
            # ADD
            p = re.sub(r'^(\w+)(.*)$', fr'\1[{i2c.get(i, 0)}]\2', p)
        elif i in i2c:
            # REPLACE
            p = re.sub(r'\[\d+\]', str(i2c[i]), p)
        path[i] = p
    # for
    return '.'.join(path)
# def pb_add_subs

# "sub" in name ==> subscripts are allowed, default to 0
def pbsubxy2path(pin, x, y, z):
    """convert pin=a.b.c:tail.port, x, y to Verilog path"""

    # FIXME CONFIG many magic strings here. probably can't get from XML.

    (bid, _, _) = g_xy2bidwh[(x, y)]    # need bid but not wo/ho
    fname = g_bid2info[bid]['name']
    bname = re.sub(r'_.*$', '', fname)
    path = re.split(r'[:.]', pin)       # split into path components
    # prepend by type and coordinate
    path[0:0] = ["fpga_top", f"grid_{fname}_{x}__{y}_"]
    path = [re.sub(r'\W', '_', p) for p in path]
    # physical mode?
    phys = (    0 if '[physical]' not in pin else
                1 if re.match(r'\w+(\[\d+\])?\[physical\]', pin) else
                2 )

    # change last 2 into last 1: FIXME weird special cases here
    a = path[-2] ; b = re.sub(r'_\d+_$', '_', a) ; c = path[-1]
    if   phys and bname == "clb" and re.match(r'ff_bypass_\d+_$', a) and \
         c.startswith("Q_0"):
        path[-2:] = [ "mem_" + b + c ]
    elif phys and bname == "io"  and re.match(    r'iopad_\d+_$', a) and \
         c.startswith("a2f_o_0"):
        path[-2:] = [ "mem_" + b + c ]
    else:
        path[-2:] = [ "mem_" + a + c ]

    # first gets tile injected including slot number if any
    p2 = re.sub(r'(_\d+_)?(_physical_)?$', r'', path[2])
    path[2] = f"logical_tile_{p2}_mode_{p2}__{z}"
    ph = "physical" if phys == 1 else "default"
    prev = f"logical_tile_{p2}_mode_{ph}__"

    # each path component gets accumulation of previous
    if phys:    # won't this always be true?
        for i in range(3, len(path) - 1):   # don't change 0, 1, 2, -1
            #old = path[i]
            p2 =   re.sub(r'(_\d+_)?(_physical_)?$', r'',   path[i])
            c = "0"
            if     re.search(r'_\d+_(_physical_)?$',        path[i]):
                c = re.sub(r'^.*_(\d+)_(_physical_)?$', r'\1', path[i])
            ph = 'physical' if phys == 2 and 'physical' in path[i] else 'default'
            base    = f"{prev}{p2}_"
            path[i] = f"{base}{c}"
            prev    = f"{base}mode_{ph}__"

            #print(
            #f"COMP i={i}\n" +
            #f"old={old}\n" +
            #f"p2={p2}\n" +
            #f"c={c}\n" +
            #f"ph={ph}\n" +
            #f"base={base}\n" +
            #f"path[i]={path[i]}\n" +
            #f"prev={prev}")
        # for

    path = '.'.join(path)
    # drop final _ on bit subscript
    if re.search(r'_\d+_$', path): path = path[:-1]
    return path
# def pbsubxy2path

def pbxy2path(pin, x, y, dups, curr):
    """convert pin=a.b.c:tail.port, x, y to Verilog path, using indices in dups/curr
       the path in "pin" CANNOT have any subscripts"""

    # FIXME CONFIG may magic strings here. Can't come from XML?

    # FIXME client should supply i2c
    # convert dups/curr into something more direct
    i2c = {}
    for c, d in zip(curr, dups):
        # don't need limit nor dotted here
        (_, i, _) = d
        i2c[i] = c
    # for

    # need bid but not wo/ho
    (bid, _, _) = g_xy2bidwh[(x, y)]
    fname = g_bid2info[bid]['name']
    bname = re.sub(r'_.*$', '', fname)

    # split into path components
    path = re.split(r'[:.]', pin)
    # prepend by type and coordinate
    path[0:0] = ["fpga_top", f"grid_{fname}_{x}__{y}_"]
    path = [re.sub(r'\W', '_', p) for p in path]

    # physical mode?
    if '[physical]' not in pin:
        phys = 0
    elif re.match(r'\w+\[physical\]', pin):
        phys = 1
    else:
        phys = 2

    # FIXME CONFIG how should I make this decision?!
    # special handling for last two elements
    # >>>it looks like you drop the number on outputs<<<
    #if phys: print(f"PATH -2 = {path[-2]}")
    #print(f"INSPECT1: path={path}")
    if   phys and bname == "clb" and re.match(r'ff_bypass_\d+_$', path[-2]) and \
         path[-1].startswith("Q_0"):
        path[-2:] = [ "mem_" + re.sub(r'_\d+_$', '_', path[-2]) + path[-1] ]
    elif phys and bname == "io"  and re.match(    r'iopad_\d+_$', path[-2]) and \
         path[-1].startswith("a2f_o_0"):
        path[-2:] = [ "mem_" + re.sub(r'_\d+_$', '_', path[-2]) + path[-1] ]
    else:
        path[-2:] = [ "mem_" +                        path[-2]  + path[-1] ]
    #print(f"INSPECT2: path={path}")

    # first component gets tile injected
    # this gets the slot number if any
    c = i2c.get(2, 0)
    match phys:
        case 0:
            path[2] = f"logical_tile_{path[2]      }_mode_{path[2]      }__{c}"
        case 1:
            prev = f"logical_tile_{path[2][:-10]}_mode_physical__"
            path[2] = f"logical_tile_{path[2][:-10]}_mode_{path[2][:-10]}__{c}"
        case 2:
            path[2] = f"logical_tile_{path[2]      }_mode_{path[2]      }__{c}"
            prev = re.sub(r'_mode_[a-z]+__0$', '_mode_default__', path[2])

    # each component gets accumulation of previous
    if phys:
        for i in range(3, len(path) - 1):
            c = i2c.get(i, 0)
            # special handling for physical modes
            if phys == 2 and 'physical' in path[i]:
                ph = 'physical'
                path[i] = f"{prev}{path[i][:-10]}_{c}"
                prev = f"{path[i][:-1]}mode_{ph}__"
            else:
                ph = 'default'
                path[i] = f"{prev}{path[i]}_{c}"
                prev = f"{path[i][:-1]}mode_{ph}__"
        # for
    path = '.'.join(path)

    # drop final _ on bit subscript
    if re.search(r'_\d+_$', path): path = path[:-1]

    # track pin --> path
    # only part with side effects
    #if pin not in g_mappings:
    #    g_mappings[pin] = path
    #    g_logger.info(f"TRANSLATE {pin} ==> {path}")

    return path
# def pbxy2path

def phy2set(phypath, phyport, phy_idx, x, y, z, duptail=True):
    """map a setting to a path/w/val triple"""

    better = g_true

    # FIXME temporary
    if not better:
        phypath = re.sub(r'\[\d+\]', r'', phypath)

    splits = phypath.split('.')
    #yhead = splits[0]
    mytail = splits[-1]
    #in = f"{myhead}.{myhead}.{phypath}.{mytail}.{phyport}"
    if better:
        if duptail:
            pin = f"{phypath}.{mytail}.{phyport}"
        else:
            pin = f"{phypath}.{phyport}"
    else:
        pin = f"{phypath}.{mytail}[0].{phyport}"    # hack: added [0]
    if better:
        path = pbsubxy2path(pin, x, y, z)
    else:
        path = pbxy2path(pin, x, y, [], [])     # FIXME this is the one to change @@@
    w = len(phy_idx)
    val = 0
    for i, v in enumerate(phy_idx, start=0):
        if v == "1": val += 1 << i
    #w = int(math.ceil(math.log2(width)))
    if g_false:
        print("@@FOUND ==>\n" + 
        f"\tphypath={phypath} phyport={phyport} phy_idx={phy_idx} ==>\n" +
        f"\tpin={pin} ==>\n" +
        f"\tpath={path} ==>\n" +
        f"\tw={w} val={val}")

    if g_false: print(
f"phy2set: phypath={phypath} phyport={phyport} phy_idx={phy_idx} ==> w={w} val={val}\n" +
f"pin={pin}\n" +
f"path={path}")

    return (path, w, val)
# def phy2set

def val2bit(n, val):
    """generate n bits from val"""
    return ''.join("1" if val & (1 << b) else "0" for b in range(n))
# def val2bit

g_announced = {}

# codesrc values, lower # is higher precedence
# 0: from .route
#    read_route
# 1: connection from .net, perhaps overridden by global
#    read_pack --> walk_block+ --> proc_conns
# 2: connection from .net, which went through a mode-dependent MAPPING
#    read_pack --> walk_block+ --> proc_conns
# 3: connection from a mode-dependent SETTING
#    read_pack --> walk_block+ --> check_for_settings

def announce(path, value, codesrc):
    """announce mux programming in write_set_muxes file"""
    if path in g_announced and g_announced[path][0] != value:
        # ignore if lower precedence
        ignore = codesrc > g_announced[path][1]
        #log(f"\tDELTA {g_announced[path]} ==> {(value, codesrc)} " +
        # f"ignore={ignore} at PATH {path}\n")
        if ignore:
            return
    if path not in g_announced:
        path2 = path
        if g_frag2grouped:
            ff = path2.split('.')
            ff[1] = g_frag2grouped.get(ff[1], ff[1])
            assert "fpga_top" == ff.pop(0)
            path2 = '.'.join(ff)
        g_write_set_muxes.write(f"{path2} {value} {codesrc}\n")
    g_announced[path] = (value, codesrc)
# def announce

def setfield(path, w, val, codesrc):
    """set a field of config bits"""

    announce(path, (bin(val)[2:][::-1] + "0" * 32)[:w], codesrc)

    #print(f"@@INTERNAL ROUTING AT:\n" +
    #f"name={name} x={x} y={y} z={z} inter={inter_name}\n" +
    #f"idx={idx} val={val} width={width} w={w}\npin={pin}\n" +
    #f"path={path}\ndotted_w_idx={dotted_w_idx}")
    for b in range(w):
        fullpath = f"{path}.mem_out[{b}]"
        bit = "1" if val & (1 << b) else "0"
        # FIXME
        global g_skipped, g_completed
        if fullpath not in g_config:
            g_skipped += 1
            # TODO elevate this is serious
            print(
f"\n@@FULLPATH NOT FOUND w={w} val={val}\n" +
f"fullpath={fullpath}\n")
            return 0    # failure
        if not b: g_completed += 1
        assert fullpath in g_config, f"bit={bit} fullpath={fullpath}"
        #print(f"SET {fullpath} {bit}")
        g_config[fullpath] = bit
    # for

    return 1    # success
# def setfield

g_completed = 0
g_skipped = 0
g_mappings_ok = 0
g_mappings_bad = 0
g_clock_complained = set()
g_pin2tree = {}                 # portname --> tree

# this is to support a weird OpenFPGA behavior
g_pack_weird_globals = defaultdict(lambda: defaultdict(set))

# codesrc will be 1 when called while reading .net.
# it will be 0 when called while reading .route.
def make_conn(  which, block_path, name, tail, x, y, z,
                conn, i, codesrc,
                dotted_w_idx, dottedpathxx, pbt, pin, inter_name):
    """make a connection. may be later re-called to implement routing"""
    idx = -1
    conntag = ""
    connhash = {}
    msg = ""
    dottedpath = dottedpathxx
    if dottedpathxx in g_inter_info and inter_name in g_inter_info[dottedpathxx]:
        (conntag, connhash, width) = g_inter_info[dottedpath][inter_name]
        # nothing to set in a "direct"
        if conntag == "direct": return
        g = re.match(r'(\w+)((?:\[\d+\])?)$', pbt)
        assert g
        tries = [ g.group(1), pbt, g.group(1) + "[0]", ]
        for tr in tries:
            pp = tr + "." + pin
            if pp in connhash:
                idx = connhash[pp]
                break
        if idx < 0:
            # TODO elevate: this is serious
            msg = (
f"@@INPUT PIN NOT FOUND port={name} conn={conn} pbt={pbt} pin={pin} inter={inter_name} " +
f"block_path={block_path}" )
    else:
        # TODO elevate: this is serious
        msg = (
f"@@PATH/INTER NOT FOUND port={name} conn={conn} pbt={pbt} pin={pin} inter={inter_name} " +
f"block_path={block_path}" )

    if msg:
        print(msg)
    if idx < 0 or msg: return

    # try to generate path to config bits
    key = f"{dotted_w_idx}:{name}[{i}]:{idx}"
    key = re.sub(r'^(\w+)\[\d+\]', r'\1[0]', key)   # zero out topmost idx
    if key not in g_anno_mappings:

        # GLOBAL CLOCK OVERRIDE
        # FIXME we probably don't want to require depth 2
        if which == "clock" and len(block_path) == 2:
            portname = f"{pbt}.{pin}"
            if portname in g_pin2tree:
                # replace previous "idx" with which clock tree
                idx = g_pin2tree[portname]
            elif " GOOF " not in g_clock_complained:
                log(" - WARNING: Failed to set clock mux\n")
                g_clock_complained.add(" GOOF ")

        #print(f"ANNO MAP FAIL BACK {key}")

        # check for connection to equivalent top-level port
        if which == "input" and codesrc == 1:
            g = re.match(r'(.+)\[(\d+)\]$', pin)
            assert g
            blkeqport = f"{pbt}.{g.group(1)}"
            #print(f"CHECKING FOR {blkeqport}")
            if (pbt, g.group(1)) in g_blkeqport2width:
                #print(f"\nFOUND EQUIVALENT: pbt={pbt} pin={pin}\n")
                netname = g_portb2net[(g.group(1), int(g.group(2)))]

                # x,y are block coordinates: convert to pin coordinates
                (bid, _, _) = g_xy2bidwh[(x, y)]
                try:
                    (ptc, _) = g_bid2info[bid]['n2info'][f"{pbt}.{pin}"]
                except:
                    print(
f"\nwhich={which}\nblock_path={block_path}\nname={name}\ntail={tail}\nx={x}\ny={y}\nz={z}" +
f"\nconn={conn}\ni={i}\ncodesrc={codesrc}\ndotted_w_idx={dotted_w_idx}" +
f"\ndottedpathxx={dottedpathxx}\npbt={pbt}\npin={pin}\ninter_name={inter_name}\nbid={bid}")
                    raise
                (wo, ho) = g_routablepins[(bid, ptc)]

                mckey = (netname, x + wo, y + ho, blkeqport)
                #print(f"SAVING mckey={mckey}")
                g_save_for_reroute[mckey].append( (
                    which, "block_path", name, tail, x, y, z,
                    conn, i, codesrc,
                    dotted_w_idx, dottedpathxx, pbt, pin, inter_name) )
                if g_dump_ppfu:
                    print(
f"pb_pin_fixup2: SAVE netname={netname} x={x} y={y} blkeqport={blkeqport}\n" +
f"\tconn={conn} i={i} pbt={pbt} pin={pin} inter_name={inter_name}")

        pin = f"{dottedpath}.{tail}.{name}[{i}]"    # extra (dupped) tail
        path = pbxy2path(pin, x, y, [], [])
        val = idx2sel(int(width), int(idx))
        w = int(math.ceil(math.log2(width)))
        # normal non-mapped/non-forced internal mux
        #print(f"SETFIELD val={val} w={w} path={path}")
        setfield(path, w, val, codesrc)
        return

    #print(f"ANNO MAP PROCEED {key}")
    for (phypath, phyport, phy_idx) in g_anno_mappings[key]:
        # FIXME CONFIG check whether input/output in general?
        dt = not phyport.startswith("outpad")
        # FIXME CONFIG SUPER HACK
        if 'ff_bypass' in phypath and phyport.startswith("D["): dt = False
        #print(f"phy2set2 phyport={phyport} dt={dt}")
        (path, w, val) = phy2set(phypath, phyport, phy_idx, x, y, z, duptail=dt)
        idx = phy_idx   # for debug msgs
        global g_mappings_ok, g_mappings_bad
        if setfield(path, w, val, 2):
            g_mappings_ok += 1
            #print("ANNO MAP SUCCESS")
            #print("SUCCESS\n" +
            #    f"\tkey={key}\n" +
            #    f"\tphypath={phypath}\n" +
            #    f"\tphyport={phyport} phy_idx={phy_idx}\n" +
            #    f"\tpath={path}\n" +
            #    f"\tw={w} val={val}")
        else:
            g_mappings_bad += 1
            print("ANNO MAP FAILURE")
    # for
# def make_conn

# called from inside walk_block (reading .net packer output)
def proc_conns(block_path, port, which):
    """show connections to/from each bit of port bus"""

    conns = port.elem.text.split()
    name = port.elem.attrib.get('name')
    #inst = port.elem.attrib.get('instance')
    tail = block_path[-1][2]                # e.g. clb_lr[0]
    # placement location
    blname = block_path[0][1]
    # unused: z/slot, layer
    (x, y, z, _) = g_blname2loc[blname]     # int*2

    #print(f"@@Entering proc_conns: name={name} inst={inst} " +
    #f"conns={conns} block_path={block_path}")

    # extent of path that matters depends on port direction
    if which == "output":
        # dottedpathm0
        dottedpathxx = bpath2text(block_path     , drop_default=True , drop_idx=True)
    else:
        # dottedpathm1
        dottedpathxx = bpath2text(block_path[:-1], drop_default=True , drop_idx=True)

    dotted_w_idx = bpath2text(block_path     , drop_default=True , drop_idx=False)
    # force the garbage index VPR puts on the top block to zero
    dotted_w_idx = re.sub(r'^(\w+)\[\d+\]', r'\1[0]', dotted_w_idx)

    for i, conn in enumerate(conns, start=0):

        # ignore no connection
        if conn == "open": continue

        # ignore driven from primary
        if '->' not in conn:

            # save block pin nets
            if which == "input" and len(block_path) == 1:
                g = re.match(r'(.*)\[\d+\]$', tail)
                assert g
                if (g.group(1), name) in g_blkeqport2width:
                    g_portb2net[(name, i)] = conn
                    if g_dump_ppfu:
                        print(f"pb_pin_fixup3: for (port)name={name} i={i} saved conn={conn}")

            if len(block_path) == 1:
                # check for top-level clock connection
                if which == "clock":
                    if conn in g_net2tree:
                        blocktype = re.sub(r'\[\d+\]$', r'', tail)
                        portname = f"{blocktype}.{name}[{i}]"
                        tree = g_net2tree[conn]
                        g_pin2tree[portname] = tree
                    elif conn not in g_clock_complained:
                        log(f" - WARNING: Unknown clock net: {conn}\n")
                        g_clock_complained.add(conn)

                # check for top-level global connection
                # we do this only to emulate an OpenFPGA bug
                if which == "input":
                    if conn in g_net2tree:
                        blocktype = re.sub(r'\[\d+\]$', r'', tail)
                        portname = f"{blocktype}.{name}[{i}]"
                        # get ptc
                        (bid, _, _) = g_xy2bidwh[(x, y)]
                        (ptc, _) = g_bid2info[bid]['n2info'][portname] 
                        g_pack_weird_globals[conn][(x, y)].add(ptc)

            continue

        # ignore pb_type-output and direct:pb_type
        if re.match(r'->.*[-:]', conn): continue

        # pull apart "pb_type.port->crossbar"
        g = re.match(r'(\w+(?:\[\d+\])?)\.(\w+\[\d+\])->(\S+)$', conn)
        assert g, f"name={name} conn={conn}"
        (pbt, pin, inter_name) = g.groups()
        if ':' in inter_name:
            continue
        make_conn(  which, block_path, name, tail, x, y, z,
                    conn, i, 1,
                    dotted_w_idx, dottedpathxx, pbt, pin, inter_name)
    # for i, conn
# def proc_conns

g_rotate_dump = False
g_lut_not_found = 0
g_lut5_open = set()
g_lut_debug = defaultdict(dict) # (x, y, flen, off) --> (bitstring, port_rotation_map_text)

def proc_rotate(block_path, port_rotation_map_text, weird):
    """use rotation map and LUT mask, or input field and a constant (wireLUT),
       to set LUT contents"""

    # instance and tile location
    inst = block_path[0][1]
    assert inst in g_blname2loc
    (x, y, _, _) = g_blname2loc[inst]   # int*2

    # flen/unit/bits position depend on wire LUT or normal LUT
    rcase = -1
    fb = -3 if weird == "wirelut" else -4

    # flen, unit/name, lutn, mode
    flen = block_path[fb+0][2]
    unitname = block_path[fb+1][1]    # will check for "open" ble5
    unit = block_path[fb+1][2]
    lutn = block_path[fb+2][2]
    mode = block_path[fb+2][3]

    unfound = False
    if weird == "wirelut":
        assert mode == "wire"
        # get bits
        onet = "<wire_LUT_internal>"
        bits = "10" # An MSB--LSB expansion like from INIT_VALUE. Not backwards.
        rcase = 0
    else:
        # FIXME CONFIG next 2 lines
        assert mode in ('lut5', 'lut6', )
        assert ('6' in mode) == ('6' in unit)
        # get bits
        if weird in ("0", "1"):
            onet = weird
            bits = weird
            rcase = 1
        else:
            onet = block_path[-1][1]
            if onet in g_out2lut:
                bits = g_out2lut[onet]
                g_out2lut_used[onet] = 1
                rcase = 2
            else:
                bits = "10"
                log(
f" - WARNING: No LUT INIT_VALUE found for {onet}, setting LUT to wire-LUT\n")
                global g_lut_not_found
                g_lut_not_found += 1
                rcase = 3
                unfound = True

    assert re.match(r'fle\[\d+\]$', flen)
    flen = int(re.sub(r'[^\d]', r'', flen))

    # FIXME CONFIG need to generate this table from XML
    lutslice = {
        'fast6[0]'  : (1,  0, 64),
        'ble6[0]'   : (1,  0, 64),
        'ble5[0]'   : (1,  0, 32),  # (1,  0), (2, 0),
        'ble5[1]'   : (1, 32, 32),  # (1, 32), (2, 1),
        'adder[0]'  : (1,  0, 32),  # UNKNOWN
    }

    if unit != "adder[0]":
        # normal behavior for almost all LUTs
        rotlist = [-1 if x == "open" else int(x) for x in port_rotation_map_text.split()]
        loud = False
        (mult, off, size) = lutslice[unit]

        # record LUT5s claiming to be don't-care
        if unitname == "open" and unit.startswith("ble5["):
            g_lut5_open.add((x, y, flen, off))

    else:
        rotlist = [-1 if x == "open" else int(x) for x in port_rotation_map_text.split()]
        loud = True
        loud = False
        mult = 1
        size = 32
        # no action on upper lut; lower lut will be extended to LUT6 
        # FIXME CONFIG
        if lutn == "lut5[0]":
            if g_false:
                # prepare a k=5 wire-LUT
                rotlist = [ -1 if x == "open" or x != "0" else 0
                            for x in port_rotation_map_text.split()]
                # extend to k=6 LUT with upper half 0
                if len(rotlist) == 5:
                    rotlist.append(1)
                bits = "1000"
            off = 0
        elif lutn == "lut5[1]":
            off = 32
        else:
            assert 0

    # save for bad LUT debugging
    g_lut_debug[(x, y, flen)][off] = (bits, port_rotation_map_text, size)

    nrl = len(rotlist)
    rlmax = max(rotlist)

    if g_proc_rotate_dump: print(
f"port_rotation_map_text={port_rotation_map_text} weird={weird} inst={inst} xy=({x},{y}) " +
f"rcase={rcase} flen={flen} unitname={unitname} unit={unit} lutn={lutn} mode={mode} " +
f"onet={onet} bits={bits} rotlist={rotlist} loud={loud} mult={mult} off={off} size={size} " +
f"nrl={nrl} rlmax={rlmax}")
    assert unit in lutslice, f"unit={unit} block_path={block_path}"

    data = []
    #rotlist = rotlist[::-1]     # TRY 1
    oor = False
    for v in range(1 << nrl):
        i = 0
        for b, m in enumerate(rotlist, start=0):
            if m < 0: continue
            if v & (1 << b): i += 1 << (rlmax - m)  # rlmax is TRY 2
        # for
        if i < len(bits):
            data.append(int(bits[i]))
        else:
            data.append(0)
            oor = True
    # for
    path = g_xys2lutpath[(x, y, flen)]
    if oor:
        unfound = " inferred wire-LUT" if unfound else ""
        log(f" - OOR: {onet}{unfound}\n")

    if g_rotate_dump:
        nbits = len(bits)
        ndata = len(data)
        bp = "\n\t\t".join([f"{b}" for b in block_path])
        print(
f"**LUT** x={x} y={y} f={flen} mode={mode} unit={unit} rcase={rcase} rotlist={rotlist}\n" +
f"\tinst={inst}\n" +
f"\tonet={onet}\n" +
f"\tbits={bits}#{nbits}\n" +
f"\tdata={data}#{ndata}\n" +
f"\tblock_path=\n\t\t{bp}")

    for i, d in enumerate(data, start=0):
        b = i * mult + off
        newpath = path + f".mem_out[{b}]"
        if loud: print(f"SET {newpath} {d}")
        g_config[newpath] = str(d)
    # for
# def proc_rotate

g_dump_replicas = False

def replicas(fname, dotted, z, keep_idx=False):
    """produce the radices (limits) deducible from the dotted path"""
    #if 'fle' in dotted:
    #    print(f"FLE seen: {dotted}")
    #np = g_dotted2npb.get(dotted, 1)
    #if np != 1:
    #    print(f"Saw dotted={dotted} --> num_pb={np}")

    dotted_no_idx = re.sub(r'\[\d+\]', r'', dotted)
    repeats = []
    curr = []
    while 1:
        np = g_dotted2npb.get(dotted_no_idx, 1)
        if g_dump_replicas:
            print(f"replicas: {dotted_no_idx} ==> #{np}")
        if np != 1 and not (keep_idx and re.search(r'\[\d+\]$', dotted)):
            g = re.search(r'\[(\d+)]$', dotted)
            idx = 0
            if g:
                idx = int(g.group(1))

            p = len(dotted_no_idx.split('.'))
            repeats.append((np, p + 1, f"{fname}.{dotted_no_idx}"))

            curr.append(idx)
        if dotted_no_idx[-1] == ']':
            assert dotted[-1] == ']'
            dotted        = dotted       [:dotted       .rindex('[')]
            dotted_no_idx = dotted_no_idx[:dotted_no_idx.rindex('[')]
            continue
        if '.' not in dotted: break
        dotted        = dotted       [:dotted       .rindex('.')]
        dotted_no_idx = dotted_no_idx[:dotted_no_idx.rindex('.')]

    c = g_capacity.get(fname, 1)
    if c != 1:
        repeats.append((c, 2, fname))
        curr.append(z)

    #if repeats:
    #    print(f"repeats = {repeats}")

    return [repeats, curr]
# def replicas

def nextrep(curr, dups):
    """increment curr respecting radices in dups"""
    if not dups: return False
    for i, d in enumerate(dups, start=0):
        curr[i] = curr[i] + 1
        if curr[i] < d[0]: return True
        curr[i] = 0
    return any(curr)
# def nextrep

def fill_cm_bits(name, cmname, modebits, x, y, z, logname, idx_offset):
    """fill mode bits inside a circuit model
       name may have subscripts or not: always replaced"""
    # if init --> logname="", do all bits over all legal subscripts
    # otherwise logname is logical location, do bits at one legal set of subscripts
    (bid, _, _) = g_xy2bidwh[(x, y)]
    fname = g_bid2info[bid]['name']
    #print(f"name={name} cmname={cmname} modebits={modebits}")
    nword = 0
    nports = 0
    for port in g_circ2port2modeinfo[cmname]:
        # not used: ptype
        (size, _, circ) = g_circ2port2modeinfo[cmname][port]
        #print(f"port={port} size={size} ptype={ptype} circ={circ}")
        assert len(modebits) == size, \
            f"size={size} len(modebits)={len(modebits)} modebits={modebits}"
        nports += 1
        assert nports == 1

        (dups, curr) = replicas(fname, name, z)
        if logname:
            extra = 0 if idx_offset is None else 1
            (dups2, curr2) = replicas(fname, logname, z)
            assert len(dups) == len(curr) == len(dups2) + extra == len(curr2) + extra, \
f"len(dups) = {len(dups)}, len(curr) = {len(curr)}, " + \
f"extra = {extra}, " + \
f"len(dups2) = {len(dups2)}, len(curr2) = {len(curr2)}\n" + \
f"\ndups = {dups}\ncurr = {curr}\ndups2 = {dups2}\ncurr2 = {curr2}\n"
            curr = curr2
            if extra:
                curr.insert(0, idx_offset)
        else:
            curr = [0] * len(dups)

        while 1:
            # break it up and drop subscripts if any
            pathlist = [re.sub(r'\[\d+\]$', r'', x) for x in re.split(r'[.]', name)]
            # add tails (see below)
            pathlist = list(pathlist) + [ pathlist[-1], pathlist[-1]  ]

            # create correct path
            textpath = '.'.join(pathlist)
            path0 = pbxy2path(textpath, x, y, dups, curr)
            path = re.sub(r'[.]mem_(\w+)\1$', fr'.{cmname}_{circ}_mem', path0)

            #if dups:
            #    print( f"name={name}\npathlist={pathlist}\n" +
            #           f"textpath={textpath}\npath={path}\n" +
            #           f"dups={dups}\ncurr={curr}")

            #print(f"MODE {modebits} ==> {path}")
            for b in range(size):
                newpath = path + f".mem_out[{b}]"
                if not logname and newpath in g_config:
                    log(f"DUP3: {newpath}\n")
                else:
                    nword += 1
                #print(f"Setting {newpath} to {modebits[b]}")
                #if logname:
                #    print(f"SET DESIGN MODE {newpath} <== {modebits[b]}")
                if logname:
                    assert newpath in g_config
                #print(f"SET {newpath} {modebits[b]}")
                g_config[newpath] = modebits[b]
            # for b

            if logname: break
            if not nextrep(curr, dups): break
        # while
    # for port
    return nword
# def fill_cm_bits

g_settings_ok = 0
g_settings_bad = 0

def check_for_settings(block_path):
    """check for logical mode --> physical mode settings"""

    dotted_w_idx = bpath2text(block_path     , drop_default=True , drop_idx=False)
    # force the garbage index VPR puts on the top block to zero
    dotted_w_idx = re.sub(r'^(\w+)\[\d+\]', r'\1[0]', dotted_w_idx)

    # check if this path triggers some settings
    if dotted_w_idx not in g_anno_settings:
        #print(f"ANNO SET FAIL BACK {dotted_w_idx}")
        return
    #print(f"ANNO SET PROCEED {dotted_w_idx}")

    # placement location
    blname = block_path[0][1]
    # unused: z/slot, layer
    (x, y, z, _) = g_blname2loc[blname] 

    # process each setting
    for (phypath, phyport, phy_idx) in g_anno_settings[dotted_w_idx]:
        # FIXME check if it's an output
        dt = phyport.startswith("Q")
        #print("phy2set1")
        (path, w, val) = phy2set(phypath, phyport, phy_idx, x, y, z, duptail=dt)
        global g_settings_ok, g_settings_bad
        if setfield(path, w, val, 3):
            #print(f"ANNO SET SUCCESS {dotted_w_idx}")
            g_settings_ok += 1
        else:
            print(f"ANNO SET FAILURE {dotted_w_idx}")
            g_settings_bad += 1
    # for
# def check_for_settings

g_wire_luts = 0
g_mode_bits = 0

g_dump_walk_block_basic = True
g_dump_walk_block_notes = False

def walk_block(block, depth, block_path):
    """ recursive routine to walk through block """
    indent = " " * depth
    g_logger.info(f"{indent}Found {showelt(block)}")
    name = block.elem.attrib.get("name")
    instance = block.elem.attrib.get("instance")
    mode = block.elem.attrib.get("mode")
    ptnm = block.elem.attrib.get("pb_type_num_modes")   # only above/on wire-LUTs
    block_path.append((block, name, instance, mode))

    ## set IO slot from placement
    #blname = block_path[0][1]
    #global g_slot
    ## x, y, z/slot, layer
    #(, , g_slot, _) = g_blname2loc[blname]

    # check for physical settings triggered by this logical path
    check_for_settings(block_path)

    indent += " "

    # BLOCK WIRE LUT CHECK
    # we should also check that this is a LUT
    # (but VPR may use mode='wire' only on LUTs)
    wirelut_from_block_mode = mode == "wire"
    assert wirelut_from_block_mode == (ptnm == "2")

    param = {}
    for parameterss in block.parameterss:
        g_logger.info(f"{indent}Found {showelt(parameterss)}")
        indent += " "
        for parameter in parameterss.parameters:
            g_logger.info(f"{indent}Found {showelt(parameter)}")
            pname = parameter.elem.attrib.get("name")
            text = ' '.join((parameter.elem.text or '').split())
            block_inst = block_path[0][1]
            (x, y, z, _) = g_blname2loc[block_inst] 
            param[pname] = text
        indent = indent[:-1]

    # process modes on this block
    if mode not in ('default', 'wire'):
        dotpath_no_idx      = bpath2text(block_path, drop_idx=True , drop_default=True)
        if g_dump_walk_block_notes:
            print(f"About to process mode in {dotpath_no_idx}")
        if dotpath_no_idx in g_physmapping:
            (physpbname, modebits1, idx_offset) = g_physmapping[dotpath_no_idx]
            if g_dump_walk_block_basic:
                if g_false: print(
f" Found physical mapping {dotpath_no_idx} ==> {physpbname} ({modebits1}) {idx_offset}")
            if physpbname in g_name2circbits:
                # don't use these modebits, which are the default values
                (cmname, modebits2) = g_name2circbits[physpbname] 
                assert len(modebits1) == len(modebits2), \
f"modebits1={modebits1} modebits2={modebits2}"
                block_inst = block_path[0][1]
                (x, y, z, _) = g_blname2loc[block_inst] 
                global g_mode_bits
                dotpath_yy_idx  = bpath2text(block_path, drop_idx=False, drop_default=True)
                if g_false: print("\n" +
f"dotpath_no_idx={dotpath_no_idx}\n" +
f"physpbname={physpbname}\n" +
f"cmname={cmname}\n" +
f"block_inst={block_inst}\n" +
f"x={x} y={y} z={z}\n" +
f"dotpath_yy_idx={dotpath_yy_idx}\n" +
f"idx_offset={idx_offset}")
                # FIXME CONFIG get MODE_BITS from XML?
                if 'MODE_BITS' in param:
                    modebits3 = param['MODE_BITS']
                    # HACK: BRAM mode is incomplete, finish it from default
                    if len(modebits3) < len(modebits1):
                        modebits3 = modebits3 + modebits1[len(modebits3):]
                else:
                    modebits3 = modebits1
                assert len(modebits1) == len(modebits3), \
f"modebits1={modebits1} modebits2={modebits2} modebits3={modebits3}"
                n = fill_cm_bits(   physpbname, cmname, modebits3,
                                    x, y, z, dotpath_yy_idx, idx_offset)
                if g_false: print(
f"  Found circuit model {cmname} ({x},{y}) {physpbname} <-- ({modebits3}) {n} bits")
                g_mode_bits += n

    for attributess in block.attributess:
        g_logger.info(f"{indent}Found {showelt(attributess)}")
        indent += " "
        for attributes in attributess.attributes:
            for attribute in attributes:
                g_logger.info(f"{indent}Found {showelt(attribute)}")
        indent = indent[:-1]

    # clear out level 1 clock info
    if len(block_path) == 1:
        g_pin2tree.clear()

    constant_from_output = None
    # Are these VPR-preferred names? Yosys?
    word2bit = { '$false': "0", '$true': "1", }
    for outputss in block.outputss:
        g_logger.info(f"{indent}Found {showelt(outputss)}")
        indent += " "
        for port in outputss.ports:
            g_logger.info(f"{indent}Found {showelt(port)}")
            proc_conns(block_path, port, "output")
            if port.elem.attrib.get("name") != "out": continue
            sense = port.elem.text
            if sense not in word2bit: continue
            assert not wirelut_from_block_mode
            #print(f"FOUND CONSTANT LUT {sense}")
            constant_from_output = word2bit[sense]
        indent = indent[:-1]

    for inputss in block.inputss:
        g_logger.info(f"{indent}Found {showelt(inputss)}")
        indent += " "
        for port in inputss.ports:
            g_logger.info(f"{indent}Found {showelt(port)}")
            proc_conns(block_path, port, "input")
            assert constant_from_output is None or not wirelut_from_block_mode
            if constant_from_output is None and not wirelut_from_block_mode:
                continue
            name = port.elem.attrib.get("name")
            # FIXME CONFIG. Should this be hardwired?
            if name != "in": continue

            prmt = [x if x == "open" else "0" for x in port.text.split()]
            assert (len([x for x in prmt if x == "0"]) == 1) == wirelut_from_block_mode
            prmt = ' '.join(prmt)
            if g_false: print(
f"FOUND WEIRD: prmt={prmt} wirelut_from_block_mode={wirelut_from_block_mode} " +
f"constant_from_output={constant_from_output}")

            if wirelut_from_block_mode:
                global g_wire_luts
                g_wire_luts += 1
                weird = "wirelut"
            else:
                weird = constant_from_output
            if g_false:
                print(f"WEIRD LUT: prmt={prmt}")
                for i, b in enumerate(block_path, start=0):
                    print(f"\t{i}:{b}")
            proc_rotate(block_path, prmt, weird)
        for port_rotation_map in inputss.port_rotation_maps:
            assert not wirelut_from_block_mode
            g_logger.info(f"{indent}Found {showelt(port_rotation_map)}")
            proc_rotate(block_path, port_rotation_map.text, "")
        # for
        indent = indent[:-1]

    for clockss in block.clockss:
        g_logger.info(f"{indent}Found {showelt(clockss)}")
        indent += " "
        for port in clockss.ports:
            g_logger.info(f"{indent}Found {showelt(port)}")
            proc_conns(block_path, port, "clock")
        indent = indent[:-1]

    for block2 in block.blocks:
        walk_block(block2, len(indent), block_path)
    indent = indent[:-1]
    block_path.pop()
# def walk_block

g_xy2iotile = {}

def proc_pin(pryfile, net2pry, pinfile, portname, direction):
    """write portname mapping to PinMapping.xml"""

    # figure out IOTile numbering
    if not g_xy2iotile:
        # pylint: disable=unbalanced-tuple-unpacking
        (xl, yl, xh, yh) = g_grid_bbox
        iotiles = []
        # up the left (include corners)
        for y in range(yl, yh+1):
            pair = (xl, y)
            #print(f"{len(iotiles)} <=> {pair}")
            iotiles.append(pair)
        # across the top (exclude corners)
        for x in range(xl+1, xh):
            pair = (x, yh)
            #print(f"{len(iotiles)} <=> {pair}")
            iotiles.append(pair)
        # down the right (include corners)
        for y in range(-yh, yl-1):
            pair = (xh, -y)
            #print(f"{len(iotiles)} <=> {pair}")
            iotiles.append(pair)
        # back across the bottom (exclude corners)
        for x in range(-xh+1, -xl):
            pair = (-x, yl)
            #print(f"{len(iotiles)} <=> {pair}")
            iotiles.append(pair)
        # save the ordering
        for i, pair in enumerate(iotiles, start=0):
            assert pair not in g_xy2iotile, f"pair={pair}"
            (x, y) = pair
            assert xl <= x <= xh and yl <= y <= yh
            g_xy2iotile[pair] = i
    # if

    # FIXME CONFIG are these hardwired?
    dir2base = {    'input':    "gfpga_pad_QL_PREIO_A2F",
                    'output':   "gfpga_pad_QL_PREIO_F2A",
    }
    base = dir2base[direction]

    # figure out bus index
    netname = portname
    if direction == "output":
        assert portname[:4] == "out:"
        netname = portname[4:]
    if portname not in g_blname2loc:
        log(f" - No pin mapping for {portname}\n")
        return
    (x, y, z, _) = g_blname2loc[portname] 
    b = g_xy2iotile[(x, y)] * 72 + z
    #b = 82

    pinfile.write(f'\t<io name="{base}[{b}:{b}]" net="{netname}" dir="{direction}"/>\n')
    netname = net2pry.get(netname, netname)
    pryfile.write(f'\t<io name="{base}[{b}:{b}]" net="{netname}" dir="{direction}"/>\n')
# def proc_pin

def read_pack_xml(xmlfile, eblifname, pryname, pinname):
    """read in net_xml and crawl the info"""
    indent = ""

    # read file specified
    log(f"{elapsed()} Reading {xmlfile}\n")
    g_vpr_xml = pack_dm.PackDataModel(xmlfile, validate=False)

    # walk through a typical pack XML
    indent += " "
    for block in g_vpr_xml.blocks:
        g_logger.info(f"{indent}Found {showelt(block)}")
        block_path = []
        walk_block(block, len(indent), block_path)
    indent = indent[:-1]
    assert not indent

    log(f" - {g_wire_luts:,d} specified wire-LUT{plural(g_wire_luts)} found\n")

    if g_lut_not_found:
        log(f" - {g_lut_not_found:,d} LUT{plural(g_lut_not_found)} had no value specified\n")

    unused = set(g_out2lut) - set(g_out2lut_used)
    nunused = len(unused)
    if nunused:
        if g_true:
            log(f" - {nunused:,d} LUT value{plural(nunused)} not used\n")
        else:
            bits = defaultdict(int)
            for lut in unused:
                bits[g_out2lut[lut]] += 1
            for pat in sorted(bits, key=lambda b: (len(b), b)):
                nunused = bits[pat]
                k = int(math.ceil(math.log2(len(pat))))
                log(f" - {nunused:,d} LUT{k} {pat} value{plural(nunused)} not used\n")
    for lut in sorted(unused, key=lambda o: (len(g_out2lut[o]), g_out2lut[o], o)):
        print(f"LUT value {g_out2lut[lut]} for {lut} not used")

    log(f" - {g_completed:,d} intra-tile mux{plural(g_completed, 'es')} configured\n")

    log(f" - {g_settings_ok:,d} mux setting{plural(g_settings_ok)} by mode")
    if g_settings_bad: log(f", plus {g_settings_bad:,d} failed")
    log("\n")

    log(f" - {g_mappings_ok:,d} mux mapping{plural(g_mappings_ok)} by mode")
    if g_mappings_bad: log(f", plus {g_mappings_bad:,d} failed")
    log("\n")

    if g_skipped:
        log(
f" - {g_skipped:,d} intra-tile mux{plural(g_skipped, 'es')} could not be configured\n")

    # don't need to do anything more if generating neither XML mapping file
    if not pryname and not pinname: return

    net2pry = {}
    primaries = defaultdict(int)
    if eblifname:

        # Step 1: use UNION-FIND structure to track connections
        connections = uf.uf()
        notable_buffs = { 'O_BUF', 'I_BUF', 'CLK_BUF', }
        with open(eblifname, encoding='ascii') as ifile:
            log(f"{elapsed()} Reading {eblifname}\n")
            saveff = []
            for line in ifile:
                ff = line.split()
                # this finishes processing of a previous ".names in out"
                # and tracks it if it is a buffer ("1 1" table)
                if saveff and len(ff) == 2 and ff[0] == "1" and ff[1] == "1":
                    connections.add(saveff[1])
                    connections.add(saveff[2])
                    connections.union(saveff[1], saveff[2])
                saveff = []
                if not ff:
                    continue
                # handle statements
                match ff[0]:
                    case '.inputs':
                        for f in ff[1:]:
                            primaries[f] |= 2
                    case '.outputs':
                        for f in ff[1:]:
                            primaries[f] |= 1
                    case '.subckt':
                        assert len(ff) > 1
                        if ff[1] not in notable_buffs: continue
                        port2net = dict([f.split('=') for f in ff[2:]])
                        connections.add(port2net['I'])
                        connections.add(port2net['O'])
                        connections.union(port2net['I'], port2net['O'])
                    case '.names':
                        # if only one input, save to recog buffer later
                        if len(ff) == 3: saveff = ff
                    case _:
                        pass
                # match
            # for
        # with

        # Step 2: convert U-F to a dictionary
        # first, for each node k add to a set keyed by its root r
        r2k = defaultdict(set)
        for k in connections.keys():
            r = connections.find(k)   # find the node's root (UNION-FIND)
            r2k[r].add(k)
        # for
        # second, for each set (known by its root), determine primaries
        # if exactly one, then map each set element to the primary
        for r in r2k:
            prys = [k for k in r2k[r] if k in primaries]
            if len(prys) != 1: continue
            pry = prys[0]
            for k in r2k[r]:
                net2pry[k] = pry
            # for
        # for
    # if

    # write PrimaryPinMapping.xml / PinMapping.xml
    if not pryname: pryname = "/dev/null"
    if not pinname: pinname = "/dev/null"
    with    open(pryname, "w", encoding='ascii') as pryfile, \
            open(pinname, "w", encoding='ascii') as pinfile:
        if pryname != "/dev/null": log(f"{elapsed()} Writing {pryname}\n")
        if pinname != "/dev/null": log(f"{elapsed()} Writing {pinname}\n")
        pryfile.write("<!--\n\t- I/O mapping\n\t- Version: \n-->\n\n")
        pryfile.write("<io_mapping>\n")
        pinfile.write("<!--\n\t- I/O mapping\n\t- Version: \n-->\n\n")
        pinfile.write("<io_mapping>\n")
        clockset = set()
        for clockss in g_vpr_xml.clockss:
            if clockss.text is None: continue
            for clock in clockss.text.split():
                proc_pin(pryfile, net2pry, pinfile, clock, "input")
                clockset.add(clock)
        for inputss in g_vpr_xml.inputss:
            for inp in inputss.text.split():
                if inp in clockset: continue
                proc_pin(pryfile, net2pry, pinfile, inp, "input")
        for outputss in g_vpr_xml.outputss:
            for output in outputss.text.split():
                proc_pin(pryfile, net2pry, pinfile, output, "output")
        pryfile.write("</io_mapping>\n")
        pinfile.write("</io_mapping>\n")
    # with
# def read_pack_xml

g_blname2loc = {}   # block_name --> (x_int, y_int, z_int, layer_int)

def read_place(placefile):
    """read placer output"""
    nbl = 0
    with open(placefile, encoding='ascii') as ifile:
        log(f"{elapsed()} Reading {placefile}\n")
        n = 0
        for line in ifile:
            n += 1
            if n <= 3 or line[0] == '#': continue
            # final block_number unused
            (block_name, x, y, z, layer, _) = line.split()
            g_blname2loc[block_name] = (int(x), int(y), int(z), int(layer))
            nbl += 1
        # for
    # while
    msg = f" - {nbl:,d} block{plural(nbl)} placed"
    log(f"{msg}\n")
# def read_place

g_route_bits_changed = 0                        # bits different in SB routing
g_route_muxes_changed = 0                       # muxes with different bits in SB routing
g_route_nets_changed = 0                        # nets with different bits/muxes in SB routing
g_route_nets_changed_pins = 0                   # nets with changed node sets
g_route_wires_altered = 0
g_route_nets_altered_wires = 0
g_route_nets_unconn_categories = [0, 0, 0, 0,]  # nets in each category of connectedness

# g_route_setting: snk --> src --> (path, value, i) FROM ARCH
# oldroute: path --> idx --> val                    FROM OLD ROUTE
# source: snk --> (src, path, value)                FROM NEW ROUTE
def checknet(source, oldroute):
    """check current route against previous route that was read in"""
    #print("\nCHECKING NET")
    nets = 0
    oldarcs = set()
    newarcs = set()
    oldsrcs = set()
    newsrcs = set()
    for snk, triple in source.items():

        # this route
        (newsrc, path, newvalue) = triple
        if "_track_" not in path and "_ipin_" not in path: continue
        width = len(newvalue)
        mx = width - 1

        # get previous route
        oldvalue = [" "] * width
        mux = 0
        #print(f"oldvalue={oldvalue}")
        #print(f"path={path}")
        for idx, val in oldroute[path].items():
            #print(f"  idx={idx} val={val}")
            idx = int(idx)
            assert idx < width
            oldvalue[idx] = val
            if newvalue[mx - idx] != val:
                #print(f"oldval={val} newval={newvalue[idx]}")
                nets = 1
                mux = 1
                global g_route_bits_changed
                g_route_bits_changed += 1
        #print(f"oldvalue={oldvalue}")
        oldvalue = ''.join(oldvalue)
        #print(f"oldvalue={oldvalue}")
        assert width == len(oldvalue)
        assert ' ' not in oldvalue
        global g_route_muxes_changed
        g_route_muxes_changed += mux
        assert int(snk) in g_route_setting, f"snk={snk}"
        oldrev = oldvalue[::-1]
        oldsrcl = [ oldsrc for oldsrc, double in g_route_setting[int(snk)].items()
                    if double[1] == oldrev]
        if len(oldsrcl) != 1:
            for oldsrc, double in g_route_setting[int(snk)].items():
                print(f"oldsrc={oldsrc} double={double}")
            assert len(oldsrcl) == 1, f"snk={snk} oldvalue=<{oldvalue}> oldsrcl={oldsrcl}"
        oldsrc = str(oldsrcl[0])
        #print(f"FOUND oldsrc={oldsrc}")

        # save graph
        oldsrcs.add(int(oldsrc))
        newsrcs.add(newsrc)
        oldarcs.add((snk, int(oldsrc)))
        newarcs.add((snk, newsrc))
        # for
    # for
    #for arc in oldarcs:
    #    print(f"OLD ARC: {arc}")
    #for arc in newarcs:
    #    print(f"NEW ARC: {arc}")

    # did the net change?
    if not nets: return
    global g_route_nets_changed
    g_route_nets_changed += nets

    # check graphs for node changes
    pins  = [s for s in oldsrcs ^ newsrcs if not g_nid2info[s][5].startswith("CHAN")]
    if pins:
        print(
f"oldsrcs={sorted(oldsrcs)} newsrcs={sorted(newsrcs)} pins={sorted(pins)}")
        global g_route_nets_changed_pins
        g_route_nets_changed_pins += 1
        return
    wires = [s for s in oldsrcs ^ newsrcs if     g_nid2info[s][5].startswith("CHAN")]
    if wires:
        global g_route_wires_altered, g_route_nets_altered_wires
        g_route_wires_altered += len(wires)
        g_route_nets_altered_wires += 1

    # same node sets: check each graph for connectedness
    category = 0
    for which, arcs in [[0, oldarcs], [1, newarcs]]:
        fwd = defaultdict(set)
        indegree = defaultdict(int)
        for snk, src in arcs:
            fwd[src].add(snk)
            indegree[snk] += 1
            indegree[src] += 0
        levels = defaultdict(set)
        for nid, cnt in indegree.items():
            levels[cnt].add(nid)
        # for
        while 1:
            wavefront = list(levels[0])
            if not wavefront: break
            levels[0].clear()
            for src in wavefront:
                for snk in fwd[src]:
                    ind = indegree[snk]
                    levels[ind].remove(snk) # remove raises error, discard does not
                    ind -= 1
                    levels[ind].add(snk)
                    indegree[snk] = ind
                # for
            # for
        # while
        unvisited = [nid for nid in indegree if indegree[nid] > 0]
        if unvisited: category += 1 << which
    # for
    g_route_nets_unconn_categories[category] += 1
# def checknet

g_tree2net = {} # for warning message only
g_net2tree = {}
g_route_in = {}

def read_repack(repackfile):
    """read repack constraints for clock"""

    # determines clock (global) routing
    warned = set()
    with open(repackfile, encoding='ascii') as ifile:
        log(f"{elapsed()} Reading {repackfile}\n")
        for line in ifile:
            g = re.match(
r'\s*<pin_constraint\s+pb_type="[^"]*"\s+pin="([^"]*)"\s+net="([^"]*)"', line)
            if not g: continue
            (pin, net) = g.groups()
            if net.upper() == "OPEN": continue
            pin = int(re.sub(r'^.*\[(\d+)\]$', r'\1', pin))
            if net not in g_net2tree:
                log(f" - Clock tree {pin} assigned {net}\n")
            g_net2tree[net] = pin
            if pin not in g_tree2net:
                g_tree2net[pin] = net
            elif net != g_tree2net[pin] and pin not in warned:
                log(f" - WARNING: Multiple nets assigned to clock tree {pin}\n")
                warned.add(pin)
        # for
    # with
# def read_repack

# (x, y) --> {net0 -> {5, 6}, net1 -> {1, 2, 8}, ...}
g_coord2listpinsets = defaultdict(lambda: defaultdict(set))

def read_route(routefile, oldroute):
    """read router output"""

    h = set()
    pinsets = defaultdict(set)
    source = {}
    src = -1
    pins = { "IPIN", "OPIN", }
    netnum = "-1"
    netcount = 0
    allpins = set()
    netname = ""

    ln = 0
    written = set()
    with open(routefile, encoding='ascii') as ifile:
        log(f"{elapsed()} Reading {routefile}\n")
        for line in ifile:
            ln += 1
            f = line.split()

            if line.startswith("Net "):

                #print(f"\tnew net")
                h = set()
                pinsets.clear()
                source = {}
                src = -1
                netnum = f[1]

                # global?
                g = re.match(r'^Net \d+ [(]([^\s()]+)[)]: global net connecting:', line)
                if not g:
                    netcount += 1
                    netname = f[2][1:-1]
                    continue

                # handle declaration of global
                glob = g.group(1)
                if glob not in g_net2tree:
                    log(f" - Unassigned global {glob}\n")
                    repackfile = "fpga_repack_constraints.xml"
                    log(f" - Please edit {repackfile}\n")

                # move previous global "connections" from packer to here
                # (weird OpenFPGA behavior)
                # DANA
                if glob in g_pack_weird_globals:
                    # this works b/c the global has its own netnum
                    # and no one else cares about what netnum's are
                    for xy, pinset in g_pack_weird_globals[glob].items():
                        g_coord2listpinsets[xy][netnum] = pinset
                    # for

                continue

            if not line.startswith("Node:"):

                # just in case globals need processing here (I think not)
                g = re.match(
r'Block (\S+) [(]#(\d+)[)] at [(](\d+),(\d+)[)], Pin class (\d+)[.]', line)
                if g:
                    # process Block here if needed
                    continue

                if not h or not oldroute: continue

                # process full net and check against old routing
                checknet(source, oldroute)

                # Step 2: record any IPIN short tracking
                for coord, pinset in pinsets.items():
                    # due to multi-grid tiles, can't (yet) throw out singletons
                    #if len(pinset) < 2: continue
                    g = re.match(r'[(](?:\d+,)?(\d+),(\d+)[)]$', coord)
                    assert g
                    (x, y) = (int(g.group(1)), int(g.group(2)))
                    g_coord2listpinsets[(x, y)][netnum] = pinset
                # for

                h = set()
                pinsets.clear()
                source = {}
                src = -1

                continue

            # process "Node:"

            # Step 1: IPIN short tracking
            # 0     1     2    3       4    5   6          7       8
            # Node: 39841 IPIN (0,3,3) Pin: 8   clb.I00[8] Switch: 0
            # Node: 7382  IPIN (0,3,1) Pad: 7   Switch:    0
            # Node: 39980 OPIN (0,5,3) Pin: 80  clb.O0[23] Switch: 3
            # Node: 51503 OPIN (0,1,5) Pad: 142 Switch:    3
            # _     _     _    coord   what pin
            if f[2] in pins:
                (_, node, _, coord, what, pin, *_) = f
                allpins.add(node)
                if what == "Pin:":
                    pinsets[coord].add(int(pin))
                    if f[2] == "IPIN":
                        # pull out coordinates
                        (_, x, y) = f[3][1:-1].split(',')
                        (x, y) = (int(x), int(y))
                        # get block.port WITHOUT bit index
                        blkeqport = re.sub(r'\[\d+\]$', r'', f[6])
                        # key to lookup record to make change
                        mckey = (netname, x, y, blkeqport)
                        # record exists?
                        #print(f"LOOKING for mckey={mckey}")
                        if mckey in g_save_for_reroute:
                            # get the arguments to when we previously configured this mux
                            for (mcwhich, _, mcname, mctail, mcx, mcy, mcz,
                                mcconn, mci, _,
                                mcdotted_w_idx, mcdottedpathxx, mcpbt, mcpin, mcinter_name
                                ) in g_save_for_reroute[mckey]:
                                # change the pin
                                if g_dump_ppfu:
                                    print(
 "pb_pin_fixup4:\n" +
f"\tmckey={mckey}\n" +
f"\tmcwhich={mcwhich} mcname={mcname} mctail={mctail}\n" +
f"\tmcx={mcx} mcy={mcy} mcz={mcz} mcconn={mcconn} mci={mci}\n" +
f"\tmcdotted_w_idx={mcdotted_w_idx} mcdottedpathxx={mcdottedpathxx}\n" +
f"\tmcpbt={mcpbt} mcpin={mcpin} mcinter_name={mcinter_name}\n" +
f"\tnew={f[6]}")
                                (blkcheck, mcpin) = f[6].split('.')
                                assert blkcheck == mcpbt
                                # alter the input mux inside the tile
                                make_conn(
                                    mcwhich, [], mcname, mctail, mcx, mcy, mcz,
                                    mcconn, mci,
                                    0, # 0 to indicate higher priority from .route
                                    mcdotted_w_idx, mcdottedpathxx,
                                    mcpbt, mcpin, mcinter_name)
                                global g_inps_synced
                                g_inps_synced += 1
                        # if
                    # if
                # if
            # if

            snk = int(f[1])
            if not h:
                h.add(snk)
                src = snk
                continue
            if snk in h:
                src = snk
                continue
            assert src != -1
            h.add(snk)
            if snk in g_route_in:
                if g_route_in[snk] != src:
                    if f[2] != "SINK":
                        print(
f"({f[2]}) g_route_in[{snk}]={g_route_in[snk]}, want to set it to {src}")
                    assert f[2] == "SINK"
            g_route_in[snk] = src

            #pair = (src, snk)
            #cf = snk in g_route_setting and
            # src in g_route_setting[snk] and
            # g_route_setting[snk][src][1] != ""
            #print(f"\tsrc={src} snk={snk} cf={cf}")
            if snk in g_route_setting and src in g_route_setting[snk]:
                (path, value, ii) = g_route_setting[snk][src]
                if value != "":
                    # remember width of mux driving this sink node
                    assert snk not in source
                    source[snk] = (src, path, value)
                    m = len(value) - 1
                    assert path not in written
                    written.add(path)
                    announce(path, value, 0)
                    for i, v in enumerate(value, start=0):
                        path2 = path + f".mem_out[{m - i}]"
                        # can't create any new config bits
                        assert path2 in g_config
                        #print(f"SET {path2} {v}")
                        g_config[path2] = v
                    # for
                    if g_mux_set_dump:
                        what = ""
                        if (g_mux_set_dump & g_mux_set_dump_sb) and '_track_' in path:
                            what = "SB"
                        if (g_mux_set_dump & g_mux_set_dump_cb) and '_ipin_'  in path:
                            what = "CB"
                        if what != "":
                            srcs = sorted(  g_route_setting[snk],
                                            key=lambda c: g_route_setting[snk][c][2])
                            srcs = [f"={s}=" if s == src else str(s) for s in srcs]
                            out(f"{what} {path} {snk} <== {src} #{ii} [{', '.join(srcs)}]\n")
                # if
            # if

            src = snk
        # for
    # with
    np = len(allpins)
    log(f" - {netcount:,d} net{plural(netcount)} {np:,d} pin{plural(np)} routed\n")

    nm = len(g_route_in)
    log(f" - {nm:,d} inter-tile mux{plural(nm, 'es')} configured\n")
    nm = g_inps_synced
    log(f" - {nm:,d} intra-tile mux{plural(nm, 'es')} synchronized to routing\n")

    # merge non-lowest grids into lowest grids of each multi-grid tile
    xylist = list(g_coord2listpinsets)
    for xy in xylist:
        (_, wo, ho) = g_xy2bidwh[xy]
        if wo or ho:
            (x, y) = (xy[0] - wo, xy[1] - ho)
            for netnum, pinset in g_coord2listpinsets[xy].items():
                g_coord2listpinsets[(x, y)][netnum].update(pinset)
    # lowest grids: remove singletons & (newly) empty coordinates
    xylist = list(g_coord2listpinsets)
    for xy in xylist:
        (_, wo, ho) = g_xy2bidwh[xy]
        if wo or ho: continue
        netnums = list(g_coord2listpinsets[xy])
        for netnum in netnums:
            pinset = g_coord2listpinsets[xy][netnum]
            if len(pinset) < 2:
                del g_coord2listpinsets[xy][netnum]
        if not g_coord2listpinsets[xy]:
            del g_coord2listpinsets[xy]
    # copy results from lowest to non-lowest
    xylist = list(g_xy2bidwh)
    for xy in xylist:
        (_, wo, ho) = g_xy2bidwh[xy]
        if wo or ho:
            (x, y) = (xy[0] - wo, xy[1] - ho)
            if (x, y) in g_coord2listpinsets:
                g_coord2listpinsets[xy] = g_coord2listpinsets[(x, y)]

    # collect stats
    ncoords = 0
    nsets = 0
    npins = 0
    for xy, num2set in g_coord2listpinsets.items():
        (_, wo, ho) = g_xy2bidwh[xy]
        if wo or ho: continue
        vv = [v for v in num2set.values() if len(v) > 1]
        p = sum(len(v) for v in vv)
        if not p: continue
        ncoords += 1
        nsets += len(vv)
        npins += p
    if ncoords:
        log(
f" - {npins:,d} pin{plural(npins)} in " +
f"{nsets:,d} shorted set{plural(nsets)} on " +
f"{ncoords:,d} grid{plural(ncoords)}\n")

    # DEBUG
    if g_false:
        for xy, num2set in g_coord2listpinsets.items():
            print(f"Route-induced pin sets at grid {xy}:")
            for pinset in num2set.values():
                print(f"\t{pinset}")

    # report net differences
    if not oldroute: return
    if not g_route_bits_changed:
        log(g_green + " - SB muxes MATCH\n" + g_black)
        return
    (b, m, n) = (g_route_bits_changed, g_route_muxes_changed, g_route_nets_changed)
    log(g_purple +
f" - {b:,d} bit{plural(b)} {m:,d} SB mux{plural(m, 'es')} {n:,d} net{plural(n)} rewired")
    lead = ": "
    if g_route_nets_changed_pins:
        n = g_route_nets_changed_pins
        log(f"{lead}{n:,d} net{plural(n)} with different pins")
        lead = ", "

    if g_route_nets_altered_wires: # net count
        (n, w) = (g_route_nets_altered_wires, g_route_wires_altered)
        log(f"{lead}{n:,d} net{plural(n)} reassigned {w:,d} segment{plural(w)}")
        lead = ", "

    msg = [
        "",                 #   "always connected",
        "were BROKEN",
        "now BROKEN",
        "always BROKEN",
    ]
    for i in range(4):
        if not msg[i]: continue
        n = g_route_nets_unconn_categories[i]
        if not n: continue
        log(f"{lead}{n:,d} net{plural(n)} {msg[i]}")
        lead = ", "
    log("\n" + g_black)
    # for
# def read_route

g_dump_ptc_check = False

def checkbadbits(oldtile):
    """find and explain mismatched bits"""

    if not oldtile: return

    # convert shorted IPINs from routing into form faster to use
    coordptc2rep = defaultdict(dict)
    for xy, num2set in g_coord2listpinsets.items():
        for pinset in num2set.values():
            if len(pinset) < 2: continue
            rep = min(pinset)
            for pin in pinset:
                coordptc2rep[xy][pin] = rep
            # for
        # for
    # for

    badblocks = defaultdict(set)
    badbits = 0
    for path, myv in g_config.items():
        g = re.match(r'(.*).mem_out\[(\d+)\]$', path)
        (word, idx) = g.groups()
        #                        x      y              block      pin
        g = re.search(r'grid_\w+_(\d+)__(\d+)_.*[.]mem_(\w+)_lr_0_(\w+)$', word)
        if not g: continue
        (x, y, _, pin) = g.groups()
        (x, y) = (int(x), int(y))
        assert word in oldtile, f"word={word}"
        if str(myv) == oldtile[word][idx]: continue
        badblocks[(x, y)].add(word)
        #print(f"BAD WORD: x={x} y={y} word={word}")
        badbits += 1
    # for
    nblk = len(badblocks)
    npin = sum(len(badblocks[xy]) for xy in badblocks)
    if not nblk:
        log(g_green + " - CB muxes MATCH\n" + g_black)
        #return

    else:
        #log(f"\t{nblk:,d} block{plural(nblk)} with {npin:,d} rewired input{plural(npin)}\n")

        samenet = 0
        diffnet = 0
        for xy, words in badblocks.items():
            for word in words:
                if g_dump_ptc_check: print("")
                oldval = "" ; newval = ""
                for b in range(9999):
                    path = f"{word}.mem_out[{b}]"
                    if word not in oldtile or str(b) not in oldtile[word]:
                        break
                    assert path in g_config, f"path={path}"
                    oldval += oldtile[word][str(b)]
                    newval += g_config[path]
                # for
                tail = word.split('.')[-1]
                if g_dump_ptc_check: print(
                    f"oldval={oldval} newval={newval} tail={tail} word={word}")
                oldptc = g_outbit2ptc[tail][oldval]
                newptc = g_outbit2ptc[tail][newval]
                if g_dump_ptc_check: print(
                    f"oldptc={oldptc} newptc={newptc} coordptc2rep[xy]={coordptc2rep[xy]}")
                if oldptc == newptc:
                    same = True
                elif xy not in coordptc2rep:
                    same = False
                else:
                    oldptcrep = coordptc2rep[xy].get(oldptc, oldptc)
                    newptcrep = coordptc2rep[xy].get(newptc, newptc)
                    if g_dump_ptc_check: print(f"oldptcrep={oldptcrep} newptcrep={newptcrep}")
                    same = bool(oldptcrep == newptcrep)
                if same:
                    samenet += 1
                else:
                    diffnet += 1
            # for
        # for

        b = badbits
        i = npin
        g = nblk
        c = diffnet
        log((g_red if c else g_purple) + 
        f" - {b:,d} bit{plural(b)} " +
        f"{i:,d} CB mux{plural(i, 'es')} " +
        f"{g:,d} grid{plural(g)} rewired with " +
        f"{c:,d} changed net{plural(c)}\n" + g_black)
    # else

    # now check for "bad" bits in don't-care LUT5s
    dclutbits = 0
    dcseen = set()
    badlutbits = 0
    badseen = set()
    badwords = set()
    newdata = defaultdict(dict)
    for path, myv in g_config.items():
        g = re.search(
            r'^(.*grid_\w+_(\d+)__(\d+)_.*__fle_(\d+)[.].*frac_lut6.*)[.]mem_out\[(\d+)\]$',
            path)
        if not g: continue
        (word, x, y, f, b) = g.groups()
        assert word in oldtile, f"word={word}"
        if str(myv) == oldtile[word][b]: continue
        #print(f"BAD LUT WORD: x={x} y={y} word={word}")
        (x, y, f) = (int(x), int(y), int(f))
        off = int(b) & 32
        key = (x, y, f, off)    # key must be all int's
        if key in g_lut5_open:
            dclutbits += 1
            dcseen.add(key)
            continue

        badlutbits += 1
        badseen.add(key)

        # prepare for more thorough check
        if g_true: continue # skip prep for experimental stuff
        key3 = (x, y, f)
        if 0 in g_lut_debug[key3]:
            (_, _, size) = g_lut_debug[key3][0]
            if size == 64:
                badwords.add((word, key3))
                newdata[key3]['6'] = ""
            elif size == 32:
                if not 32 & int(b):
                    badwords.add((word, key3))
                    newdata[key3]['50'] = ""
            else:
                assert 0, f"key3={key3} size={size}"
        if 32 in g_lut_debug[key3]:
            (_, _, size) = g_lut_debug[key3][32]
            assert size == 32
            if 32 & int(b):
                badwords.add((word, key3))
                newdata[key3]['51'] = ""
        else:
            assert 0, f"key3={key3}"
    # for

    if badlutbits or dclutbits:
        (ub, ul, fb, fl) = (badlutbits, len(badseen), dclutbits, len(dcseen))
        ms = []
        if badlutbits:
            ms.append(f"{ub:,d} altered bit{plural(ub)} in {ul:,d} used LUT{plural(ul)}")
        if dclutbits:
            ms.append(f"{fb:,d} altered bit{plural(fb)} in {fl:,d} unused LUT5{plural(fl)}")
        log(g_red + " - " + ', '.join(ms) + "\n" + g_black)
    else:
        log(g_green + " - LUTs MATCH\n" + g_black)

    # skip following experimental stuff
    if g_true: return

    for pair in badwords:
        (word, key3) = pair
        (x, y, f) = key3
        oldbits = ""
        for b in range(64):
            oldbits += oldtile[word][str(b)]
        # for
        assert len(oldbits) == 64
        for which in newdata[key3]:
            if which == '6':
                lutbits = oldbits
                off = 0
            elif which == '50':
                lutbits = oldbits[:32]
                off = 0
            elif which == '51':
                lutbits = oldbits[32:]
                off = 32
            else:
                assert 0
            (newbits, prm, size) = g_lut_debug[key3][off]
            print(
            f"\nword={word} ({x},{y},{f}) which={which} prm={prm} off={off} size={size}\n" +
            f"\tlutbits={lutbits}\n\tnewbits={newbits}")
            rotlist = [-1 if x == "open" else int(x) for x in prm.split()]
            nrl = len(rotlist)
            rlmax = max(rotlist)
            if nrl != 5 or rlmax != 3: continue
            cands = set()
            for i in range(rlmax + 1):
                for j in range(i + 1, rlmax + 1):
                    nochance = False
                    for v in range(1 << nrl):
                        if lutbits[v] == "0": continue
                        if ((v & (1 << i)) == 0) != ((v & (1 << j)) == 0):
                            nochance = True
                            break
                    if nochance: break
                    cands.add((i, j))
            print(f"cands = {cands}")
            ii = [i for i, v in enumerate(rotlist, start=0) if v < 0]
            assert len(ii) == 1
            idx2copy = ii[0]
            for extra in range(1 + rlmax):
                ii = [i for i, v in enumerate(rotlist, start=0) if v == extra]
                assert len(ii) == 1
                idx2orig = ii[0]
                #rotlist[idx2copy] = extra

                data = ["0"] * (1 << nrl)   # default for bits not part of the function
                for v in range(1 << nrl):
                    i = 0
                    for b, m in enumerate(rotlist, start=0):
                        if m < 0: continue
                        if v & (1 << b): i |= 1 << (rlmax - m)
                    # for
                    assert i < len(newbits), (
                    f"newbits={newbits} prm={size} size={size} rotlist={rotlist} " +
                    f"nrl={nrl} rlmax={rlmax} ii={ii} extra={extra} v={v} i={i}")
                    # with 2 inputs tied together, don't populate inaccessible bits
                    if ((v & (1 << idx2orig)) == 0) != ((v & (1 << idx2copy)) == 0):
                        continue
                    data[i] = newbits[i]
                # for

                #rotlist[idx2copy] = -1

                data = ''.join(data)
                print(
                f"\tidx2copy={idx2copy} extra={extra} idx2orig={idx2orig}\n" +
                f"\tdata={data}")
        # for
    # for
# def checkbadbits

def main():
    """run it all from top level"""
    parser = argparse.ArgumentParser(
        description="Generate bitstream file or directory of routing Verilog modules",
        epilog="Files processed in order listed and any ending .gz are compressed")
    # --commercial (routing)
    parser.add_argument('--commercial',
                        action='store_true',
                        help="Assume commercial not academic VPR routing")
    # --last_clock
    parser.add_argument('--default_clock',
                        help="Default selection for clock muxes")
    # --mux_set_dump sb,cb,lb
    parser.add_argument('--mux_set_dump',
                        help="Write mux settings")
    # --read_vpr_arch k6n8_vpr.xml
    parser.add_argument('--read_vpr_arch',
                        required=True,
                        help="Read .xml architecture read by VPR")
    # --read_openfpga_arch openfpga_arch.xml
    parser.add_argument('--read_openfpga_arch',
                        required=True,
                        help="Read .xml architecture read by OpenFPGA")
    # --read_rr_graph rr_graph_out.xml
    # --read_rr_graph rr_graph_out.json.gz
    parser.add_argument('--read_rr_graph',
                        required=True,
                        help="Read .xml, .pyx, or .json routing graph")
    # --write_device_muxes muxdump
    parser.add_argument('--write_device_muxes',
                        required=False,
                        help="Write text SB mux descriptions")
    # --write_rr_graph rr_graph_out.json
    parser.add_argument('--write_rr_graph',
                        required=False,
                        help="Write .json routing graph for faster reloading")
    # --write_routing_verilog_dir SRC/n
    parser.add_argument('--write_routing_verilog_dir',
                        required=False,
                        help="Write .v Verilog modules for routing and STOP")
    # --read_verilog .../fabric_add_1bit_post_synth.v
    parser.add_argument('--read_verilog',
                        required=True,
                        help="Read .v design Verilog written by VPR")
    # --read_place_file .../fabric_add_1bit_post_synth.place
    parser.add_argument('--read_place_file',
                        required=True,
                        help="Read .place placement result written by VPR")
    # --read_design_constraints	fpga_repack_constraints.xml
    parser.add_argument('--read_design_constraints',
                        required=True,
                        help="Read .xml clock repacking instructions")
    # --write_set_muxes xxx
    parser.add_argument('--write_set_muxes',
                        required=False,
                        help="Write muxes set by design")
    # --read_pack_file .../fabric_add_1bit_post_synth.net.post_routing
    parser.add_argument('--read_pack_file',
                        required=True,
                        help="Read .net packing result written by VPR")
    # --read_eblif .../add_1bit_post_synth.eblif
    parser.add_argument('--read_eblif',
                        required=False,
                        help="Read .eblif for top connections")
    # --write_primary_mapping PrimaryPinMapping.xml
    parser.add_argument('--write_primary_mapping',
                        required=False,
                        help="Write .xml primary pin mapping")
    # --write_pin_mapping PinMapping.xml
    parser.add_argument('--write_pin_mapping',
                        required=False,
                        help="Write .xml top mapping")
    # --read_fabric_bitstream golden.csv
    parser.add_argument('--read_fabric_bitstream',
                        required=False,
                        help="Read .csv bitstream for comparison")
    # --read_route_file .../fabric_add_1bit_post_synth.route
    parser.add_argument('--read_route_file',
                        required=True,
                        help="Read .route routing result written by VPR")
    # --read_map	coordinates
    parser.add_argument('--read_map',
                        required=False,
                        help="Read .csv bitstream map")
    ## --read_tile_grouping _run_dir/SRC/tile/grouping.csv
    #parser.add_argument('--read_tile_grouping',
    #                    required=False,
    #                    help="Read tile grouping map")
    # --read_extra_bitstream bitstream.xml
    parser.add_argument('--read_extra_bitstream',
                        required=False, nargs='+',
                        help="Read .xml bitstream to overwrite settings")
    # --write_fabric_bitstream top.csv
    # --write_fabric_bitstream fabric_bitstream.xml
    # --write_fabric_bitstream fabric_bitstream.bit
    parser.add_argument('--write_fabric_bitstream',
                        required=True, nargs='+',
                        help="Write .csv, .xml, or .bit bitstream ")

    # process and show args
    args = parser.parse_args()
    log(f"\nInvocation: {sys.argv[0]}\n")
    ml = 0
    for k, v in vars(args).items():
        if v is None: continue
        ml = max(ml, len(k))
    ml += 1
    for k, v in vars(args).items():
        if v is None: continue
        log(f"--{k:<{ml}s}{v}\n")
    log( "\n")

    # interpret args
    global g_commercial, g_default_clock, g_mux_set_dump
    g_commercial = 1 if args.commercial else 0
    if args.default_clock is None:
        g_default_clock = "11111111111111111111111111111111"
    elif not args.default_clock.isdigit():
        log(" - Argument to --default_clock must be integer\n")
        sys.exit(1)
    else:
        # bin(11) --> 0b1011 [2:] --> 1011 [::-1] --> 1101, then pad beyond 32
        g_default_clock = bin(int(args.default_clock))[2:][::-1] + ("0" * 32)
    muxbits = { 'sb': g_mux_set_dump_sb, 'cb': g_mux_set_dump_cb, 'lb': g_mux_set_dump_lb, }
    if args.mux_set_dump:
        for mtyp in args.mux_set_dump.split(','):
            g_mux_set_dump |= muxbits[mtyp.lower()]
        # for

    # check for interactions between args
    if args.write_fabric_bitstream and not args.read_map:
        for wfb in args.write_fabric_bitstream:
            if wfb.endswith('.gz'): wfb = wfb[:-3]
            if wfb.endswith('.bit'):
                log(
" - Writing .bit bitstream requires --read_map\n")
                sys.exit(1)
            if wfb.endswith('.xml'):
                log(
" - Writing .xml bitstream with no map -- one-hot addresses will be omitted\n")
    if args.write_primary_mapping and not args.read_eblif:
        log(" - --write_primary_mapping requires --read_eblif\n")
        sys.exit(1)

    # set color codes
    global g_black, g_red, g_purple, g_green
    (g_black, g_red, g_purple, g_green) = (
        ("\033[0;30m", "\033[0;31m", "\033[0;35m", "\033[0;32m")
        if sys.stderr.isatty() else
        ("", "", "", "") )

    # set up logging
    g_logger.setLevel(logging.INFO)
    # create console handler
    logit = g_false
    logit = g_true
    #if logit:
    #    g_logger.addHandler(logging.StreamHandler())
    # create file handler
    logname = f"{os.path.splitext(os.path.basename(sys.argv[0]))[0]}.log"
    log_fh = logging.FileHandler(logname, mode='w')
    if logit:
        g_logger.addHandler(log_fh)

    # read architecture
    read_vpr_xml(args.read_vpr_arch)
    read_open_xml(args.read_openfpga_arch)

    # complete logical <=> physical mapping.
    # I believe the "repacker" exists to do this dynamically for each instance
    # (as well as fix VPR bugs).
    # Here we assemble the relationship once,
    # and use it similarly as the pb_type_annotations
    read_anno()

    read_graph_xml( args.read_rr_graph,
                    args.write_device_muxes,
                    args.write_rr_graph)

    # pylint: disable=pointless-string-statement
    """
and.csv:1,fpga_top.grid_bram_7__5_.logical_tile_bram_mode_bram__0.
    mem_bram_lr_0_ ADDR_B2_i_13 .mem_out[0]
top.csv:1,fpga_top.grid_bram_7__5_.logical_tile_bram_mode_bram__0.
        bram_lr_0_.ADDR_B2_i_13_.mem_out[0]
                                   0                                  
        1          2
    """

    # use info from vpr.xml and routing graph to describe top-level wiring
    prep_top_wiring()

    # here we write out routing modules if requested and exit
    if args.write_routing_verilog_dir:
        write_cb_sb(args.write_routing_verilog_dir)
        log(f"{elapsed()} FIN\n")
        sys.exit(0)

    # create config bits
    nmux = 0
    nspecial = 0
    nword = 0
    for x, y in g_xy2bidwh:
        (bid, wo, ho) = g_xy2bidwh[(x, y)]
        if wo or ho: continue
        fname = g_bid2info[bid]['name']
        bname = re.sub(r'_.*$', '', fname)
        #print(f"{bname} lives at ({x},{y})")

        # create config bits for internal muxing
        # mux "pin", n selects, w inputs
        # not used: w
        for pin, n, _ in g_muxinfo[bname]:

            # if last clock default requested, flip dval if clock pin
            if (bname, pin) in g_blockclock:
                #log(f"\tCLOCK DEFAULT: pin={pin} ({x},{y})\n")
                dval = g_default_clock
            else:
                # no mux will have 2^32 inputs
                dval = "11111111111111111111111111111111"

            dotted = pin[:pin.index(':')]
            (dups, curr) = replicas(fname, dotted, 0)
            curr = [0] * len(dups)

            while 1:
                path = pbxy2path(pin, x, y, dups, curr)

                # entry for each bit
                for b in range(n):
                    # create the config bit location
                    newpath = path + f".mem_out[{b}]"
                    if newpath in g_config:
                        pass
                        #log(   f"DUP1:\t{newpath}\n" +
                        #       f"\tdotted={dotted}\n" +
                        #       f"\tdups={dups}\n" +
                        #       f"\tcurr={curr}\n")
                    else:
                        nmux += 1
                        #if 'mem_ff_bypass_Q_0' in newpath:
                        #    log(   f"ORG1:\t{newpath}\n" +
                        #           f"\tdotted={dotted}\n" +
                        #           f"\tdups={dups}\n" +
                        #           f"\tcurr={curr}\n")
                    #print(f"SET {newpath} {1}")
                    g_config[newpath] = dval[b]
                # for b

                if not nextrep(curr, dups): break
            # while

        # for pin, n, w

        # create config bits associated with special pb_types
        for path, obj in g_pbtypes:
            name = obj.elem.attrib.get('name')
            if name not in g_lutcirc2size: continue
            # not used: inpsize
            (_, ramsize) = g_lutcirc2size[name]
            #print( f"fname={fname} bname={bname} " +
            #       f"path[0].name={path[0].elem.attrib.get('name')}")
            if bname != path[0].elem.attrib.get('name'): continue
            #print(f"path={path} obj={obj} inpsize={inpsize} ramsize={ramsize}")

            path = list(path) + [ path[-1], path[-1]  ]
            textpath = path2text(path)

            (dups, curr) = replicas(fname, textpath, 0)
            assert len(dups) == 1
            curr = [0] * len(dups)

            while 1:

                path0 = pbxy2path(textpath, x, y, dups, curr)

                # FIXME not clear how to derive this correction from XML files
                path = re.sub(r'[.]mem_(\w+)\1$', r'.\1_RS_LATCH_mem', path0)
                #print(f"path = {path} <-- path0={path0}")

                # save paths to all LUTs
                g_xys2lutpath[tuple([x, y] + curr)] = path

                # entry for each bit
                for b in range(ramsize):
                    # create the config bit location
                    newpath = path + f".mem_out[{b}]"
                    if newpath in g_config:
                        log(f"DUP2: {newpath}\n")
                    else:
                        nspecial += 1
                    # FIXME more magic: LUTs default to 0
                    #print(f"SET {newpath} {0}")
                    g_config[newpath] = "0"
                # for b

                if not nextrep(curr, dups): break
            # while

        # for

        # declare mode words and set their default values
        if bname in g_base2pathcircbits:
            #print("match 1")
            for name, cmname, modebits in g_base2pathcircbits[bname]:
                nword += fill_cm_bits(name, cmname, modebits, x, y, 0, "", None)
            # for name...
        # if
    # for x, y

    ntot = g_nroute + nmux + nspecial + nword
    w = int(math.floor( 4 * (math.ceil(math.log10(ntot)) - 1) / 3)) + 1
    log(f" - {g_nroute:{w},d} config bits in inter-tile muxes set to defaults\n")
    log(f" - {nmux:{w},d} config bits in intra-tile muxes set to defaults\n")
    log(f" - {nspecial:{w},d} config bits in specials (LUTs) set to defaults\n")
    log(f" - {nword:{w},d} config bits in mode words set to defaults\n")
    log(f" - {ntot:{w},d} total\n")

    # read design (save LUTs)
    read_design_post_synth_v(args.read_verilog)
    #ead_design_blif("top.blif")

    # read pack/place/route results
    read_place(args.read_place_file)     # NB must read place before net
    read_repack(args.read_design_constraints)

    # translation for tiling needed in several places
    g_frag2grouped.clear()  # translate Gemini/Virgo --> Rigel
    grouped2frag = {}       # translate Rigel --> Gemini/Virgo
    def xenter(l, r):
        """enter path translation"""
        g_frag2grouped[l] = r
        grouped2frag[r] = l
    # def xenter
    for xy in sorted(g_xy2bidwh):
        (bid, wo, ho) = g_xy2bidwh[xy] 
        (x, y) = xy
        if bid == 0: continue
        if not wo and not ho:
            bname = g_bid2info[bid]['name']
            xenter(f"grid_{bname}_{x}__{y}_", f"tile_{x}__{y}_.lb")
        xo = f"x{wo}" if wo else ""
        yo = f"y{ho}" if ho else ""
        xenter(f"sb_{x}__{y}_", f"tile_{x-wo}__{y-ho}_.sb{xo}{yo}")
        xenter(f"cbx_{x}__{y}_", f"tile_{x-wo}__{y-ho}_.tb{xo}{yo}")
        xenter(f"cby_{x}__{y}_", f"tile_{x-wo}__{y-ho}_.rb{xo}{yo}")
    # for

    # set this up for read_pack_xml and read_route to dump mux configs
    muxfilename = args.write_set_muxes or "/dev/null"
    global g_write_set_muxes
    with open(muxfilename, "w", encoding='ascii') as g_write_set_muxes:
        if muxfilename != "/dev/null": log(f"{elapsed()} Writing {muxfilename}\n")

        # MODE_BITS also live here
        read_pack_xml(args.read_pack_file,
            args.read_eblif, args.write_primary_mapping, args.write_pin_mapping)

        # OPTIONAL read old config bits to diagnose OpenFPGA rewiring problem
        oldroute = defaultdict(dict)
        oldtile = defaultdict(dict)
        filename = args.read_fabric_bitstream
        if filename:
            with gzopenread(filename) as ifile:
                log(f"{elapsed()} Reading {filename}\n")
                for line in ifile:
                    g = re.match(r'(\d),(.*)[.]mem_out\[(\d+)\]$', line.strip())
                    (val, path, idx) = g.groups()
                    if "_track_" in line or "_ipin_" in path:
                        oldroute[path][idx] = val
                    else:
                        oldtile[path][idx] = val
                # for
            # with

        read_route(args.read_route_file, oldroute)
    # with

    # check for bad bits
    checkbadbits(oldtile)

    # read coordinates if specified
    filename = args.read_map
    xy2path = defaultdict(dict)
    if filename:
        with gzopenread(filename) as ifile:
            log(f"{elapsed()} Reading {filename}\n")
            for line in ifile:
                if not line or line[0] == '#': continue
                ff = line.split(',')
                if len(ff) != 3: continue
                (x, y, path) = ff
                (x, y) = (int(x), int(y))
                xy2path[y][x] = path.strip()
            # for
        # with
        # figure out extremes and counts
        ys = xy2path
        (ymin, ymax) = (min(ys), max(ys))
        ny = 1 + ymax - ymin
        xs = set(itertools.chain.from_iterable(xy2path.values()))
        (xmin, xmax) = (min(xs), max(xs))
        nx = 1 + xmax - xmin
    # if

    # read additional bitstream(s)
    for filename in args.read_extra_bitstream or []:
        if not filename.endswith(".xml") and not filename.endswith(".xml.gz"):
            log(f" - Unrecognized bitstream format/suffix: {filename}\n")
            sys.exit(1)
        with gzopenread(filename) as ifile:
            log(f"{elapsed()} Reading {filename}\n")
            for line in ifile:
                if not re.match(r'\s*<bit\s', line): continue
                vals = dict( re.findall(r'\s(\w+)="([^"]*)"', line) )
                (v, path) = (vals['value'], vals['path'])
                # if needed: translate Rigel --> Gemini/Virgo (latter used internally)
                if grouped2frag:
                    ff = path.split('.')
                    mid = f"{ff[1]}.{ff[2]}"
                    if mid in grouped2frag:
                        ff[1:3] = [ grouped2frag[mid] ]
                    path = '.'.join(ff)
                # if
                # handle alternate overwrite format that omits .mem_out
                g = re.match(r'(.+)([.][^.]+)(\[\d+\])$', path)
                assert g
                if g.group(2) != ".mem_out":
                    path = f"{g.group(1)}{g.group(2)}.mem_out{g.group(3)}"
                # overwrite
                g_config[path] = v
            # for
        # with
    # for

    # write bitstream(s)
    for filename in args.write_fabric_bitstream:
        nwritten = 0
        which = filename[-7:-3] if filename.endswith('.gz') else filename[-4:]
        if   which == ".csv":
            with gzopenwrite(filename) as ofile:
                log(f"{elapsed()} Writing {filename}\n")
                #or path in sorted(g_config, key=dict_aup_nup):
                for path in sorted(g_config):
                    # NB we do not inject tile grouping here
                    # since point of csv is to compare against OpenFPGA
                    ofile.write(f"{g_config[path]},{path}\n")
                    nwritten += 1
                # for
            # with
        elif which == ".xml":
            with gzopenwrite(filename) as ofile:
                log(f"{elapsed()} Writing {filename}\n")
                ofile.write( '<!--\n')
                ofile.write( '\t- Fabric bitstream\n')
                ofile.write( '-->\n')
                ofile.write( '\n')
                ofile.write( '<fabric_bitstream>\n')
                ofile.write( '\t<region id="0">\n')
                if xy2path:
                    for y in sorted(xy2path):
                        a = "0" * (y - ymin) + "1" + "0" * (ymax - y)
                        for x in sorted(xy2path[y]):
                            i = nwritten
                            p = xy2path[y][x]
                            v = str(g_config[p])
                            if g_frag2grouped:
                                ff = p.split('.')
                                ff[1] = g_frag2grouped.get(ff[1], ff[1])
                                p = '.'.join(ff)
                            ofile.write(f'\t\t<bit id="{i}" value="{v}" path="{p}">\n')

                            # do we really want this verbosity?
                            b = "x" * (x - xmin) + "1" + "x" * (xmax - x)
                            ofile.write(f'\t\t\t<bl address="{b}"/>\n')
                            ofile.write(f'\t\t\t<wl address="{a}"/>\n')

                            ofile.write( '\t\t</bit>\n')
                            nwritten += 1
                        # for
                    # for
                else:
                    for path in sorted(g_config):
                        v = str(g_config[path])
                        if g_frag2grouped:
                            ff = path.split('.')
                            ff[1] = g_frag2grouped.get(ff[1], ff[1])
                            path = '.'.join(ff)
                        ofile.write(f'\t\t<bit id="{nwritten}" value="{v}" path="{path}"/>\n')
                        nwritten += 1
                    # for
                ofile.write( '\t</region>\n')
                ofile.write( '</fabric_bitstream>\n')
            # with
        elif which == ".bit":
            with gzopenwrite(filename) as ofile:
                log(f"{elapsed()} Writing {filename}\n")
                ofile.write( "// Fabric bitstream\n")
                ofile.write( "// Version: \n")
                ofile.write( "// Date: \n")
                ofile.write(f"// Bitstream length: {ny}\n")
                ofile.write(
f"// Bitstream width (LSB -> MSB): <bl_address {nx} bits><wl_address {ny} bits>\n")
                for y in sorted(xy2path):
                    bits = ["x"] * nx
                    for x in xy2path[y]:
                        path = xy2path[y][x]
                        bits[x] = str(g_config[path])
                        nwritten += 1
                    # for
                    bits = ''.join(bits)
                    addr = "0" * (y - ymin) + "1" + "0" * (ymax - y)
                    ofile.write(f"{bits}{addr}\n")
                # for
                ofile.write("\n")
            # with
        else:
            log(f" - Unrecognized bitstream format/suffix: {filename}\n")
            sys.exit(1)
        log(f" - {nwritten:,d} config bit{plural(nwritten)} written\n")
    # for

    # checkbadbits used to be here

    log(f"{elapsed()} FIN\n")
# def main

main()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
