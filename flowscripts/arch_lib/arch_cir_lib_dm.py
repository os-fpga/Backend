#!/usr/bin/env python3

""" Rapid Silicon's XML data model """
# see documentation in arch_dm.py

# pylint: disable=missing-function-docstring

from lxml import etree
from arch_lib.xml_dm import RSElement

class ArchDesignTechnology(RSElement):
    """ design_technology element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("design_technology"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    @property
    def topology(self):
        return self.elem.attrib.get("topology")

    @topology.setter
    def topology(self, value):
        self.elem.attrib["topology"] = value

    @property
    def size(self):
        return self.elem.attrib.get("size")

    @size.setter
    def size(self, value):
        self.elem.attrib["size"] = value

class ArchDeviceTechnology(RSElement):
    """ device_technology element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("device_technology"))
        else:
            super().__init__(elem)

    @property
    def device_model_name(self):
        return self.elem.attrib.get("device_model_name")

    @device_model_name.setter
    def device_model_name(self, value):
        self.elem.attrib["device_model_name"] = value

class ArchPort(RSElement):
    """ port element """

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
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    @property
    def prefix(self):
        return self.elem.attrib.get("prefix")

    @prefix.setter
    def prefix(self, value):
        self.elem.attrib["prefix"] = value

    @property
    def lib_name(self):
        return self.elem.attrib.get("lib_name")

    @lib_name.setter
    def lib__name(self, value):
        self.elem.attrib["lib_name"] = value

    @property
    def size(self):
        return self.elem.attrib.get("size")

    @size.setter
    def size(self, value):
        self.elem.attrib["size"] = value

    @property
    def is_global(self):
        return self.elem.attrib.get("is_global")

    @is_global.setter
    def is_global(self, value):
        self.elem.attrib["is_global"] = value

    @property
    def is_io(self):
        return self.elem.attrib.get("is_io")

    @is_io.setter
    def is_io(self, value):
        self.elem.attrib["is_io"] = value

    @property
    def default_val(self):
        return self.elem.attrib.get("default_val")

    @default_val.setter
    def default_val(self, value):
        self.elem.attrib["default_val"] = value

    @property
    def physical_mode_port(self):
        return self.elem.attrib.get("physical_mode_port")

    @physical_mode_port.setter
    def physical_mode_port(self, value):
        self.elem.attrib["physical_mode_port"] = value

class ArchDelayMatrix(RSElement):
    """ delay_matrix element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("delay_matrix"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

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

    @property
    def mode_select(self):
        return self.elem.attrib.get("mode_select")

    @mode_select.setter
    def mode_select(self, value):
        self.elem.attrib["mode_select"] = value

    @property
    def circuit_model_name(self):
        return self.elem.attrib.get("circuit_model_name")

    @circuit_model_name.setter
    def circuit_model_name(self, value):
        self.elem.attrib["circuit_model_name"] = value

class ArchWireParam(RSElement):
    """ wire_param element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("wire_param"))
        else:
            super().__init__(elem)

    @property
    def model_type(self):
        return self.elem.attrib.get("model_type")

    @model_type.setter
    def model_type(self, value):
        self.elem.attrib["model_type"] = value

    # pylint: disable=invalid-name
    @property
    def R(self):
        return self.elem.attrib.get("R")

    @R.setter
    def R(self, value):
        self.elem.attrib["R"] = value

    @property
    def C(self):
        return self.elem.attrib.get("C")

    @C.setter
    def C(self, value):
        self.elem.attrib["C"] = value
    # pylint: enable=invalid-name

    @property
    def num_level(self):
        return self.elem.attrib.get("num_level")

    @num_level.setter
    def num_level(self, value):
        self.elem.attrib["num_level"] = value

class ArchInputOutputBuffer(RSElement):
    """ input_buffer/output_buffer elements """

    @property
    def exist(self):
        return self.elem.attrib.get("exist")

    @exist.setter
    def exist(self, value):
        self.elem.attrib["exist"] = value

    @property
    def circuit_model_name(self):
        return self.elem.attrib.get("circuit_model_name")

    @circuit_model_name.setter
    def circuit_model_name(self, value):
        self.elem.attrib["circuit_model_name"] = value

class ArchInputBuffer(ArchInputOutputBuffer):
    """ input_buffer """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("input_buffer"))
        else:
            super().__init__(elem)

