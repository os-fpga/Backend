#!/usr/bin/env python3
""" Rapid Silicon's Architectural XML data model """

# see documentation in vpr_dm.py

# pylint: disable=missing-function-docstring
# extension-pkg-allow-list=lxml

from lxml import etree

# Rapid Silicon XML datamodel
from arch_lib.xml_dm import RSElement

class VprXY(RSElement):
    """ base class for x and y """

    @property
    def distr(self):
        return self.elem.attrib.get("distr")

    @distr.setter
    def distr(self, value):
        self.elem.attrib["distr"] = value

    @property
    def peak(self):
        return self.elem.attrib.get("peak")

    @peak.setter
    def peak(self, value):
        self.elem.attrib["peak"] = value

class VprX(VprXY):
    """ x """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("x"))
        else:
            super().__init__(elem)

class VprY(VprXY):
    """ y """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("y"))
        else:
            super().__init__(elem)

class VprChanWidthDistr(RSElement):
    """ chan_width_distr """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("chan_width_distr"))
        else:
            super().__init__(elem)

    # pylint: disable=invalid-name
    @property
    def xs(self):
        return [VprX(elem) for elem in self.elem.iterchildren("x")]

    @property
    def ys(self):
        return [VprY(elem) for elem in self.elem.iterchildren("y")]
    # pylint: enable=invalid-name

class VprArea(RSElement):
    """ area """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("area"))
        else:
            super().__init__(elem)

    @property
    def grid_logic_tile_area(self):
        return self.elem.attrib.get("grid_logic_tile_area")

    @grid_logic_tile_area.setter
    def grid_logic_tile_area(self, value):
        self.elem.attrib["grid_logic_tile_area"] = value

class VprSwitchBlock(RSElement):
    """ switch_block """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("switch_block"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    # pylint: disable=invalid-name
    @property
    def fs(self):
        return self.elem.attrib.get("fs")

    @fs.setter
    def fs(self, value):
        self.elem.attrib["fs"] = value
    # pylint: enable=invalid-name

    @property
    def sub_type(self):
        return self.elem.attrib.get("sub_type")

    @sub_type.setter
    def sub_type(self, value):
        self.elem.attrib["sub_type"] = value

    @property
    def sub_fs(self):
        return self.elem.attrib.get("sub_fs")

    @sub_fs.setter
    def sub_fs(self, value):
        self.elem.attrib["sub_fs"] = value

class VprConnectionBlock(RSElement):
    """ connection_block """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("connection_block"))
        else:
            super().__init__(elem)

    @property
    def input_switch_name(self):
        return self.elem.attrib.get("input_switch_name")

    @input_switch_name.setter
    def input_switch_name(self, value):
        self.elem.attrib["input_switch_name"] = value

class VprSizing(RSElement):
    """ sizing """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("sizing"))
        else:
            super().__init__(elem)

    # pylint: disable=invalid-name
    @property
    def R_minW_nmos(self):
        return self.elem.attrib.get("R_minW_nmos")

    @R_minW_nmos.setter
    def R_minW_nmos(self, value):
        self.elem.attrib["R_minW_nmos"] = value

    @property
    def R_minW_pmos(self):
        return self.elem.attrib.get("R_minW_pmos")

    @R_minW_pmos.setter
    def R_minW_pmos(self, value):
        self.elem.attrib["R_minW_pmos"] = value
    # pylint: enable=invalid-name

class VprDevice(RSElement):
    """ device """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("device"))
        else:
            super().__init__(elem)

    @property
    def sizings(self):
        return [VprSizing(elem) for elem in self.elem.iterchildren("sizing")]

    @property
    def areas(self):
        return [VprArea(elem) for elem in self.elem.iterchildren("area")]

    @property
    def chan_width_distrs(self):
        return [VprChanWidthDistr(elem) for elem in self.elem.iterchildren("chan_width_distr")]

    @property
    def switch_blocks(self):
        return [VprSwitchBlock(elem) for elem in self.elem.iterchildren("switch_block")]

    @property
    def connection_blocks(self):
        return [VprConnectionBlock(elem) for elem in self.elem.iterchildren("connection_block")]
