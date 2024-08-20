#!/usr/bin/env python3
"""compare EBLIF to netlist extracted from base netlist and bitstream"""

import sys
import re
import time
import math
import subprocess
from collections import defaultdict
import argparse

import pack_dm
import uf

### WARNING ### WARNING ### WARNING ###
###
### Unlike bitgen.py, lvsbit.py makes
### much less effort to be general and
### configurable. In this script, there
### are MANY things that are hardwired.
###
### Code should be added to read config
### files and put the result in various
### tables and strings. This effort has
### focused on including all the proper
### behaviors, but is not sufficiently
### table-driven yet.
###
### WARNING ### WARNING ### WARNING ###

# general
g_false = False
g_true = True
g_translate = {}        # '[': "X", ']': "Y", '.': "Z", other non-word: "W"
g_time = 0              # used by elapsed()
g_black = ""
g_red = ""
g_green = ""

# set by read_base_nets
# coordkey is: (pbtail, x, y, z, b), z is slot/FLE or 0, b is which FF or 0
# physinst is: f"{pbtail}_x{x}_y{y}_z{z}_b{b}"
# path_wo_pin --> coordkey
#g_path2coords = {}

# coordkey --> path_wo_pin
g_coords2path = {}

# physinst --> coordkey
#g_inst2coords = {}

# coordkey --> physinst
#g_coords2inst = {}

# pbtail --> set of x-coordinates
g_cols = defaultdict(set)

# max coordinates of base array
(g_xmax, g_ymax) = (0, 0)

# path_wo_pin --> base die module
g_path2mod = {}

# path_wo_pin --> set of pins on base die instance
g_path2pins = defaultdict(set)

# (x, y) --> (wo, ho)
g_xy2wh = {}

# (x, y) --> grid tile basename (clb, dsp, io, etc)
g_xy2bn = {}

# path_wo_pinZipin --> path_wo_pinZopin driving it in base die net
g_ipin2netopin = {}

# path_wo_pinZpin --> "i" or "o"
g_pin2dir = {}

# set by read_place
# block_name --> (x_int, y_int, z_int, layer_int)
g_blname2loc = {}

# set by read_pack
# coordkey --> "name" value on block from .net (leaf pbtype)
g_coords2name = {}

# coordkey on last LUT
g_prevkey = ("", 0, 0, 0, 0)

# coordkey --> "instance" value on block from .net (leaf pbtype)
g_coords2mode = {}

# coordkey --> (.net "instance", port_rotation_map text, had_prm, name=)
g_coords2instrmap = {}

# set by read_verilog
# output --> LUT mask
g_out2lutbits = {}

# flag to read_verilog / read_transformed_eblif / read_lut (bitstream)
g_dump_luts = False

# set by read_bitstream
# path_wo_pin --> bit_index --> 0 or 1
g_settings = defaultdict(dict)

# set by read_set_muxes
# (info in function return value) 

# set by apply_bitstream_muxes
# set of path_wo_pinZipin if sel'd by mux, AND, if supplied, that mux in mux settings
g_used_ipins = set()

# set of path_wo_pinZout, under same conditions
g_used_opins = set()

# memoize sel2idx()
g_siz2sel2idx = defaultdict(dict)

# set by write_b_netlist
# coordkey --> (inputs, lmin, llim, opin)
g_coords2info = {}

# path_wo_pinZpin --> preferred net name
g_node2net = {}

# nets seen
g_nets = set()

# FIXME not set by anyone
# 
g_out2lut_used = {}

# MULTI updated by read_base_nets apply_bitstream_muxes write_b_netlist
# all indices and values are path_wo_pinZpin
# driver --> set of driven
g_fwdshort = defaultdict(set)

# driven --> set of driver (singleton)
g_bwdshort = defaultdict(set)

g_shorts = False        # dump out shorting (.union calls) for bitstream cleanup

# simplification control
g_kgen = 1              # constant generators
g_wlut = 2              # wire-LUTs (buffers)
g_kinv = 4              # constant inverters
g_simplify = g_kgen     # what to simplify in bitstream
g_simplify = g_kgen + g_wlut
#_simplify = 0

# do we check for LUT input ordering? (pushed to Gemini)
g_ordered_luts = True
g_out2inps = {}         # from Verilog
g_short_globals = False
g_use_verilog = False
g_tolerate_dc = True    # tolerate DON'T-CARE LUT inputs?

### STANDARD ROUTINES

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

# IMPLEMENT --read_base_nets

def proc_base_net(pins):
    """save driving output for each pin"""
    sets = defaultdict(set)
    for pin in pins:
        sets[g_pin2dir[pin]].add(pin)
    # for
    if "o" not in sets:
        opin = ""
    else:
        assert len(sets["o"]) == 1, f"sets['o']={sets['o']}"
        opin = list(sets["o"])[0]
    for ipin in sets["i"]:
        g_ipin2netopin[ipin] = opin
        if opin != "":
            g_fwdshort[opin].add(ipin)
            g_bwdshort[ipin].add(opin)
    # for
    pins.clear()
# def proc_base_net

def path2instandkey(path):
    """translate non-mux path (without mod or pin) to key elements and inst name"""
    path = path.translate(g_translate)
    # CONSISTENT: use mux not mem everywhere
    path = re.sub(r'^(.*Z)mem([^Z]+)$' , r'\1mux\2', path)

    # pull out pb_type
    # pylint: disable-next=pointless-string-statement
    '''
logical_tile_bram_mode_default__bram_lr_mode_physical__bram_phy
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__
    adder_carry
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__
    ff_bypass_mode_default__ff_phy
logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_mode_default__
    frac_lut6
logical_tile_dsp_mode_default__dsp_lr_mode_physical__dsp_phy
logical_tile_io_mode_physical__iopad_mode_default__ff
logical_tile_io_mode_physical__iopad_mode_default__pad
    '''
    pbtail = path.split('__')[-1]
    (pbtail, idx) = re.sub(r'^(.*)_(\d+)$', r'\1 \2', pbtail).split()
    idx = int(idx)

    # extract tile coords
    g = re.match(r'^tile_(\d+)__(\d+)_Z[lstr]b(?:x(\d+))?(?:y(\d+))?[Z]', path)
    assert g
    (x, y, wo, ho) = (  int(g.group(1)), int(g.group(2)),
                        int(g.group(3) or "0"), int(g.group(4) or "0"))
    x += wo
    y += ho

    # do this here so we don't need to return wo/ho & double-checking
    if (x, y) not in g_xy2wh:
        g_xy2wh[(x, y)] = (wo, ho)
    else:
        assert g_xy2wh[(x, y)] == (wo, ho)

    # extract slot
    z = 0
    g = re.search(r'_io_mode_io__(\d+)[Z]', path)
    if g: z = int(g.group(1))

    # extract fle
    g = re.search(r'__fle_(\d+)[Z]', path)
    if g: z = int(g.group(1))

    # extract bit (FF)
    b = 0
    g = re.search(r'__ff_bypass_(\d+)[Z]', path)
    if g: b = int(g.group(1))
    g = re.search(r'__ff_(\d+)$', path)
    if g:
        b = int(g.group(1))
        assert idx == b

    key = (pbtail, x, y, z, b)
    inst = f"{pbtail}_x{x}_y{y}_z{z}_b{b}"
    return (inst, key)
# def path2instandkey

g_clk2pin = {}
g_t2b = {
    'bram_phy':     'bram',
    'dsp_phy':      'dsp',
    'ff':           'io',
    'pad':          'io',
    'adder_carry':  'clb',
    'ff_phy':       'clb',
    'frac_lut6':    'clb',
}

def read_base_nets(filename):
    """read base nets"""
    netlist = uf.uf()   # we will accumulate info here
    net = -1
    seen = defaultdict(set)
    n = 0
    prev = None
    pins = []
    #clkbit = -1
    with gzopenread(filename) as ifile:
        log(f"{elapsed()} Reading {filename}\n")
        for line in ifile:
            line = line.translate(g_translate)
            ff = line.split()
            if not ff: continue

            n += 1
            if not n % 100000: log(f"\r  {n:,d}\r")

            if ff[0] == "NET":
                # process previous batch of pins
                proc_base_net(pins)
                net += 1
                prev = None
                #clkbit = -1
                # CONFIG FIXME hardwired top clock name here
                #g = re.match(r'clkX(\d+)Y$', ff[1])
                #if g:
                #    clkbit = int(g.group(1))
                #    #print(f"\tRecognized clock {clkbit}")
                continue

            if ff[0] != "PIN": continue

            assert len(ff) == 5, f"line={line}\nff={ff}"
            (_, path, mod, pin, pdir) = ff

            # CONSISTENT: use mux not mem everywhere
            path = re.sub(r'^(.*Z)mem([^Z]+)$' , r'\1mux\2', path)

            ismux = re.search(r'_size\d+$', mod)
            if ismux:
                if pin == "outX0Y": pin = "out"
                bn = ""
            else:
                # remember non-mux instances by pbtype/coordinates
                # create pbtype/coordinates and standard inst name
                #inst  key
                (   _, key) = path2instandkey(path)
                (pbtail, x, y, z, b) = key 
                assert pbtail == mod.split('__')[-1]
                bn = g_t2b[pbtail]

                # save array size and column mix
                global g_xmax, g_ymax
                if x > g_xmax: g_xmax = x
                if y > g_ymax: g_ymax = y
                g_cols[pbtail].add(x)

                # save xy --> tile type map
                # (will be updated below outside this loop)
                # g_xy2bn is used in only one place!
                if (x, y) not in g_xy2bn:
                    g_xy2bn[(x, y)] = bn
                else:
                    assert g_xy2bn[(x, y)] == bn

                # maintain path <==> coords maps
                #if key not in g_path2coords:
                #    g_path2coords[path] = key
                #else:
                #    assert g_path2coords[path] == key
                if key not in g_coords2path:
                    g_coords2path[key] = path
                else:
                    assert g_coords2path[key] == path

                # and inst <==> coords maps
                #if key not in g_inst2coords:
                #    g_inst2coords[inst] = key
                #else:
                #    assert g_inst2coords[inst] == key
                #if key not in g_coords2inst:
                #    g_coords2inst[key] = inst
                #else:
                #    assert g_coords2inst[key] == inst

                # FIXME special case: we double-enter the LUT (fracturing)
                if pbtail == 'frac_lut6':
                    assert b == 0
                    key = (pbtail, x, y, z, 1)
                    g_coords2path[key] = path
            # if

            seen[mod].add(path)

            g_path2pins[path].add(pin)

            this = f"{path}Z{pin}"
            # save ANY pin on the clock net
            #if g_short_globals:
            #    g_clk2pin[clkbit] = this
            pins.append(this)
            g_pin2dir[this] = pdir
            # FIXME is this needed?
            netlist.add(this)

            if prev is not None:
                # union from basenets: remain quiet
                netlist.union(prev, this)
                # g_fwdshort / g_bwdshort handled in proc_base_net
            prev = this

            g_path2mod[path] = mod

        # for
    # with
    # process last batch of pins
    proc_base_net(pins)

    for prev in g_fwdshort:
        assert g_pin2dir[prev] == "o"
        for this in g_fwdshort[prev]:
            assert g_pin2dir[this] == "i"
    for this in g_bwdshort:
        assert g_pin2dir[this] == "i"
        for prev in g_bwdshort[this]:
            assert g_pin2dir[prev] == "o"

    # summary messages
    log(
f" - Read {n:,d} lines with " +
f"{n-(net+1):,d} pins on {net+1:,d} nets and " +
f"{len(g_path2mod):,d} instances of {len(seen):,d} leaves\n")
    b = "bram_phy"
    d = "dsp_phy"
    assert b in g_cols, f"b={b}, g_cols={g_cols}"
    assert d in g_cols
    log(
f" - Deduced {g_xmax}x{g_ymax} array with " +
f"BRAMs at x={','.join(str(x) for x in sorted(g_cols[b]))} and " +
f"DSPs at x={','.join(str(x) for x in sorted(g_cols[d]))}\n")

    # correct IO tiles now that we know array size
    later = {}
    for xy, bn in g_xy2bn.items():
        if bn != "io": continue
        (x, y) = xy
        # check x before y
        if x == 1:
            bn = "io_left"
        elif x == g_xmax:
            bn = "io_right"
        elif y == 1:
            bn = "io_bottom"
        elif y == g_ymax:
            bn = "io_top"
        else:
            assert 0
        later[xy] = bn
        #g_xy2bn[xy] = bn
    # for
    g_xy2bn.update(later)

    return netlist
