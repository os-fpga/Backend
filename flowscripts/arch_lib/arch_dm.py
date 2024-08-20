#!/usr/bin/env python3
""" Rapid Silicon's Architectural XML data model """

#
# Files in this set include
#   arch_dm.py:   handles the following elements:
#     - openfpga_architecture
#     - technology_library
#     - configuration_protocol
#     - connection_block
#     - switch_block
#     - routing_segment
#     - direct_connection
#     - tile_annotations
#
#   arch_dev_lib_dm.py:   handles the following elements:
#     - device_library and below
#
#   arch_cir_lib_dm.py:   handles the following elements:
#     - circuit_library and below
#
#   arch_pb_lib_dm.py:   handles the following elements:
#     - pb_type_annotations and below



# pylint: disable=missing-function-docstring
# extension-pkg-allow-list=lxml

import os
import sys
from lxml import etree

# Rapid Silicon XML datamodel
from arch_lib.xml_dm import RSElement

# architecture XML support
from arch_lib.arch_dev_lib_dm import ArchDeviceLibrary
from arch_lib.arch_dev_lib_dm import ArchVariationLibrary
from arch_lib.arch_cir_lib_dm import ArchCircuitLibrary
from arch_lib.arch_pb_lib_dm import ArchPbTypeAnnotations

class ArchTechnologyLibrary(RSElement):
    """ technology_library element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("technology_library"))
        else:
            super().__init__(elem)

    @property
    def device_librarys(self):
        return [ArchDeviceLibrary(elem) for elem in self.elem.iterchildren("device_library")]

    @property
    def variation_librarys(self):
        return [ArchVariationLibrary(elem) for elem in self.elem.iterchildren("variation_library")]

class ArchBl(RSElement):
    """ bl element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("bl"))
        else:
            super().__init__(elem)
    @property
    def protocol(self):
        return self.elem.attrib.get("protocol")

    @protocol.setter
    def protocol(self, value):
        self.elem.attrib["protocol"] = value

