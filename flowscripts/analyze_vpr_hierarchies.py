#!/usr/bin/env python
"""convert XML to JSON"""

# This example script shows how to walk through the architecture XML file to grab various items


import argparse
import sys
import os

import logging
from arch_lib.vpr_dm import VprDataModel
from arch_lib.arch_dm import ArchDataModel
from arch_lib.xml_dm import validate

from lxml import etree
from copy import deepcopy

def walk_pb_type(pb_type, level=0, arch_prefix=None, verilog_prefix=None, pb2cmod_dict=dict(), cmod_dict=dict()):
    """ recursive routine to walk through pb_type """

    # compute module name
    # treat top level special
    if verilog_prefix is None:
        verilog_module_name = f"logical_tile_{pb_type.name}_mode_{pb_type.name}_"
        arch_module_name = f"{pb_type.name}"
    else:
        verilog_module_name = f"{verilog_prefix}__{pb_type.name}"
        arch_module_name = f"{arch_prefix}.{pb_type.name}"

    # figure out global ports at this level
    global_ports = set()
    if arch_module_name in pb2cmod_dict:
        cmodname = pb2cmod_dict[arch_module_name]
        if cmodname in cmod_dict:
            # now figure out global ports, if any

            cm = cmod_dict[cmodname]
            for port in cm.ports:
                if port.is_global and port.is_global == "true":
                    # we have a global por

                    # figure out the name
                    if port.is_io and port.is_io == "true":
                        portname = f"gfpga_{pb_type.name}_{cm.name}_{port.prefix}"
                    else:
                        portname = f"{port.prefix}"

                    global_ports.add(portname)

    #
    # find children
    #
    # explicit mode-based children
    childrenstring = ""
    for mode in pb_type.modes:
        # skip modes that are not physical
        if mode.disable_packing is None or mode.disable_packing != "true":
            continue

        mode_name = mode.name
        for pbt in mode.pb_types:
            if verilog_prefix is None:
                next_verilog_prefix = f"logical_tile_{pb_type.name}_mode_{mode_name}"
                next_arch_prefix = f"{pb_type.name}"
            else:
                next_verilog_prefix = f"{verilog_prefix}__{pb_type.name}_mode_{mode_name}"
                next_arch_prefix = f"{arch_prefix}.{pb_type.name}[{mode.name}]"

            (child_global_ports, childstring) = walk_pb_type(pbt, level+1, arch_prefix=next_arch_prefix, verilog_prefix=next_verilog_prefix, pb2cmod_dict=pb2cmod_dict, cmod_dict=cmod_dict)
            global_ports.update(child_global_ports)
            childrenstring += childstring

    # anything below here means mode=default
    mode_name = "default"

    for pbt in pb_type.pb_types:

        if verilog_prefix is None:
            next_verilog_prefix = f"logical_tile_{pb_type.name}_mode_{mode_name}"
            next_arch_prefix = f"{pb_type.name}"
        else:
            next_verilog_prefix = f"{verilog_prefix}__{pb_type.name}_mode_{mode_name}"
            next_arch_prefix = f"{arch_prefix}.{pb_type.name}"

        (child_global_ports, childstring) = walk_pb_type(pbt, level+1, arch_prefix=next_arch_prefix, verilog_prefix=next_verilog_prefix, pb2cmod_dict=pb2cmod_dict, cmod_dict=cmod_dict)
        global_ports.update(child_global_ports)
        childrenstring += childstring

    indent = level*' '*4
    modstring = "%sPBTYPE module: %s\n" % (indent, verilog_module_name)

    # print out ports, now that we have all of them
    # dump ports in order of Verilog module enumeration:
    #    inputs, bl/wl, outputs, clocks
    for portname in global_ports:
        modstring +="%s    GPORT %s\n" % (indent, portname)

    # inputs
    for port in pb_type.inputs:
        portname = f"{pb_type.name}_{port.name}"
        modstring += "%s    PORT %s\n" % (indent, portname)

    # TODO: calculate number of bitlines/wordlines
    modstring += "%s    PORT bl\n" % indent
    modstring += "%s    PORT wl\n" % indent

    # outputs
    for port in pb_type.outputs:
        portname = f"{pb_type.name}_{port.name}"
        modstring += "%s    PORT %s\n" % (indent, portname)

    # clocks, inputs, outputs from vpr.xml
    for port in pb_type.clocks:
        portname = f"{pb_type.name}_{port.name}"
        modstring += "%s    PORT %s\n" % (indent, portname)

    return global_ports, modstring + childrenstring


