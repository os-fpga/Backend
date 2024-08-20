#!/usr/bin/env python3
""" Rapid Silicon's Architectural XML data model """

# see documentation in vpr_dm.py

# pylint: disable=missing-function-docstring
# extension-pkg-allow-list=lxml

from lxml import etree

# Rapid Silicon XML datamodel
from arch_lib.xml_dm import RSElement
from arch_lib.vpr_direct_lib_dm import VprPackPattern

class VprDelayConstant(RSElement):
    """ delay_constant """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("delay_constant"))
        else:
            super().__init__(elem)

    @property
    def max(self):
        return self.elem.attrib.get("max")

    @max.setter
    def max(self, value):
        self.elem.attrib["max"] = value

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

class VprCb(RSElement):
    """ cb """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("cb"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value
class VprSb(RSElement):
    """ sb """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("sb"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

class VprMux(RSElement):
    """ mux """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("mux"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

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

    @property
    def delay_constants(self):
        return [VprDelayConstant(elem) for elem in self.elem.iterchildren("delay_constant")]

class VprSegment(RSElement):
    """ segment """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("segment"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def freq(self):
        return self.elem.attrib.get("freq")

    @freq.setter
    def freq(self, value):
        self.elem.attrib["freq"] = value

    @property
    def length(self):
        return self.elem.attrib.get("length")

    @length.setter
    def length(self, value):
        self.elem.attrib["length"] = value

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    # pylint: disable=invalid-name
    @property
    def Rmetal(self):
        return self.elem.attrib.get("Rmetal")

    @Rmetal.setter
    def Rmetal(self, value):
        self.elem.attrib["Rmetal"] = value

    @property
    def Cmetal(self):
        return self.elem.attrib.get("Cmetal")

    @Cmetal.setter
    def Cmetal(self, value):
        self.elem.attrib["Cmetal"] = value
    # pylint: enable=invalid-name

    @property
    def muxs(self):
        return [VprMux(elem) for elem in self.elem.iterchildren("mux")]

    @property
    def sbs(self):
        return [VprSb(elem) for elem in self.elem.iterchildren("sb")]

    @property
    def cbs(self):
        return [VprCb(elem) for elem in self.elem.iterchildren("Cb")]

class VprSegmentlist(RSElement):
    """ segmentlist """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("segmentlist"))
        else:
            super().__init__(elem)

    @property
    def segments(self):
        return [VprSegment(elem) for elem in self.elem.iterchildren("segment")]