# def read_base_nets

# IMPLEMENT --read_place

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

# IMPLEMENT --read_pack

g_adderlocs2net = {}
g_tilelocs2net = {}     # tile-level moded pb_type's

def walk_block(block, block_path):
    """ recursive routine to walk through block """

    name = block.elem.attrib.get("name")
    instance = block.elem.attrib.get("instance")
    mode = block.elem.attrib.get("mode")

    #ptnm = block.elem.attrib.get("pb_type_num_modes")   # only above/on wire-LUTs
    block_path.append((block, name, instance, mode))

    # recognize pb_types
    # *NB added lut5[1] here
    terminal = set('''
        inpad[0]                    outpad[0]
        ff[0]                       adder_carry[0]
        lut5[0]                     lut5[1]
        lut6[0]
        mem_36K[0]
        RS_DSP_MULT[0]              RS_DSP_MULT_REGIN[0]
        RS_DSP_MULT_REGOUT[0]       RS_DSP_MULT_REGIN_REGOUT[0]
        RS_DSP_MULTADD[0]           RS_DSP_MULTADD_REGIN[0]
        RS_DSP_MULTADD_REGOUT[0]    RS_DSP_MULTADD_REGIN_REGOUT[0]
        RS_DSP_MULTACC[0]           RS_DSP_MULTACC_REGIN[0]
        RS_DSP_MULTACC_REGOUT[0]    RS_DSP_MULTACC_REGIN_REGOUT[0]      '''.split())

    while instance in terminal and (name != "open" or mode == "wire"):

        name0 = block_path[0][1]
        (x, y, z, _) = g_blname2loc[name0]
        b = 0

        # check for IOs
        if   x == 1 or x == g_xmax or y == 1 or y == g_ymax:
            inst2pbtail = {
                    'ff[0]':        'ff',
                    'inpad[0]':     'pad',
                    'outpad[0]':    'pad',
            }
            pbtail = inst2pbtail[instance]
            # look back one and look for io_output[0] or io_input[0]
            previnst = block_path[-2][2]
            assert previnst in ('io_output[0]', 'io_input[0]', )
            if instance == 'ff[0]':
                # from an anno in openfpga.xml
                b = 1 if previnst == 'io_input[0]' else 0

        # check for BRAMs
        elif x in g_cols['bram_phy']:
            pbtail = "bram_phy"
        # check for DSPs
        elif x in g_cols['dsp_phy']:
            pbtail = "dsp_phy"
        # assume FF/carry/LUT
        else:
            inst2pbtail = {
                    'ff[0]':            'ff_phy',
                    'adder_carry[0]':   'adder_carry',
                    'lut5[0]':          'frac_lut6',
                    'lut5[1]':          'frac_lut6',    # *NB added
                    'lut6[0]':          'frac_lut6',
            }
            pbtail = inst2pbtail[instance]

            found = False
            negb = 0
            for i in block_path:
                g = re.search(r'^fle\[(\d+)\]$', i[2])
                if g:
                    z = int(g.group(1))
                    found = True
                if i[2] == "ble5[1]": b = 1
                if i[2] == "lut5[1]": b = 1 # *NB added
                # these two from an anno in openfpga.xml
                if i[2] == "ble6[0]": b = 1
                if i[2] == "adder[0]": negb = 1
            assert found
            if pbtail == "ff_phy":  # *NB made conditional on ff
                assert not b or not negb

            # look for wire-LUT declaration --> special handling
            ptnm = block.elem.attrib.get("pb_type_num_modes")
            assert (mode == "wire") == (ptnm == "2")
            if name == "open" and mode == "wire" or name != "open" and pbtail == "frac_lut6":
                # bitstream is not read yet
                #print(f"\tFOUND mode=wire")
                for inputss in block.inputss:
                    for port in inputss.ports:
                        assert port.elem.attrib.get("name") == "in"
                        prm = ' '.join([t if t == "open" else "0" for t in port.text.split()])
                        key = (pbtail, x, y, z, b)
                        #print(f"\tSAVED prm={prm} key={key}")
                        g_coords2instrmap[key] = (instance, prm, 0, name)
                if name == "open" and mode == "wire":
                    break

        # now enter this info
        key = (pbtail, x, y, z, b)
        g_coords2name[key] = name0
        g_coords2mode[key] = instance   # lut5 or lut6?
        global g_prevkey
        g_prevkey = key

        if pbtail == "adder_carry":
            g_adderlocs2net[(x, y, z)] = name
            if g_false: print(f"\t({x}, {y}, {z}) <== {name}")

        if pbtail == "dsp_phy":
            g_tilelocs2net[(x, y)] = name

        break
    # while
            
    # recognize "lut" inside lut variants and
    # look for port_rotation_map
    if instance == "lut[0]":
        for inputss in block.inputss:
            for port_rotation_map in inputss.port_rotation_maps:
                # save text if found
                g_coords2instrmap[g_prevkey] = (
                        g_coords2mode[g_prevkey], port_rotation_map.text, 1, name)
                #print(f"\tSAVED prm={port_rotation_map.text} key={g_prevkey}")
            # for
        # for
    # if

    for block2 in block.blocks:
        walk_block(block2, block_path)
    block_path.pop()

# def walk_block

def read_pack(xmlfile):
    """read in net_xml and crawl the info"""

    # read file specified
    log(f"{elapsed()} Reading {xmlfile}\n")
    g_vpr_xml = pack_dm.PackDataModel(xmlfile, validate=False)

    # walk through a typical pack XML
    for block in g_vpr_xml.blocks:
        block_path = []
        walk_block(block, block_path)

    unused = set(g_out2lutbits) - set(g_out2lut_used)
    nunused = len(unused)
    if nunused:
        if g_true:
            log(f" - {nunused:,d} LUT value{plural(nunused)} not used\n")
        else:
            bits = defaultdict(int)
            for lut in unused:
                bits[g_out2lutbits[lut]] += 1
            for pat in sorted(bits, key=lambda b: (len(b), b)):
                nunused = bits[pat]
                k = int(math.ceil(math.log2(len(pat))))
                log(f" - {nunused:,d} LUT{k} {pat} value{plural(nunused)} not used\n")
    for lut in sorted(unused, key=lambda o: (len(g_out2lutbits[o]), g_out2lutbits[o], o)):
        print(f"LUT value {g_out2lutbits[lut]} for {lut} not used")

# def read_pack

# describe global clock tree use
g_bit2net = {}      # clock# --> net
g_net2bit = {}      # net --> clock#
g_xyz2global = {}   # (x, y, z) --> net

def read_design_constraints(filename):
    """read fpga_repack_constraints.xml"""
    if filename is None or filename == "/dev/null": return
    with gzopenread(filename) as ifile:
        log(f"{elapsed()} Reading {filename}\n")
        for line in ifile:
            g = re.match(
r'\s*<pin_constraint\s+pb_type="([^"]+)" pin="([^"]+\[(\d+)\])" net="([^"]+)"', line)
            if not g: continue
            (pbt, pin, bit, net) = g.groups()
            if net.upper() == "OPEN": continue
            assert pbt in ("clb", "io", "bram", "dsp", )
            assert pin.startswith("clk[")
            bit = int(bit)

            announce = False
            if bit not in g_bit2net:
                g_bit2net[bit] = net
                announce = True
            elif g_bit2net[bit] != net:
                log(f" - WARNING: Clock tree {bit} is assigned multiple nets\n")

            if net not in g_net2bit:
                g_net2bit[net] = bit
                announce = True
            elif g_net2bit[net] != bit:
                log(f" - WARNING: Net {net} is assigned to multiple global trees\n")

            if announce:
                log(f" - Clock tree {bit} assigned net {net}\n")
                if net in g_blname2loc:
                    (x, y, z, _) = g_blname2loc[net]
                    g_xyz2global[(x, y, z)] = net
                else:
                    log( " - WARNING: This net absent from placed IO slots\n")
        # for
    # with
# def read_design_constraints

# IMPLEMENT --

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

def strip_complex(contents):
    """remove comments, strings, attributes, newlines"""
    # form tokens respecting comments, strings, attributes, others
    tokens = re.findall(r'''
        [/][*](?:[^*]|[*][^/])*[*][/]   |   # /* --- */
        [/][/][^\n]*                    |   # // ---
        ["](?:[^"\\]|\\.)*["]           |   # " --- "
        [(][*]                          |   # (*
        [*][)]                          |   # *)
        `[^\n]*                         |   # `something
        \n                              |   # newline
        .                                   # anything else
    ''', contents, re.VERBOSE)
    # replace comments, strings, and newlines with spaces
    comments = { "/*", "//", }
    tokens = [" " if t[:2] in comments or t[0] in '`"\n\t\f\r' else t for t in tokens]
    # reassemble and remove attributes (* --- *)
    return re.sub(r'[(][*]([^*]|[*][^)])*[*][)]', r' ', ''.join(tokens))
# def strip_complex

def next_tok(tokens, decls):
    """return next identifier"""
    for token in tokens:
        # handle normal identifiers
        g = re.match(r'([a-zA-Z_$][\w$]*)(?:\s*\[\s*(\d+)\s*(?::\s*(\d+)\s*)?\])?$', token)
        if not g:
            # handle escaped identifiers
            g = re.match(r'\\(\S+)(?:\s+\[\s*(\d+)\s*(?::\s*(\d+)\s*)?\])?$', token)
        if g:
            (t, l, r) = g.groups()
            if l is None and t in decls:
                (_, l, r) = decls[t]
            if l is None:
                yield t
                continue
            if r is None:
                yield f"{t}[{l}]"
                continue
            (l, r) = (int(l), int(r))
            inc = 1 if l <= r else -1
            for b in range(l, r + inc, inc):
                yield f"{t}[{b}]"
            continue
        # all else
        yield token
    # for
# def next_tok

