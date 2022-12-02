#pragma once

#include <unordered_map>
#include <map>

#include "pinc_log.h"

/*
Supported PCF commands:

* set_io  <net> <pad> - constrain a given <net> to a given physical <pad> in eFPGA pinout.
* set_clk <pin> <net> - constrain a given global clock <pin> to a given <net>

  Every tile where <net> is present will be constrained to use a given global clock.
*/

namespace pinc {

using std::string;
using std::vector;

struct BlifReader
{
    vector<string> inputs_;
    vector<string> outputs_;

public:
    BlifReader() = default;
    BlifReader(const std::string &f)
    {
        assert(f.length());
        read_blif(f);
    }

    bool read_blif(const std::string &f);

    const vector<string>& get_inputs() const { return inputs_; }
    const vector<string>& get_outputs() const { return outputs_; }
};

}

