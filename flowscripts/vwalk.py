#!/nfs_home/dhow/bin/python3
#!/usr/bin/env python3
"""load Verilog or json db, possibly write json, provide walking functions"""

import sys
import re
import time
import math
import subprocess
from collections import defaultdict
import json
import itertools
import argparse

## pylint: disable-next=import-error,unused-import
from traceback_with_variables import activate_by_import, default_format

import uf

## part of traceback_with_variables
default_format.max_value_str_len = 10000

g_new = True   # enable latest changes

# API SUMMARY
#
# log(): log to stderr.
# out(): log to stdout.
# dict_aup_nup(): sort key for tcl lsort -dictionary.
# dict_aup_ndn(): same but with numbers decreasing.
# elapsed(): return elapsed time.
# plural(): format plurals suffixes.
#
# load(): FIRST CALL BEFORE REST: interpret arguments and load/store netlists.
#
# walkmods([mod]) --> yield mod, each mod in design under mod INCLUDING mod, once.
# walkuses(root_mod) --> yield (path, mod), AFTER initial mod [], path includes tail inst.
# walknets(mod[, path]) --> yield (path, mod, bus, b).
# walkpins(path, mod, bus, b) --> yield (path, mod, bus, b).
# getportinfo(mod, bus) --> (category, left, right) or None.
# walkports(mod) --> yield (bus, category, left, right).

# general
g_false = False
g_true = True
g_time = 0              # used by elapsed()

g_rootdir = ""

g_fcount = 0

# from command line or `define
g_defined = {}

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

g_d2bits = {'0': '0000', '1': '0001', '2': '0010', '3': '0011',
            '4': '0100', '5': '0101', '6': '0110', '7': '0111',
            '8': '1000', '9': '1001', 'a': '1010', 'b': '1011',
            'c': '1100', 'd': '1101', 'e': '1110', 'f': '1111', }

g_flip = { '0': '1', '1': '0', }

def val2bits(value):
    """convert Verilog value to bitstring"""
    if "'" in value:
        g = re.match(r"\s*([-+]?)\s*(\d*)\s*'([sS]?)([bBoOdDhH])\s*(\w+)\s*$", value)
        assert g
        (pm, w, _, radix, digits) = g.groups()
        # TODO handle (i) "-", (ii) signed, (iii) decimal
        assert pm != "-"
        w = int(w) if w else 32
        # expand to binary
        chunk = {'h':4, 'o':3, 'b':1, }[radix.lower()]
        bits = ''.join(g_d2bits[d.lower()][4-chunk:] for d in digits)
        # ensure always w bits
        if len(bits) < w:
            bits = "0" * (w - len(bits)) + bits
        else:
            bits = bits[-w:]
    else:
        g = re.match(r"\s*([-+]?)\s*(\d+)\s*$", value)
        assert g
        (pm, digits) = g.groups()
        v = int(digits)
        hi = 1 if pm == "-" and v else 0
        bits = bin(v - hi)[2:]
        if hi: bits = bits.translate(g_flip)
        if len(bits) < 32: bits = (str(hi) * (32 - len(bits))) + bits
    return bits
# def val2bits

