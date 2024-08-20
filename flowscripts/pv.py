#!/usr/bin/python3
"""generate structural verilog using function calls"""

import sys
import atexit
import inspect
import warnings
import re 
from collections import defaultdict

def dict_aup_ndn(k):
    """sort with alpha ascending and numeric descending"""
    r = re.split(r'(\d+)', k)
    for i in range(1, len(r), 2):
        r[i] = -int(r[i])
    return r
# def dict_aup_ndn

# per run
g_filename2first = {}           # str : file path --> str : module name
g_name2master = {}              # str --> Master
# per module
g_script = None                 # str : path to user's script
g_base = ""                     # str : path to script minus suffix
g_newmodule = ""                # str : name of new module
g_portlist = []                 # [Port]
g_wiredecls = {}                # str : (left, right)
g_instances = defaultdict(int)  # str : module name --> int : auto-generated name count
g_name2instance = {}            # str : instance name --> Instance
g_assigns = {}                  # str : lhs_name --> (str : lhs, str : rhs)
g_sorted_assigns = True
g_sorted_instances = True
g_port_downto = True
g_written = -1                  # -1 = implicitly, 0 = module() called, 1 = written

class Port:
    """port on the module we are building, or on master we read"""
    def __init__(self, inout, name, left=None, right=None):
        """make a new Port"""
        self.inout = inout  # str
        self.name = name    # str
        if left is not None and right is None: right = left
        self.left = left    # int or None
        self.right = right  # int or None
    # def __init__
    @property
    def idx(self):
        """return the index expression for this port"""
        return f"[{self.left}:{self.right}]" if self.left is not None else ""
    # def idx
    @property
    def full(self):
        """return the full expression for this port"""
        return self.name + self.idx
    # def full
# class Port

# declare (API) : add port or wire to new module
#
# mode is input/inout/output
# name is port name
# left is left bit index or None
# right is right bit index or None
#
# no return value

def declare(mode, name, left=None, right=None):
    """add input/inout/output port to new module"""
    if right is None: right = left
    if mode == "wire":
        g_wiredecls[name] = (left, right)
    else:
        g_portlist.append(Port(mode, name, left, right))
# def declare

# assign (API) : add assign inside module
#
# rhs is a Verilog bundle of wires
# name is the name of the bus assigned to
# left is bus index if wire declaration needed
# right is bus index if wire declaration needed
#
# no return value

def assign(lhs, rhs):
    """add assign to lhs from rhs"""
    name = re.sub(r'[\d*:?\d+]$', r'', lhs)
    if name in g_assigns:
        raise ValueError(f"more than one assignment to the same bus '{name}'")
    g_assigns[name] = (lhs, rhs)
# def assign

class Master:
    """master footprint created from Verilog file"""
    def __init__(self, name, masterports):
        """construct new Master"""
        self.name = name    # str
        self.ports = masterports  # [Port]
        self.n2p = { p.name : p for p in masterports }    # name --> Port
    # def __init__
    def port(self, name):
        """look up a port by name"""
        return self.n2p.get(name, None)
    # def port
# class Master

