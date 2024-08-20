#!/usr/bin/env python
"""convert XML to JSON"""

# This example script shows how to walk through the architecture XML file to grab various items


import argparse
import sys
from lxml import etree
from arch_lib.arch_dm import ArchDataModel
from copy import deepcopy

def main():
    """ main routine """

    parser = argparse.ArgumentParser(
        prog='dump_arch_xml',
        description='dump architecture XML',
        epilog='Text at bottom of help')
    
    parser.add_argument('--input') 
    parser.add_argument('--output')
    parser.add_argument('-v', '--verbose',
                        action='store_true')  # on/off flag

    args = parser.parse_args()

    # read the input file specified
    arch = ArchDataModel(args.input, validate=False)

    # walk through a typical architecture XML
    for techlib in arch.technology_librarys:
        print("Found technology_library element")

        for devlib in techlib.device_librarys:
            print("Found device_library")

            for devmod in devlib.device_models:
                print("Found device_model %s" % devmod.name)

                for lib in devmod.libs:
                    print("Found lib with attributes %s" % devmod.attribs)

                    for design in devmod.designs:
                        print("Found design with vdd = %s, pn_ratio = %s" % (design.vdd, design.pn_ratio))

                    for xtr in devmod.pmoss:
                        print("Found xtr with attributes %s" % xtr.attribs)

                    for xtr in devmod.nmoss:
                        print("Found xtr with attributes %s" % xtr.attribs)

        for varlib in techlib.variation_librarys:
            print("Found variation library")

            for variation in varlib.variations:
                print("Found variation %s" % variation.name)

    for circlib in arch.circuit_librarys:
        print("Found circuit library")

        for circmod in circlib.circuit_models:
            print("Found circuit model with attributes %s" % circmod.attribs)

            for destech in circmod.design_technologys:
                print("Found design_technology with attributes %s" % destech.attribs)

            for port in circmod.ports:
                print("Found port with attributes %s" % port.attribs)

            for delaymatrix in circmod.delay_matrixs:
                if delaymatrix.text is None:
                    value = None
                else:
                    value = delaymatrix.text.strip()

                print("Found delay_matrix with value %s and attributes %s" % (value, port.attribs))

            for inbuf in circmod.input_buffers:
                print("Found input_buffer with attributes %s" % inbuf.attribs)

            for outbuf in circmod.output_buffers:
                print("Found output_buffer with attributes %s" % outbuf.attribs)

            for pglogic in circmod.pass_gate_logics:
                print("Found pass_gate_logic with attributes %s" % pglogic.attribs)

    for config_proto in arch.configuration_protocols:
        print("Found configuration_protocol")

        for org in config_proto.organizations:
            print("Found organization with attribute %s" % org.attribs)

            for bl in org.bls:
                print("found bl with protocol = %s" % bl.protocol)

            for bl in org.wls:
                print("found wl with protocol = %s" % bl.protocol)

    for conblock in arch.connection_blocks:
        print("Found connection block")

        for switch in conblock.switchs:
            print("Found switch with attribute = %s" % switch.attribs)

    for swblock in arch.switch_blocks:
        print("Found switch_block")

        for switch in swblock.switchs:
            print("Found switch with attribute = %s" % swblock.attribs)

    for rseg in arch.routing_segments:
        print("Found routing_segment")

        for seg in rseg.segments:
            print("Found segment with attribute = %s" % seg.attribs)


    for dcon in arch.direct_connections:
        print("Found direct_connection")

        for direct in dcon.directs:
            print("Found direct %s with circuit_model_name = %s" % (direct.name, direct.circuit_model_name))

    for tileann in arch.tile_annotationss:
        print("Found tile annotations")

        for globport in tileann.global_ports:
            print("Found global_port %s with attributes %s" % (globport.name, globport.attribs))

            for tile in globport.tiles:
                print("Found tile %s with attributes %s" % (tile.name,  tile.attribs))

    for pta in arch.pb_type_annotationss:
        print("Found pb_type_annotations")

        for pbt in pta.pb_types:
            print("Found pb_type with attributes %s" % pbt.attribs)

            for interconn in pbt.interconnects:
                print("Found interconnect with attributes %s" % interconn.attribs)

            for port in pbt.ports:
                print("Found port %s with attributes %s" % (port.name, port.attribs))


    # write out the current dataset
    arch.dump("output.xml")

if __name__ == "__main__":
    main()