def filetokens(filename):
    """generate tokens from a file"""

    # read entire file
    with gzopenread(filename) as ifile:
        log(f"{elapsed()}\tReading {filename}\n")
        global g_fcount
        g_fcount += 1
        contents = ifile.read()

    attrtoks = { '(*': True, '*)': False, }
    skip = False
    disable = 0    # each bit is a level
    # identify and iterate through tokens
    # NB (?!\S) is negative lookahead: can't be followed by another \S
    for m in re.finditer(r'''
[/][*](?:[^*]|[*]+[^*/])*[*][/]                         |   # /* --- */
[/][/][^\n]*                                            |   # // ---
["](?:[^"\\]|\\.)*["]                                   |   # " --- "
[(][*]                                                  |   # (*
[*][)]                                                  |   # *)
`[^\n]*                                                 |   # `something
    ( (?:[a-zA-Z_$][\w$]*) | (?:\\\S+(?!\S)) ) \s*          # continued ...
    (?: \[ ([^][:]+?) (?: : ([^][:]+?) )? \] )?         |   # id maybe with select
\d*\s*'[sS]?[bB]\s*[_0-1]*                              |   # constant with base
\d*\s*'[sS]?[oO]\s*[_0-7]*                              |   # constant with base
\d*\s*'[sS]?[dD]\s*[_0-9]*                              |   # constant with base
\d*\s*'[sS]?[hH]\s*[_0-9a-fA-F]*                        |   # constant with base
\d+                                                     |   # decimal constant
[-+/?|^&!~<>%\#().;,=]                                      # illegal ops & punctuation
    ''', contents, re.VERBOSE):
        t = m.group()
        #log(f"TOKEN <{t}> skip={skip} disable={disable}\n")

        # handle illegal operators, comments, strings, preprocessor directives.
        # NB: removed * since *) was mistaken for it.
        if t[0] in '/"`-+/?|^&!~<>%':
            if len(t) == 1:
                log(f"{filename}:\nIllegal non-structural {t} operator seen -- aborting\n")
                sys.exit(1)
            if t[0] in '/"': continue

            g = re.match(r'`\s*(ifn?def)\s+(\S+)', t)
            if g:
                dis = (0 if (g.group(1) == "ifdef") == (g.group(2) in g_defined) else 1)
                disable = (disable << 1) | dis
                continue

            g = re.match(r'`\s*(else|endif)', t)
            if g:
                if g.group(1) == "else":
                    disable = disable ^ 1
                else:
                    disable = disable >> 1
                continue

            if disable: continue
            g = re.match(r'`\s*(define|undef)\s+(\S+)', t)
            if g:
                if g.group(1) == "define":
                    g_defined[g.group(2)] = 1
                else:
                    del g_defined[g.group(2)]
                continue
            g = re.match(r'`\s*include\s+(\S+?)', t)
            if g:
                filename = g.group(1)
                if filename[0] != '/':
                    filename = f"{g_rootdir}/{filename}"
                yield from filetokens(filename)
                continue

            continue
        if disable: continue

        # toss attributes
        if t in attrtoks:
            skip = attrtoks[t]
            continue
        if skip: continue

        # return everything else
        (u, l, r) = m.groups()
        if not u:
            #print(f"<{t}, {None}, {None}>")
            #log(f"YIELD {t}\n")
            yield (t, None, None)
            continue

        if not l:
            #print(f"<{u}, {None}, {None}>")
            #log(f"YIELD {u}\n")
            yield (u, None, None)
            continue

        # pylint: disable-next=eval-used
        l = eval(l)
        if r:
            #print(f"<{u}, {l}, {r}>")
            # pylint: disable-next=eval-used
            #log(f"YIELD {u}\n")
            yield (u, l, eval(r))
        else:
            #print(f"<{u}, {l}, {l}>")
            #log(f"YIELD {u}\n")
            yield (u, l, l)
    # for
# def filetokens

# REVERSED below is b/c Verilog pads or truncates on the left.
# This approach means "equate, right to left in Verilog, until one stops"

# filled in by Verilog reading

# module --> bus --> [#...]
# [#...] is REVERSED
g_portlist = {}

# module --> bus --> (wire/input/inout/output, l, r)
g_declared = {}

# module --> usename --> master
g_template = {}

# module --> usename --> port --> [(name, #)...]
# [(name, #)...] is REVERSED
g_instance = {}

# module --> [ [ lhs = [(name, #)...], rhs = [(name, #)...] ]... ]
# [(name, #)...] is REVERSED
g_assigned = {}

# filled in by promote_shorts etc
# TODO rewrite so g_netshort is local and g_pinshort is not O(n^2)

# module --> union-find on (name, #) keys for all assigned nets
g_netshort = {}

# module --> (name, #) --> {(name, #)}, only here if a shorted port
g_pinshort  = {}

# module --> (name, #) --> {(use, name, #)}
g_net2pins = {}

# module --> root node --> port node
g_root2port = {}

# pylint: disable-next=dangerous-default-value
def extref(n, l, r, declared = {}):
    """return list of expanded references"""
    # sub in declaration limits when asked (declared supplied)
    if l is None and n in declared:
        (_, l, r) = declared[n]
    # cases: foo and foo[3]
    if r is None: return [(n, l)]
    # case: foo[3:7]
    inc = 1 if l <= r else -1
    return [(n, b) for b in range(l, r + inc, inc)]
# def extref