def read_verilog(filename):
    """read Verilog file, pull out tokens, and save module footprints"""
    # read entire Verilog file
    with open(filename, encoding='ascii') as vfile:
        contents = vfile.read()
    sys.stderr.write(f"Read {filename}\n")

    # side data accumulated during parsing
    mode = None
    g = None
    # clear on module name
    mastername = None
    order = []
    port2mode = {}
    port2nums = {}
    # clear on input/inout/output
    nums = []

    # each parsing action is a small def
    def act_nop():
        """do nothing"""
    def act_setname():
        """save called module name"""
        nonlocal mastername; mastername = g
        order.clear()
        port2mode.clear()
        port2nums.clear()
    # def act_setname
    def act_order():
        """note port order in old-style Verilog"""
        order.append(g)
    # def act_order
    def act_input():
        """notice 'input'"""
        nonlocal mode; mode = 'input'
        nums.clear()
    # def act_input
    def act_inout():
        """notice 'inout'"""
        nonlocal mode; mode = 'inout'
        nums.clear()
    # def act_inout
    def act_output():
        """notice 'output'"""
        nonlocal mode; mode = 'output'
        nums.clear()
    # def act_output
    def act_num():
        """save bus index declarations"""
        nums.append(g)
    # def act_num
    def act_order_port():
        """note port order and decl in new-style Verilog"""
        order.append(g)
        port2mode[g] = mode
        port2nums[g] = list(nums)
    # def act_order_port
    def act_port():
        """note port decl in old-style Verilog"""
        port2mode[g] = mode
        port2nums[g] = list(nums)
    # def act_port
    def act_finish():
        """finish the module"""
        masterports = []
        for p in order:
            (left, right, *_) = port2nums[p] + [None, None]
            masterports.append(Port(port2mode[p], p, left, right))
        # for p
        g_name2master[mastername] = Master(mastername, masterports)
        if filename not in g_filename2first:
            g_filename2first[filename] = mastername
    # def act_finish

    # states are named after what we're waiting for
    w_module = 0    # module
    w_id     = 1    # name in "module name"
    w_paren  = 2    # ( in "module name ("
    w_pord   = 3    # port or declaration after (
    w_port   = 4    # port after input/inout/output after (
    w_semi   = 5    # semicolonm after )
    w_stmt   = 6    # input/inout/output after "module () ;"
    w_port2  = 7    # port after input/inout/output after "module () ;"

    # the "parser" state machine
    response = {
        (w_module,   'module'):      [w_id,       act_nop],
        (w_id,       'id'):          [w_paren,    act_setname],
        (w_paren,    ';'):           [w_stmt,     act_nop],
        (w_paren,    '('):           [w_pord,     act_nop],
        (w_pord,     'id'):          [w_pord,     act_order],
        (w_pord,     ')'):           [w_semi,     act_nop],
        (w_pord,     'input'):       [w_port,     act_input],
        (w_pord,     'inout'):       [w_port,     act_inout],
        (w_pord,     'output'):      [w_port,     act_output],
        (w_port,     'num'):         [w_port,     act_num],
        (w_port,     'id'):          [w_port,     act_order_port],
        (w_port,     ','):           [w_pord,     act_nop],
        (w_port,     ')'):           [w_semi,     act_nop],
        (w_semi,     ';'):           [w_stmt,     act_nop],
        (w_stmt,     'input'):       [w_port2,    act_input],
        (w_stmt,     'inout'):       [w_port2,    act_inout],
        (w_stmt,     'output'):      [w_port2,    act_output],
        (w_port2,    'num'):         [w_port2,    act_num],
        (w_port2,    'id'):          [w_port2,    act_port],
        (w_port2,    ';'):           [w_stmt,     act_nop],
        (w_stmt,     'endmodule'):   [w_module,   act_finish],
    }

    # list of keywords
    keywords = { k[1] for k in response if len(k[1]) > 1 }
    keywords.difference_update({'id', 'num'})
    # how to recognize identifiers and numbers in bit indices
    classify = {}
    for c in range(128):
        cc = chr(c)
        if cc.isalpha() or cc == "\\":  classify[cc] = "id"
        if cc.isdigit():                classify[cc] = "num"
    # for c

    # split input into tokens and act on each
    state = w_module
    for g in re.finditer(r'''
            //.*\n                              |   # line comments
            /[*](?:[^*]+|[*]+[^*/])*[*]/        |   # block comments
            "(?:[^\\"]+|\\.)*"                  |   # strings
            -?\d+                               |   # numbers
            -?\d*\s*'\s*s?\s*b\s*[_?xz01]+      |   # binary numbers
            -?\d*\s*'\s*s?\s*d\s*[_?xz0-9]+     |   # decimal numbers
            -?\d*\s*'\s*s?\s*o\s*[_?xz0-7]+     |   # octal numbers
            -?\d*\s*'\s*s?\s*h\s*[_?xz0-9a-f]+  |   # hex numbers
            \\\S+                               |   # escaped identifiers
            [a-z_][a-z_0-9$]*                   |   # identifiers
            [(),;]                                  # needed punctuation
            ''', contents, re.VERBOSE | re.IGNORECASE):
        # ignore: ~& ~| ~^ ^~ << >> && || >= <= === !== == !=       
        g = g.group()   # g retains original token
        key = (state, g if g in keywords else classify.get(g[0], g))
        if key in response:
            (state, func) = response[key]
            func()
    # for g
# def read_verilog

class Instance:
    """master footprint created from Verilog file"""
    def __init__(self, master, name):
        self.master = master    # str
        self.name = name        # str
        self.n2c = {}
    # def __init__
# class Instance

# instance (API) : add instance to new module
#
# filename is path to file containing definition
# module is name of module to use or None to use first in file
# instname is name of new instance or None to use module_<number>
#
# return value is instance name