def parse_stmt(stmt, decls):
    """parse one statement"""
    # 1: tokenize
    tokens = re.findall(r'''
[a-zA-Z_$][\w$]* \s* \[ \s* \d+ \s* : \s* \d+ \s* \]    |   # identifier with range
[a-zA-Z_$][\w$]* \s* \[ \s* \d+ \s* \]                  |   # identifier with select
[a-zA-Z_$][\w$]*                                        |   # identifier
\\\S+ \s+ \[ \s* \d+ \s* : \s* \d+ \s* \]               |   # escaped identifier with range
\\\S+ \s+ \[ \s* \d+ \s* \]                             |   # escaped identifier with select
\\\S+                                                   |   # escaped identifier
\d*\s*'[sS]?[bB]\s*[_0-1]*                              |   # constant
\d*\s*'[sS]?[oO]\s*[_0-7]*                              |   # constant
\d*\s*'[sS]?[dD]\s*[_0-9]*                              |   # constant
\d*\s*'[sS]?[hH]\s*[_0-9a-f]*                           |   # constant
[\#().]                                                     # punctuation
    ''', stmt, re.VERBOSE)

    # ignore "module" and "assign" statements
    other = { 'module', 'assign', }
    if not tokens or tokens[0] in other: return [[], {}, {}]

    # save declarations (ranges) from input/output/inout/wire
    g = re.match(
        r'(input|output|inout|wire)(?:\s*\[\s*(\d+)\s*(?::\s*(\d+)\s*)?\])?$', tokens[0])
    if g:
        (t, l, r) = g.groups()
        l = l if l is None else int(l)
        r = l if r is None else int(r)
        # save declarations
        for tok in tokens[1:]:
            if tok in "#().": continue
            if tok[0] == "\\": tok = tok[1:]
            decls[tok] = (t, l, r)
        return [[], {}, {}]

    # is this a valid instantiation?
    if tokens[-1] != ")" or (tokens[1] != "#" and tokens[2] != "("):
        return [[], {}, {}]

    # 2: break into categories
    delta = { "(": +1, ")": -1, }
    useinfo = [ ]
    parms = [] ; conns = []
    receive = conns
    d = 0
    for t in next_tok(tokens, decls):
        #print(f"t={t} d={d} useinfo={useinfo} parms={parms} conns={conns}")
        if t in delta:
            d += delta[t]
            if not d:
                receive = conns
            continue
        if t == "#":
            receive = parms
            continue
        if t == ".":
            receive.append([])
            continue

        # not needed?
        t = re.sub(r'\s', r'', t)
        if t[0] == "\\": t = t[1:]

        if d:
            receive[-1].append(t)
        else:
            useinfo.append(t)
    # for
    assert len(useinfo) == 2

    # 3: convert to hashtables
    hparms = {} ; hconns = {}
    for l, h in [[parms, hparms], [conns, hconns]]:
        for sublist in l:
            if sublist: h[sublist[0]] = sublist[1:]
        # for
    # for

    # RESULT: useinfo, hparms, hconns
    #print(f"useinfo={useinfo}\nhparms={hparms}\nhconns={hconns}")
    return [useinfo, hparms, hconns]
# def parse_stmt

def genports(block0, table, drop = r'', split = r''):
    """create list of used ports"""
    block = block0
    skip = False
    splits = defaultdict(list)
    for name, num in zip(table[0::2], table[1::2]):
        # handle ports conditional on block passed in
        if num == 0:
            skip = not bool(re.search(name, block0))
            continue
        if skip: continue

        # handle dropping subfields from name
        if drop != r'': name = re.sub(drop, r'', name)

        # handle splits
        if split != r'':
            g = re.search(r'_[AB]', name)
            if g:
                block = block0 + g.group()
            else:
                block = ""

        # handle port
        newports = [name] if num is None else [f"{name}[{b}]" for b in range(num)]
        if block:
            splits[block].extend(newports)
            continue
        for b, ports in splits.items():
            ports.extend(newports)
    # for
    return splits
# def genports
            
g_bram_ports = [
   "CLK_A1_i",       1,     
   "CLK_A2_i",       1,     
   "WDATA_A1_i",    18,     
   "WDATA_A2_i",    18,     
   "ADDR_A1_i",     15,     
   "ADDR_A2_i",     14,     
   "REN_A1_i",       1,     
   "REN_A2_i",       1,     
   "WEN_A1_i",       1,     
   "WEN_A2_i",       1,     
   "BE_A1_i",        2,     
   "BE_A2_i",        2,     
   "RDATA_A1_o",    18,     
   "RDATA_A2_o",    18,     

   "CLK_B1_i",       1,     
   "CLK_B2_i",       1,     
   "WDATA_B1_i",    18,     
   "WDATA_B2_i",    18,     
   "ADDR_B1_i",     15,     
   "ADDR_B2_i",     14,     
   "REN_B1_i",       1,     
   "REN_B2_i",       1,     
   "WEN_B1_i",       1,     
   "WEN_B2_i",       1,     
   "BE_B1_i",        2,     
   "BE_B2_i",        2,     
   "RDATA_B1_o",    18,     
   "RDATA_B2_o",    18,     

   "FLUSH1_i",       1,     
   "FLUSH2_i",       1,     
]

g_dsp_ports = [
    r'^RS_DSP_MULT.',    0,
    "clk",               1,     
    "lreset",            1,     

    r'.',                0,
    "a_i",              20,     
    "b_i",              18,     
    "feedback",          3,     
    "unsigned_a",        1,     
    "unsigned_b",        1,     
    "z_o",              38,     

    r'MULTADD|MULTACC',  0,
    "subtract",          1,
    "load_acc",          1,
    "saturate_enable",   1,     
    "shift_right",       6,
    "round",             1,

    r'MULTADD',          0,
    "acc_fir_i",         6,     
    "dly_b_o",          18,     
]

# NO LONGER USED
def pbports(v):
    """expand pb_type ports"""
    result = []
    for item in v.split(','):
        g = re.match(r'(\w+)\[(\d+):(\d+)\]$', item)
        if not g:
            result.append(item)
            continue
        (w, l, r) = g.groups()
        (l, r) = (int(l), int(r))
        inc = 1 if l <= r else -1
        result.extend([f"{w}[{b}]" for b in range(int(l), int(r) + inc, inc)])
    # for
    return result
# def pbports

# one for each DSP mode
g_dpins = { # NO LONGER USED
    "NC1": "acc_fir_i[0:5]", "NC2": "load_acc[0]", "NC3": "lreset[0]",
    "NC4": "saturate_enable[0]", "NC5": "shift_right[0:5]", "NC6": "round[0]",
    "NC7": "subtract[0]", "NC8": "clk[0]",
    "A1": "a_i[0:9]", "A2": "a_i[10:19]",
    "B1": "b_i[0:8]", "B2": "b_i[9:17]",
    "DLY_B1": "dly_b_o[0:8]", "DLY_B2": "dly_b_o[9:17]",
    "FEEDBACK": "feedback[0:2]", "UNSIGNED_A": "unsigned_a[0]", "UNSIGNED_B": "unsigned_b[0]",
    "Z1": "z_o[0:18]", "Z2": "z_o[19:37]",
}

# TDP_RAM36K
g_bpins = { # NO LONGER USED
    "NC0": "ADDR_A2_i[0:13]", "NC1": "ADDR_B2_i[0:13]",
    "NC2": "REN_A2_i[0]", "NC3": "WEN_A2_i[0]", "NC4": "REN_B2_i[0]", "NC5": "WEN_B2_i[0]",
    "NC6": "FLUSH1_i[0]", "NC7": "FLUSH2_i[0]",
    "NC8": "CLK_A2_i[0]", "NC9": "CLK_B2_i[0]",
    "RDATA_A": "RDATA_A1_o[0:15],RDATA_A2_o[0:15]",
    "RPARITY_A": "RDATA_A1_o[16:17],RDATA_A2_o[16:17]",
    "RDATA_B": "RDATA_B1_o[0:15],RDATA_B2_o[0:15]",
    "RPARITY_B": "RDATA_B1_o[16:17],RDATA_B2_o[16:17]",
    "WDATA_A": "WDATA_A1_i[0:15],WDATA_A2_i[0:15]",
    "WPARITY_A": "WDATA_A1_i[16:17],WDATA_A2_i[16:17]",
    "WDATA_B": "WDATA_B1_i[0:15],WDATA_B2_i[0:15]",
    "WPARITY_B": "WDATA_B1_i[16:17],WDATA_B2_i[16:17]",
    "ADDR_A": "ADDR_A1_i[0:14]", "ADDR_B": "ADDR_B1_i[0:14]",
    "BE_A": "BE_A1_i[0:1],BE_A2_i[0:1]",
    "BE_B": "BE_B1_i[0:1],BE_B2_i[0:1]",
    "REN_A": "REN_A1_i[0]", "REN_B": "REN_B1_i[0]",
    "WEN_A": "WEN_A1_i[0]", "WEN_B": "WEN_B1_i[0]",
    "CLK_A": "CLK_A1_i[0]", "CLK_B": "CLK_B1_i[0]",
}

# NO LONGER USED
def read_verilog(sfilename, vfilename):
    """read LUT contents from Verilog"""
    with open(vfilename, encoding='ascii') as ifile:
        log(f"{elapsed()} Reading {vfilename}\n")
        contents = ifile.read()
    # simplify
    contents = strip_complex(contents)

    # first pass through statements
    # determine I_BUF --> CLK_BUF nets
    triples = []
    clk_buf_i_port_nets = set()
    decls = {}
    for stmt in contents.split(';'):
        (useinfo, hparms, hconns) = parse_stmt(stmt, decls)
        if not useinfo: continue
        triples.append([useinfo, hparms, hconns])
        if useinfo[0] == "CLK_BUF":
            clk_buf_i_port_nets.add(hconns['I'][0])
    # for

    # step through statements, update info, write a.sim
    defined = set()
    shorts = uf.uf()
    counts = defaultdict(int)
    g_nc = 0
    with open(sfilename, "w", encoding='ascii') as ofile:
        log(f"{elapsed()} Writing {sfilename}\n")
        ofile.write("; default to MIT format\n")
        lines = []
        for triple in triples:
            (useinfo, hparms, hconns) = triple

            # LUT
            if 'INIT_VALUE' in hparms and 'Y' in hconns and useinfo[0].startswith("LUT"):

                k = int(useinfo[0][3:])

                value = hparms['INIT_VALUE'][0]
                bits = val2bits(value)
                assert 1 << k == len(bits)

                output = hconns['Y'][0]
                assert output not in g_out2lutbits
                g_out2lutbits[output] = bits

                if g_dump_luts:
                    print(f"LUT k={k} value={bits} inst={useinfo[1]} output={output}")

                # write out a.sim LUT instance
                mask = [int(b) for b in bits]
                inputs = bin(len(mask))[::-1].index('1')
                insigs = hconns['A']
                assert inputs == len(insigs), \
