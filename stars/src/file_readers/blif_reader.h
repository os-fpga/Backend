#pragma once
#ifndef _pln_file_readers_BLIF_READER_H
#define _pln_file_readers_BLIF_READER_H

#include "util/pln_log.h"

namespace pln {

using std::string;
using std::vector;

/*
Supported PCF commands:

* set_io  <net> <pad> - constrain a given <net> to a given physical <pad> in eFPGA pinout.
* set_clk <pin> <net> - constrain a given global clock <pin> to a given <net>

  Every tile where <net> is present will be constrained to use a given global clock.
*/

class BlifReader {
  vector<string> inputs_;
  vector<string> outputs_;

public:
  BlifReader() noexcept = default;

  BlifReader(const string& f) { read_blif(f); }
  bool read_blif(const string& f);

  const vector<string>& get_inputs() const noexcept { return inputs_; }
  const vector<string>& get_outputs() const noexcept { return outputs_; }
};

}

#endif