def instance(filename, modname=None, instname=None):
    """create an instance"""
    # read this Verilog file if we haven't seen it before
    if filename not in g_filename2first:
        read_verilog(filename)
    # if no module specified, default to first in file
    if modname is None:
        modname = g_filename2first[filename]
    # check that the requested master exists
    if modname not in g_name2master:
        raise LookupError(f"no definition for requested master '{modname}'")
    # if no instance name specified, generate one
    if instname is None:
        instname = f"{modname}_{g_instances[modname]}"
        g_instances[modname] += 1
    # create the instance
    master = g_name2master[modname]
    inst = g_name2instance[instname] = Instance(master, instname)
    # make the default connections
    for p in master.ports:
        inst.n2c[p.name] = f"{instname}__{p.full}"
    return instname
# def instance

# getports (API) : return list of ports on instance
#
# instname is name of the previously created instance
#
# return value is list of Ports

def getports(instname):
    """returns list of port objects for named instance"""
    if instname not in g_name2instance:
        raise LookupError(f"no such instance '{instname}'")
    return g_name2instance[instname].master.ports
# def getports

# getport (API) : return port on instance
#
# instname is name of the previously created instance
# portname is name of its port
#
# return value is a Port, None if none known by portname

def getport(instname, portname):
    """return Port on master of instance"""
    if instname not in g_name2instance:
        raise LookupError(f"no such instance '{instname}'")
    return g_name2instance[instname].master.port(portname)
# def getport

# bundle (API) : connect instance port to bundle or wires
#
# instname is name of the previously created instance
# portname is name of the instance port
# wires is any legal argument to a Verilog port
# - including concatenations and repetitions
#
# no return value

def bundle(instname, portname, wires):
    """connects port on named instance to listed wires"""
    if instname not in g_name2instance:
        raise LookupError(f"no instance '{instname}'")
    inst = g_name2instance[instname]
    if portname not in inst.n2c:
        raise LookupError(f"no pin '{portname}' on instance '{instname}'")
    inst.n2c[portname] = wires
# def bundle

# bus (API) : connect instance port to a bus
#
# instname is name of the previously created instance
# portname is name of the instance port
# bus is name of the bus
#
# no return value

def bus(instname, portname, arg):
    """connects port on named instance to bus with same indices"""
    if instname not in g_name2instance:
        raise LookupError(f"no instance '{instname}'")
    inst = g_name2instance[instname]
    if portname not in inst.n2c:
        raise LookupError(f"no pin '{portname}' on instance '{instname}'")
    master = inst.master
    port = master.port(portname)
    inst.n2c[portname] = f"{arg}{port.idx}"
# def bus