f"k={k} value={value} bits={bits} mask={mask} inputs={inputs} insigs={insigs}"
                #which, lut, args, activein, bits
                (    _, lut, args,        _,    _) = (
                    proc_lut(mask, inputs, 0, len(mask)))   # unused
                if lut not in defined:
                    ofile.write(f"DEFINE {lut} {args}\n")
                    defined.add(lut)
                #insigs = ' '.join(insigs)
                lines.append([lut] + insigs + [output])
                counts['LUT'] += 1
                #ofile.write(f"{lut} {insigs} {output}\n")

                continue
            # if

            # O_BUF, CLK_BUF: served by 1 slot
            if useinfo[0] in ("O_BUF", "CLK_BUF", ):
                if useinfo[0] == "O_BUF":
                    f = hconns['I']
                elif useinfo[0] == "CLK_BUF":
                    f = hconns['O']
                    assert f[0] in g_net2bit
                    #print("GOT CLK_BUF")
                else:
                    assert 0
                assert len(f) == 1
                f = f[0]
                if "OPAD" not in defined:
                    ofile.write( "DEFINE OPAD f\n")
                    defined.add("OPAD")
                lines.append(["OPAD", f])
                counts['OPAD'] += 1
                #ofile.write(f"OPAD {f} {a}\n")
                continue
            # if

            # I_BUF: served by 2 slots unless clock
            if useinfo[0] == "I_BUF":

                f = hconns['O']
                assert len(f) == 1
                f = f[0]
                if f not in clk_buf_i_port_nets:
                    if "IPAD" not in defined:
                        ofile.write( "DEFINE IPAD f\n")
                        defined.add("IPAD")
                    lines.append(["IPAD", f])
                    counts['IPAD'] += 1
                    #ofile.write(f"IPAD {a} {f}\n")
                else:
                    log(f" - Dropped I_BUF driving clock {f}\n")

                f = hconns['EN']
                assert len(f) == 1
                f = f[0]
                if "OPAD" not in defined:
                    ofile.write( "DEFINE OPAD f\n")
                    defined.add("OPAD")
                lines.append(["OPAD", f])
                counts['OPAD'] += 1
                #ofile.write(f"OPAD {f} {a}\n")
                continue
            # if

            # FF
            if useinfo[0] == "DFFRE":
                (cpin, dpin, epin, rpin, qpin) = [hconns[n][0] for n in "CDERQ"]
                if "DFFRE" not in defined:
                    ofile.write( "DEFINE DFFRE c d e r q\n")
                    defined.add("DFFRE")
                lines.append(["DFFRE", cpin, dpin, epin, rpin, qpin])
                counts['DFFRE'] += 1
                continue
            # if

            # DSP19X2
            if useinfo[0] == "DSP19X2":
                # we ignore parameters since we don't check DSP modes
                if "DSP" not in defined:
                    allpins = []
                    for _, v in g_dpins.items():
                        allpins.extend(pbports(v))
                    ofile.write(f"DEFINE DSP {' '.join(allpins)}\n")
                    defined.add("DSP")
                conns = []
                for k, v in g_dpins.items():
                    ports = pbports(v)
                    lp = len(ports)
                    if k.startswith("NC"):
                        conns.extend([f"nc{g_nc + i}" for i in range(lp)])
                        g_nc += lp
                        continue
                    lc = len(hconns[k])
                    if lc >= lp:
                        conns.extend(hconns[k][-lp:])
                        continue
                    e = lp - lc
                    conns.extend([f"nc{g_nc + i}" for i in range(e)])
                    conns.extend(hconns[k])
                    g_nc += e
                # for
                lines.append(["DSP", *conns])
                counts['DSP'] += 1
                continue
            # if

            # TDP_RAM36K
            if useinfo[0] == "TDP_RAM36K":
                if "BRAM" not in defined:
                    allpins = []
                    for _, v in g_bpins.items():
                        allpins.extend(pbports(v))
                    ofile.write(f"DEFINE BRAM {' '.join(allpins)}\n")
                    defined.add("BRAM")
                conns = []
                for k, v in g_bpins.items():
                    ports = pbports(v)
                    lp = len(ports)
                    if k.startswith("NC"):
                        conns.extend([f"nc{g_nc + i}" for i in range(lp)])
                        g_nc += lp
                        continue
                    lc = len(hconns[k])
                    if lc >= lp:
                        conns.extend(hconns[k][-lp:])
                        continue
                    e = lp - lc
                    conns.extend([f"nc{g_nc + i}" for i in range(e)])
                    conns.extend(hconns[k])
                    g_nc += e
                # for
                lines.append(["BRAM", *conns])
                counts['BRAM'] += 1
                continue
            # if

            assert 0, f"useinfo={useinfo}"
        # for

        # write out instances, incorporating net renaming from shorts
        for line in lines:
            for i, l in enumerate(line, start=0):
                if i: line[i] = shorts.find(l)
            ofile.write(' '.join(line) + "\n")
    # with

    nl = len(g_out2lutbits)
    log(f" - {nl:,d} LUT{plural(nl)} specified\n")
    counts = ' '.join([f"{k}#{v:,d}" for k, v in sorted(counts.items())])
    log(f" - Wrote {counts}\n")

# def read_verilog

# NOT CURRENTLY USED
def read_verilog_order(vfilename):
    """read LUT contents from Verilog"""
    if not g_use_verilog: return
    with open(vfilename, encoding='ascii') as ifile:
        log(f"{elapsed()} Reading {vfilename}\n")
        contents = ifile.read()

    # step through statements and collect LUT info
    decls = {}
    lines = []
    for stmt in strip_complex(contents).split(';'):
        (useinfo, hparms, hconns) = parse_stmt(stmt, decls)
        if not useinfo: continue

        if 'INIT_VALUE' in hparms and 'Y' in hconns and useinfo[0].startswith("LUT"):

            k = int(useinfo[0][3:])

            value = hparms['INIT_VALUE'][0]
            bits = val2bits(value)
            assert 1 << k == len(bits)

            output = hconns['Y'][0]
            if g_false:
                assert output not in g_out2lutbits
                g_out2lutbits[output] = bits

            if g_dump_luts:
                print(f"LUT k={k} value={bits} inst={useinfo[1]} output={output}")

            mask = [int(b) for b in bits]
            inputs = bin(len(mask))[::-1].index('1')
            insigs = hconns['A']
            assert inputs == len(insigs), \
f"k={k} value={value} bits={bits} mask={mask} inputs={inputs} insigs={insigs}"
            #which, lut, args, activein, bits
            (    _, lut,    _,        _,    _) = (
                proc_lut(mask, inputs, 0, len(mask)))   # unused
            lines.append([lut] + insigs + [output])
            g_out2inps[output] = insigs
            if g_dump_luts: print(f" - SAVING {output}")
        # if
    # for

    nl = len(lines)
    log(f" - {nl:,d} LUT{plural(nl)} specified\n")
# def read_verilog_order

g_drop_adders = False
g_net2addertype = {}
g_net2tiletype = {}     # (out) net name --> driving tile's type variant

def read_transformed_eblif(simfilename, ebliffilename):
    """read netlist from transformed EBLIF written on request by VPR"""
    inputs = set()      # inputs 
    outputs = set()     # outputs 
    g_out2lutinps = {}  # LUTs 
    subckts = []        # SUBCKTs 
    with open(ebliffilename, encoding='ascii') as ifile:
        log(f"{elapsed()} Reading {ebliffilename}\n")
        for line in ifile:
            ff = line.split()
            # blanks and comments
            if not ff or ff[0][0] == "#": continue
            # NB make the rest a jump table if too slow
            # useless commands
            if ff[0] in (".model", ".param", ".end"): continue
            # inputs
            if ff[0] == ".inputs":
                inputs.update(ff[1:])
                continue
            # outputs
            if ff[0] == ".outputs":
                outputs.update(ff[1:])
                continue
            # LUT connections
            if ff[0] == ".names":
                (_, *innets, outnet) = ff
                bits = [0] * (1 << len(innets))
                assert outnet not in g_out2lutbits
                g_out2lutbits[outnet] = bits
                g_out2lutinps[outnet] = innets
                continue
            # LUT values
            if ff[0] in ("0", "1"):
                idx = sum(int(v) << i for i, v in enumerate(ff[:-1], start=0))
                # this inverts all inputs, a property of OpenFPGA's frac_lut6_mux
                idx = (len(bits) - 1) - idx
                bits[idx] = int(ff[-1])
                continue
            # SUBCKTs
            if ff[0] == ".subckt":
                conns = dict(f.split('=') for f in ff[2:])
                subckts.append([ff[1], conns])
                continue
            assert 0, f"ff={ff}"
        # for
    # with

    if g_dump_luts:
        for outnet, bits in g_out2lutbits.items():
            print(f" - LUT {bits} ==> {outnet}")

    # discover wire-LUTs used as "assign"s and short them out
    shorts0 = uf.uf()
    removed = set()
    kgen = 0
    wlut = 0
    kgens = 0
    wluts = 0
    for sweep in range(2):
        for outnet, bits in g_out2lutbits.items():
            if sweep == 0 and len(bits) == 2:
                wlut += 1
                if not g_wlut & g_simplify: continue
                # NB inputs are inverted
                if bits[0] != 1 or bits[1] != 0: continue
                #print("FOUND ASSIGN")
                innets = g_out2lutinps[outnet]
                assert len(innets) == 1
                shorts0.union(innets[0], outnet)
                removed.add(outnet)
                wluts += 1
            if sweep == 1 and len(bits) == 1:
                kgen += 1
                if not g_kgen & g_simplify: continue
                #print(f"FOUND CONSTANT {bits[0]}")
                if bits[0] == 0:
                    shorts0.union(outnet, "$false")
                    removed.add(outnet)
                else:
                    shorts0.union(outnet, "$true")
                    removed.add(outnet)
                kgens += 1
        #  for
    # for

    # give each group of aliases its preferred name
    # create set for each root
    root2nodes = defaultdict(set)
    for n in shorts0.keys():
        r = shorts0.find(n)
        root2nodes[r].add(n)
    # for
    # select preferred name
    consts = { '$false': 0, '$undef': 1, '$true': 2, }
    shorts = {}
    for r, nodes in root2nodes.items():
        nodes = sorted(nodes, key=lambda n: (consts.get(n, 3), len(n)))
        for n in nodes:
            shorts[n] = nodes[0]
        # for
    # for

    # create all statements
    # we will uniformly apply shorting (assigns) at the end
    defined = set()
    counts = defaultdict(int)
    g_nc = 0
    lines0 = []
    lines = []
    dropped = []
    with open(simfilename, "w", encoding='ascii') as ofile:
        log(f"{elapsed()} Writing {simfilename}\n")
        lines0.append("; default to MIT format\n")

        # inputs
        for inp in inputs:
            if inp in g_net2bit:
                dropped.append(f" - Dropped input slot driving global {inp}\n")
                continue
            if "IPAD" not in defined:
                lines0.append( "DEFINE IPAD f\n")
                defined.add("IPAD")
            lines.append(["IPAD", inp])
            counts['IPAD'] += 1
        # for

        # outputs
        for outp in outputs:
            if "OPAD" not in defined:
                lines0.append( "DEFINE OPAD f\n")
                defined.add("OPAD")
            lines.append(["OPAD", outp])
            counts['OPAD'] += 1
            continue
        # for

        # LUTs
        for outnet, bits in g_out2lutbits.items():
            if outnet in removed: continue
            # order from EBLIF
            innets0 = g_out2lutinps[outnet]

            if g_use_verilog:
                # order from Verilog
                if len(bits) <= 2:
                    # no difference for (bits, inps) being (1, 0) or (2, 1)
                    innets  = innets0
                else:
                    innets  = g_out2inps[outnet]
                    if g_dump_luts:
                        print(f" - EBLIF   inputs = {innets0}")
                        print(f" - Verilog inputs = {innets}")
                assert len(innets0) == len(innets)
                assert set(innets0) == set(innets)
            else:
                # ignoring Verilog
                innets = innets0

            k = len(innets)
            assert 1 << k == len(bits)
            # write out a.sim LUT instance

            #which, lut, args, activein, bits   # eblif --> a.sim
            prm = None
            if g_tolerate_dc:
                prm = list(range(k))
            (    _, lut, args,        _,    _) = proc_lut(bits, k, 0, len(bits), prm)
            if lut not in defined:
                lines0.append(f"DEFINE {lut} {args}\n")
                defined.add(lut)
            if not g_ordered_luts:
                innets = sorted(innets, key=dict_aup_nup)
            lines.append([lut] + [outnet] + innets)
            counts['LUT'] += 1
        # for

        # subcircuits
        for master, hconns in subckts:

            # FF
            # DFFRE in Verilog, dffre in EBLIF. We don't track clock polarity.
            if master in ("dffre", "dffnre"):
                (cpin, dpin, epin, rpin, qpin) = [hconns[n] for n in "CDERQ"]
                if "DFFRE" not in defined:
                    lines0.append( "DEFINE DFFRE q c d e r\n")
                    defined.add("DFFRE")
                lines.append(["DFFRE", qpin, cpin, dpin, epin, rpin])
                counts['DFFRE'] += 1
                continue
            # if

            # adder_carry
            if master == "adder_carry":
                allpins = ["cout", "sumout", "cin", "g", "p", ]
                cpins  = [p for p in allpins if p in hconns]
                addtypes = {
                    "cout g p":             "ADDLSB",   # cin=nc, sumout=nc
                    "cout sumout cin g p":  "ADD",      # all connected
                    "sumout cin g p":       "ADDMSB",   # cout=nc
                }
                add = addtypes[' '.join(cpins)]
                if add != "ADD":
                    g_net2addertype[hconns[cpins[0]]] = add
                    if g_false: print(f"\t{hconns[cpins[0]]} ({cpins[0]}) <== {add}")
                if add not in defined:
                    lines0.append(f"DEFINE {add} {' '.join(cpins)}\n")
                    defined.add(add)
                instline = [ add ]
                for p in allpins:
                    if p in hconns:
                        instline.append(hconns[p])
                # for
                if not g_drop_adders:
                    lines.append(instline)
                counts[add] += 1
                continue
            # if

            if master.startswith("RS_DSP_"):
                # reading transformed EBLIF
                dsp_info = genports(master, g_dsp_ports, r'_[io]$')
                #print(f"dsp_info={dsp_info}\nhconns={hconns}")
                for block, allpins in dsp_info.items():
                    if block not in defined:
                        lines0.append(f"DEFINE {block} {' '.join(allpins)}\n")
                        defined.add(block)
                    instline = [ block ]
                    for p in allpins:
                        if p in hconns:
                            instline.append(hconns[p])
                            # need better "output" test
                            if p.startswith("z") or p.startswith("dly_b"):
                                assert hconns[p] not in g_net2tiletype
                                g_net2tiletype[hconns[p]] = master
                        elif p.endswith("[0]") and p[:-3] in hconns:
                            p = p[:-3]
                            instline.append(hconns[p])
                            # need better "output" test
                            if p.startswith("z") or p.startswith("dly_b"):
                                assert hconns[p] not in g_net2tiletype
                                g_net2tiletype[hconns[p]] = master
                        else:
                            instline.append(f"nc{g_nc}")
                            g_nc += 1
                    # for
                    lines.append(instline)
                    counts[block] += 1
                continue
            # if

            if master == "RS_TDP36K":
                bram_info = genports("BRAM", g_bram_ports, r'_[io]$', r'_[AB]')
                for block, allpins in bram_info.items():
                    if block not in defined:
                        lines0.append(f"DEFINE {block} {' '.join(allpins)}\n")
                        defined.add(block)
                    instline = [ block ]
                    for p in allpins:
                        if p in hconns:
                            instline.append(hconns[p])
                        elif p.endswith("[0]") and p[:-3] in hconns:
                            instline.append(hconns[p[:-3]])
                        else:
                            instline.append(f"nc{g_nc}")
                            g_nc += 1
                    # for
                    lines.append(instline)
                    counts[block] += 1
                # for
                continue
            # if

            # didn't recognize this block
            assert 0, f"master={master} hconns={hconns}"
        # for

        # write out headers and DEFINEs
        for line in sorted(lines0, key=dict_aup_nup):
            ofile.write(line)

        # write out instances, renaming signals subject to shorting
        lines0 = []
        for line in lines:
            for i, l in enumerate(line, start=0):
                if i: line[i] = shorts.get(l, l)
            # for
            lines0.append(' '.join(line) + "\n")
        # for
        lead = " "
        for line in sorted(lines0, key=dict_aup_nup):
            if not line.startswith(lead):
                lead = re.sub(r'[^A-Z].*', r'', line)
                for _ in range(2):
                    ofile.write(f"; section {lead}\n")
            ofile.write(line)
    # with

    nl = len(g_out2lutbits)
    log(f" - {nl:,d} LUT{plural(nl)} specified\n")
    #log(
    #f" - Recognized {kgen:,d} constant generator{plural(kgen)} and " +
    #f"{wlut:,d} wire-LUT{plural(wlut)}\n")
    if kgens or wluts:
        log(
f" - Simplified {kgens:,d} constant generator{plural(kgen)} and " +
f"{wluts:,d} wire-LUT{plural(wlut)}\n")
    for d in dropped:
        log(d)
    counts = ' '.join([f"{k}#{v:,d}" for k, v in sorted(counts.items())])
    log(f" - Wrote {counts}\n")