def read_verilog(filename):
    """process statements from filename"""

    modname = ""
    portlist = {}
    declared = {}
    template = {}
    instance = defaultdict(dict)
    assigned = []

    # prepare the file iterator
    src = filetokens(filename)
    t = "" ; l = None ; r = None

    # pylint: disable-next=unused-argument
    def f_formal(end = ""):
        """handle formal"""
        if t not in end:
            # this remembers port order
            # declaration will fill this in later
            portlist[t] = []
        return ""
    # def f_formal

    def f_module():
        """process module declaration"""
        nonlocal modname, t, l, r
        nonlocal portlist, declared, template, instance, assigned

        # name of module being defined
        (modname, _, _) = next(src)
        # bus --> [#...]
        portlist = g_portlist[modname] = {}
        # bus --> (input/..., l, r)
        declared = g_declared[modname] = {}
        # use --> master
        template = g_template[modname] = {}
        # use --> port --> [(name, #)...]
        instance = g_instance[modname] = defaultdict(dict)
        # [ [ [(name, #)...], [(name, #)...] ]... ]
        assigned = g_assigned[modname] = []

        (t, _, _) = next(src)
        if t == ";": return
        assert t == "("
        while 1:
            # NB do nothing with formals since declared later
            (t, l, r) = next(src)
            if t == ")": break
            if tok2act.get(t, f_formal)(end=',)') == ')': break
        # while
        (t, _, _) = next(src)
        assert t == ";"
        #log(f"{filename} : MODULE {modname} STARTED\n")
    # def f_module

    def f_port(end = ';'):
        """process port declaration"""
        nonlocal t, l, r
        while 1:
            (u, _, _) = next(src)
            if u in end: return u
            if u == ",": continue
            pl = [i for n, i in extref("", l, r)]   # NO decl lookup needed
            pl.reverse()
            portlist[u] = pl
            # port declaration always overrides
            declared[u] = (t, l, r)
        # while
    # def f_port

    def f_wire():
        """process wire declaration"""
        # NB can't handle comma in "wire lhs = {rhs1, rhs2};"
        # commas are handled inside "wire lhs1 = rhs1, lhs2 = rhs2;"
        nonlocal t, l, r
        (n, _, _) = next(src)
        rhs = []
        args = [ [n, l, r, rhs, ], ]
        while 1:
            (vt, vl, vr) = next(src)
            if vt == "=":
                while 1:
                    (vt, vl, vr) = next(src)
                    if vt in ",;": break
                    rhs.extend(extref(vt, vl, vr, declared))  # YES 1 decl lookup needed
            if vt == ";": break
            if vt == ",":
                (n, _, _) = next(src)
                rhs = []
                args.append([ n, l, r, rhs, ])
                continue
            assert 0, f"vt={vt} vl={vl} vr={vr}"
        # while
        for n, l, r, rhs in args:
            # wire can't change pre-existing declaration
            if n not in declared:
                declared[n] = ("wire", l, r)
            if not rhs: continue
            lhs = extref(n, l, r)   # NO decl lookup needed
            lhs.reverse()
            rhs.reverse()
            assigned.append([lhs, rhs])
    # def f_wire

    def f_assign():
        """process assignment"""
        # NB can't handle comma in "assign lhs1 = rhs1, lhs2 = rhs2;"
        # commas are handled inside "assign lhs = {rhs1, rhs2};"
        lhs = [] ; rhs = []
        args = [ [lhs, rhs,], ]
        refs = lhs
        while 1:
            (t, l, r) = next(src)
            if t == ",": continue
            if t == ";": break
            if t == "=":
                refs = rhs
                continue
            refs.extend(extref(t, l, r, declared))    # YES 2 decl lookup needed
        # for
        for lhs, rhs in args:
            lhs.reverse()
            rhs.reverse()
            assigned.append([lhs, rhs])
        # for
    # def f_assign

    def f_instance():
        """process instance"""
        delta = { "(": +1, ")": -1, }
        notref = True ; d = 0   # only setting
        parms = [] ; conns = []

        nonlocal t
        useinfo = [ t ]
        last = useinfo
        (t, l, r) = next(src)
        if t == "#":
            receive = parms
        else:
            receive = conns
            last.append(t)
            notref = False

        def fi_nop():
            """do nothing"""
            return
        def fi_delta():
            nonlocal d, receive, last, notref
            d += delta[t]
            if not d:
                receive = conns
                last = useinfo
                notref = False  # only clearing
        def fi_dot():
            nonlocal last
            #t  l  r
            (t, _, _) = next(src)
            last = [ t ]
            receive.append(last)
        def fi_other():
            if notref or not d:
                last.append(t)
            else:
                last.extend(extref(t, l, r, declared))    # YES 3 decl lookup needed

        inst_tok2act = {
            '(':    fi_delta,   ')':    fi_delta,
            '.':    fi_dot,     ',':    fi_nop,
        }

        while 1:
            (t, l, r) = next(src)
            if t == ";": break
            inst_tok2act.get(t, fi_other)()
        # for
        assert len(useinfo) == 2
        # pylint: disable-next=unbalanced-tuple-unpacking
        (master, use) = useinfo
        template[use] = master
        for conn in conns:
            (port, *refs) = conn
            refs.reverse()
            instance[use][port] = refs
        # for
    # def f_instance

    def f_endmodule():
        return
    # def f_endmodule

    tok2act = {
            'module':       f_module,
            'input':        f_port,
            'output':       f_port,
            'inout':        f_port,
            'wire':         f_wire,
            'assign':       f_assign,
            'endmodule':    f_endmodule,
    }

    # read tokens until file exhausted
    try:
        while 1:
            (t, l, r) = next(src)
            tok2act.get(t, f_instance)()
        # while
    except StopIteration:
        pass
