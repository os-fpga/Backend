#!/usr/bin/env python3
""" Rapid Silicon's Architectural XML data model """

#
# Files in this set include
#   vpr_dm.py:   handles the following elements:
#     - architecture
#
#   vpr_mod_lib_dm.py:   handles the following elements:
#     - models and below
#
#   vpr_tile_lib_dm.py:   handles the following elements:
#     - tiles and below
#

# pylint: disable=missing-function-docstring
# extension-pkg-allow-list=lxml

import sys
import os
from lxml import etree

# Rapid Silicon XML datamodel
from arch_lib.xml_dm import RSElement

# architecture XML support
from arch_lib.vpr_mod_lib_dm import VprModels
from arch_lib.vpr_tile_lib_dm import VprTiles
from arch_lib.vpr_dev_lib_dm import VprDevice
from arch_lib.vpr_switch_lib_dm import VprSwitchlist
from arch_lib.vpr_complex_lib_dm import VprComplexblocklist
from arch_lib.vpr_seg_lib_dm import VprSegmentlist
from arch_lib.vpr_direct_lib_dm import VprDirectlist
from arch_lib.vpr_layout_lib_dm import VprLayout

class VprDataModel(RSElement):
    """ architecture element """

    def __init__(self, filename=None, validate=False):

        if filename is None:
            super().__init__(etree.Element("architecture"))

        else:
            parser = etree.XMLParser(remove_comments=True, remove_blank_text=True)
            tree = etree.parse(filename, parser)

            if validate:
                xsd_file = "%s/arch_etc/vpr.xsd" % os.path.dirname(os.path.realpath(sys.argv[0]))
                xmlschema_doc = etree.parse(xsd_file)
                xmlschema = etree.XMLSchema(xmlschema_doc)
                if not xmlschema.validate(tree):
                    print(f"Found the following errors when loading {filename}:")
                    print(xmlschema.error_log)
                sys.exit(1)

            elem = tree.getroot()
            if elem.tag != "architecture":
                raise Exception(f"{filename} was not an VPR architecture file")

            super().__init__(elem)

    def dump(self, filename):
        """ dump XML file """

        etree.ElementTree(self.elem).write(filename, pretty_print=True)

    @property
    def modelss(self):
        return [VprModels(elem) for elem in self.elem.iterchildren("models")]

    @property
    def tiless(self):
        return [VprTiles(elem) for elem in self.elem.iterchildren("tiles")]

    @property
    def devices(self):
        return [VprDevice(elem) for elem in self.elem.iterchildren("device")]

    @property
    def switchlists(self):
        return [VprSwitchlist(elem) for elem in self.elem.iterchildren("switchlist")]

    @property
    def segmentlists(self):
        return [VprSegmentlist(elem) for elem in self.elem.iterchildren("segmentlist")]

    @property
    def directlists(self):
        return [VprDirectlist(elem) for elem in self.elem.iterchildren("directlist")]

    @property
    def complexblocklists(self):
        return [VprComplexblocklist(elem) for elem in self.elem.iterchildren("complexblocklist")]

    @property
    def layouts(self):
        return [VprLayout(elem) for elem in self.elem.iterchildren("layout")]
