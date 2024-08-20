#!/usr/bin/python3
"""rename masters in a big layout to reuse pieces of a smaller layout"""

import sys
import os
import re
from collections import defaultdict
import hashlib

# USAGE

# % cd big_pnr
# % reusetiles.py   _run_dir/SRC/fpga_top.v                         \
#                   ../small_pnr/_run_dir/SRC/{routing,tile}/*.v    \
#                   _run_dir/SRC/{routing,tile}/*.v
#
# various checks are made on the file paths, names, and contents.
#
# this will update master names in fpga_top.v and in _run_dir/SRC/tile/*.v,
# assuming we have already implemented the blocks in small_pnr.
# a 2nd run will do nothing.

g_false = False
g_true = True

def log(text):
    """write to stderr"""
    sys.stderr.write(text)
# def log

def dict_aup_nup(k):
    """sort with alpha ascending and numeric ascending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    return r
# def dict_aup_nup

def dict_digits(k):
    """sort by part starting with digits"""
    return dict_aup_nup(re.sub(r'^(.*?)(\d[^/]*)$', r'\2_\1', k))

def compare(oldlist, newlist, oldmapping, newmapping):
    """compare sets of files"""

    def myreplace(g):
        """update master names"""
        ff = g.group(0)[:-1].split()
        if ff[0] == "module":
            ff[1] = "d"
        elif mapping:
            ff[0] = mapping[ff[0]]
        ff.append("(")
        return ' '.join(ff)

    # build hashes of all files
    hw2file = defaultdict(dict)
    for w, ff, mapping in [[0, oldlist, oldmapping], [1, newlist, newmapping]]:
        for f in ff:
            with open(f, encoding='ascii') as ifile:
                log(f"Reading {f}\n")
                rtlhash = hashlib.sha1()
                for line in ifile:
                    line = re.sub(r'^(\w+)\s+(\w+)\s*[(]', myreplace, line)
                    rtlhash.update(line.encode(encoding='ascii'))
                # for
                hd = rtlhash.hexdigest()
                hw2file[hd][w] = f
            # with
        # for
    # for

    # make sure all new files have matching old files
    unknown = { hw2file[hd][1] for hd in hw2file
                    if 1 in hw2file[hd] and 0 not in hw2file[hd] }
    if unknown:
        log("Unrecognized new files:\n")
        for f in sorted(unknown):
            log(f"\t{f}\n")
        log("Exiting on error\n")
        sys.exit(1)

    # return the map from new to old
    return {    re.split('[/.]', hw2file[hd][1])[-2]:
                re.split('[/.]', hw2file[hd][0])[-2]
                for hd in hw2file if 1 in hw2file[hd] }
# def compare

def showmap(mapdict):
    """show mapping result"""
    (maxl, maxr) = (0, 0)
    for mod in mapdict:
        if mapdict[mod] == mod: continue
        maxl = max(len(mod), maxl)
        maxr = max(len(mapdict[mod]), maxr)
    # for mod
    for mod in sorted(mapdict, key=dict_digits):
        if mapdict[mod] == mod: continue
        log(f"{mod:{maxl}s} ==> {mapdict[mod]:{maxr}s}\n")
    # for mod
# def showmap

def toplogic():
    """do everything"""
    # check and collect directories
    bad = { f for f in sys.argv[2:] if not re.search(r'/(?:routing|tile)/\w+[.]v$', f) }
    dirs = list( '/'.join(f.split('/')[:-2]) for f in sys.argv[2:] )
    if bad or len(dirs) < 2 or dirs[0] == dirs[-1]:
        p = sys.argv[0].rsplit('/', 1)[-1]
        l = '{'; r = '}'
        log(
f"Usage: {p} REUSE/fpga_top.v BASE/{l}routing,tile{r}/*.v REUSE/{l}routing,tile{r}/*.v\n")
        sys.exit(1)
    # if
    (olddir, newdir) = (dirs[0], dirs[-1])
    if g_false:
        log(f"OLD = {olddir} NEW = {newdir}\n")

    # produce mapping for routing boxes
    log("Step 1/3: Classify routing\n")
    oldrout = [ f for f in sys.argv[2:] if re.match(fr'{olddir}/routing/\w+[.]v$', f) ]
    oldrout = sorted(oldrout, key=dict_digits)
    newrout = [ f for f in sys.argv[2:] if re.match(fr'{newdir}/routing/\w+[.]v$', f) ]
    newrout = sorted(newrout, key=dict_digits)
    rnew2old = compare(oldrout, newrout, {}, {})
    if g_false:
        log(f"rnew2old = {rnew2old}\n")
    showmap(rnew2old)

    # don't choke on logic blocks
    mm = (
'grid_io_bottom grid_io_top grid_io_left grid_io_right grid_clb grid_bram grid_dsp')
    for m in mm.split():
        rnew2old[m] = m

    # produce mapping for tiles
    log("\n")
    log("Step 2/3: Classify tile\n")
    oldtile = [ f for f in sys.argv[2:] if re.match(fr'{olddir}/tile/\w+[.]v$', f) ]
    oldtile = sorted(oldtile, key=dict_digits)
    newtile = [ f for f in sys.argv[2:] if re.match(fr'{newdir}/tile/\w+[.]v$', f) ]
    newtile = sorted(newtile, key=dict_digits)
    tnew2old = compare(oldtile, newtile, {}, rnew2old)
    if g_false:
        log(f"tnew2old = {tnew2old}\n")
    showmap(tnew2old)

    # check for weird renaming problems
    olds = sorted(set(tnew2old.values()), key=dict_digits)
    bad = False
    for oldm in olds:
        if oldm in tnew2old:
            if tnew2old[oldm] == oldm: continue
            log(
f"Have like-named old tile and new tile '{oldm}' but latter changes to '{tnew2old[oldm]}'\n")
            bad = True
    if bad:
        log("Exiting on error\n")
        sys.exit(1)
    # in case already mapped: old tiles stay the same
    for oldm in olds:
        tnew2old[oldm] = oldm
    if g_false:
        log(f"tnew2old = {tnew2old}\n")

    if g_false:
        sys.exit(1)

    def myreplace0(g):
        """update master name in an instantiation"""
        ff = g.group(0)[:-1].split()
        return f"{tnew2old[ff[0]]} {ff[1]} (" if ff[0] != "module" else g.group(0)
    def myreplace1(g):
        """update module name in a declaration"""
        ff = g.group(0)[:-1].split()
        return f"module {tnew2old[ff[1]]} ("

    # update files
    updated = 0
    debugsuffix = ".dana"
    debugsuffix = ""
    log("\n")
    log("Step 3/3: Update top + tile\n")
    for i, f in enumerate([sys.argv[1]] + newtile, start=0):
        # i == 0: update master names in "MASTER instance ("
        # i >= 1: update master names in "module MASTER ("
        with open(f, encoding='ascii') as ifile:
            lines = []
            changed = 0
            for line in ifile:
                if not i:
                    line2 = re.sub(r'^\w+\s+\w+\s*[(]', myreplace0, line)
                else:
                    line2 = re.sub(r'^module\s+\w+\s*[(]', myreplace1, line)
                if line == line2:
                    lines.append(line)
                else:
                    lines.append(line2)
                    changed += 1
            # for
        # with
        if not changed:
            log(f"Reusable {f}\n")
            continue
        updated += 1
        log(    f"Updating {f}\n")

        if g_false: continue

        # write out altered file
        before = f + ".noreuse"
        if not os.path.isfile(before):
            os.rename(f, before)
        with open(f + debugsuffix, "w", encoding='ascii') as ofile:
            for line in lines:
                ofile.write(line)
            # for
        # with
    # for

    log("\n")
    log(f"{updated} files updated\n")
# def toplogic

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: ignorecase filetype=python autoindent hlsearch syntax=on fileformat=unix
