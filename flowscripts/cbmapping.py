#!/nfs_home/dhow/bin/python3
#!/usr/bin/env python3
"""visit base die flat nets touching BL or WL ports on latches, produce cbmapping"""

import re
from collections import defaultdict

## pylint: disable-next=import-error,unused-import
from traceback_with_variables import activate_by_import, default_format

import vwalk

## part of traceback_with_variables
default_format.max_value_str_len = 10000

def memlines():
    """generate all memlines (BLs/WLs)"""

    vwalk.log( "\tEnumerating nets\n")
    wxmin = 999999; bymax = 0
    for pbus, _, _, _ in vwalk.walkports("fpga_top"):
        g = re.match(r'[bw]l__abut_(\d+)__(\d+)_$', pbus)
        if not g: continue
        if pbus[0] == 'w' and int(g.group(1)) < wxmin: wxmin = int(g.group(1))
        if pbus[0] == 'b' and int(g.group(2)) > bymax: bymax = int(g.group(2))
    # for
    net2paths = defaultdict(list)
    for net in vwalk.walknets("fpga_top"):
        (npath, nmod, nbus, nb) = net 
        # want only top nets
        if npath: break
        # want only BLs or WLs
        if not re.match(r'[bw]l__', nbus): continue
        #g = re.match(r'[bw]l__abut_(\d+)__(\d+)_$', nbus)
        #if not g: continue

        assert nb is not None
        netname = nbus + f'[{nb}]'
        #if nbus[0] == 'w':
        #    netname = f'wl__abut_{wxmin}__{g.group(2)}_[{nb}]'
        #else:
        #    netname = f'bl__abut_{g.group(1)}__{bymax}_[{nb}]'
        for pin in vwalk.walkpins(npath, nmod, nbus, nb):
            #ppath  pmod  pbus  pb
            (ppath, pmod,    _,  _) = pin
            if pmod != "RS_LATCH": continue

            # convert a b RS_LATCH_\d+_ to fpga_top.a.b.mem_out[\d+]
            sresult = re.sub(r'^RS_LATCH_(\d+)_$', r'mem_out[\1]', ppath[-1])
            spath = '.'.join(["fpga_top"] + ppath[0:-1] + [sresult])
            net2paths[netname].append(spath)
        # for
    # for
    vwalk.log( "\tGenerating cbmapping\n")

    lsnet = sorted(net2paths, key=vwalk.dict_aup_nup)

    both = len(lsnet)
    nbl = len([n for n in net2paths if n.startswith("bl__")])
    nwl = both - nbl

    dsbit2cbl = {}
    for cbl in range(nbl):
        for sbit in net2paths[lsnet[cbl]]:
            dsbit2cbl[sbit] = cbl

    dsbit2cwl  = {}
    for cbl in range(nbl, both):
        cwl = cbl - nbl
        for sbit in net2paths[lsnet[cbl]]:
            dsbit2cwl[sbit] = cwl

    assert len(dsbit2cbl) == len(dsbit2cwl), f"{len(dsbit2cbl)} {len(dsbit2cwl)}"

    # this doesn't need to be sorted
    for sbit, cbl in dsbit2cbl.items():
        cwl = dsbit2cwl[sbit]
        print(f"{cbl},{cwl},{sbit}")

    nv = round(100 * (1 - (len(dsbit2cbl) / (nbl * nwl))))
    vwalk.log(
f"{vwalk.elapsed()}\tWrote {len(dsbit2cbl):,d} mappings for a " +
f"{nbl:,d} BL * {nwl:,d} WL array ({nv}% vacant)\n")
# def memlines

vwalk.load()
memlines()

# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
