#ifndef RAPID_CSV_READER_H
#define RAPID_CSV_READER_H

#include "rapidcsv.h"
#include <ctype.h>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;
class pin_location;
class rapidCsvReader {

  std::map<string, std::vector<string>> modes_map;
  std::vector<string> mode_names;
  friend pin_location;
  // this is a little bit ugly, we can have a more beautiful data structure at a
  // cost of memory
  std::vector<string> bump_pin_name; // "Bump/Pin Name"
  std::vector<string> io_tile_pin;   // "IO_tile_pin"
  std::vector<int> io_tile_pin_x;    // "IO_tile_pin_x"
  std::vector<int> io_tile_pin_y;    // "IO_tile_pin_y"
  std::vector<int> io_tile_pin_z;    // "IO_tile_pin_z"
  int start_position = 0; // "GBX GPIO" group start position in pin table row

public:
  // constructor
  rapidCsvReader() {}

  // file i/o
  bool read_csv(const std::string &f, bool check);
  void write_csv(string csv_file_name);
  void print_csv();

  // data query
  int get_pin_x_by_bump_name(string mode, string bump_name);
  int get_pin_y_by_bump_name(string mode, string bump_name);
  int get_pin_z_by_bump_name(string mode, string bump_name);
  int get_pin_x_by_pin_name(string mode, string pin_name);
  int get_pin_y_by_pin_name(string mode, string pin_name);
  int get_pin_z_by_pin_name(string mode, string pin_name);
  string get_bump_name_by_pin_name(string mode, string pin_name);
  string get_pin_name_by_bump_name(string mode, string bump_name);
  bool find_io_pin(string pin_name);
  int get_pin_x_by_pin_idx(int i);
  int get_pin_y_by_pin_idx(int i);
  int get_pin_z_by_pin_idx(int i);
};

#endif
