#!/usr/bin/env python3
""" Rapid Silicon's Packer XML data model """

# PackBlock handles the following elements:
#   attributes
#   parameters
#       parameter name="XX" TEXT
#   inputs
#       port name="XX" TEXT
#       port_rotation_map name="XX" TEXT
#   outputs
#       port name="XX" TEXT
#   clocks
#       port name="XX" TEXT
#   block name="XX" instance="YY" mode="ZZ" [architecture_id="XX"] [atom_netlist_id="XX"]

# pylint: disable=missing-function-docstring
# extension-pkg-allow-list=lxml

import os
import sys
from lxml import etree

# Rapid Silicon XML datamodel
from arch_lib.xml_dm import RSElement

class PackAttributes(RSElement):
    """attributes"""

    def __init__(self, elem=None):
        if elem is None:
            super().__init__(etree.Element("attributes"))
        else:
            super().__init__(elem)

    @property
    def attributes(self):
        return []   # always empty (so far)

class PackParameter(RSElement):
    """parameter"""

    def __init__(self, elem=None):
        if elem is None:
            super().__init__(etree.Element("parameter"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

class PackParameters(RSElement):
    """parameters"""

    def __init__(self, elem=None):
        if elem is None:
            super().__init__(etree.Element("parameters"))
        else:
            super().__init__(elem)

    @property
    def parameters(self):
        return [PackParameter(elem) for elem in self.elem.iterchildren("parameter")]

class PackPort(RSElement):
    """port"""

    def __init__(self, elem=None):
        if elem is None:
            super().__init__(etree.Element("port"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

class PackPortRotationMap(RSElement):
    """port_rotation_map"""

    def __init__(self, elem=None):
        if elem is None:
            super().__init__(etree.Element("port_rotation_map"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

class PackInputs(RSElement):
    """inputs"""

    def __init__(self, elem=None):
        if elem is None:
            super().__init__(etree.Element("inputs"))
        else:
            super().__init__(elem)

    @property
    def ports(self):
        return [PackPort(elem) for elem in self.elem.iterchildren("port")]

    @property
    def port_rotation_maps(self):
        return [PackPortRotationMap(elem) for elem in self.elem.iterchildren("port_rotation_map")]

class PackOutputs(RSElement):
    """outputs"""

    def __init__(self, elem=None):
        if elem is None:
            super().__init__(etree.Element("outputs"))
        else:
            super().__init__(elem)

    @property
    def ports(self):
        return [PackPort(elem) for elem in self.elem.iterchildren("port")]

class PackClocks(RSElement):
    """clocks"""

    def __init__(self, elem=None):
        if elem is None:
            super().__init__(etree.Element("clocks"))
        else:
            super().__init__(elem)

    @property
    def ports(self):
        return [PackPort(elem) for elem in self.elem.iterchildren("port")]

class PackBlock(RSElement):
    """block"""

    def __init__(self, elem=None):
        if elem is None:
            super().__init__(etree.Element("block"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def instance(self):
        return self.elem.attrib.get("instance")

    @instance.setter
    def instance(self, value):
        self.elem.attrib["instance"] = value

    @property
    def mode(self):
        return self.elem.attrib.get("mode")

    @mode.setter
    def mode(self, value):
        self.elem.attrib["mode"] = value

    @property
    def attributess(self):
        return [PackAttributes(elem) for elem in self.elem.iterchildren("attributes")]

    @property
    def parameterss(self):
        return [PackParameters(elem) for elem in self.elem.iterchildren("parameters")]

    @property
    def inputss(self):
        return [PackInputs(elem) for elem in self.elem.iterchildren("inputs")]

    @property
    def outputss(self):
        return [PackOutputs(elem) for elem in self.elem.iterchildren("outputs")]

    @property
    def clockss(self):
        return [PackClocks(elem) for elem in self.elem.iterchildren("clocks")]

    @property
    def blocks(self):
        return [PackBlock(elem) for elem in self.elem.iterchildren("block")]

class PackDataModel(RSElement):
    """full packer file"""

    def __init__(self, filename=None, validate=False):

        if filename is None:
            super().__init__(etree.Element("block"))

        else:
            parser = etree.XMLParser(remove_blank_text=True)
            tree = etree.parse(filename, parser)

            if validate:
                # this is untested (copied from elsewhere)
                xsd_file = "%s/arch_etc/openfpga.xsd" % os.path.dirname(os.path.realpath(sys.argv[0]))
                xmlschema_doc = etree.parse(xsd_file)
                xmlschema = etree.XMLSchema(xmlschema_doc)
                if not xmlschema.validate(tree):
                    print(f"Found the following errors when loading {filename}:")
                    print(xmlschema.error_log)
                sys.exit(1)

            elem = tree.getroot()
            if elem.tag != "block":
                raise Exception(f"{filename} was not a packer file")

            super().__init__(elem)


    def dump(self, filename):
        """ dump XML file """
        etree.ElementTree(self.elem).write(filename, pretty_print=True)

    @property
    def inputss(self):
        return [PackInputs(elem) for elem in self.elem.iterchildren("inputs")]

    @property
    def outputss(self):
        return [PackOutputs(elem) for elem in self.elem.iterchildren("outputs")]

    @property
    def clockss(self):
        return [PackClocks(elem) for elem in self.elem.iterchildren("clocks")]

    @property
    def blocks(self):
        return [PackBlock(elem) for elem in self.elem.iterchildren("block")]


# vim: filetype=python
# vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab smarttab
# vim: autoindent hlsearch syntax=on fileformat=unix