# def read_transformed_eblif

# IMPLEMENT --read_bitstream

def read_bitstream(filename):
    """read fabric bitstream from xml"""
    # read bit stream
    assert filename.endswith(".xml") or filename.endwith(".xml.gz")
    with gzopenread(filename) as ifile:
        log(f"{elapsed()} Reading {filename}\n")
        for line in ifile:
            if '<bit ' not in line: continue
            (path, idx) = re.sub(r'.* path="fpga_top.([^"]+).mem_out\[(\d+)\]".*$', r'\1 \2',
                                line).split()
            path = path.translate(g_translate)
            # CONSISTENT: use mux not mem everywhere
            path = re.sub(r'^(.*Z)mem([^Z]+)$' , r'\1mux\2', path)
            val = re.sub(r'^.* value="([01])".*$', r'\1', line, flags=re.DOTALL)
            g_settings[path][int(idx)] = int(val)
        # for
    # with
# def read_bitstream

# IMPLEMENT --read_set_muxes

def read_set_muxes(filename):
    """read in set_muxes.txt file, return dictionary of set muxes"""

    # Step 1
    frag2grouped = {}
    for xy in sorted(g_xy2wh):
        (wo, ho) = g_xy2wh[xy] 
        (x, y) = xy
        if not wo and not ho:
            bname = g_xy2bn[xy]
            frag2grouped[f"grid_{bname}_{x}__{y}_"] = f"tile_{x}__{y}_Zlb"
        xo = f"x{wo}" if wo else ""
        yo = f"y{ho}" if ho else ""
        frag2grouped[f"sb_{x}__{y}_"] = f"tile_{x-wo}__{y-ho}_Zsb{xo}{yo}"
        frag2grouped[f"cbx_{x}__{y}_"] = f"tile_{x-wo}__{y-ho}_Ztb{xo}{yo}"
        frag2grouped[f"cby_{x}__{y}_"] = f"tile_{x-wo}__{y-ho}_Zrb{xo}{yo}"
    # for

    # Step 2
    mux_paths = {}
    with open(filename, encoding='ascii') as ifile:
        if filename != "/dev/null": log(f"{elapsed()} Reading {filename}\n")
        for line in ifile:
            line = line.translate(g_translate)
            ff = line.split()
            if not ff: continue
            pp = ff[0].split('Z')

            if g_false:
                # this is now done in writing, no longer reading
                top = pp.pop(0)
                assert top == "fpga_top"
                pp[0] = frag2grouped[pp[0]]

            assert pp[-1].startswith("mem_")
            # CONSISTENT: use mux not mem everywhere
            pp[-1] = re.sub(r'^mem_', r'mux_', pp[-1])
            path = 'Z'.join(pp)
            mux_paths[path] = 0
        # for
    # with
    return mux_paths
# def read_set_muxes

# IMPLEMENT apply_bitstream_muxes

# this handles the way muxes are designed by OpenFPGA
def sel2idx(sz, sel):
    """ given mem value, what idx is selected?"""

    if sz not in g_siz2sel2idx:
        power2 = sz
        pairs = 0
        while power2 & (power2 - 1):
            power2 -= 1
            pairs += 1
        assert not power2 & (power2 - 1)
        assert sz == pairs + power2
        if pairs:
            for p in range(pairs):
                s = (power2 - 1 - p ) * 2 + 1; i = p * 2 + 0
                g_siz2sel2idx[sz][s] = i
                s = (power2 - 1 - p ) * 2 + 0; i = p * 2 + 1
                g_siz2sel2idx[sz][s] = i
            # for
            for ii in range(pairs * 2, sz):
                # last line handles don't care
                s = (sz     - 1 - ii) * 2 + 0
                g_siz2sel2idx[sz][s] = ii
                s = (sz     - 1 - ii) * 2 + 1
                g_siz2sel2idx[sz][s] = ii
            # for
        else:
            for ii in range(sz):
                s =  sz     - 1 - ii 
                g_siz2sel2idx[sz][s] = ii
            # for
        # if
        #if sz == 28:
        #    print(f"SIZE {sz:2d}")
        #    for sel2 in sorted(g_siz2sel2idx[sz]):
        #        idx2 = g_siz2sel2idx[sz][sel2]
        #        print(f"\ts={sel2:2d} idx2={idx2:2d}")
    # if

    return g_siz2sel2idx[sz][sel]
# def sel2idx

def apply_bitstream_muxes(netlist, mux_paths):
    """short out muxes as specified by bitstream and settings"""
    # short out muxes
    skipped = 0
    applied = 0
    g_used_ipins.clear()
    for path, mod in g_path2mod.items():
        g = re.search(r'_size(\d+)$', mod)
        if not g: continue

        if mux_paths and path not in mux_paths:
            #print(f"SKIPPED: {path}")
            skipped += 1
            continue
        applied += 1
        mux_paths[path] += 1

        sz = int(g.group(1))
        selection = 0
        assert path in g_settings, f"path={path}"
        for i, idx in enumerate(sorted(g_settings[path]), start=0):
            assert i == idx
            selection += g_settings[path][idx] << i
        idx = sel2idx(sz, selection)
        node1 = f"{path}ZinX{idx}Y"
        node2 = f"{path}Zout"
        assert node1 in netlist._dict
        assert node2 in netlist._dict
        g_used_ipins.add(node1)
        g_used_opins.add(node2)
        assert node1 in g_pin2dir and g_pin2dir[node1] == "i"
        assert node2 in g_pin2dir and g_pin2dir[node2] == "o"
        #assert node1 in netlist._dict
        #assert node2 in netlist._dict
        #netlist.add(node1)
        #netlist.add(node2)
        #print(f"MUX\t{node1}\n==>\t{node2}")
        netlist.union(node1, node2)
        g_fwdshort[node1].add(node2)
        g_bwdshort[node2].add(node1)
        #print(f"sz={sz} selection={selection} idx={idx} path={path}")
        #print(f"g_settings[path] = {g_settings[path]}")
        #print(f"node1={node1}")
        #print(f"node2={node2}")
    # for
    unvisited = len([p for p, c in mux_paths.items() if c == 0])
    log(
f" - {applied:,d} base muxes configured, " +
f"{skipped:,d} skipped, " +
f"{unvisited:,d} settings unvisited\n")

    cin = 0
    for p, c in mux_paths.items():
        if c: continue
        if re.match(
r'tile_\d+__\d+_ZlbZlogical_tile_clb_mode_clb__0Zclb_lrZmux_fle_0_cin_0$', p):
            cin += 1
        else:
            log(f" - WARNING: Unvisited setting: {p}\n")
    # for
    if cin:
        log(
f" - {cin:,d} unvisited setting{plural(cin)} " +
 "due to erroneous 'complete' on CLB-level carry_in\n")
# def apply_bitstream_muxes

# IMPLEMENT write_b_netlist

