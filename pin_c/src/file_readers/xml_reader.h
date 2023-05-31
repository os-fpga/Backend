#pragma once
#ifndef __rs_fileRd__XML_READER_H
#define __rs_fileRd__XML_READER_H

#include "util/pinc_log.h"
#include <map>

#include "pugixml.hpp"

namespace fio {

class PinMappingData
{
    public:
        PinMappingData(std::string p_name, std::string map_pin, int x, int y, int z)
         : port_name_(p_name), mapped_pin_(map_pin), x_(x), y_(y), z_(z) {}
        std::string get_port_name () { return port_name_; }
        std::string get_mapped_pin () { return mapped_pin_; }
        int get_x () { return x_; }
        int get_y () { return y_; }
        int get_z () { return z_; }
    private:
        std::string port_name_;
        std::string mapped_pin_;
        int x_ = 0;
        int y_ = 0;
        int z_ = 0;
};

class XmlReader
{
  std::map<std::string, PinMappingData> port_map_;
public:
  XmlReader() {}
  bool read_xml(const std::string &f);
  const std::map<std::string, PinMappingData>& get_port_map()const { return port_map_;}
  std::vector<std::string> vec_to_scalar(std::string str);

  bool parse_io_cell (const pugi::xml_node xml_orient_io,
        int row_or_col, int io_per_cell, std::map<std::string, PinMappingData> *port_map);

  bool parse_io (const pugi::xml_node xml_io,
        int width, int height, int io_per_cell, std::map<std::string, PinMappingData> *port_map);
};

} // NS fio

#endif