class ArchOutputBuffer(ArchInputOutputBuffer):
    """ output_buffer """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("output_buffer"))
        else:
            super().__init__(elem)


class ArchPassGateLogic(RSElement):
    """ pass_gate_logic """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("pass_gate_logic"))
        else:
            super().__init__(elem)

    @property
    def circuit_model_name(self):
        return self.elem.attrib.get("circuit_model_name")

    @circuit_model_name.setter
    def circuit_model_name(self, value):
        self.elem.attrib["circuit_model_name"] = value

class ArchLutInputInverter(RSElement):
    """ lut_input_inverter """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("lut_input_inverter"))
        else:
            super().__init__(elem)

    @property
    def exist(self):
        return self.elem.attrib.get("exist")

    @exist.setter
    def exist(self, value):
        self.elem.attrib["exist"] = value

    @property
    def circuit_model_name(self):
        return self.elem.attrib.get("circuit_model_name")

    @circuit_model_name.setter
    def circuit_model_name(self, value):
        self.elem.attrib["circuit_model_name"] = value

class ArchLutInputBuffer(RSElement):
    """ lut_input_buffer """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("lut_input_buffer"))
        else:
            super().__init__(elem)

    @property
    def exist(self):
        return self.elem.attrib.get("exist")

    @exist.setter
    def exist(self, value):
        self.elem.attrib["exist"] = value

    @property
    def circuit_model_name(self):
        return self.elem.attrib.get("circuit_model_name")

    @circuit_model_name.setter
    def circuit_model_name(self, value):
        self.elem.attrib["circuit_model_name"] = value

class ArchCircuitModel(RSElement):
    """ circuit_model element """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("circuit_model"))
        else:
            super().__init__(elem)

    @property
    def type(self):
        return self.elem.attrib.get("type")

    @type.setter
    def type(self, value):
        self.elem.attrib["type"] = value

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def prefix(self):
        return self.elem.attrib.get("prefix")

    @prefix.setter
    def prefix(self, value):
        self.elem.attrib["prefix"] = value

    @property
    def is_default(self):
        return self.elem.attrib.get("is_default")

    @is_default.setter
    def is_default(self, value):
        self.elem.attrib["is_default"] = value

    @property
    def verilog_netlist(self):
        return self.elem.attrib.get("verilog_netlist")

    @verilog_netlist.setter
    def verilog_netlist(self, value):
        self.elem.attrib["verilog_netlist"] = value

    @property
    def dump_structural_verilog(self):
        return self.elem.attrib.get("dump_structural_verilog")

    @dump_structural_verilog.setter
    def dump_structural_verilog(self, value):
        self.elem.attrib["dump_structural_verilog"] = value

    @property
    def design_technologys(self):
        return [ArchDesignTechnology(elem) for elem in self.elem.iterchildren("design_technology")]

    @property
    def device_technologys(self):
        return [ArchDeviceTechnology(elem) for elem in self.elem.iterchildren("device_technology")]

    @property
    def ports(self):
        return [ArchPort(elem) for elem in self.elem.iterchildren("port")]

    @property
    def delay_matrixs(self):
        return [ArchDelayMatrix(elem) for elem in self.elem.iterchildren("delay_matrix")]

    @property
    def input_buffers(self):
        return [ArchInputBuffer(elem) for elem in self.elem.iterchildren("input_buffer")]

    @property
    def output_buffers(self):
        return [ArchOutputBuffer(elem) for elem in self.elem.iterchildren("output_buffer")]

    @property
    def pass_gate_logics(self):
        return [ArchPassGateLogic(elem) for elem in self.elem.iterchildren("pass_gate_logic")]

    @property
    def lut_input_inverters(self):
        return [ArchLutInputInverter(elem) for elem in self.elem.iterchildren("lut_input_inverter")]

    @property
    def lut_input_buffers(self):
        return [ArchLutInputBuffer(elem) for elem in self.elem.iterchildren("lut_input_buffer")]

class ArchCircuitLibrary(RSElement):
    """ circuit_library """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("circuit_library"))
        else:
            super().__init__(elem)

    @property
    def circuit_models(self):
        return [ArchCircuitModel(elem) for elem in self.elem.iterchildren("circuit_model")]