# NB: Note all inputs to LUTs are inverted, due to 2:1 mux wiring.
def proc_lut(mask, inputs, lmin, llim, prm=None):
    """process LUT mask"""

    # which inputs change the output? posunate? negunate?
    activein = [0] * inputs
    posunate = [1] * inputs
    negunate = [1] * inputs
    for i in range(inputs):
        im = 1 << i
        wiggled = 0
        for b in range(lmin, llim):
            mb = b ^ im
            (f, mf) = (mask[b], mask[mb])
            wiggled |= f ^ mf
            # NB: inversion handled by swapping the two if's
            # inverted input falls, logical input rises
            if b > mb:
                # output rises --> not negunate
                if f < mf: negunate[i] = 0
                # output falls --> not posunate
                if f > mf: posunate[i] = 0
            # inverted input rises, logical input falls
            if b < mb:
                # output rises --> not posunate
                if f < mf: posunate[i] = 0
                # output falls --> not negunate
                if f > mf: negunate[i] = 0
        activein[i] = wiggled
        # unchanging inputs are not either unate
        if not wiggled:
            posunate[i] = 0
            negunate[i] = 0
    # for

    # tolerate DON'T-CARE inputs
    if prm:
        a = 0; dc = 0
        prevactivein = list(activein)
        for i, p in enumerate(prm, start=0):
            if p == "open": continue
            if not activein[i]:
                dc += 1
                activein[i] = 1
            a += 1
        # for
        assert a == sum(activein), (
f"mask={mask} inputs={inputs} lmin={lmin} llim={llim} prm={prm} a={a} dc={dc} " +
f"prevactivein={prevactivein} activein={activein}")
        if dc:
            log(f" - WARNING: a LUT{a} has {dc} don't-care input{plural(dc)}\n")
    # if

    # number of active inputs
    actives = sum(activein)

    # classify
    if   actives == 0:
        # 0 (ZERO) or -1 (ONE)
        which = -mask[lmin]
    elif actives == 1:
        # no such thing as a non-unate single input func
        assert sum(negunate) + sum(posunate) == 1
        # 1 (INV) or -2 (wire-LUT)
        if sum(negunate):
            which = 1
        else:
            which = -2
    else:
        # 2+ (function)
        which = actives
    # if

    # other return values
    pu = len([u for u in posunate if u])
    nu = len([u for u in negunate if u])
    lut = f"LUT{actives}P{pu}N{nu}"
    if g_ordered_luts:
        args = [ f"i{i}" for i in range(actives) ]
    else:
        args = [ f"i{0}" for _ in range(actives) ]
    args.insert(0, "out")
    args = ' '.join(args)

    if g_false:
        print(f"\tw{inputs},{lmin:02d}-{llim:02d} activein={activein} " +
              f"posunate={posunate} negunate={negunate}")
        print(f"\t\tDEVICE {lut} {args}")

    return [which, lut, args, activein, mask[lmin:llim]]
# def proc_lut

def read_lut(x, y, z, inputs, lmin, llim, prm=None):
    """read lut mask from config bits"""
    path = [
 #"fpga_top",
f"tile_{x}__{y}_",
 "lb",
 "logical_tile_clb_mode_clb__0",
 "logical_tile_clb_mode_default__clb_lr_0",
f"logical_tile_clb_mode_default__clb_lr_mode_default__fle_{z}",
 "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__fabric_0",
 "logical_tile_clb_mode_default__clb_lr_mode_default__fle_mode_physical__" +
    "fabric_mode_default__frac_lut6_0",
 "frac_lut6_RS_LATCH_mem",
    ]
    path = 'Z'.join(path)
    assert path in g_settings

    # NB: mask always has all bits.
    # below we limit the access to [lmin, llim)
    mask = [int(g_settings[path][b]) for b in range(64)]
    assert len(mask) == 64

    if g_dump_luts:
        tmask = ''.join(f"{m}" for m in mask)
        print(f"\tLUT {x},{y},{z} bits: mask={tmask}")

    return proc_lut(mask, inputs, lmin, llim, prm)  # inside read_lut
# def read_lut

# for net dump: root --> (netname, start node)
g_netstart = {}

def deduce_net_names(root2nodes):
    """figure out most usable net names"""

    constants = { '$true', '$false', }
    root2net = {}

    # try it again
    strange = 0
    big_iso_nets = 0
    big_iso_pins = 0
    for r in sorted(root2nodes):
        nodes = root2nodes[r]

        # classify nodes
        consts = set()  # constant node
        iso = set()     # not bwd not fwd
        starts = set()  # not bwd yet fwd
        dumpiso = False
        configured = False
        for n in nodes:

            if n in constants:
                consts.add(n)
                continue

            if n in g_bwdshort:
                if n in g_used_ipins or n in g_used_opins:
                    configured = True
                continue
            if n in g_fwdshort:
                if n in g_used_ipins or n in g_used_opins:
                    configured = True
                starts.add(n)
                continue

            # handle iso
            (path, pin) = re.sub(r'^(.+)Z([^Z]+)$', r'\1 \2', n).split()
            mod = g_path2mod[path]
            ismux = re.search(r'_size\d+$', mod)
            if ismux:
                if n in g_used_ipins or n in g_used_opins:
                    if not dumpiso: print("ISO DUMP")
                    dumpiso = True
            iso.add(n)
        # for
        c = 1 if consts else 0
        i = 1 if iso    else 0
        s = 1 if starts else 0
        if c + i + s != 1:
            # FIXME figure out what causes this
            # it's a root clock net
            strange += 1
            root2net[r] = r
            #print(f"\tSTRANGE {strange} root = {r} " +
            #      f"c={len(consts)} i={len(iso)} s={len(starts)}")
            #for j, n in enumerate(consts, start=0):
            #    print(f"C{j:2d}\t{n}")
            #for j, n in enumerate(iso, start=0):
            #    print(f"I{j:2d}\t{n}")
            #for j, n in enumerate(starts, start=0):
            #    print(f"S{j:2d}\t{n}")
            continue
            #assert c + i + s == 1, f"c={c} i={i} s={s} nodes={nodes}"

        derive = 0
        if c:
            assert len(consts) == 1
            net = list(consts)[0]
        elif i:
            if len(iso) == 1:
                net = list(iso)[0]
                derive = 1
            else:
                net = f"big_iso_{big_iso_nets}"
                big_iso_nets += 1
                big_iso_pins += len(nodes) - len(consts)
        else:
            assert len(starts) == 1
            net = list(starts)[0]
            derive = 1
        # if

        if i and not dumpiso:
            root2net[r] = net
            continue

        if derive:
            (path, pin) = re.sub(r'^(.+)Z([^Z]+)$', r'\1 \2', net).split()
            mod = g_path2mod[path]
            ismux = re.search(r'_size\d+$', mod)
            if not ismux:
                # handle pb_types
                (inst,   _) = path2instandkey(path)
                net = f"W{inst}Z{pin}"

        root2net[r] = net

        # save info for dumping net later
        if s and configured: g_netstart[r] = (net, list(starts)[0])
    # for r

    if big_iso_nets:
        log(f" - Ignored {big_iso_nets:,d} unconfigured large net{plural(big_iso_nets)} " +
            f"with {big_iso_pins:,d} pin{plural(big_iso_pins)} " +
             "(rails w/o tie-off cells, unused clocks)\n")
    if strange:
        log(f" - Ignored {strange:,d} strange net{plural(strange)} (used clocks)\n")

    return root2net
# def deduce_net_names

def nextnode(n, prev, visited):
    """iterate through nodes on route"""
    visited.add(n)
    yield (n, prev)
    if n not in g_fwdshort: return
    for n2 in g_fwdshort[n]:

        # if n2 is a mux input/pin that has no fwdshort --> ignore
        if n2 not in g_fwdshort:
            #path, pin
            (path,   _) = re.sub(r'^(.+)Z([^Z]+)$', r'\1 \2', n).split()
            mod = g_path2mod[path]
            ismux = re.search(r'_size\d+$', mod)
            if ismux:
                visited.add(n2)
                continue

        yield from nextnode(n2, n, visited)
# def nextnode

def write_b_net_dump(filename, root2nodes):
    """diagnostic net-only dump"""
    with open(filename, "w", encoding='ascii') as ofile:
        ff = ""
        for r in sorted(root2nodes):
            if r not in g_netstart: continue

            # note form feed
            (net, start) = g_netstart[r]
            ofile.write(f"{ff}NET {net} \n")
            visited = set()
            seen = defaultdict(int)
            for (n, prev) in nextnode(start, "", visited):
                if prev == "":
                    ofile.write(f"\tSTART\n\t{n}\n")
                    seen[n] += 1
                else:
                    f = "*" if seen[prev] > 1 else ""
                    ofile.write(f"\n{f}\t{prev}\n\t{n}\n")
                    seen[n] += 1
            # for
            assert not root2nodes[r] - visited
            ff = "\f"
        # for r
    # with
# def write_b_net_dump

def mode2lutinfo(mode, b):
    """translate mode to lut info"""
    # what part of lutmask
    if   'lut6' in mode:
        inputs = 6
        lmin = 0 ; llim = 64 
        opin = "frac_lut6_lut6_outX0Y"
    elif 'lut5' in mode:
        inputs = 5
        if not b:
            (lmin, llim) = (0, 32)
            opin = "frac_lut6_lut5_outX0Y"
        else:
            (lmin, llim) = (32, 64)
            opin = "frac_lut6_lut5_outX1Y"
    else:
        assert 0
    return (inputs, lmin, llim, opin)
# def mode2lutinfo

def write_b_netlist(simfilename, netlist):
    """write out b.sim "sim" netlist"""

    # first, (1) process wire-LUTs, (2) process constant LUTs, (3) recognize inverters
    dropme = set()
    wlut = 0
    genk = 0
    wluts = 0
    genks = 0
    inverters = set()
    k2nodes = defaultdict(set)

    # these shouldn't be needed
    netlist.add('$false')
    netlist.find('$false')
    netlist.add('$true')
    netlist.find('$true')

    # first handle "hidden" wire-LUTs
    for key, quad in g_coords2instrmap.items():
        (pbtail, x, y, z, b) = key 
        (mode, prm, flag, outname) = quad
        #if x == 9 and y == 7 and z == 0:
        #    print(f"\n\nSAW key={key} quad={quad}\n\n")
        if flag: continue
        ii = [i for i, t in enumerate(prm.split(), start=0) if t != "open"]
        if len(ii) != 1: continue
        assert len(ii) == 1, f"prm={prm}"
        (inputs, lmin, llim, opin) = mode2lutinfo(mode, b)
        (which,   _,    _, activein, _) = (
            read_lut(x, y, z, inputs, lmin, llim))  # inside write_b_netlist
        # must be a buffer (-2); FIXME DEBUG: allow inverter (1)
        assert which in (-2, 1), \
