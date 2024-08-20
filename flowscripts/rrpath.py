#!/nfs_home/dhow/bin/python3
"""load routing graph and test its properties"""

import sys
import re 
#import subprocess
#from collections import defaultdict
import time
import math

import networkx as nx
#import numpy as np
import igraph as ig

g_false = False
g_true  = True
g_time  = 0

def log(text):
    """write to stderr"""
    sys.stderr.write(text)

def out(text):
    """write to stdout"""
    sys.stdout.write(text)
    sys.stdout.flush()

def elapsed():
    """return elapsed time"""
    global g_time
    thistime = time.time()
    if not g_time:
        g_time = thistime
        return "00:00:00"
    thistime -= g_time
    s = int(thistime % 60)
    thistime -= s
    m = int((thistime % 3600) / 60)
    thistime -= m * 60
    h = int(math.floor(thistime / 3600))
    return f"{h:02d}:{m:02d}:{s:02d}"
# def elapsed

def toplogic():
    """create graph and operate on it"""

    assert len(sys.argv) == 4
    # pylint: disable=unbalanced-tuple-unpacking
    (_, graphfile, asrc, asnk) = sys.argv[0:4]
    (asrc, asnk) = (int(asrc), int(asnk))

    edges = []
    nodes = set()
    with open(graphfile, encoding='ascii') as ifile:
        for line in ifile:
            # <edge sink_node="1803" src_node="363" switch_id="0"/>
            g = re.search(r'<edge\s+sink_node="(\d+)"\s+src_node="(\d+)"', line)
            if not g: continue
            (snk, src) = g.groups()
            (snk, src) = (int(snk), int(src))
            nodes.add(snk)
            nodes.add(src)
            edges.append((src, snk))
        # for
    # with

    #dg = nx.DiGraph()   # directed
    #ug = nx.Graph()     # undirected
    dg = nx.DiGraph(edges)
    log(f"Built fabric graph with {len(nodes):,d} nodes and {len(edges):,d} edges\n")

    if g_false:
        comps = dg.biconnected_components(return_articulation_points=False)
        if len(comps) == 1:
            log("Graph is biconnected\n")
        else:
            log("Graph is NOT biconnected\n")

    #if g_false:
    #    maxflow = nx.maximum_flow_value(dg, sn, tn)
    #    out(f"flow={maxflow}\n")

    if g_true:
        nodelist = nx.shortest_path(dg, source=asrc, target=asnk)
        for node in nodelist:
            out(f"\t{n2id[node]}\n")
        if not nodelist:
            out("NO PATH RETURNED\n")

# def

toplogic()


# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: ignorecase filetype=python autoindent hlsearch syntax=on fileformat=unix