# def read_verilog

def ref2str(ref):
    """convert ref to Verilog-compatible form"""
    (n, b) = ref
    if n[0] == "\\": n += " "
    if b is not None: n += f"[{b}]"
    return n
# def ref2str

def write_verilog(filename):
    """dump everything to Verilog"""
    n = 0
    with gzopenwrite(filename) as ofile:
        log(f"{elapsed()}\tWriting {filename}\n")
        for mod, u_p in g_instance.items():
            ofile.write(f"module {mod} ")
            lead = "("
            for bus, triple in g_declared[mod].items():
                (wdir, l, r) = triple
                if wdir == "wire": continue
                lr = "" if l is None else f" [{l}:{r}]"
                ofile.write(f"{lead}\n\t{wdir}{lr} {bus}")
                lead = " ," if bus[0] == "\\" else ","
            # for
            if lead == "(":
                ofile.write("();\n")
            else:
                ofile.write("\n);\n")
            blank = "\n"
            for bus, triple in g_declared[mod].items():
                (wdir, l, r) = triple
                if wdir != "wire" or l is None: continue
                lr = f" [{l}:{r}]"
                lead = " " if bus[0] == "\\" else ""
                ofile.write(f"{blank}{wdir}{lr} {bus}{lead};\n")
                blank = ""
            # for
            blank = "\n"
            for assign in g_assigned[mod]:
                assert len(assign) == 2
                (lhs, rhs) = assign
                lhs = [ref2str(l) for l in lhs]
                lhs.reverse()
                rhs = [ref2str(r) for r in rhs]
                rhs.reverse()
                lhs = lhs[0] if len(lhs) == 1 else "{" + ','.join(lhs) + "}"
                rhs = rhs[0] if len(rhs) == 1 else "{" + ','.join(rhs) + "}"
                ofile.write(f"{blank}assign {lhs} = {rhs};\n")
                blank = ""
            # for
            blank = "\n"
            for use, ports in u_p.items():
                master = g_template[mod][use]
                ofile.write(f"{blank}{master} {use} ")
                lead = "("
                for port, conns in ports.items():
                    if conns is None: continue
                    #print(f"port={port} conns = {conns}")
                    conns = [ref2str(c) for c in conns]
                    conns.reverse()
                    conns = conns[0] if len(conns) == 1 else "{" + ','.join(conns) + "}"
                    sep = " " if port[0] == "\\" else ""
                    ofile.write(f"{lead}\n\t.{port}{sep}({conns})")
                    lead = ","
                # for
                if lead == "(":
                    ofile.write("();\n")
                else:
                    ofile.write("\n);\n")
                blank = ""
            # for
            ofile.write("\nendmodule\n\n")
            n += 1
            #if n >= 2: break
        # for
    # with
# def write_verilog

def write_design(filename):
    """dump everything in db"""

    if filename is None: return

    ff = filename.rsplit('.', 1)
    if len(ff) == 2 and ff[1] == "gz":
        ff = ff[0].rsplit('.', 1)
    assert len(ff) == 2

    match ff[1]:
        case "v":
            write_verilog(filename)
        case "json":
            topelt = {
                'portlist': g_portlist,
                'declared': g_declared,
                'template': g_template,
                'instance': g_instance,
                'assigned': g_assigned,
            }
            with gzopenwrite(filename) as ofile:
                log(f"{elapsed()}\tWriting {filename}\n")
                #son.dump(topelt, ofile, indent=None, separators=(',', ':'), sort_keys=False)
                json.dump(topelt, ofile, indent=1   , separators=(',', ':'), sort_keys=False)
            # with
        case _:
            assert 0
    # match