f"which={which} activein={activein} inputs={inputs} lmin={lmin} llim={llim} opin={opin}"
        if which != -2: continue
        wlut += 1
        if not g_wlut & g_simplify: continue

        jj = [j for j, v in enumerate(activein, start=0) if v]
        assert len(jj) == 1
        assert ii[0] == jj[0]

        path = g_coords2path[key]
        patho = f"{path}Z{opin}"
        assert patho in netlist._dict, f"patho={patho}"
        assert g_pin2dir[patho] == "o"

        ipin = ii[0]
        pathi = f"{path}Zfrac_lut6_inX{ipin}Y"
        assert pathi in netlist._dict
        assert g_pin2dir[pathi] == "i"

        if g_shorts: print(f"WLUT1\t{key}\n L\t{pathi}\n R\t{patho}")
        netlist.union(pathi, patho)
        g_fwdshort[pathi].add(patho)
        g_bwdshort[patho].add(pathi)
        wluts += 1
    # for

    # process "explicit" objects
    for key, mode in g_coords2mode.items():
        (pbtail, x, y, z, b) = key 

        if 'frac_lut6' not in pbtail: continue
        if 'lut' not in mode: continue

        (inputs, lmin, llim, opin) = mode2lutinfo(mode, b)

        g_coords2info[key] = (inputs, lmin, llim, opin)
        # -2    wire-LUT (buffer)
        # -1    constant ONE
        #  0    constant ZERO
        # >0    function including inverter omitting buffer
        #which  lut  args  activein
        (which,   _,    _, activein, _) = (
            read_lut(x, y, z, inputs, lmin, llim))  # inside write_b_netlist
        # no processing of 2+ input functions
        if which >= 2: continue

        path = g_coords2path[key]
        patho = f"{path}Z{opin}"

        # remember inverters
        if which == 1:

            ii = [i for i, v in enumerate(activein, start=0) if v]
            assert len(ii) == 1
            ipin = ii[0]
            pathi = f"{path}Zfrac_lut6_inX{ipin}Y"
            assert pathi in netlist._dict
            assert g_pin2dir[pathi] == "i"

            path = g_coords2path[key]
            assert g_pin2dir[patho] == "o"

            inverters.add((pathi, patho, key))

            continue

        # remaining cases: 0, -1, -2

        assert patho in netlist._dict, f"patho={patho}"
        # wire-LUT: short correct input + output
        if which == -2:
            wlut += 1
            if not g_wlut & g_simplify: continue
            ii = [i for i, v in enumerate(activein, start=0) if v]
            assert len(ii) == 1
            ipin = ii[0]
            pathi = f"{path}Zfrac_lut6_inX{ipin}Y"
            assert pathi in netlist._dict
            assert g_pin2dir[pathi] == "i"

            assert g_pin2dir[patho] == "o"
            if g_shorts: print(f"WLUT2\t{key}\n L\t{pathi}\n R\t{patho}")
            netlist.union(pathi, patho)
            g_fwdshort[pathi].add(patho)
            g_bwdshort[patho].add(pathi)
            dropme.add(key)
            wluts += 1
            continue

        # constant: add '$false' or '$true' to names
        constants = { -1: '$true', 0: '$false', }
        if which in constants:
            genk += 1
            if not g_kgen & g_simplify: continue
            k = constants[which]
            netlist.add(k)
            netlist.find(k)
            assert g_pin2dir[patho] == "o"
            if g_shorts: print(f"CGEN\t{key}\n L\t{k}\n R\t{patho}")
            netlist.union(k, patho)
            g_fwdshort[k].add(patho)
            g_bwdshort[patho].add(k)
            g_pin2dir[k] = "o"
            k2nodes[k].add(patho)
            dropme.add(key)
            genks += 1
            continue

        assert 0
    # for

    # second, handle inverters driven by constants
    propagated = 0
    propvals = set()
    while g_kinv & g_simplify:
        inverted = set()
        for triple in inverters:
            (pathi, patho, key) = triple
            ri = netlist.find(pathi)
            ro = netlist.find(patho)
            assert ri != ro
            rf = netlist.find('$false')
            rt = netlist.find('$true')
            assert rf != rt
            if ri == rf and ro != rt:
                assert ro != rf
                if g_shorts: print(f"CINV1\t{key}\n L\t{ro}\n R\t{rt}")
                netlist.union(ro, rt)
                g_fwdshort[pathi].add(patho)
                g_bwdshort[patho].add(pathi)
                inverted.add(triple)
                dropme.add(key)
                propvals.add(0)
            if ri == rt and ro != rf:
                assert ro != rt
                if g_shorts: print(f"CINV2\t{key}\n L\t{ro}\n R\t{rf}")
                netlist.union(ro, rf)
                g_fwdshort[pathi].add(patho)
                g_bwdshort[patho].add(pathi)
                inverted.add(triple)
                dropme.add(key)
                propvals.add(1)
        if not inverted: break
        propagated += len(inverted)
        inverters -= inverted
    # while

    # normally would have been after "with open" below
    log(f"{elapsed()} Writing {simfilename}\n")

    #ninv = len(inverters)
    propvals = len(propvals)
    #log(f" - Recognized {genk:,d} constant generator{plural(genk)}, " +
    #    f"{wlut:,d} wire-LUT{plural(wlut)}, and " +
    #    f"{ninv:,d} inverter{plural(ninv)}\n")
    if genks or wluts or propagated:
        log(f" - Simplified {genks:,d} constant generator{plural(genks)}, " +
            f"{wluts:,d} wire-LUT{plural(wluts)}, and " +
            f"{propagated:,d} inverter{plural(propagated)} " +
            f"sinking {propvals:,d} constant{plural(propvals)}\n")

    # match VPR's default behavior of adding input slots for globals.
    # HOWEVER, better to filter them out, since there's no connection.
    if g_short_globals:
        #   key, name
        for key,    _ in g_coords2name.items():
            # key, name from .net
            (pbtail, x, y, z, b) = key 
            xyz = (x, y, z)
            if xyz not in g_xyz2global: continue
            net = g_xyz2global[xyz]
            bit = g_net2bit[net]
            # get ANY pin on the clock net
            clkpin = g_clk2pin[bit]
            path = g_coords2path[key] 
            #inst = f"{pbtail}_x{x}_y{y}_z{z}_b{b}"
            mod = g_path2mod[path].split('__')[-1]
            #print(f"\tFOUND xyz={xyz} net={net} bit={bit} path={path} inst={inst} mod={mod}")
            # pins on thing from .net
            for pin in sorted(g_path2pins[path]):
                node = f"{path}Z{pin}"
                #print(f"\t\tpin={pin} node={node}")
                if pin != "pad_outpadX0Y": continue
                #print(f"\t\tUNION:\t{clkpin}\t\t\t{node}")
                if g_true: print(f"CLOCK\t{key}\n L\t{clkpin}\n R\t{node}")
                netlist.union(clkpin, node)
            # for
        # for
    # if

    # process aliases: crunch down the netlist
    # ensure roots precalculated
    keyset = set()
    for nid in netlist.keys():
        keyset.add(nid)
        r = netlist.find(nid)
    # create set for each root
    root2nodes = defaultdict(set)
    for nid in netlist.keys():
        r = netlist.find(nid)
        root2nodes[r].add(nid)
    # compute new net names
    root2net = deduce_net_names(root2nodes)

    # diagnostic dump 
    write_b_net_dump("bnetdump", root2nodes)

    # figure out what's mentioned in .net / .place: extract it
    defined = set()
    counts = defaultdict(int)
    g_nc = 0
    lutm = 0
    lutmm = 0
    with open(simfilename, "w", encoding='ascii') as sfile:
        sfile.write("; default to MIT format\n")

        #   key  name
        for key, name in sorted(g_coords2name.items()):

            # ignore if previously found to be wire-LUT
            # or constant generator
            if key in dropme: continue

            (pbtail, x, y, z, b) = key 
            path = g_coords2path[key] 
            #inst = f"{pbtail}_x{x}_y{y}_z{z}_b{b}"
            mod = g_path2mod[path].split('__')[-1]

            if pbtail == "frac_lut6":
                (inputs, lmin, llim, opin) = g_coords2info[key] 
                prm = None
                if g_tolerate_dc and key in g_coords2instrmap:
                    (_, prm, _, _) = g_coords2instrmap[key]
                    prm = prm.split()

                # -2    wire-LUT (buffer)
                # -1    constant ONE
                #  0    constant ZERO
                # >0    function including inverter omitting buffer
                #which  lut  args  activein    bits
                (which, lut, args, activein, b_bits) = (
                    read_lut(x, y, z, inputs, lmin, llim, prm))  # inside write_b_netlist

                if which == -2 and g_wlut & g_simplify: continue
                if which == -1 and g_kgen & g_simplify: continue
                if which ==  0 and g_kgen & g_simplify: continue

                #print(f"b.sim: @ {key}")
                (mode, prm, flag, outname) = g_coords2instrmap[key] 
                #assert flag == 1
                # hardware input count
                prm = prm.split()
                assert inputs == len(prm)
                # active/used input count
                actives = sum(activein)
                inlist = [None] * actives
                # DEBUG
                prm_actives = [p for p in prm if p != "open"]
                if len(prm_actives) != actives:
                    # this is a result of a don't-care input
                    # getting through synthesis
                    print(
f" - WARNING: activein={activein} but inputs={inputs} prm={prm} key={key} name={name}")
                # fill in inputs
                j = 0
                for i in range(inputs):
                    if not activein[i]:
                        continue
                    node = f"{path}Zfrac_lut6_inX{i}Y"
                    #et = g_node2net[netlist.find(node)]
                    net = root2net[netlist.find(node)]
                    if g_ordered_luts:
                        inlist[int(prm[i])] = net
                    else:
                        if j < len(inlist):
                            inlist[        j  ] = net
                        else:
                            print(
f"OOR: j={j} len(inlist)={len(inlist)} inputs={inputs} prm={prm} key={key} name={name}")
                    j += 1
                    #inlist.append(net)
                # for
                node = f"{path}Z{opin}"
                #net = g_node2net[netlist.find(node)]
                onet = root2net[netlist.find(node)]
                if g_dump_luts:
                    print(f"\tkey={key} prm={prm} mode={mode} onet={onet}")
                    print(f"\tinlist={inlist}")

                if lut not in defined:
                    sfile.write(f"DEFINE {lut} {args}\n")
                    defined.add(lut)
                if not g_ordered_luts:
                    inlist = sorted(inlist, key=dict_aup_nup)
                sfile.write(f"{lut} {onet} {' '.join(inlist)}\n")
                counts['LUT'] += 1

                # rotate & compare LUT contents here
                a_bits = g_out2lutbits[outname]
                # prm_actives
                # b_bits
                # prm
                #print(f" - A {a_bits} {prm_actives}")
                #print(f" - B {b_bits} {prm}")
                r_bits = []
                for v in range(1 << len(prm)):
                    r = 0
                    for b, a in enumerate(prm, start=0):
                        if a == 'open': continue
                        if v & (1 << b): r += (1 << int(a))
                    r_bits.append(a_bits[r])
                #print(f" - C {r_bits}")
                if b_bits != r_bits:
                    lutmm += 1
                else:
                    lutm += 1

                continue
            # if frac_lut6

            pin2net = {}
            dumpflag = mod == "adder_carry"
            dumpflag = False
            for pin in sorted(g_path2pins[path]):
                node = f"{path}Z{pin}"
                assert node in keyset, f"node={node}"
                #assert node in netlist._dict
                #et = g_node2net[netlist.find(node)]
                root = netlist.find(node)
                net = root2net[root]
                # for more general coding below
                pin2net[pin] = net
                if dumpflag:
                    print(f"\tpin={pin}\n\tnode={node}\n\troot={root}\n\tnet={net}")
            # for

            # write to b.sim
            if mod == "pad":
                if z < 24:
                    # drop if connected to global
                    xyz = (x, y, z)
                    if xyz in g_xyz2global:
                        glob = g_xyz2global[xyz]
                        if glob in g_net2bit:
                            log(f" - Dropped input slot driving global {glob}\n")
                            continue

                    assert "pad_inpadX0Y" in pin2net, f"pin2net={pin2net}"
                    fnet = pin2net["pad_inpadX0Y"]
                    if "IPAD" not in defined:
                        sfile.write( "DEFINE IPAD f\n")
                        defined.add("IPAD")
                    sfile.write(f"IPAD {fnet}\n")
                    counts['IPAD'] += 1
                else:
                    assert "pad_outpadX0Y" in pin2net, f"pin2net={pin2net}"
                    fnet = pin2net["pad_outpadX0Y"]
                    if "OPAD" not in defined:
                        sfile.write( "DEFINE OPAD f\n")
                        defined.add("OPAD")
                    sfile.write(f"OPAD {fnet}\n")
                    counts['OPAD'] += 1
                continue
            # if pad

            if mod == "ff_phy":
                # pin2net contents already went through root2net
                (cpin, dpin, epin, rpin, qpin) = [pin2net[f"ff_phy_{p}X0Y"] for p in "CDERQ"]
                if "DFFRE" not in defined:
                    sfile.write( "DEFINE DFFRE q c d e r\n")
                    defined.add("DFFRE")
                sfile.write(f"DFFRE {qpin} {cpin} {dpin} {epin} {rpin}\n")
                counts['DFFRE'] += 1
                continue
            # if ff_phy

            if mod == "bram_phy":
                bram_info = genports("BRAM", g_bram_ports, r'', r'_[AB]')
                for block, allpins in bram_info.items():
                    allnets = [ pin2net[f"bram_phy_{p.translate(g_translate)}"]
                                for p in allpins ]
                    if block not in defined:
                        # correct vpr.xml to get rid of this
                        allpins = [re.sub(r'_[io]((?:\[\d+\])?)$', r'\1', p) for p in allpins]
                        sfile.write(f"DEFINE {block} {' '.join(allpins)}\n")
                        defined.add(block)
                    sfile.write(f"{block} {' '.join(allnets)}\n")
                    counts[block] += 1
                continue
            # if bram_phy

            if mod == "dsp_phy":
                # write B netlist
                xy = (x, y)
                if xy in g_tilelocs2net:
                    anet = g_tilelocs2net[xy]
                    mode = g_net2tiletype.get(anet, "RS_DSP_MULT")
                else:
                    mode = "RS_DSP_MULT"
                dsp_info = genports(mode, g_dsp_ports)
                for block, allpins in dsp_info.items():
                    allnets = [ pin2net[f"dsp_phy_{p.translate(g_translate)}"]
                                for p in allpins ]
                    if block not in defined:
                        # correct vpr.xml to get rid of this
                        allpins = [re.sub(r'_[io]((?:\[\d+\])?)$', r'\1', p) for p in allpins]
                        sfile.write(f"DEFINE {block} {' '.join(allpins)}\n")
                        defined.add(block)
                    sfile.write(f"{block} {' '.join(allnets)}\n")
                    counts[block] += 1
                continue
            # if dsp_phy

            # adder_carry
            if mod == "adder_carry":
                # write B netlist

                block = "ADD"
                allpins = ["cout[0]", "sumout[0]", "cin[0]", "g[0]", "p[0]", ]

                xyz = (x, y, z)
                if g_false: print(f"\tChecking ({x}, {y}, {z})")
                if xyz in g_adderlocs2net:
                    anet = g_adderlocs2net[xyz]
                    if g_false: print(f"\tChecking {anet}")
                    if anet in g_net2addertype:
                        block = g_net2addertype[anet]
                        if g_false: print(f"\tGOT {block}")
                        if block == "ADDLSB":
                            allpins = ["cout[0]", "g[0]", "p[0]", ]
                        elif block == "ADDMSB":
                            allpins = ["sumout[0]", "cin[0]", "g[0]", "p[0]", ]
                        else:
                            assert 0

                #pins = list(g_path2pins[path])
                allnets = []
                for p in allpins:
                    pin = f"adder_carry_{p.translate(g_translate)}"
                    if pin in pin2net:
                        allnets.append(pin2net[pin])
                    else:
                        allnets.append(f"nc{g_nc}")
                        g_nc += 1
                # for
                #print(f"pins={pins}\nallpins={allpins}\nallnets={allnets}")
                if block not in defined:
                    allpins = [a[:-3] for a in allpins]
                    sfile.write(f"DEFINE {block} {' '.join(allpins)}\n")
                    defined.add(block)
                if not g_drop_adders:
                    sfile.write(f"{block} {' '.join(allnets)}\n")
                counts[block] += 1
                continue
            # if adder_carry

            assert 0, f"mod={mod}"

        # for
    # with

    if lutmm:
        log(g_red +
            f" - ERROR: {lutmm} LUT{plural(lutmm)} do{plural(lutmm, '', 'es')}n't match\n" +
            g_black)
    else:
        log(g_green +
            " - All LUT bits MATCH\n" +
            g_black)
    assert lutm + lutmm == counts.get("LUT", 0)
    counts = ' '.join([f"{k}#{v:,d}" for k, v in sorted(counts.items())])
    log(f" - Wrote {counts}\n")
