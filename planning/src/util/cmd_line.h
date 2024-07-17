#pragma once
#ifndef __rs_pln_CMD_LINE_H_3837317b793_
#define __rs_pln_CMD_LINE_H_3837317b793_

#include <unordered_map>
#include <unordered_set>

#include "util/pln_log.h"

namespace pln {

using std::string;
using std::unordered_map;
using std::unordered_set;

struct cmd_line {
  unordered_map<string, string> params_;
  unordered_set<string> flags_;

  cmd_line(int argc, const char **argv);

  const unordered_set<string>&  get_flag_set() const noexcept { return flags_; }
  const unordered_map<string, string>&  get_param_map() const noexcept { return params_; }

  bool is_flag_set(const string& fl) const noexcept { return flags_.count(fl); }

  string get_param(const string& key) const noexcept {
    auto fitr = params_.find(key);
    if (fitr == params_.end()) return {};
    return fitr->second;
  }

  void set_flag(const string& fl) { flags_.insert(fl); }

  void set_param_value(const string& key, const string& val) { params_[key] = val; }

  void print_options() const;
};

}

#endif
