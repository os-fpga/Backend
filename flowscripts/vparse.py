#!/usr/bin/python3
"""read verilog module and port declarations"""

import re 

class Port:
    """port on the module"""
    def __init__(self, inout, name, left=None, right=None):
        """make a new Port"""
        self.inout = inout  # str
        self.name = name    # str
        # left/right are both #s or both None
        if left is not None and right is None: right = left
        self.left = left    # int or None
        self.right = right  # int or None
    # def __init__

    @property
    def width(self):
        """return how many bits in this port"""
        # TODO use -1 instead of None
        return 1 if self.left is None else 1 + abs(int(self.left) - int(self.right))
    @property
    def idx(self):
        """return the index expression for this port"""
        return  "" if self.left is None else \
                f"[{self.left}]" if self.left == self.right else \
                f"[{self.left}:{self.right}]"
    # def idx
# class Port

class Module:
    """module footprint created from Verilog file"""
    def __init__(self, name, ports):
        """construct new Module"""
        self.name = name                            # str
        self.ports = ports                          # [Port]
        self.n2p = { p.name : p for p in ports }    # name --> Port
    # def __init__
    def port(self, name):
        """look up a port by name"""
        return self.n2p.get(name, None)
    # def port
# class Module

def read_verilog(name2module, filename):
    """ 1. read Verilog file
        2. save module names and port info in name2module
        3. return list of module names read in order"""

    # 1: side data accumulated during parsing
    modules     = []    # module definitions read (in order)
    t           = None  # raw token just seen
    mname       = None  # name of module
    # clear on module name
    order       = []    # ports in order
    # clear info on input/inout/output
    info        = []    # [input/inout/output, #, #, ...]
    port2info   = {}    # port --> [input/inout/output, #, #, ...]

    # 2: read entire Verilog file
    with open(filename, encoding='ascii') as vfile:
        contents = vfile.read()

    # 3: each parsing action is a small def
    def act_nop():
        """do nothing"""
    def act_setname():
        """save module name"""
        nonlocal mname
        mname = t
        order.clear()
        port2info.clear()
    # def act_setname
    def act_order():
        """note port order in old-style Verilog"""
        order.append(t)
    # def act_order
    def act_dir():
        """notice 'input'/'inout'/'output'"""
        nonlocal info
        info = [t]
    # def act_dir
    def act_num():
        """save bus index declarations"""
        info.append(t)
    # def act_num
    def act_order_port():
        """note port order and decl in new-style Verilog"""
        order.append(t)
        port2info[t] = list(info)
    # def act_order_port
    def act_port():
        """note port decl in old-style Verilog"""
        port2info[t] = list(info)
    # def act_port
    def act_finish():
        """finish the module"""
        ports = []
        for p in order:
            (p, b) = (p[1:], p) if p[0] == "\\" else (p, "\\" + p)
            info = port2info.get(p, port2info.get(b, ['undeclared']))
            (mode, left, right, *_) = info + [None, None]
            ports.append(Port(mode, p, left, right))
        # for p
        name2module[mname] = Module(mname, ports)
        modules.append(mname)
    # def act_finish

    # 4: START TABLE INIT #

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
        # in state  see     THEN    new state   action
        (w_module,  'module'):      [w_id,      act_nop],
        (w_id,      'id'):          [w_paren,   act_setname],
        (w_paren,   ';'):           [w_stmt,    act_nop],
        (w_paren,   '('):           [w_pord,    act_nop],
        (w_pord,    'id'):          [w_pord,    act_order],
        (w_pord,    ')'):           [w_semi,    act_nop],
        (w_pord,    'input'):       [w_port,    act_dir],
        (w_pord,    'inout'):       [w_port,    act_dir],
        (w_pord,    'output'):      [w_port,    act_dir],
        (w_port,    'num'):         [w_port,    act_num],
        (w_port,    'id'):          [w_port,    act_order_port],
        (w_port,    ','):           [w_pord,    act_nop],
        (w_port,    ')'):           [w_semi,    act_nop],
        (w_semi,    ';'):           [w_stmt,    act_nop],
        (w_stmt,    'input'):       [w_port2,   act_dir],
        (w_stmt,    'inout'):       [w_port2,   act_dir],
        (w_stmt,    'output'):      [w_port2,   act_dir],
        (w_port2,   'num'):         [w_port2,   act_num],
        (w_port2,   'id'):          [w_port2,   act_port],
        (w_port2,   ';'):           [w_stmt,    act_nop],
        (w_stmt,    'endmodule'):   [w_module,  act_finish],
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

    # 4: END TABLE INIT #

    # 5: split input into tokens and act on each
    state = w_module
    for g in re.finditer(r'''
            //.*\n                              |   # line comments
            /[*](?:[^*]+|[*]+[^*/])*[*]/        |   # block comments
            "(?:[^\\"]+|\\.)*"                  |   # strings
            -?\d+                               |   # numbers
            -?\d*\s*'\s*s?\s*b\s*[_?xz01]+      |   # binary numbers
            -?\d*\s*'\s*s?\s*o\s*[_?xz0-7]+     |   # octal numbers
            -?\d*\s*'\s*s?\s*d\s*[_?xz0-9]+     |   # decimal numbers
            -?\d*\s*'\s*s?\s*h\s*[_?xz0-9a-f]+  |   # hex numbers
            \\\S+                               |   # escaped identifiers
            [a-z_][a-z_0-9$]*                   |   # identifiers
            [(),;]                                  # needed punctuation
            ''', contents, re.VERBOSE | re.IGNORECASE):
        # ignore: ~& ~| ~^ ^~ << >> && || >= <= === !== == !=       
        t = g.group()   # t retains original token
        key = (state, t if t in keywords else classify.get(t[0], t))
        if key in response:
            (state, func) = response[key]
            func()
    # for g

    # 6: return modules read
    return modules
# def read_verilog


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
