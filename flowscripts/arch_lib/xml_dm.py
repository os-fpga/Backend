#!/usr/bin/env python3

""" Rapid Silicon's XML data model """
# pylint: disable=missing-function-docstring

import lxml.etree as etree

class RSElement:
    """ element wrapper """

    def __init__(self, elem):
        """ constructor """

        self.elem = elem

    def get_attribute(self, attribute):
        """ get_attribute in case property not yet available in API """

        return self.elem.attrib.get(attribute)

    def set_attribute(self, attribute, value):
        """ set_attribute in case property not yet available in API """

        self.elem.attrib[attribute] = value

    def append(self, elem):
        """ append to current element """

        self.elem.append(elem.elem)

    @property
    def attribs(self):
        return dict(self.elem.attrib)

    @property
    def text(self):
        return self.elem.text

    @text.setter
    def text(self, value):
        self.elem.text = value

def validate(xml_path: str, xsd_path: str) -> bool:

    xmlschema_doc = etree.parse(xsd_path)
    xmlschema = etree.XMLSchema(xmlschema_doc)

    xml_doc = etree.parse(xml_path)
    result = xmlschema.validate(xml_doc)

    return result
