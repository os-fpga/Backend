#!/usr/bin/env python3
""" Rapid Silicon's Architectural XML data model """

# see documentation in vpr_dm.py

# pylint: disable=missing-function-docstring
# extension-pkg-allow-list=lxml

from lxml import etree

# Rapid Silicon XML datamodel
from arch_lib.xml_dm import RSElement

class VprSwitch(RSElement):
    """ switch """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("switch"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    # pylint: disable=invalid-name
    @property
    def R(self):
        return self.elem.attrib.get("R")

    @R.setter
    def R(self, value):
        self.elem.attrib["R"] = value

    @property
    def Cout(self):
        return self.elem.attrib.get("Cout")

    @Cout.setter
    def Cout(self, value):
        self.elem.attrib["Cout"] = value

    @property
    def Cin(self):
        return self.elem.attrib.get("Cin")

    @Cin.setter
    def Cin(self, value):
        self.elem.attrib["Cin"] = value

    @property
    def Tdel(self):
        return self.elem.attrib.get("Tdel")

    @Tdel.setter
    def Tdel(self, value):
        self.elem.attrib["Tdel"] = value
    # pylint: enable=invalid-name

    @property
    def mux_trans_size(self):
        return self.elem.attrib.get("mux_trans_size")

    @mux_trans_size.setter
    def mux_trans_size(self, value):
        self.elem.attrib["mux_trans_size"] = value

    @property
    def buf_size(self):
        return self.elem.attrib.get("buf_size")

    @buf_size.setter
    def buf_size(self, value):
        self.elem.attrib["buf_size"] = value

class VprSwitchlist(RSElement):
    """ switchlist """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("switchlist"))
        else:
            super().__init__(elem)

    @property
    def switchs(self):
        return [VprSwitch(elem) for elem in self.elem.iterchildren("switch")]