# def debug_dump

g_promoted_shorts = 0
g_assigned_shorts = 0
g_shorted_ports = 0

def promote_shorts(mod, done):
    """promote shorts after doing so in all children"""
    global g_promoted_shorts, g_assigned_shorts, g_shorted_ports

    # 1: process children first
    done.add(mod)
    for use in g_instance[mod]:
        downmod = g_template[mod][use]
        if downmod in done: continue
        done.add(downmod)
        if downmod in g_instance:
            promote_shorts(downmod, done)
    # for

    # 2: promote shorts inside instantiations
    #print(f"Step 2: mod={mod}")
    netshort = g_netshort[mod] = uf.uf()
    for use in g_instance[mod]:
        downmod = g_template[mod][use]
        if downmod not in g_instance: continue

        if not g_pinshort[downmod]: continue
        ports = { nb[0] for nb in g_pinshort[downmod] }
        #print(f"use={use} shorted ports={ports}")

        # topnbs is [(name, #)...]
        save = {}
        for port, topnbs in g_instance[mod][use].items():
            if port not in ports: continue
            # botbs is [#...]
            botbs = g_portlist[downmod][port]
            #print(f"processing port={port} topnbs={topnbs} botbs={botbs}")
            for pair in itertools.zip_longest(topnbs, botbs, fillvalue=""):
                (topnb, botb) = pair
                #print(f"1 considering topnb={topnb} botb={botb}")
                if isinstance(topnb, str) or isinstance(botb, str): continue
                botnb = (port, botb)
                #print(f"2 considering topnb={topnb} botnb={botnb}")
                if botnb not in g_pinshort[downmod]: continue
                save[botnb] = topnb
            # for
        # for
        #print(f"save={save}")
        for botnb, topnb in save.items():
            for botnb2 in g_pinshort[downmod][botnb]:
                if botnb2 not in save: continue
                topnb2 = save[botnb2]
                #print(f"union topnb={topnb} topnb2={topnb2}")
                netshort.union(topnb, topnb2)
                g_promoted_shorts += 1
            # for
        # for
    # for

    # 3: shorts inside this module
    #print(f"Step 3: mod={mod}")
    for assign in g_assigned[mod]:
        assert len(assign) == 2
        (lhs, rhs) = assign
        for pair in itertools.zip_longest(lhs, rhs, fillvalue=""):
            (lhs, rhs) = pair
            if isinstance(lhs, str) or isinstance(rhs, str): continue
            #print(f"union lhs={lhs} rhs={rhs}")
            netshort.union(lhs, rhs)
            g_assigned_shorts += 1
        # for
    # for

    # 4: mark our port shorts
    #print(f"Step 4: mod={mod}")
    root2nodes = defaultdict(set)
    root2port = g_root2port[mod] = {}
    for node in netshort.keys():
        root = netshort.find(node)
        root2nodes[root].add(node)
        if node[0] in g_portlist[mod]:
            #print(f"node={node} has root={root}")
            root2port[root] = node
    # for
    pinshort = g_pinshort[mod] = {}
    for root, nodes in root2nodes.items():
        nodes = { nb for nb in nodes if nb[0] in g_portlist[mod] }
        if len(nodes) < 2: continue
        g_shorted_ports += len(nodes)
        #print(f"port shorts: {nodes}")
        # O(n) space, but O(n^2) if written to JSON and reloaded
        for node in nodes:
            pinshort[node] = nodes
        # for
        #txt = ' '.join([ nb[0] if nb[1] is None else f"{nb[0]}#{nb[1]}" for nb in nodes ])
        #print(f"{mod} {txt}")
    # for

    # 5: build netlist for later use by walknets/walkpins
    #print(f"Step 5: mod={mod}")
    net2pins = g_net2pins[mod] = defaultdict(set)
    seg2pins = defaultdict(set) # NEW
    for use, ports in g_instance[mod].items():
        downmod = g_template[mod][use]
        for port, topnbs in ports.items():
            if downmod in g_instance:
                botbs = g_portlist[downmod][port]
            else:
                # handle no leaf definition
                botbs = list(range(len(topnbs)))
            for pair in itertools.zip_longest(topnbs, botbs, fillvalue=""):
                (topnb, botb) = pair
                if isinstance(topnb, str) or isinstance(botb, str): continue
                root = netshort.find(topnb)
                if not g_new: root = root2port.get(root, root)    # REMOVE
                #print(f"root={root} gets pin {use}.{port}#{botb}")
                net2pins[root].add((use, port, botb))
                seg2pins[topnb].add((use, port, botb))  # NEW
            # for
        # for
    # for

    if not g_new: return
    # 6: choose better roots now that we know local netlist NEW
    old2new = {}
    consts = { "1'b0", "1'b1", }
    for root, nodes in root2nodes.items():
        cands = []
        for node in nodes:
            (n, b) = node
            # consider: constants
            if n in consts:
                # constants are priority 0
                cands.append((0, 4, node))
                continue
            # consider: ports on us (mod)
            if n in g_declared[mod]:
                cat = g_declared[mod][n][0]
                if cat == "input":
                    # input port is priority 1
                    cands.append((1, len(n), node))
                else:
                    # other ports are priority 2
                    cands.append((2, len(n), node))
                continue
            # consider: seg owning inst output not shorted to inst input
            add = False
            for use, port, b in seg2pins[node]:
                downmod = g_template[mod][use]
                if downmod not in g_instance: continue
                if g_declared[downmod][port][0] != "output": continue
                portb = (port, b)
                add = True
                if portb in g_pinshort[downmod]:
                    for (n2, _) in g_pinshort[downmod][portb]:
                        if g_declared[downmod][n2][0] == "input":
                            add = False
                            break
                if add: break
            if add:
                # unshorted instance output is priority 3
                cands.append((3, len(n), node))
                continue
            # consider: others
            # others are priority 4
            cands.append((4, len(n), node))
        # for
        old2new[root] = min(cands)[2]
    # for
    # change roots from old to new
    # update g_root2port
    root2port2 = g_root2port[mod] = {}
    for root, port in root2port.items():
        root2port2[old2new[root]] = port
    # for

    # update g_netshort
    netshort2 = g_netshort[mod] = uf.uf()
    for node in netshort.keys():
        root = netshort.find(node)
        # uf trick: re-entering with intended root in 2nd for all
        netshort2.union(node, old2new[root])
    # for
    newroots = set(old2new.values())
    assert len(old2new) == len(newroots)
    for node in netshort2.keys():
        assert netshort2.find(node) in newroots
    # for

    # update g_net2pins
    net2pins2 = g_net2pins[mod] = defaultdict(set)
    for root, pins in net2pins.items():
        root = netshort2.find(root)
        if not g_new: root = root2port2.get(root, root)   # REMOVE
        if root in old2new:
            net2pins2[old2new[root]] = pins
        else:
            net2pins2[        root ] = pins
    # for
