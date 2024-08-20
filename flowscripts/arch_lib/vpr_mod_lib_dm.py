#!/usr/bin/env python3
""" Rapid Silicon's Architectural XML data model """

# see documentation in vpr_dm.py

# pylint: disable=missing-function-docstring
# extension-pkg-allow-list=lxml

from lxml import etree

# Rapid Silicon XML datamodel
from arch_lib.xml_dm import RSElement

class VprPort(RSElement):
    """ port """

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

    @property
    def is_clock(self):
        return self.elem.attrib.get("is_clock")

    @is_clock.setter
    def is_clock(self, value):
        self.elem.attrib["is_clock"] = value

    @property
    def clock(self):
        return self.elem.attrib.get("clock")

    @clock.setter
    def clock(self, value):
        self.elem.attrib["clock"] = value

    @property
    def combinational_sink_ports(self):
        return self.elem.attrib.get("combinational_sink_ports")

    @combinational_sink_ports.setter
    def combinational_sink_ports(self, value):
        self.elem.attrib["combinational_sink_ports"] = value

class VprInputPorts(RSElement):
    """ input_ports """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("input_ports"))
        else:
            super().__init__(elem)

    @property
    def ports(self):
        return [VprPort(elem) for elem in self.elem.iterchildren("port")]

class VprOutputPorts(RSElement):
    """ output_ports"""

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("output_ports"))
        else:
            super().__init__(elem)

    @property
    def ports(self):
        return [VprPort(elem) for elem in self.elem.iterchildren("port")]

class VprModel(RSElement):
    """ model"""

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("model"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def input_portss(self):
        return [VprInputPorts(elem) for elem in self.elem.iterchildren("input_ports")]

    @property
    def output_portss(self):
        return [VprOutputPorts(elem) for elem in self.elem.iterchildren("output_ports")]

class VprModels(RSElement):
    """ models """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("models"))
        else:
            super().__init__(elem)

    @property
    def models(self):
        return [VprModel(elem) for elem in self.elem.iterchildren("model")]
