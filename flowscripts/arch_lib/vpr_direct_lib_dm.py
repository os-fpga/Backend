#!/usr/bin/env python3
""" Rapid Silicon's Architectural XML data model """

# see documentation in vpr_dm.py

# pylint: disable=missing-function-docstring
# extension-pkg-allow-list=lxml

from lxml import etree

# Rapid Silicon XML datamodel
from arch_lib.xml_dm import RSElement

class VprPackPattern(RSElement):
    """ cb """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("pack_pattern"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def in_port(self):
        return self.elem.attrib.get("in_port")

    @in_port.setter
    def in_port(self, value):
        self.elem.attrib["in_port"] = value

    @property
    def out_port(self):
        return self.elem.attrib.get("out_port")

    @out_port.setter
    def out_port(self, value):
        self.elem.attrib["out_port"] = value

class VprDirect(RSElement):
    """ direct """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("direct"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def from_pin(self):
        return self.elem.attrib.get("from_pin")

    @from_pin.setter
    def from_pin(self, value):
        self.elem.attrib["from_pin"] = value

    @property
    def to_pin(self):
        return self.elem.attrib.get("to_pin")

    @to_pin.setter
    def to_pin(self, value):
        self.elem.attrib["to_pin"] = value

    @property
    def x_offset(self):
        return self.elem.attrib.get("x_offset")

    @x_offset.setter
    def x_offset(self, value):
        self.elem.attrib["x_offset"] = value

    @property
    def y_offset(self):
        return self.elem.attrib.get("y_offset")

    @y_offset.setter
    def y_offset(self, value):
        self.elem.attrib["y_offset"] = value

    @property
    def z_offset(self):
        return self.elem.attrib.get("z_offset")

    @z_offset.setter
    def z_offset(self, value):
        self.elem.attrib["z_offset"] = value

    @property
    def input(self):
        return self.elem.attrib.get("input")

    @input.setter
    def input(self, value):
        self.elem.attrib["input"] = value

    @property
    def output(self):
        return self.elem.attrib.get("output")

    @output.setter
    def output(self, value):
        self.elem.attrib["output"] = value

    @property
    def pack_patterns(self):
        return [VprPackPattern(elem) for elem in self.elem.iterchildren("pack_pattern")]

class VprDirectlist(RSElement):
    """ directlist """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("directlist"))
        else:
            super().__init__(elem)

    @property
    def directs(self):
        return [VprDirect(elem) for elem in self.elem.iterchildren("direct")]