# def promote_shorts

def read_design(filename):
    """load a netlist in several ways"""

    global g_rootdir, g_fcount
    ff = filename.rsplit('.', 1)
    if len(ff) == 2 and ff[1] == "gz":
        ff = ff[0].rsplit('.', 1)
    assert len(ff) == 2
    gg = ff[0].rsplit('/', 1)
    assert 0 < len(gg) < 3
    g_rootdir = "." if len(gg) == 1 else gg[0]

    match ff[1]:
        case "f":
            with gzopenread(filename) as ifile:
                log(f"{elapsed()}\tReading {filename}\n")
                g_fcount += 1
                for line in ifile:
                    line = line.strip()
                    if not line or line[0] == '#': continue
                    line = re.sub(r'\s+#.*$', r'', line)
                    assert not re.search(r'\s', line)
                    if line[0] != '/': line = f"{g_rootdir}/{line}"
                    read_verilog(line)
                # for
            # with
        case "v":
            read_verilog(filename)
        case "json":
            with gzopenread(filename) as ifile:
                log(f"{elapsed()}\tReading {filename}\n")
                g_fcount += 1
                topelt = json.load(ifile)
            # with
            global g_portlist, g_declared, g_template, g_instance, g_assigned
            g_portlist = topelt['portlist']
            g_declared = topelt['declared']
            g_template = topelt['template']
            g_instance = topelt['instance']
            g_assigned = topelt['assigned']
            # g_declared: lists --> tuples
            for mod, b_t in g_declared.items():
                for bus, triple in b_t.items():
                    b_t[bus] = tuple(triple)
            # g_instance: lists --> tuples
            for mod, u_t in g_instance.items():
                for _, p_c in u_t.items():    # use=_
                    for port, conns in p_c.items():
                        p_c[port] = [tuple(c) for c in conns]
            # g_assigned: lists --> tuples
            for mod, pairs in g_assigned.items():
                for pair in pairs:
                    for p in range(2):
                        pair[p] = [tuple(nb) for nb in pair[p]]
        case _:
            assert 0
    # match
    # summarize what we loaded
    nm = len(g_instance)
    ni = sum(len(u_p) for m, u_p in g_instance.items())
    nf = g_fcount
    log(
f"{elapsed()}\tDefined {nm:,d} module{plural(nm)} " +
f"with {ni:,d} instance{plural(ni)} " +
f"from {nf:,d} file{plural(nf)}\n")

    # TODO move below into new promote_shorts wrapper "link_design"
    # called by read_design caller

    # process assigns/shorts and summarize
    log( "\tLifting internal shorts following post-order DFS\n")
    done = set()
    for mod in g_instance:
        promote_shorts(mod, done)
    # for
    ds = g_assigned_shorts
    sp = g_shorted_ports
    ps = g_promoted_shorts
    log(
f"{elapsed()}\t{ds:,d} assigned short{plural(ds)}, " +
f"{sp:,d} shorted port{plural(sp)}, " +
f"{ps:,d} promoted short{plural(ps)}\n")
# def read_design

