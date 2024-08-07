#pragma once
#ifndef _pln_BLIF_READER_H_4e501fa40b33e91_
#define _pln_BLIF_READER_H_4e501fa40b33e91_

#include "util/pln_log.h"

namespace pln {

using std::string;
using std::vector;

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
