#pragma once
#ifndef __rsbe__sta_lib_writer_H
#define __rsbe__sta_lib_writer_H

#include <iostream>
#include <map>
#include <vector>
#include "sta_lib_data.h"

namespace rsbe {

// writer
class sta_lib_writer {

private:
  std::map<std::string, std::string> lib_header;
  std::map<std::string, std::string> lib_info;
  std::map<std::string, std::string> lib_unit;
  std::map<std::string, std::string> lib_cap_load;
  std::map<std::string, std::string> lib_feature;
  std::map<std::string, std::string> lib_data_setting;
  std::map<std::string, std::map<std::string, std::string>> lib_operating_conditions;
  std::map<std::string, std::string> lib_operating_typical;
  std::map<std::string, std::string> lib_bus_type;

public:
  sta_lib_writer() = default;

  void init_lib();

  void write_header(std::ostream& os);
  void write_bus_type(std::ostream& os, int from, int to, bool down_to);
  void write_cell(std::ostream& os, const lib_cell& cell);
  void write_cell_pin(std::ostream& os, const lib_cell& cell);
  void write_timing_relation(std::ostream& os, const TimingArc& timing);
  void write_footer(std::ostream& os);
};

}  // NS rsbe

#endif