# API
#
# walkmods() --> yield mod, for each mod in design, once
# walkmods(mod) --> yield mod, each mod in design under mod INCLUDING mod, once

def walkmods(mod=None, seen=None):
    """enumerate modules"""
    if mod is None:
        for mod2 in g_instance:
            yield mod2
        return
        # for
    # if

    if seen is None:
        seen = set()
        yield mod

    if mod not in g_instance: return

    downmods = { downmod for _, downmod in g_template[mod].items() } - seen
    for downmod in downmods:
        yield downmod
        seen.add(downmod)
    # for
    for downmod in downmods:
        yield from walkmods(downmod, seen)
    # for
# def walkmods

# API
#
# walkuses(root_mod) --> yield (path, mod), AFTER initial mod [], path includes tail inst

def walkuses(mod, path=None):
    """iterate through uses in/under mod"""
    # 1. pass ourself back
    if path is None:
        path = []
        yield (path, mod)

    if mod not in g_instance: return

    path.append("")

    # 2. our instances
    for use in g_instance[mod]:
        downmod = g_template[mod][use]
        path[-1] = use
        yield (path, downmod)
    # for

    # 3. children's instances
    for use in g_instance[mod]:
        downmod = g_template[mod][use]
        path[-1] = use
        yield from walkuses(downmod, path)
    # for

    path.pop()
# def walkuses

# API
# 
# walknets(mod[, path]) --> yield (path, mod, bus, b)
#  path is list of instances to module containing bus. It may be [].
#  mod is the module containing the bus.
#  bus is the name of the bus. It may start with \\.
#  b is the bit in bus, else None.

# You can call this from down in the hierarchy with a non-null path.
#
# You should be able to call walkpins with results from walknets

def walknets(mod, path=None):
    """walk all nets from 'mod' on down"""

    if path is None: path = []

    # 1. visit normal nets
    #   nb  pins
    for nb,    _ in g_net2pins[mod].items():
        # in this step, ignore nets connected to non-top ports
        if path and nb[0] in g_portlist[mod]: continue
        yield (path, mod, nb[0], nb[1])
    # for
            
    path.append("")

    # 2. visit nets underneath pins not up-connected
    # if down-shorted, all must be unconnected
    for use, ports in g_instance[mod].items():
        downmod = g_template[mod][use]
        if downmod not in g_instance: continue
        unconnports = set(g_portlist[downmod]) - set(ports)
        if not unconnports: continue
        path[-1] = use
        for port in unconnports:
            for b in g_portlist[downmod][port]:
                root = g_netshort[downmod].find((port, b))
                bad = False
                if root in g_root2port[downmod]:    # ONLY PLACE USED
                    ptnb = g_root2port[mod][root]
                    for n, b in g_pinshort[ptnb]:
                        if n not in unconnports:
                            bad = True
                            break
                    # for
                # if
                if bad: continue
                yield (path, downmod, port, b)
            # for
        # for
    # for

    # 3. go down
    for use in g_instance[mod]:
        downmod = g_template[mod][use]
        if downmod not in g_instance: continue
        path[-1] = use
        yield from walknets(downmod, path)
    # for

    path.pop()
# def walknets

# API
# 
# walkpins(path, mod, bus, b) --> yield (path, mod, bus, b)
# Call:
#  path = list of instances to module containing bus. May be [].
#  mod = module containing bus.
#  bus = name of bus.
#  b = bit in bus, else None.
#
# Yield:
#  path = list of instances to module containing bus. May be []. Includes tail inst.
#  mod = module containing bus.
#  bus = name of bus. It may start with \\.
#  b = bit in bus, else None.
#
# You should be able to call walkpins with results from walknets

