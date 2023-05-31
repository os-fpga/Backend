#pragma once
#ifndef __rs_file_readers_BLIF_READER_H
#define __rs_file_readers_BLIF_READER_H

#include <unordered_map>
#include <map>

#include "util/pinc_log.h"

namespace pinc {

using std::string;
using std::vector;

/*
Supported PCF commands:

* set_io  <net> <pad> - constrain a given <net> to a given physical <pad> in eFPGA pinout.
* set_clk <pin> <net> - constrain a given global clock <pin> to a given <net>

  Every tile where <net> is present will be constrained to use a given global clock.
*/

class BlifReader
{
  vector<string> inputs;
  vector<string> outputs;

public:
  BlifReader() {}
  BlifReader(const string& f)
  {
    read_blif(f);
  }
  bool read_blif(const string& f);

  const vector<string>& get_inputs()const { return inputs; }
  const vector<string>& get_outputs()const { return outputs; }
};

}

#endif

