#ifndef CSV_READER_H
#define CSV_READER_H

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <ctype.h>
#include <map>

using namespace std;

class CsvReader
{
  vector<vector<string>> entries;
  map<string, string> port_map;
public:
  CsvReader() {}
  bool read_csv(const std::string &f); 
  const vector<vector<string>>& get_entries()const { return entries;}
  const map<string, string>& get_port_map()const { return port_map;}
};

#endif
