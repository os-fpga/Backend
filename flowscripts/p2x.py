#!/usr/bin/python3
"""convert pyx to xml"""

import sys
import os
# note xmlstarlet pyx input.xml > output.pyx will convert xml --> pyx

def unescape(s):
    """handle pyx escapes"""
    return s.replace(r'\t','\t').replace(r'\\','\\')

g_get_attrs = 0

def pyx_proc(line):
    """handle ?"""
    sys.stdout.write(f'<?{line[1:-1]}?>\n') # Proc Instr

def pyx_open(line):
    """handle ("""
    sys.stdout.write(f'<{line[1:-1]}')
    global g_get_attrs
    g_get_attrs = 1

def pyx_attr(line):
    """handle A"""
    name, val = line[1:].split(None, 1)
    sys.stdout.write(f' {name}="{unescape(val)[:-1]}"')

def pyx_misc(line):
    """handle -"""
    if line[:3] == r'-\n':
        sys.stdout.write(os.linesep)        # Newline
    else:
        sys.stdout.write(unescape(line[1:-1]))  # Misc content

def pyx_close(line):
    """handle )"""
    sys.stdout.write(f'</{line[1:-1]}>')  # Close tag

g_pyx_jump = {
    '?': pyx_proc,
    '(': pyx_open,
    'A': pyx_attr,
    '-': pyx_misc,
    ')': pyx_close,
}

def toplogic():
    """convert pyx --> xml"""
    global g_get_attrs
    for line in sys.stdin:
        if g_get_attrs and line[0] != 'A':
            g_get_attrs = 0           # End of tag attribues
            sys.stdout.write('>')
        g_pyx_jump[line[0]](line)
    # for line
# def toplogic

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch
# vim: filetype=python
# vim: syntax=on
# vim: fileformat=unix
