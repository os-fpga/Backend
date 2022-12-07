#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "pinc_log.h"

/*
Supported PCF commands:

* set_io  <net> <pad> - constrain a given <net> to a given physical <pad> in
eFPGA pinout.
* set_clk <pin> <net> - constrain a given global clock <pin> to a given <net>

  Every tile where <net> is present will be constrained to use a given global
clock.
*/

namespace pinc {

using std::string;
using std::vector;

struct PcfReader {
  vector<vector<string>> commands_;
  // std::map<string, string> pcf_pin_map;

  PcfReader() = default;

  PcfReader(const string& f) { read_pcf(f); }

  bool read_pcf(const string& f);
  bool read_os_pcf(const string& f);

  const vector<vector<string>>& get_commands() const { return commands_; }

  // unordered_map<string, string>& get_pcf_pin_map()const { return pcf_pin_map;
  // }
};

}  // namespace pinc