# print out Verilog on exit
def write_verilog(vfile, dfile, wire2info):
    """write out the module we have built"""

    inout2mask = { 'output' : 1, 'input': 2, 'inout': 4, }
    vfile.write(f"module {g_newmodule} (")

    # write out new-style ports forced by user declarations
    #  remember 1=output 2=input 4=inout
    portwritten = set()
    sep = ""
    lastwritten = None
    for port in g_portlist:
        portname = port.name
        mask = inout2mask[port.inout]
        (left, right) = (port.left, port.right)   # could be reversed
        if portname in wire2info:
            (imask, imax, imin) = wire2info[portname]  # always None or imax >= imin
            # combine indices
            if left is not None and imax is None:
                raise ValueError(
        f"bus '{portname}' used internally w/o indices and externally with indices")
            # merging imax, imin (usage) into left, right from declare() --> left, right
            if imax is not None:
                if left is None:
                    # if port indices missing, default to left>right from usage
                    if g_port_downto:
                        (left, right) = (imax, imin)
                    else:
                        (left, right) = (imin, imax)
                elif left >= right:
                    # if port indices NOT left<right, default to left>right & grow to usage
                    (left, right) = (max(left, imax), min(right, imin))
                else:   # left < right
                    # we generate left<right ONLY when user said so in declare()
                    (left, right) = (min(left, imin), max(right, imax))
                    # confusing! left/right backwards here
            # check directions
            imask2 = imask & 7
            if mask == 2 and imask2 == 2:
                pass
            elif (mask ^ imask2) & ~2:
                raise ValueError(f"declared direction of port '{portname}' incompatible with internal use")
            # check index falling vs rising
            if left is not None and (imask & 24):
                if left > right and imask & 16 or left < right and imask & 8:
                    warnings.warn(
                        "bit index direction mismatch between port and internal use")
        else:
            if mask != 2:
                warnings.warn("ports not connected internally should be inputs")
        if left is None:
            vfile.write(f"{sep}\n\t{port.inout}\t\t{portname}")
        else:
            vfile.write(f"{sep}\n\t{port.inout} [{left}:{right}]\t{portname}")
        sep = ","
        portwritten.add(portname)
        lastwritten = 0
    # for port

    # write out new-style ports deduced from internal connections
    wires = sorted(wire2info, key=dict_aup_ndn)
    #  remember 1=output 2=input 4=inout
    for mask, inout in [[2, "input"], [4, "inout"], [1, "output"]]:
        for wire in wires:
            if wire in g_wiredecls: continue
            (imask, imax, imin) = wire2info[wire]
            if (imask & 7) != mask: continue
            if wire in portwritten: continue

            if lastwritten is not None and lastwritten != mask:
                sep += "\n"
            lastwritten = mask

            if imax is None:
                vfile.write(f"{sep}\n\t{inout}\t\t{wire}")
            else:
                if not g_port_downto:
                    (imax, imin) = (imin, imax)
                vfile.write(f"{sep}\n\t{inout} [{imax}:{imin}]\t{wire}")
            sep = "," if wire[0] != "\\" else " ,"
            portwritten.add(wire)
        # for wire
    # for mask, inout

    # finish module statement
    vfile.write("\n);\n")

    # write out wire declarations
    lastwritten = None
    for wire in wires:
        if wire in portwritten: continue
        if lastwritten is None:
            vfile.write("\n")
            lastwritten = 1
        (imask, imax, imin) = wire2info[wire]
        if wire in g_wiredecls:
            (wleft, wright) = g_wiredecls[wire]
            if imax is None and wleft is not None:
                raise ValueError(f"wire '{wire}' declares and uses indices inconsistently")
            if wleft is not None:
                # retain l<r or l>r used in declaration
                if wleft < wright:
                    (imax, imin) = ( min(imin, wleft), max(imax, wright) )
                else:
                    (imax, imin) = ( max(imax, wleft), min(imin, wright) )
        sp = "" if wire[0] != "\\" else " "
        if imax is None:
            vfile.write(f"wire\t\t{wire}{sp};\n")
        else:
            vfile.write(f"wire [{imax}:{imin}]\t{wire}{sp};\n")
    # for wire

    # write out assigns
    lastwritten = None
    if g_sorted_assigns:
        assigns = sorted(g_assigns, key=dict_aup_ndn)
    else:
        assigns = list(g_assigns)
    for name in assigns:
        (lhs, rhs) = g_assigns[name]
        if lastwritten is None:
            vfile.write("\n")
            lastwritten = 1
        rsp = "" if rhs[0] != "\\" else " "
        vfile.write(f"assign {lhs} = {rhs}{rsp};\n")
    # for name

    # write out instances
    if g_sorted_instances:
        instances = sorted(g_name2instance, key=dict_aup_ndn)
    else:
        instances = list(g_name2instance)
    for name in instances:
        inst = g_name2instance[name]
        master = inst.master
        vfile.write(f"\n{master.name} {name} (")
        sep = ""
        for p in master.ports:
            conn = inst.n2c[p.name]
            csp = "" if not re.search(r'\\\S+$', conn) else " "
            vfile.write(f"{sep}\n\t.{p.name}({conn}{csp})")
            sep = ","
        # for p
        vfile.write(");\n")
    # for name

    # endmodule
    vfile.write("\nendmodule\n")

    # Makefile dependencies
    for file in sorted(g_filename2first, key=dict_aup_ndn):
        dfile.write(f"{g_base}.v: {file}\n")
    dfile.write(f"{g_base}.v: {g_script}\n")
# def write_verilog

def markwires(wire2info, conn, mask):
    """update info about wires based on usage"""
    conn = re.sub(r'\s', r'', conn)
    conn = re.sub(r'[{},]', r' ', conn)
    for c in conn.split():
        # ignore constants
        if c[0] in "-0123456789'": continue
        # pull out bus name and possible indices
        g = re.match(r'([a-zA-Z_0-9$]+)(?:\[(\d+)(?::(\d+))?\])?$', c)
        if not g:
            raise ValueError(f"malformed signal reference '{c}'")
        (n, left, right) = g.groups()
        if right is not None:
            (left, right) = (int(left), int(right))
            direc = 8 if left > right else 16 if left < right else 0
            upper = max(left, right)
            lower = min(left, right)
        elif left is not None:
            direc = 0
            upper = lower = int(left)
        else:
            direc = 0
            upper = lower = None
        if n in wire2info:
            (_, imax, imin) = wire2info[n]
            if (upper is None) != (imax is None):
                raise ValueError(f"bus '{n}' used with and without bit indices")
            wire2info[n][0] |= mask | direc
            if upper is not None:
                if upper > imax: wire2info[n][1] = upper
                if lower < imin: wire2info[n][2] = lower
        else:
            wire2info[n] = [mask | direc, upper, lower]

    # for c
