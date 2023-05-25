#pragma once
#ifndef __rs_filer__CSV_READER_H_h_
#define __rs_filer__CSV_READER_H_h_

#include <unordered_map>
#include <map>

#include "util/pinc_log.h"

namespace filer {

using std::string;
using std::vector;

class CsvReader
{
  vector<vector<string>> entries;
  std::map<string, string> port_map;

 public:
  CsvReader() {}

  bool read_csv(const string& f);

  const vector<vector<string>>& get_entries() const { return entries; }
  const std::map<string, string>& get_port_map() const { return port_map; }
};

} // NS filer

#endif

