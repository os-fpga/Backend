#pragma once
#ifndef __pln__sta_lib_writer_H
#define __pln__sta_lib_writer_H

#include <map>

#include "RS/LCell.h"

namespace pln {

using std::string;
using std::vector;

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

  void write_lcell(std::ostream& os, const LCell& lc) const;

  void write_lcell_pins(std::ostream& os, const LCell& lc) const;

  void write_timing_arc(std::ostream& os, const LibPin& basePin, const PinArc& arc) const;

  void write_footer(std::ostream& os) const;
};

}  // NS pln

#endif
