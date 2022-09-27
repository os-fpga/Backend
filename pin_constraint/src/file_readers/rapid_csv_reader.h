#ifndef RAPID_CSV_READER_H
#define RAPID_CSV_READER_H

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include <ctype.h>
#include <map>
#include "rapidcsv.h"

using namespace std;
class pin_location;
class rapidCsvReader {

  friend pin_location;
  // this is a little bit ugly, we can have a more beautiful data structure at a cost of memory
  std::vector<string> bump_pin_name;// "Bump/Pin Name"
  std::vector<string> io_tile_pin;  // "IO_tile_pin"
  std::vector<int> io_tile_pin_x;   // "IO_tile_pin_x"
  std::vector<int> io_tile_pin_y;   // "IO_tile_pin_y"
  std::vector<int> io_tile_pin_z;   // "IO_tile_pin_z"
  std::vector<string> usable; // "Usable"

public:
  // constructor
  rapidCsvReader () {}

  // file i/o
  bool read_csv(const std::string &f); 
  void write_csv(string csv_file_name);
  void print_csv();

  // data query
  int get_pin_x_by_bump_name(string bump_name);
  int get_pin_y_by_bump_name(string bump_name);
  int get_pin_z_by_bump_name(string bump_name);
  int get_pin_x_by_pin_name(string pin_name);
  int get_pin_y_by_pin_name(string pin_name);
  int get_pin_z_by_pin_name(string pin_name);
  string get_bump_name_by_pin_name(string pin_name);
  string get_pin_name_by_bump_name(string bump_name);
  bool find_io_pin(string pin_name);
  int get_pin_x_by_pin_idx(int i);
  int get_pin_y_by_pin_idx(int i);
  int get_pin_z_by_pin_idx(int i);

};

#endif
