#!/usr/bin/env python3
""" Rapid Silicon's Architectural XML data model """

# see documentation in vpr_dm.py

# pylint: disable=missing-function-docstring
# extension-pkg-allow-list=lxml

from lxml import etree

# Rapid Silicon XML datamodel
from arch_lib.xml_dm import RSElement
from arch_lib.vpr_tile_lib_dm import VprClock
from arch_lib.vpr_tile_lib_dm import VprInput
from arch_lib.vpr_tile_lib_dm import VprOutput
from arch_lib.vpr_direct_lib_dm import VprDirect
from arch_lib.vpr_seg_lib_dm import VprMux
from arch_lib.vpr_seg_lib_dm import VprDelayConstant

class VprMode(RSElement):
    """ mode """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("mode"))
        else:
            super().__init__(elem)

    @property
    def name(self):
        return self.elem.attrib.get("name")

    @name.setter
    def name(self, value):
        self.elem.attrib["name"] = value

    @property
    def disable_packing(self):
        return self.elem.attrib.get("disable_packing")

    @disable_packing.setter
    def disable_packing(self, value):
        self.elem.attrib["disable_packing"] = value

    @property
    def pb_types(self):
        return [VprPbType(elem) for elem in self.elem.iterchildren("pb_type")]

    @property
    def interconnects(self):
        return [VprInterconnect(elem) for elem in self.elem.iterchildren("interconnect")]

class VprTSetup(RSElement):
    """ T_setup """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("T_setup"))
        else:
            super().__init__(elem)

    @property
    def value(self):
        return self.elem.attrib.get("value")

    @value.setter
    def value(self, value):
        self.elem.attrib["value"] = value

    @property
    def port(self):
        return self.elem.attrib.get("port")

    @port.setter
    def port(self, value):
        self.elem.attrib["port"] = value

    @property
    def clock(self):
        return self.elem.attrib.get("clock")

    @clock.setter
    def clock(self, value):
        self.elem.attrib["clock"] = value

class VprTHold(RSElement):
    """ T_hold """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("T_hold"))
        else:
            super().__init__(elem)

    @property
    def value(self):
        return self.elem.attrib.get("value")

    @value.setter
    def value(self, value):
        self.elem.attrib["value"] = value

    @property
    def port(self):
        return self.elem.attrib.get("port")

    @port.setter
    def port(self, value):
        self.elem.attrib["port"] = value

    @property
    def clock(self):
        return self.elem.attrib.get("clock")

    @clock.setter
    def clock(self, value):
        self.elem.attrib["clock"] = value

class VprTClockToQ(RSElement):
    """ T_clock_to_Q """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("T_clock_to_Q"))
        else:
            super().__init__(elem)

    @property
    def max(self):
        return self.elem.attrib.get("max")

    @max.setter
    def max(self, value):
        self.elem.attrib["max"] = value

    @property
    def port(self):
        return self.elem.attrib.get("port")

    @port.setter
    def port(self, value):
        self.elem.attrib["port"] = value

    @property
    def clock(self):
        return self.elem.attrib.get("clock")

    @clock.setter
    def clock(self, value):
        self.elem.attrib["clock"] = value

class VprDelayMatrix(RSElement):
    """ delay_matrix """

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

class VprComplete(RSElement):
    """ complete """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("complete"))
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


class VprInterconnect(RSElement):
    """ interconnect """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("interconnect"))
        else:
            super().__init__(elem)

    @property
    def directs(self):
        return [VprDirect(elem) for elem in self.elem.iterchildren("direct")]

    @property
    def muxs(self):
        return [VprMux(elem) for elem in self.elem.iterchildren("mux")]

    @property
    def completes(self):
        return [VprComplete(elem) for elem in self.elem.iterchildren("complete")]

class VprPbType(RSElement):
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
    def num_pb(self):
        return self.elem.attrib.get("num_pb")

    @num_pb.setter
    def num_pb(self, value):
        self.elem.attrib["num_pb"] = value

    @property
    def blif_model(self):
        return self.elem.attrib.get("blif_model")

    @blif_model.setter
    def blif_model(self, value):
        self.elem.attrib["blif_model"] = value

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
    def modes(self):
        return [VprMode(elem) for elem in self.elem.iterchildren("mode")]

    # pylint: disable=invalid-name
    @property
    def T_setups(self):
        return [VprTSetup(elem) for elem in self.elem.iterchildren("T_setup")]

    @property
    def T_holds(self):
        return [VprTHold(elem) for elem in self.elem.iterchildren("T_hold")]

    @property
    def T_clock_to_Qs(self):
        return [VprTClockToQ(elem) for elem in self.elem.iterchildren("T_clock_to_Q")]
    # pylint: enable=invalid-name

    @property
    def interconnects(self):
        return [VprInterconnect(elem) for elem in self.elem.iterchildren("interconnect")]

    @property
    def delay_matrixs(self):
        return [VprDelayMatrix(elem) for elem in self.elem.iterchildren("delay_matrix")]

    @property
    def delay_constants(self):
        return [VprDelayConstant(elem) for elem in self.elem.iterchildren("delay_constant")]

    @property
    def pb_types(self):
        return [VprPbType(elem) for elem in self.elem.iterchildren("pb_type")]

class VprComplexblocklist(RSElement):
    """ complexblocklist """

    def __init__(self, elem=None):

        if elem is None:
            super().__init__(etree.Element("complexblocklist"))
        else:
            super().__init__(elem)

    @property
    def pb_types(self):
        return [VprPbType(elem) for elem in self.elem.iterchildren("pb_type")]