# def write_b_netlist

def eqsim(equfile, bfile, cfile):
    """produce cfile from bfile using replacements in equfile"""

    # read equate file
    b2a = {}
    with open(equfile, encoding='ascii') as ifile:
        log(f"{elapsed()} Reading {equfile}\n")
        for line in ifile:
            ff = line.split()
            if len(ff) != 3 or ff[0] != "=": continue
            b2a[ff[2]] = ff[1]
        # for
    # with

    lines0 = []
    lines1 = []
    with open(bfile, encoding='ascii') as ifile:
        log(f"{elapsed()} Reading {bfile}\n")
        for line in ifile:
            ff = line.split()
            if not ff or ff[0] in (";", "DEFINE"):
                lines0.append(line.strip())
                continue
            for j, t in enumerate(ff, start=0):
                if j: ff[j] = b2a.get(t, t)
            if not g_ordered_luts and ff[0].startswith("LUT"):
                ff[2:] = sorted(ff[2:], key=dict_aup_nup)
            lines1.append(' '.join(ff))
        # for
    # with

    with open(cfile, "w", encoding='ascii') as ofile:
        log(f"{elapsed()} Writing {cfile}\n")
        for line in sorted(lines0, key=dict_aup_nup):
            ofile.write(line + "\n")
        lead = " "
        for line in sorted(lines1, key=dict_aup_nup):
            if not line.startswith(lead):
                lead = re.sub(r'[^A-Z].*', r'', line)
                for _ in range(2):
                    ofile.write(f"; section {lead}\n")
            ofile.write(line + "\n")
        # for
    # with
# def eqsim

# MAIN SEQUENCE

def main():
    """run it all from top level"""
    parser = argparse.ArgumentParser(
        description="Verify bitstream file",
        epilog="Files processed in order listed and any ending .gz are compressed")

    # --read_base_nets k6n8_vpr.xml
    parser.add_argument('--read_base_nets',
                        required=True,
                        help="Read base die netlist")

    # --read_place .../fabric_add_1bit_post_synth.place
    parser.add_argument('--read_place',
                        required=True,
                        help="Read .place placement result written by VPR")

    # --read_pack .../fabric_add_1bit_post_synth.net.post_routing
    parser.add_argument('--read_pack',
                        required=True,
                        help="Read .net packing result written by VPR")

    # --read_design_constraints fpga_repack_constraints.xml
    parser.add_argument('--read_design_constraints',
                        required=True,
                        help="Read .xml clock repacking instructions")

    # --read_verilog .../add_1bit_post_synth.v
    if g_use_verilog: parser.add_argument('--read_verilog',
                        required=True,
                        help="Read .v design Verilog written by VPR")

    # --read_eblif .../transform_add_1bit_post_synth.eblif
    parser.add_argument('--read_eblif',
                        required=True,
                        help="Read .eblif transformed EBLIF written by VPR")

    # --read_map .../fabric_add_1bit_post_synth.place
    parser.add_argument('--read_map',
                        required=False,
                        help="Read .csv config bit array map")

    # --read_bitstream top.csv
    # --read_bitstream fabric_bitstream.xml
    # --read_bitstream fabric_bitstream.bit
    parser.add_argument('--read_bitstream',
                        required=True,
                        help="Read .csv, .xml, or .bit bitstream ")

    # --read_set_muxes mux_settings.txt
    parser.add_argument('--read_set_muxes',
                        required=False,
                        help="Read design-driven mux setting paths")

    # --gemini_exec ~/bin/gemini
    parser.add_argument('--gemini_exec',
                        required=False,
                        help="Path to gemini netlist comparator")

    # process and show args
    args = parser.parse_args()
    log(f"\nInvocation: {sys.argv[0]}\n")
    ml = 0
    for k, v in vars(args).items():
        if v is None: continue
        l = len(k)
        ml = max(ml, l)
    ml += 1
    for k, v in vars(args).items():
        if v is None: continue
        l = len(k)
        log(f"--{k:<{ml}s}{v}\n")
    log( "\n")

    what = []
    if g_kgen & g_simplify: what.append("constant generators")
    if g_wlut & g_simplify: what.append("wire-LUTs")
    if g_kinv & g_simplify: what.append("constant inverters")
    match len(what):
        case 0: txt = "nothing"
        case 1: txt = what[0]
        case 2: txt = ' and '.join(what)
        case 3:
            what[2] = "and " + what[2]
            txt = ', '.join(what)
    log(f" - Note: will simplify {txt}\n")

    # process and check for interactions between args
    if not args.read_map:
        wfb = args.read_bitstream
        if wfb.endswith('.gz'): wfb = wfb[:-3]
        if wfb.endswith('.bit'):
            log(" - Reading .bit bitstream requires --read_map\n")
            sys.exit(1)

    # create translation map
    # NB: re-applying this translation doesn't change the result
    for c in range(128):
        cc = chr(c)
        if   cc == '[': rhs = "X"
        elif cc == ']': rhs = "Y"
        elif cc == '.': rhs = "Z"
        # \s and \n appear below to leave white-space (fields) as-is
        elif not re.search(r'[\s\n_0-9a-zA-Z]', cc): rhs = "W"
        else: continue
        g_translate[c] = ord(rhs)
    # for

    # set color codes
    global g_black, g_red, g_green
    (g_black, g_red, g_green) = (("\033[0;30m", "\033[0;31m", "\033[0;32m")
        if sys.stderr.isatty() else ("", "", ""))

    # read the basedie nets 
    netlist = read_base_nets(args.read_base_nets)
    # "netlist" is a u-f structure with unions from nets connecting primitive pins

    # read place and pack
    read_place(args.read_place)     # NB must read place before net
    read_pack(args.read_pack)

    # find out what's attached to globals
    read_design_constraints(args.read_design_constraints)

    # read Verilog for LUT input order
    if g_use_verilog: read_verilog_order(args.read_verilog)

    # read design
    afilename = "a.sim"
    read_transformed_eblif(afilename, args.read_eblif)

    # TODO implement --read_map if we want to support .bit
    if args.read_map:
        log("--read_map is not yet supported\n")
        sys.exit(1)

    read_bitstream(args.read_bitstream)

    # read in set_muxes if requested
    mux_paths = read_set_muxes(args.read_set_muxes or "/dev/null")

    apply_bitstream_muxes(netlist, mux_paths)
    # "netlist" now updated with unions from bits shorting mux inputs to outputs

    bfilename = "b.sim"
    write_b_netlist(bfilename, netlist)

    # run gemini
    gemini = args.gemini_exec or "~/bin/gemini"
    gequ = "ab.equ"
    gout = "ab.out"
    gerr = "ab.err"
    command = f"{gemini} -D{gequ} {afilename} {bfilename} 1> {gout} 2> {gerr}"
    log(f"{elapsed()} Running {command}\n")
    subprocess.run(command, shell=True, check=True)
    rc = 0
    numbers = []
    with open(gout, encoding='ascii') as ifile:
        for line in ifile:
            if 'All nodes were matched' in line: rc |= 1
            if 'do not match' in line: rc |= 2
            if 'The circuits are different' in line: rc |= 2
            if 'Number of ' in line:
                numbers.append(int(line.split()[-1]))
        # for
    # with
    if rc == 1:
        log(g_green +
            " - EBLIF connectivity and bitstream MATCH\n" +
            g_black) 
    else:
        msg = ""
        if len(numbers) == 4:
            # pylint: disable-next=unbalanced-tuple-unpacking
            (adev, anet, bdev, bnet) = numbers
            msg = f" ({adev:,d}:{bdev:,d} devices, {anet:,d}:{bnet:,d} nets)"
        log(g_red +
            f" - ERROR: EBLIF connectivity and bitstream do NOT match: see {gout}{msg}\n" +
            g_black)
        broken = 0
        with open(gout, encoding='ascii') as ifile:
            for line in ifile:
                g = re.match(r'NET "tile_.*_ipin_', line)
                if g: broken += 1
        if broken:
            log(f" - {broken:,d} broken IPIN net{plural(broken)} mismatched\n")
    # if/else

    # produce new c.sim from b.sim with names from a.sim via ab.equ
    eqsim("ab.equ", "b.sim", "c.sim")

    # all done
    log(f"{elapsed()} FIN\n")
    return 0 if rc == 1 else 1
# def main

sys.exit(main())


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
