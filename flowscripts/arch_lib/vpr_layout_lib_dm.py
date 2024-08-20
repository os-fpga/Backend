#!/usr/bin/env python3
""" Rapid Silicon's Architectural XML data model """

# see documentation in vpr_dm.py

# pylint: disable=missing-function-docstring
# extension-pkg-allow-list=lxml

from lxml import etree

# Rapid Silicon XML datamodel
from arch_lib.xml_dm import RSElement

class VprRow(RSElement):
    """ row """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("row"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    @property
    def starty(self):
        return self.elem.attrib.get("starty")

    @starty.setter
    def starty(self, value):
        self.elem.attrib["starty"] = value

    @property
    def priority(self):
        return self.elem.attrib.get("priority")

    @priority.setter
    def priority(self, value):
        self.elem.attrib["priority"] = value

class VprCol(RSElement):
    """ col """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("col"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    @property
    def startx(self):
        return self.elem.attrib.get("startx")

    @startx.setter
    def startx(self, value):
        self.elem.attrib["startx"] = value

    @property
    def priority(self):
        return self.elem.attrib.get("priority")

    @priority.setter
    def priority(self, value):
        self.elem.attrib["priority"] = value

class VprCorners(RSElement):
    """ corners """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("corners"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    @property
    def priority(self):
        return self.elem.attrib.get("priority")

    @priority.setter
    def priority(self, value):
        self.elem.attrib["priority"] = value

class VprFill(RSElement):
    """ fill """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("fill"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    @property
    def priority(self):
        return self.elem.attrib.get("priority")

    @priority.setter
    def priority(self, value):
        self.elem.attrib["priority"] = value

class VprRegion(RSElement):
    """ region """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("region"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    @property
    def startx(self):
        return self.elem.attrib.get("startx")

    @startx.setter
    def startx(self, value):
        self.elem.attrib["startx"] = value

    @property
    def endx(self):
        return self.elem.attrib.get("endx")

    @endx.setter
    def endx(self, value):
        self.elem.attrib["endx"] = value

    @property
    def starty(self):
        return self.elem.attrib.get("starty")

    @starty.setter
    def starty(self, value):
        self.elem.attrib["starty"] = value

    @property
    def endy(self):
        return self.elem.attrib.get("endy")

    @endy.setter
    def endy(self, value):
        self.elem.attrib["endy"] = value

    @property
    def priority(self):
        return self.elem.attrib.get("priority")

    @priority.setter
    def priority(self, value):
        self.elem.attrib["priority"] = value

class VprFixedLayout(RSElement):
    """ fixed_layout """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("fixed_layout"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def width(self):
        return self.elem.attrib.get("width")

    @width.setter
    def width(self, value):
        self.elem.attrib["width"] = value

    @property
    def height(self):
        return self.elem.attrib.get("height")

    @height.setter
    def height(self, value):
        self.elem.attrib["height"] = value

    @property
    def rows(self):
        return [VprRow(elem) for elem in self.elem.iterchildren("row")]

    @property
    def cols(self):
        return [VprCol(elem) for elem in self.elem.iterchildren("col")]

    @property
    def cornerss(self):
        return [VprCorners(elem) for elem in self.elem.iterchildren("corners")]

    @property
    def fills(self):
        return [VprFill(elem) for elem in self.elem.iterchildren("fill")]

    @property
    def regions(self):
        return [VprRegion(elem) for elem in self.elem.iterchildren("region")]

class VprLayout(RSElement):
    """ layout """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("layout"))
        else:
            super().__init__(elem)

    @property
    def tileable(self):
        return self.elem.attrib.get("tileable")

    @tileable.setter
    def tileable(self, value):
        self.elem.attrib["tileable"] = value

    @property
    def through_channel(self):
        return self.elem.attrib.get("through_channel")

    @through_channel.setter
    def through_channel(self, value):
        self.elem.attrib["through_channel"] = value

    @property
    def fixed_layouts(self):
        return [VprFixedLayout(elem) for elem in self.elem.iterchildren("fixed_layout")]
