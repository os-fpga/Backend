#pragma once
#ifndef __rsbe__sta_lib_writer_H
#define __rsbe__sta_lib_writer_H

#include <iostream>
#include <map>
#include <vector>
#include "sta_lib_data.h"

namespace rsbe {

using std::string;
using std::vector;
using namespace pinc;

class LibWriter {
  std::map<string, string>  lib_header;
  std::map<string, string>  lib_info;
  std::map<string, string>  lib_unit;
  std::map<string, string>  lib_cap_load;
  std::map<string, string>  lib_feature;
  std::map<string, string>  lib_data_setting;
  std::map<string, std::map<string, string>>  lib_operating_conditions;
  std::map<string, string>  lib_operating_typical;
  std::map<string, string>  lib_bus_type;

public:
  LibWriter() = default;

  void init_lib();

  void write_header(std::ostream& os);

  void write_bus_type(std::ostream& os, int from, int to, bool down_to);

  void write_lcell(std::ostream& os, const lib_cell& lc) const;

  void write_lcell_pins(std::ostream& os, const lib_cell& lc) const;

  void write_timing_arc(std::ostream& os, const LibPin& basePin, const TimingArc& arc) const;

  void write_footer(std::ostream& os) const;
};

}  // NS rsbe

#endif
