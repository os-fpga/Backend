#!/usr/bin/env python3
"""compare sorted bitstream files"""

import sys

left = right = mismatched = matched = 0

args = list(sys.argv)
showeq = False  # show equal lines
showun = True   # show unique lines (in only 1 file)
while len(args) > 3:
    if   args[1] == "-a":
        del args[1]
        showeq = True
    elif args[1] == "-u":
        del args[1]
        showun = False
    else:
        sys.stderr("Usage: cdiff [-a] [-u] file1 file2\n")
        sys.exit(1)
# while

with open(args[1], encoding='ascii') as afile, \
     open(args[2], encoding='ascii') as bfile:

    # step through files
    act = 16+32
    while True:

        # handle reading
        # False = EOF
        if act & 16:
            try:
                aline = next(afile)
            except:
                aline = False
        if act & 32:
            try:
                bline = next(bfile)
            except:
                bline = False

        if aline is False and bline is False: break

        # print a by itself?
        if bline is False or aline is not False and aline[1:] < bline[1:]:
            left += 1
            act = 4+16
        # print b by itself?
        elif aline is False or bline is not False and bline[1:] < aline[1:]:
            right += 1
            act = 8+32
        # how much do they match?
        else:
            assert aline[1:] == bline[1:]
            if aline[0] != bline[0]:
                mismatched += 1
                act = 2+16+32
            else:
                matched += 1
                act = 1+16+32

        # handle printing
        if showeq and (act & 1):
            print(f"= {aline}", end='')
        if             act & 2:
            print(f"! {aline}", end='')
        if showun and (act & 4):
            print(f"< {aline}", end='')
        if showun and (act & 8):
            print(f"> {bline}", end='')
    # while
# with

sys.stderr.write(f"{left:,d} left unique, {matched:,d} matched, {mismatched:,d} mismatched, {right:,d} right unique\n")


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
