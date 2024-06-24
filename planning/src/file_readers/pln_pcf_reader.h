#pragma once

#include "util/pln_log.h"

namespace pln {

using std::string;
using std::vector;

struct PcfReader {

  struct Cmd {
    vector<string> cmd_;
    string internalPin_;

    Cmd() noexcept = default;

    size_t size() const noexcept { return cmd_.size(); }

    bool hasInternalPin() const noexcept { return not internalPin_.empty(); }

    void setInternalPin(const string& nm) noexcept {
      assert(not nm.empty());
      internalPin_ = nm;
    }
    void clearInternalPin() noexcept { internalPin_.clear(); }
  };

  vector<Cmd> commands_;

  PcfReader() noexcept = default;

  PcfReader(const string& f) { read_pcf(f); }

  bool read_pcf(const string& f);
};

}