def walkpins(path, mod, bus, b):
    """walk pins on the indicated path/net"""

    netshort = g_netshort[mod]
    root2port = g_root2port[mod]
    net2pins = g_net2pins[mod]
    template = g_template[mod]

    path.append("")

    # enumerate pins at this level
    net = (bus, b)
    root = netshort.find(net)
    if not g_new: root = root2port.get(root, root)    # REMOVE
    dive = {}
    for usepin in net2pins[root]:
        # already visited other pin to which this is shorted?
        if usepin in dive: continue
        (use, port, b) = usepin
        # find shorted pins on this instance if it has a master
        downmod = template[use]
        if downmod in g_instance: 
            # remember shorted pins on this instance
            if (port, b) in g_pinshort[downmod]: 
                for port2, b2 in g_pinshort[downmod][(port, b)]:
                    usepin2 = (use, port2, b2)
                    dive[usepin2] = 1
        # mark that we went down THIS pin
        dive[usepin] = 0
        path[-1] = use
        yield (path, downmod, port, b)
    # for

    # enumerate pins below us
    for usepin, flag in dive.items():
        # ignore shorted pins we didn't visit
        if flag: continue
        (use, port, b) = usepin
        downmod = template[use]
        if downmod not in g_instance: continue
        path[-1] = use
        yield from walkpins(path, downmod, port, b)
    # for

    path.pop()
# def walkpins

# API
#
# getportinfo(mod, bus) --> (category, left, right) or None

def getportinfo(mod, bus):
    """return info for port"""
    if mod not in g_declared or bus not in g_declared[mod]: return None
    return g_declared[mod][bus]
# def getportinfo

# API
#
# walkports(mod) --> yield (bus, category, left, right)

def walkports(mod):
    """return ports"""
    for bus, info in g_declared[mod].items():
        (cat, l, r) = info
        yield (bus, cat, l, r)
    # for
# def walkports

def dump_top(top):
    """dump netlist in simple format for debugging"""
    if top is None: return
    dsdir2schar = { "input":        "i",    "output":       "o",
                    "inout":        "b",    "wire":         "w", }
    consts = { "1'b0", "1'b1", }
    for net in walknets(top):
        (npath, nmod, nbus, nb) = net
        tnpath = '.'.join(npath + [nbus])
        if nb is not None: tnpath += f'[{nb}]'
        if nbus in consts:
            print(f"NET {nbus[-1]}")
        else:
            print(f"NET {tnpath}")
        for pin in walkpins(npath, nmod, nbus, nb):
            (ppath, pmod, pbus, pb) = pin
            info = getportinfo(pmod, pbus)
            if info is None: continue
            dchar = dsdir2schar[info[0]]
            if pb is not None: pbus += f"[{pb}]"
            print(f"PIN {'.'.join(ppath)} {pmod} {pbus} {dchar}")
        # for
    # for
# def dump_top

# API
#
# load(): interpret arguments and load/store netlists.
# caller then uses other API calls to enumerate.

def load(parents=None):
    """run it all from top level"""
    if parents is None: parents = []
    parser = argparse.ArgumentParser(
        parents=parents,
        description="Verify bitstream file",
        epilog="Files processed in order listed and any ending .gz are compressed")

    # --define symbol
    parser.add_argument('--define',
                        required=False, nargs=1, action='append',
                        help="Mark a symbol as defined")

    # --read_design .../fabric_add_1bit_post_synth.v
    parser.add_argument('--read_design',
                        required=True,
                        help="Read .v, .f, or .json design")

    # --write_design debugdump.v
    parser.add_argument('--write_design',
                        required=False,
                        help="Write .v or .json design")

    # --dump_top module
    parser.add_argument('--dump_top',
                        required=False,
                        help="Dump hierarchical net/pinlist from argument to stdout")

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

    # remember command-line defines
    if args.define is not None:
        for definlist in args.define:
            for defin in definlist:
                g_defined[defin] = 1
            # for
        # for
    # if

    # read design
    read_design(args.read_design)

    log(f"{elapsed()}\tLINKED\n")

    # write_design
    write_design(args.write_design)

    # dump_top
    dump_top(args.dump_top)

    return args
# def load

# TOP LEVEL
# if executed directly, run load()
if __name__ == "__main__":
    load()


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
