#!/usr/bin/env python3
""" Rapid Silicon's Architectural XML data model """

# see documentation in vpr_dm.py

# pylint: disable=missing-function-docstring
# extension-pkg-allow-list=lxml

from lxml import etree

# Rapid Silicon XML datamodel
from arch_lib.xml_dm import RSElement


class VprClockInputOutput(RSElement):
    """ base class for clock, input, output """

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def num_pins(self):
        return self.elem.attrib.get("num_pins")

    @num_pins.setter
    def num_pins(self, value):
        self.elem.attrib["num_pins"] = value

    @property
    def port_class(self):
        return self.elem.attrib.get("port_class")

    @port_class.setter
    def port_class(self, value):
        self.elem.attrib["port_class"] = value

class VprClock(VprClockInputOutput):
    """ clock """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("clock"))
        else:
            super().__init__(elem)

class VprInput(VprClockInputOutput):
    """ site """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("input"))
        else:
            super().__init__(elem)

class VprOutput(VprClockInputOutput):
    """ site """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("output"))
        else:
            super().__init__(elem)

class VprSite(RSElement):
    """ site """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("site"))
        else:
            super().__init__(elem)

    @property
    def pb_type(self):
        return self.elem.attrib.get("pb_type")

    @pb_type.setter
    def pb_type(self, value):
        self.elem.attrib["pb_type"] = value

class VprEquivalentSites(RSElement):
    """ equivalent_sites """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("equivalent_sites"))
        else:
            super().__init__(elem)

    @property
    def sites(self):
        return [VprSite(elem) for elem in self.elem.iterchildren("site")]

class VprFcOverride(RSElement):
    """ fc_override """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("fc_override"))
        else:
            super().__init__(elem)

    @property
    def port_name(self):
        return self.elem.attrib.get("port_name")

    @port_name.setter
    def port_name(self, value):
        self.elem.attrib["port_name"] = value

    @property
    def fc_type(self):
        return self.elem.attrib.get("fc_type")

    @fc_type.setter
    def fc_type(self, value):
        self.elem.attrib["fc_type"] = value

    @property
    def fc_val(self):
        return self.elem.attrib.get("fc_val")

    @fc_val.setter
    def fc_val(self, value):
        self.elem.attrib["fc_val"] = value

class VprFc(RSElement):
    """ fc """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("fc"))
        else:
            super().__init__(elem)

    @property
    def in_type(self):
        return self.elem.attrib.get("in_type")

    @in_type.setter
    def in_type(self, value):
        self.elem.attrib["in_type"] = value

    @property
    def in_val(self):
        return self.elem.attrib.get("in_val")

    @in_val.setter
    def in_val(self, value):
        self.elem.attrib["in_val"] = value

    @property
    def out_type(self):
        return self.elem.attrib.get("out_type")

    @out_type.setter
    def out_type(self, value):
        self.elem.attrib["out_type"] = value

    @property
    def out_val(self):
        return self.elem.attrib.get("out_val")

    @out_val.setter
    def out_val(self, value):
        self.elem.attrib["out_val"] = value

    @property
    def fc_overrides(self):
        return [VprFcOverride(elem) for elem in self.elem.iterchildren("fc_override")]

class VprLoc(RSElement):
    """ loc """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("loc"))
        else:
            super().__init__(elem)

    @property
    def side(self):
        return self.elem.attrib.get("side")

    @side.setter
    def side(self, value):
        self.elem.attrib["side"] = value

    @property
    def yoffset(self):
        return self.elem.attrib.get("yoffset")

    @yoffset.setter
    def yoffset(self, value):
        self.elem.attrib["yoffset"] = value

    @property
    def xoffset(self):
        return self.elem.attrib.get("xoffset")

    @xoffset.setter
    def yxoffset(self, value):
        self.elem.attrib["xoffset"] = value

class VprPinlocations(RSElement):
    """ pinlocations """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("pinlocations"))
        else:
            super().__init__(elem)

    @property
    def pattern(self):
        return self.elem.attrib.get("pattern")

    @pattern.setter
    def pattern(self, value):
        self.elem.attrib["pattern"] = value

    @property
    def locs(self):
        return [VprLoc(elem) for elem in self.elem.iterchildren("loc")]

class VprSubTile(RSElement):
    """ sub_tile """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("sub_tile"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def capacity(self):
        return self.elem.attrib.get("capacity")

    @capacity.setter
    def capacity(self, value):
        self.elem.attrib["capacity"] = value

    @property
    def equivalent_sitess(self):
        return [VprEquivalentSites(elem) for elem in self.elem.iterchildren("equivalent_sites")]

    @property
    def clocks(self):
        return [VprClock(elem) for elem in self.elem.iterchildren("clock")]

    @property
    def inputs(self):
        return [VprInput(elem) for elem in self.elem.iterchildren("input")]

    @property
    def outputs(self):
        return [VprOutput(elem) for elem in self.elem.iterchildren("output")]

    @property
    def fcs(self):
        return [VprFc(elem) for elem in self.elem.iterchildren("fc")]

    @property
    def pinlocationss(self):
        return [VprPinlocations(elem) for elem in self.elem.iterchildren("pinlocations")]

class VprTile(RSElement):
    """ tile"""

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("tile"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def area(self):
        return self.elem.attrib.get("area")

    @area.setter
    def area(self, value):
        self.elem.attrib["area"] = value

    @property
    def height(self):
        return self.elem.attrib.get("height")

    @height.setter
    def height(self, value):
        self.elem.attrib["height"] = value

    @property
    def width(self):
        return self.elem.attrib.get("width")

    @width.setter
    def width(self, value):
        self.elem.attrib["width"] = value

    @property
    def sub_tiles(self):
        return [VprSubTile(elem) for elem in self.elem.iterchildren("sub_tile")]

class VprTiles(RSElement):
    """ tiles """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("tiles"))
        else:
            super().__init__(elem)

    @property
    def tiles(self):
        return [VprTile(elem) for elem in self.elem.iterchildren("tile")]
