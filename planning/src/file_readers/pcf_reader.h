#pragma once

#include "util/pln_log.h"

namespace pln {

using std::string;
using std::vector;

struct PcfReader {

  struct Cmd {
    vector<string> cmd_;
    bool hasInternalPin_ = false;

    Cmd() noexcept = default;

    size_t size() const noexcept { return cmd_.size(); }
  };

  vector<Cmd> commands_;

  PcfReader() noexcept = default;

  PcfReader(const string& f) { read_pcf(f); }

  bool read_pcf(const string& f);

  //// const vector<Cmd>& get_commands() const noexcept { return commands_; }
};

}

