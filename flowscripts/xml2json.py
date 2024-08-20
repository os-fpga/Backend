#!/usr/bin/python3
"""convert XML to JSON"""

import sys
import os
import re
import subprocess
import json

g_false = False

def unescape(s):
    """process pyx escapes"""
    return s.replace(r'\t','\t').replace(r'\\','\\')

def pyx_nop(*_):
    """ignore ? and C"""

def pyx_open(stack, line):
    """handle ("""
    addme = [ line[1:-1], {} ]
    stack[-1].append(addme)
    stack.append(addme)

def pyx_attr_noconvert(stack, line):
    """handle A"""
    # must handle 'Aname ' properly (name="" in XML)
    n, v = line[1:-1].split(" ", 1)
    v = unescape(v)
    # save result
    stack[-1][1][n] = v

def pyx_attr_convert(stack, line):
    """handle A"""
    # must handle 'Aname ' properly (name="" in XML)
    n, v = line[1:-1].split(" ", 1)
    v = unescape(v)
    # NB: these conversionms can be dangerous, if
    # e.g. we want to retain the width of "0000" and not reduce it to 0.
    if re.match(r'-?\d+$', v):
        v = int(v)
    elif re.match(r'\d*[.]?\d*(e-?\d+)?$', v) and re.match(r'[.]?\d', v):
        v = float(v)
    # save result
    stack[-1][1][n] = v

def pyx_text_keepnl(stack, line):
    """handle - and keep newlines"""
    stack[-1].append(os.linesep if line[:3] == r'-\n' else unescape(line[1:-1]))

def pyx_text_dropnl(stack, line):
    """handle - and drop newlines"""
    if line[:3] == r'-\n': return
    stack[-1].append(unescape(line[1:-1]))

def pyx_close(stack, line):
    """handle )"""
    assert line[1:-1] == stack[-1][0]
    stack.pop()

g_pyx_jump = {
    '?': pyx_nop,
    'C': pyx_nop,
    '(': pyx_open,
    'A': pyx_attr_noconvert,    # option may change this
    '-': pyx_text_dropnl,       # option may change this
    ')': pyx_close,
}

g_o = '{'

def dump_json(ofile, root, lead=""):
    """write out JSON with whitespace compression specific to XML"""
    if lead != "":
        ofile.write(f",\n{lead}")
    if isinstance(root, list):
        # write ["tag", {
        ofile.write(f'["{root[0]}", {g_o}')
        # write attributes if any
        notfirst = False
        h = root[1]
        for k in sorted(h):
            if notfirst:
                # write ,
                ofile.write(", ")
            v = h[k]
            if isinstance(v, str):
                # write "key": "str"
                ofile.write(f'"{k}": "{v}"')
            else:
                # write "key": val
                ofile.write(f'"{k}": {v}')
            notfirst = True
        # write }
        ofile.write("}")
        # concatenate adjacent strings
        i = len(root) - 1
        while i > 2:
            if not isinstance(root[i], str):
                i -= 1
                continue
            j = i
            while j > 2:
                if not isinstance(root[j - 1], str):
                    break
                j -= 1
            if j == i:
                i -= 1
                continue
            root[j:i + 1] = [''.join(root[j:i + 1])]
            i = j - 1
        # print out children
        lead += " "
        for child in root[2:]:
            dump_json(ofile, child, lead)
        # write ]
        ofile.write(']')
    elif isinstance(root, str):
        ofile.write(f'"{root}"')
    else:
        ofile.write(root)
# def

def toplogic():
    """everything"""
    while len(sys.argv) > 1 and sys.argv[1][0] == '-':
        # newlines option
        if sys.argv[1] == "-n":
            g_pyx_jump['-'] = pyx_text_keepnl
            del sys.argv[1]
            continue
        # value option
        if sys.argv[1] == "-v":
            g_pyx_jump['A'] = pyx_attr_convert
            del sys.argv[1]
            continue
        sys.stderr.write(f"Unrecognized option: {sys.argv[1]}\n")
        sys.exit(1)
    # too many args?
    if len(sys.argv) > 2:
        sys.stderr.write("Too many arguments\n")
        sys.exit(1)
    # determine command to produce PYX from XML
    xmlstarlet = "/usr/bin/xmlstarlet"
    if len(sys.argv) < 2:
        command = f"{xmlstarlet} pyx"
    elif sys.argv[1].endswith(".gz"):
        command = f"gunzip -c {sys.argv[1]} | {xmlstarlet} pyx"
    else:
        command = f"{xmlstarlet} pyx < {sys.argv[1]}"
    if g_false:
        command += f" | tee {sys.argv[1]}.pyx" 

    # read in XML
    stack = [ ['document', {}] ]    # last in stack is being modified
    with subprocess.Popen(command,
            shell=True, stdout=subprocess.PIPE, universal_newlines=True).stdout as xfile:
        for line in xfile:
            g_pyx_jump[line[0]](stack, line)

    # write out JSON
    if g_false:
        json.dump(stack[0][2], sys.stdout, sort_keys=True, indent=1)
    else:
        dump_json(sys.stdout, stack[0][2])
    sys.stdout.write("\n")
# def toplogic

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: filetype=python autoindent syntax=on hlsearch fileformat=unix
