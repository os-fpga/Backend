#!/usr/bin/env python
"""convert XML to JSON"""

# This example script shows how to walk through the architecture XML file to grab various items


import argparse
import sys
import os

import logging
from arch_lib.vpr_dm import VprDataModel
from arch_lib.xml_dm import validate

from lxml import etree
from copy import deepcopy

def walk_pb_type(pb_type):
    """ recursive routine to walk through pb_type """

    # first dump local attributes

    for clock in pb_type.clocks:
        print("Found clock %s" % clock.name)

    for input in pb_type.inputs:
        print("Found input %s" % input.name)

    for output in pb_type.outputs:
        print("Found output %s" % output.name)

    for setup in pb_type.T_setups:
        print("Found setup with attributes %s" % setup.attribs)

    for hold in pb_type.T_holds:
        print("Found hold with attributes %s" % hold.attribs)

    for ctq in pb_type.T_clock_to_Qs:
        print("Found clock-to-q with attributes %s" % ctq.attribs)

    for dm in pb_type.delay_matrixs:
        print("Found delay_matrix with attributes %s and text %s" % (dm.attribs, dm.text))

    for dc in pb_type.delay_constants:
        print("Found delay_constants with attributes %s" % dc.attribs)

    for mode in pb_type.modes:
        print("Found mode %s" % mode.name)

        for intc in mode.interconnects:
            print("Found interconnect element")

            for complete in intc.completes:
                print("Found complete with attributes %s" % complete.attribs)

            for dir in intc.directs:
                print("Found direct with attributes %s" % intc.attribs)

            for mux in intc.muxs:
                print("Found mux with attributes %s" % mux.attribs)

                for dc in mux.delay_constants:
                    print("Found delay_constant with attributes %s" % dc.attribs)

        for pbt in mode.pb_types:
            walk_pb_type(pbt)

    for pbt in pb_type.pb_types:
        walk_pb_type(pbt)

def main():
    """ main routine """

    parser = argparse.ArgumentParser(
        prog='dump_vpr_xml',
        description='dump vpr XML',
        epilog='Text at bottom of help')

    parser.add_argument('--input')
    parser.add_argument('--output')
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


    # read the input file specified
    arch = VprDataModel(args.input, validate=False)
    logger.info(f"Read in {args.input}")

    # walk through a typical architecture XML
    for mods in arch.modelss:
        print("Found models element")
        for mod in mods.models:

            print("Found model %s" % mod.name)
            for inp in mod.input_portss:
                print("Found input_ports element")

                for port in inp.ports:
                    print("Found input port %s" % port.name)

            for outp in mod.output_portss:
                print("Found output_ports element")

                for port in outp.ports:
                    print("Found output port %s" % port.name)

    for tiletop in arch.tiless:
        print("Found tiles element")

        for tile in tiletop.tiles:
            print("Found tile %s" % tile.name)
            print("Found tile %s" % tile.attribs)

            for sub_tile in tile.sub_tiles:
                print("Found sub_tile with attributes %s" % sub_tile.attribs)


                for equivsite in sub_tile.equivalent_sitess:
                    print("Found equivalent_sites element")

                    for site in equivsite.sites:
                        print("Found pb_site %s" % site.pb_type)

                for clock in sub_tile.clocks:
                    print("Found clock %s" % clock.name)

                for input in sub_tile.inputs:
                    print("Found input %s" % input.name)

                for output in sub_tile.outputs:
                    print("Found output %s" % output.name)

                for fc in sub_tile.fcs:
                    print("Found fc with attributes %s" % fc.attribs)

                    for fc_override in fc.fc_overrides:
                        print("Found fc_override with attributes %s" % fc_override.attribs)

                for pinloc in sub_tile.pinlocationss:
                    print("Found pinlocation patter %s" % pinloc.pattern)

                    for loc in pinloc.locs:
                        print("Found location %s with value %s" % (loc.side, loc.text))

    for dev in arch.devices:
        print("Found device element")

        for sizing in dev.sizings:
            print("Found sizing with attributes %s" % sizing.attribs)

        for area in dev.areas:
            print("Found area with attributes %s" % area.attribs)

        for cwd in dev.chan_width_distrs:
            print("Found chan_width_distr element")

            for x in cwd.xs:
                print("Found x with attributes %s" % x.attribs)

            for y in cwd.ys:
                print("Found y with attributes %s" % y.attribs)

        for sb in dev.switch_blocks:
            print("Found switch_block with attributes %s" % sb.attribs)

        for cb in dev.connection_blocks:
            print("Found connection_block with attributes %s" % cb.attribs)

    for swlist in arch.switchlists:
        print("Found switchlist element")

        for switch in swlist.switchs:
            print("Found switch with attributes %s" % switch.attribs)

    for seglist in arch.segmentlists:
        print("Found segmentlist element")

        for segment in seglist.segments:
            print("Found segment with attributes %s" % segment.attribs)

            for mux in segment.muxs:
                print("Found mux named %s" % mux.name)

            for sb in segment.sbs:
                print("Found sb %s with text %s" % (sb.type, sb.text))

    for dirlist in arch.directlists:
        print("Found directlist element")

        for dir in dirlist.directs:
            print("Found direct with attributes %s" % dir.attribs)

    for cblist in arch.complexblocklists:
        print("Found complexblocklist element")

        for pbt in cblist.pb_types:
            walk_pb_type(pbt)

    for layout in arch.layouts:
        print("Found layout with attributes %s" % layout.attribs)

        for fl in layout.fixed_layouts:
            print("Found fixed_layout %s" % fl.name)

            for row in fl.rows:
                print("Found row with attributes %s" % row.attribs)

            for col in fl.cols:
                print("Found col with attributes %s" % col.attribs)

            for corner in fl.cornerss:
                print("Found corner with attributes %s" % corner.attribs)

            for fill in fl.fills:
                print("Found fill with attributes %s" % fill.attribs)

            for region in fl.regions:
                print("Found region with attributes %s" % region.attribs)


    # write out the current dataset
    arch.dump("vpr_output.xml")
    logger.info("Wrote vpr_output.xml")

if __name__ == "__main__":
    main()