class ArchWl(RSElement):
    """ wl element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("wl"))
        else:
            super().__init__(elem)
    @property
    def protocol(self):
        return self.elem.attrib.get("protocol")

    @protocol.setter
    def protocol(self, value):
        self.elem.attrib["protocol"] = value

class ArchOrganization(RSElement):
    """ organization element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("organization"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    @property
    def circuit_model_name(self):
        return self.elem.attrib.get("circuit_model_name")

    @circuit_model_name.setter
    def circuit_model_name(self, value):
        self.elem.attrib["circuit_model_name"] = value

    @property
    def bls(self):
        return [ArchBl(elem) for elem in self.elem.iterchildren("bl")]

    @property
    def wls(self):
        return [ArchWl(elem) for elem in self.elem.iterchildren("wl")]

class ArchConfigurationProtocol(RSElement):
    """ configuration_protocol element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("configuration_protocol"))
        else:
            super().__init__(elem)

    @property
    def organizations(self):
        return [ArchOrganization(elem) for elem in self.elem.iterchildren("organization")]

class ArchSwitch(RSElement):
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
    def circuit_model_name(self):
        return self.elem.attrib.get("circuit_model_name")

    @circuit_model_name.setter
    def circuit_model_name(self, value):
        self.elem.attrib["circuit_model_name"] = value

class ArchConnectionBlock(RSElement):
    """ connection_block """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("connection_block"))
        else:
            super().__init__(elem)

    @property
    def switchs(self):
        return [ArchSwitch(elem) for elem in self.elem.iterchildren("switch")]

class ArchSwitchBlock(RSElement):
    """ switch_block """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("switch_block"))
        else:
            super().__init__(elem)

    @property
    def switchs(self):
        return [ArchSwitch(elem) for elem in self.elem.iterchildren("switch")]

class ArchSegment(RSElement):
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
    def circuit_model_name(self):
        return self.elem.attrib.get("circuit_model_name")

    @circuit_model_name.setter
    def circuit_model_name(self, value):
        self.elem.attrib["circuit_model_name"] = value

class ArchRoutingSegment(RSElement):
    """ routing_segment """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("routing_segment"))
        else:
            super().__init__(elem)

    @property
    def segments(self):
        return [ArchSegment(elem) for elem in self.elem.iterchildren("segment")]

class ArchDirect(RSElement):
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
    def circuit_model_name(self):
        return self.elem.attrib.get("circuit_model_name")

    @circuit_model_name.setter
    def circuit_model_name(self, value):
        self.elem.attrib["circuit_model_name"] = value

class ArchDirectConnection(RSElement):
    """ direct_connection """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("direct_connection"))
        else:
            super().__init__(elem)

    @property
    def directs(self):
        return [ArchDirect(elem) for elem in self.elem.iterchildren("direct")]

class ArchTile(RSElement):
    """ tile """

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
    def port(self):
        return self.elem.attrib.get("port")

    @port.setter
    def port(self, value):
        self.elem.attrib["port"] = value

    # pylint: disable=invalid-name
    @property
    def x(self):
        return self.elem.attrib.get("x")

    @x.setter
    def x(self, value):
        self.elem.attrib["x"] = value

    @property
    def y(self):
        return self.elem.attrib.get("y")

    @y.setter
    def y(self, value):
        self.elem.attrib["y"] = value

    # pylint: enable=invalid-name

class ArchGlobalPort(RSElement):
    """ global_port """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("global_port"))
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
    def circuit_model_name(self, value):
        self.elem.attrib["is_clock"] = value

    @property
    def default_val(self):
        return self.elem.attrib.get("default_val")

    @default_val.setter
    def default_val(self, value):
        self.elem.attrib["default_val"] = value

    @property
    def tiles(self):
        return [ArchTile(elem) for elem in self.elem.iterchildren("tile")]

class ArchTileAnnotations(RSElement):
    """ tile_annotations """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("tile_annotations"))
        else:
            super().__init__(elem)

    @property
    def global_ports(self):
        return [ArchGlobalPort(elem) for elem in self.elem.iterchildren("global_port")]

class ArchDataModel(RSElement):
    """ openfpga_architecture element """

    def __init__(self, filename=None, validate=False):

        if filename is None:
            super().__init__(etree.Element("openfpga_architecture"))

        else:
            parser = etree.XMLParser(remove_blank_text=True)
            tree = etree.parse(filename, parser)

            if validate:
                xsd_file = "%s/arch_etc/openfpga.xsd" % os.path.dirname(os.path.realpath(sys.argv[0]))
                xmlschema_doc = etree.parse(xsd_file)
                xmlschema = etree.XMLSchema(xmlschema_doc)
                if not xmlschema.validate(tree):
                    print(f"Found the following errors when loading {filename}:")
                    print(xmlschema.error_log)
                sys.exit(1)

            elem = tree.getroot()
            if elem.tag != "openfpga_architecture":
                raise Exception(f"{filename} was not an OpenFPGA architecture file")

            super().__init__(elem)


    def dump(self, filename):
        """ dump XML file """

        etree.ElementTree(self.elem).write(filename, pretty_print=True)

    @property
    def technology_librarys(self):
        return [ArchTechnologyLibrary(elem) for elem in self.elem.iterchildren("technology_library")]

    @property
    def circuit_librarys(self):
        return [ArchCircuitLibrary(elem) for elem in self.elem.iterchildren("circuit_library")]

    @property
    def configuration_protocols(self):
        return [ArchConfigurationProtocol(elem) for elem in self.elem.iterchildren("configuration_protocol")]

    @property
    def connection_blocks(self):
        return [ArchConnectionBlock(elem) for elem in self.elem.iterchildren("connection_block")]

    @property
    def switch_blocks(self):
        return [ArchSwitchBlock(elem) for elem in self.elem.iterchildren("switch_block")]

    @property
    def routing_segments(self):
        return [ArchRoutingSegment(elem) for elem in self.elem.iterchildren("routing_segment")]

    @property
    def direct_connections(self):
        return [ArchDirectConnection(elem) for elem in self.elem.iterchildren("direct_connection")]

    @property
    def tile_annotationss(self):
        return [ArchTileAnnotations(elem) for elem in self.elem.iterchildren("tile_annotations")]

    @property
    def pb_type_annotationss(self):
        return [ArchPbTypeAnnotations(elem) for elem in self.elem.iterchildren("pb_type_annotations")]
