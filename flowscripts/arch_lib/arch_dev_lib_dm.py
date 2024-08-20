#!/usr/bin/env python3

""" Rapid Silicon's XML data model """

# see documentation in arch_dm.py

# pylint: disable=missing-function-docstring

from lxml import etree
from arch_lib.xml_dm import RSElement

class ArchPmosNmos(RSElement):
    """ pmos element """

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def chan_length(self):
        return self.elem.attrib.get("chan_length")

    @chan_length.setter
    def chan_length(self, value):
        self.elem.attrib["chan_length"] = value

    @property
    def min_width(self):
        return self.elem.attrib.get("min_width")

    @min_width.setter
    def min_width(self, value):
        self.elem.attrib["min_width"] = value

    @property
    def variation(self):
        return self.elem.attrib.get("variation")

    @variation.setter
    def variation(self, value):
        self.elem.attrib["variation"] = value

# Pmos and Nmos elements are identical
class ArchPmos(ArchPmosNmos):
    """ pmos device model """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("pmos"))
        else:
            super().__init__(elem)

class ArchNmos(ArchPmosNmos):
    """ nmos device model """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("nmos"))
        else:
            super().__init__(elem)


class ArchDesign(RSElement):
    """ design element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("design"))
        else:
            super().__init__(elem)

    @property
    def vdd(self):
        return self.elem.attrib.get("vdd")

    @vdd.setter
    def vdd(self, value):
        self.elem.attrib["vdd"] = value

    @property
    def pn_ratio(self):
        return self.elem.attrib.get("pn_ratio")

    @pn_ratio.setter
    def pn_ratio(self, value):
        self.elem.attrib["pn_ratio"] = value

class ArchLib(RSElement):
    """ lib element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("lib"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    @property
    def corner(self):
        return self.elem.attrib.get("corner")

    @corner.setter
    def corner(self, value):
        self.elem.attrib["corner"] = value

    @property
    def ref(self):
        return self.elem.attrib.get("ref")

    @ref.setter
    def ref(self, value):
        self.elem.attrib["ref"] = value

    @property
    def path(self):
        return self.elem.attrib.get("path")

    @path.setter
    def path(self, value):
        self.elem.attrib["path"] = value

class ArchDeviceModel(RSElement):
    """ device_model element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("device_model"))
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

    @property
    def libs(self):
        return [ArchLib(elem) for elem in self.elem.iterchildren("lib")]

    @property
    def pmoss(self):
        return [ArchPmos(elem) for elem in self.elem.iterchildren("pmos")]

    @property
    def nmoss(self):
        return [ArchNmos(elem) for elem in self.elem.iterchildren("nmos")]

    @property
    def designs(self):
        return [ArchDesign(elem) for elem in self.elem.iterchildren("design")]

class ArchVariation(RSElement):
    """ variation """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("variation"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def abs_deviation(self):
        return self.elem.attrib.get("abs_deviation")

    @abs_deviation.setter
    def abs_deviation(self, value):
        self.elem.attrib["abs_deviation"] = value

    @property
    def num_sigma(self):
        return self.elem.attrib.get("num_sigma")

    @num_sigma.setter
    def num_sigma(self, value):
        self.elem.attrib["num_sigma"] = value

class ArchVariationLibrary(RSElement):
    """ variation_ibrary """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("variation_library"))
        else:
            super().__init__(elem)

    @property
    def variations(self):
        return [ArchVariation(elem) for elem in self.elem.iterchildren("variation")]

class ArchDeviceLibrary(RSElement):
    """ device_library element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("device_library"))
        else:
            super().__init__(elem)

    @property
    def device_models(self):
        return [ArchDeviceModel(elem) for elem in self.elem.iterchildren("device_model")]
