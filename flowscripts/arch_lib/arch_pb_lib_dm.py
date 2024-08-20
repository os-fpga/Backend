#!/usr/bin/env python3

""" Rapid Silicon's XML data model """
# pylint: disable=missing-function-docstring

# see documentation in arch_dm.py

from lxml import etree
from arch_lib.xml_dm import RSElement
from arch_lib.arch_cir_lib_dm import ArchPort

class ArchInterconnect(RSElement):
    """ interconnect """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("interconnecct"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def circuit_model_name(self):
        return self.elem.attrib.get("circuit_model_name")

    @circuit_model_name.setter
    def circuit_model_name(self, value):
        self.elem.attrib["circuit_model_name"] = value


class ArchPbType(RSElement):
    """ pb_type """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("pb_type"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def physical_mode_name(self):
        return self.elem.attrib.get("physical_mode_name")

    @physical_mode_name.setter
    def physical_mode_name(self, value):
        self.elem.attrib["physical_mode_name"] = value

    @property
    def circuit_model_name(self):
        return self.elem.attrib.get("circuit_model_name")

    @circuit_model_name.setter
    def circuit_model_name(self, value):
        self.elem.attrib["circuit_model_name"] = value


    @property
    def physical_pb_type_name(self):
        return self.elem.attrib.get("physical_pb_type_name")

    @physical_pb_type_name.setter
    def physical_pb_type_name(self, value):
        self.elem.attrib["physical_pb_type_name"] = value

    @property
    def physical_pb_type_index_offset(self):
        return self.elem.attrib.get("physical_pb_type_index_offset")

    @physical_pb_type_index_offset.setter
    def physical_pb_type_index_offset(self, value):
        self.elem.attrib["physical_pb_type_index_offset"] = value


    @property
    def mode_bits(self):
        return self.elem.attrib.get("mode_bits")

    @mode_bits.setter
    def mode_bits(self, value):
        self.elem.attrib["mode_bits"] = value

    @property
    def interconnects(self):
        return [ArchInterconnect(elem) for elem in self.elem.iterchildren("interconnect")]

    @property
    def ports(self):
        return [ArchPort(elem) for elem in self.elem.iterchildren("port")]

class ArchPbTypeAnnotations(RSElement):
    """ pb_type_annotations """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("pb_type_annotations"))
        else:
            super().__init__(elem)

    @property
    def pb_types(self):
        return [ArchPbType(elem) for elem in self.elem.iterchildren("pb_type")]
