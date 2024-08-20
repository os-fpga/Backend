#!/nfs_home/dhow/bin/python3
"""load base die nets, load settings, and look for paths"""

import sys
import subprocess
import re
from collections import defaultdict
import networkx as nx
#mport igraph as ig

g_false = False
g_true = True
g_translate = {}        # '[': "X", ']': "Y", '.': "Z", other non-word: "W"
g_written = defaultdict(int)

def out(text):
    """write to stdout"""
    sys.stdout.write(text)
    sys.stdout.flush()
# def out

def log(text):
    """write to stderr"""
    sys.stderr.write(text)
# def log

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

def dict_aup_nup(k):
    """sort with alpha ascending and numeric ascending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] =  int(r[i])
    return r
# def dict_aup_nup

def procpins(nodes, edges, pins):
    """add pins to graph"""
    opins = [op[0] for op in pins if op[1] == "o"]
    ipins = [ip[0] for ip in pins if ip[1] == "i"]
    if not opins or not ipins:
        msg = f"NOTHING {len(opins)}+{len(ipins)}"
        #print(msg)
        g_written[msg] += 1
        return

    nodes.update(opins)
    nodes.update(ipins)

    for opin in opins:
        for ipin in ipins:
            edges.append((opin, ipin))
            #print(f"o {opin} --> i {ipin}")
        # for
    # for
    msg = f"SOMETHING {len(opins)}+{len(ipins)}"
    #print(msg)
    g_written[msg] += 1
    pins.clear()
# def procpins

def buildpath(edges, path, dir2ports):
    """populate edges inside muxes etc"""
    if "i" not in dir2ports or "o" not in dir2ports: return
    inps = dir2ports["i"]
    outs = dir2ports["o"]
    for inp1 in inps:
        for out1 in outs:
            ipin = f"{path}Z{inp1}"
            opin = f"{path}Z{out1}"
            edges.append((ipin, opin))
            #print(f"i {ipin} --> o {opin}")
        # for
    # for
# def buildpath

def toplogic():
    """create graph and operate on it"""

    assert len(sys.argv) == 5
    # pylint: disable=unbalanced-tuple-unpacking
    (_, basefile, setfile, asrc, asnk) = sys.argv[0:5]
    # Having setfile=none or null will add paths through all muxes
    log( "\n")
    log(f"Source:\t{asrc}\n")
    log(f"Sink:\t{asnk}\n")
    log( "\n")

    edges = []
    nodes = set()

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

    # read base die nets
    pins = []
    pathpin2dir = {}
    path2dir2ports = defaultdict(lambda: defaultdict(set))
    with gzopenread(basefile) as ifile:
        log(f"Reading {basefile}\n")
        for line in ifile:
            line = line.translate(g_translate)
            ff = line.split()
            if not ff: continue
            if ff[0] == "NET":
                procpins(nodes, edges, pins)
            if ff[0] == "PIN":
                assert len(ff) == 5
                if g_true:
                    # CONSISTENT: use mux not mem everywhere
                    ff[1] = re.sub(r'^(.*Z)mem([^Z]+)$' , r'\1mux\2', ff[1])
                # 0   1    2   3    4
                # PIN path mod port i/o
                path2dir2ports[ff[1]][ff[4]].add(ff[3])
                ff[0:4] = [f"{ff[1]}Z{ff[3]}"]
                #print(f"ff = {ff}")
                # 0         1
                # path.port i/o
                pathpin2dir[ff[0]] = ff[1]
                pins.append(ff)
        # for
    # with
    procpins(nodes, edges, pins)
    for src, snk in edges:
        nodes.add(src)
        nodes.add(snk)

    log(f"Made {len(nodes):,d} nodes and {len(edges):,d} edges\n")
    #for msg in sorted(g_written, key=dict_aup_nup):
    #    print(f"{msg} {g_written[msg]}")

    # read the settings
    if setfile.lower() in ("none", "null"):
        log(f"Populating all edges inside {len(path2dir2ports):,d} leaf blocks\n")
        for path, dir2ports in path2dir2ports.items():
            buildpath(edges, path, dir2ports)
    else:
        r = 0
        with gzopenread(setfile) as ifile:
            log(f"Reading {setfile}\n")
            for line in ifile:
                line = line.translate(g_translate)
                line = re.sub(r'Zmem', r'Zmux', line)
                ff = line.split()
                if not ff: continue
                dir2ports = path2dir2ports[ff[0]]
                buildpath(edges, ff[0], dir2ports)
                r += 1
            # for
        # with
        log(f"{r:,d} muxes in design\n")
    # if

    for src, snk in edges:
        nodes.add(src)
        nodes.add(snk)

    # build graph
    dg = nx.DiGraph(edges)
    log(f"Built fabric graph with {len(nodes):,d} nodes and {len(edges):,d} edges\n")
    log("\n")

    # try to find path
    assert asrc in nodes and asnk in nodes, f"\nasrc={asrc}\nasnk={asnk}\n"
    nodelist = nx.shortest_path(dg, source=asrc, target=asnk)
    if nodelist:
        out("PATH RETURNED:\n")
    else:
        out("NO PATH RETURNED\n")
    for i, node in enumerate(nodelist, start=1):
        out(f"{i:2d}\t{node}\n")

# def

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: ignorecase filetype=python autoindent hlsearch syntax=on fileformat=unix