# def markwires

# finalize module and print
def endmodule():
    """collect info and write out Verilog"""
    global g_written
    if g_written ==  1:
        return
    if g_written == -1 and not g_portlist and not g_assigns and not g_name2instance:
        return

    # go through all instances and build info about nets seen
    inout2mask = { 'output' : 1, 'input': 2, 'inout': 4, }
    wire2info = {}
    for inst in g_name2instance.values():
        master = inst.master
        # go through all connections
        for portname, conn in inst.n2c.items():
            port = master.port(portname)
            mask = inout2mask[port.inout]
            markwires(wire2info, conn, mask)
        # for portname
    # for inst

    # do the same for assigns
    for lhs, rhs in g_assigns.values():
        markwires(wire2info, lhs, inout2mask['output'])
        markwires(wire2info, rhs, inout2mask['input'])

    # ensure there are no busses with both output and inout pins
    # check for consistent bit index usage
    for n, triple in wire2info.items():
        mask = triple[0]
        if mask & 5 == 5:
            raise ValueError(f"internal signal '{n}' has both output and inout pins")
        if mask & 24 == 24:
            warnings.warn("internal signal used with both falling and rising indices")
    # for _

    # now write out the Verilog (and dependencies)
    with open(f"{g_base}.v",      "w", encoding='ascii') as vfile, \
         open(f"{g_base}.depend", "w", encoding='ascii') as dfile:
        write_verilog(vfile, dfile, wire2info)
    sys.stderr.write(f"Made {g_base}.v\n")

    g_portlist.clear()
    g_wiredecls.clear()
    g_assigns.clear()
    g_name2instance.clear()
    g_written = 1
# def endmodule

def module(name, *, sorted_assigns = True, sorted_instances = True, port_downto = True):
    """start new module"""
    endmodule()
    global g_newmodule, g_base, g_written
    global g_sorted_assigns, g_sorted_instances, g_port_downto
    g_newmodule = name
    g_base = re.sub(r'\w+$', name, g_base)
    g_written = 0
    g_sorted_assigns = sorted_assigns
    g_sorted_instances = sorted_instances
    g_port_downto = port_downto
    g_portlist.clear()
    g_wiredecls.clear()
    g_instances.clear()
    g_name2instance.clear()
    g_assigns.clear()
# def module

# TOP LEVEL
# if executed directly, show usage message
if __name__ == "__main__":
    print('''
pv is a Python package to generate structural netlists.

Here's an example of how to use it:

#!/usr/bin/python

import pv

# this will declare "output [7:0] qout" or wider.
pv.declare("output", "qout", 7, 0):
# usually ports and wires are inferred for you and this provides an override.
# the indices are optional.

# assign some bundle of wires to a new bus
pv.assign("d_feed", "{8{d[0]}}")

# this will read regfile.v to determine all ports on regfile.
# new instance will be named uregfile,
# which defaults to <module>_# where # starts at 0.
# module defaults to first in file (all are read).
pv.instance("regfile.v", "regfile", "uregfile")
# Each port is preconnected to <instance>__<port>; use bundle/bus to change.
# The return value is the name of the instance.

# step through the ports on an instance
for port in pv.getports("uregfile"):
    # here are the available attributes:
    port.inout  str : input/inout/output
    port.name   str : name of port
    port.left   int : left index or None
    port.right  int : right index or None
    port.idx    str : "[left:right]" or ""
    port.full   str : name + idx

# get one port on an instance (same attributes as above)
port = getport(instname, portname)

# connect an instance port to a bundle of wires
pv.bundle("uregfile", "d", "{qout[7:1],di}"):

# connect an instance port to a bus with matching indices
pv.bus("uregfile", "q", "qout"):

# Verilog to <script_base_name>.v & dependencies to <script_base_name>.depend.
# Please use as shown above -- running directly gets this message.
''')
    sys.exit(0)

# TOP LEVEL
# determine default name of module (if no module() call)
for frame in inspect.stack()[1:]:
    if frame.filename[0] != '<':
        # remove leading directory
        g_script = frame.filename.split('/')[-1]
        break
if not g_script:
    raise ImportError("unknown caller")
g_base = re.sub(r'[.][^./]*$', r'', g_script)
module(g_base.split('/')[-1])
g_written = -1

# TOP LEVEL
atexit.register(endmodule)


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