def find_arch_info(arch):
    """ find various information needed for processing """

    # pb2circ_dict:  dictionary mapping pb_type to circuit name
    # e.g. 
    #  <pb_type name="io[physical].iopad.pad" circuit_model_name="QL_PREIO" mode_bits=""/>
    # 
    # would result in an entry with a key named io[physical].iopad.pad and value "QL_PREIO".
    # The value then corresponds to a key in the circuit model dictionary cmod_dict.
    
    # cmod_dict:  dictionary mapping a circuit model to a circuit_model element of the arch XML
    #

    # process pb_type first, as it is earlier in the arch XML
    pb2cmod_dict = {}
    
    for pta in arch.pb_type_annotationss:
        for pb in pta.pb_types:

            # store only those that have circuit_model_name
            if pb.circuit_model_name:
                pb2cmod_dict[pb.name] = pb.circuit_model_name

    cmod_dict = {}
    for circlib in arch.circuit_librarys:

        # buid a dictionary where the key is the type name and the value is a list of ports for which is_global="true"
        #
        # <circuit_model>:
        #    prefix: circmod.prefix
        #    ports:  <list of port prefix for ports that have is_global == "true"
        for circmod in circlib.circuit_models:

            # assumption: global ports exist only for models with verilog_netlist
            if circmod.verilog_netlist is None:
                continue

            if circmod.name in cmod_dict:
                print(f"Error:  Found duplicate circuit_model named {circmod.name}. Keeping first one.")
            else:
                cmod_dict[circmod.name] = circmod

    return (pb2cmod_dict, cmod_dict)

def main():
    """ main routine """

    parser = argparse.ArgumentParser(
        prog='dump_vpr_xml',
        description='dump vpr XML',
        epilog='Text at bottom of help')

    parser.add_argument('--vpr')
    parser.add_argument('--arch')
    parser.add_argument('--log')
    parser.add_argument('-v', '--verbose',
                        action='store_true')  # on/off flag

    args = parser.parse_args()

    # set up logger
    logname = f"{os.path.splitext(os.path.basename(sys.argv[0]))[0]}.log"
    if args.log:
        logname = args.log

    #
    # set up logging
    #
    logger = logging.getLogger('main')
    logger.setLevel(logging.INFO)

    # create console handler
    log_ch = logging.StreamHandler()
    logger.addHandler(log_ch)

    # create file handler
    log_fh = logging.FileHandler(logname, mode='w')
    logger.addHandler(log_fh)

    #
    # Process data
    #

    arch_file = args.arch
    vpr_file = args.vpr

    # read the vpr file specified
    arch = ArchDataModel(arch_file, validate=False)
    logger.info(f"Read in {arch_file}")

    # build a list of of global ports from the openfpga augmentation file
    (pb2cmod_dict, cmod_dict) = find_arch_info(arch)

    # read the vpr file specified
    vpr = VprDataModel(vpr_file, validate=False)
    logger.info(f"Read in {vpr_file}")

    # identify verilog modules from tiles
    for tiletop in vpr.tiless:
        for tile in tiletop.tiles:
            print(f"TILE module: {tile.name}")
            
            # children is from the site
            for sub_tile in tile.sub_tiles:
                for equiv_site in sub_tile.equivalent_sitess:
                    for site in equiv_site.sites:
                        module_name = f"logical_tile_{site.pb_type}_mode_{site.pb_type}_"
                        print(f"    PBTYPE module:  {module_name}")

    # identify verilog modules from complexblocklist
    for cblist in vpr.complexblocklists:
        for pbt in cblist.pb_types:
            (_ , outstring) = walk_pb_type(pbt, arch_prefix=None, verilog_prefix=None, pb2cmod_dict=pb2cmod_dict, cmod_dict=cmod_dict)
            print(outstring)

if __name__ == "__main__":
    main()
