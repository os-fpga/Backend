#include "rapid_csv_reader.h"
#include "rapidcsv.h"
#include <algorithm>
#include <set>

bool rapidCsvReader::read_csv(const std::string &f, bool check) {
#ifdef DEBUG_PIN_C
  cout << "Reading data ... ";
#endif
  rapidcsv::Document doc(
      f /*csv file name*/,
      rapidcsv::LabelParams() /*label params - use default, no offset*/,
      rapidcsv::SeparatorParams() /*separator params - use default ","*/, rapidcsv::ConverterParams(true /*user default number for non-numberical strings*/, 0.0 /* default float*/, -1 /*default integer*/), rapidcsv::LineReaderParams() /*skip lines with specified prefix, use default "false"*/);
  std::vector<string> header_data = doc.GetRow<string>(-1);
  for (unsigned int i = 0; i < header_data.size(); i++) {
    if (header_data[i].find("Mode_") != std::string::npos) {
      std::vector<string> mode_data = doc.GetColumn<string>(header_data[i]);
      modes_map.insert(
          std::pair<string, std::vector<string>>(header_data[i], mode_data));
      mode_names.push_back(header_data[i]);
    }
  }
  std::vector<string> group_name = doc.GetColumn<string>("Group");
  for (int i = 0; i < group_name.size(); i++) {
    if (group_name[i] == "GBOX GPIO") {
      start_position = i;
      break;
    }
  }
  bump_pin_name = doc.GetColumn<string>("Bump/Pin Name");
  io_tile_pin = doc.GetColumn<string>("IO_tile_pin");
  io_tile_pin_x = doc.GetColumn<int>("IO_tile_pin_x");
  io_tile_pin_y = doc.GetColumn<int>("IO_tile_pin_y");
  io_tile_pin_z = doc.GetColumn<int>("IO_tile_pin_z");

  assert((bump_pin_name.size() == io_tile_pin.size()) &&
         (io_tile_pin.size() == io_tile_pin_x.size()) &&
         (io_tile_pin_x.size() == io_tile_pin_y.size()) &&
         (io_tile_pin_y.size() == io_tile_pin_z.size()));

  // do a sanity check
  if (check) {
    // 1. check IO bump dangling (unused package pin)
    std::vector<string> group_name = doc.GetColumn<string>("Group");
    std::set<string> connected_bump_pins;
    for (unsigned int i = 0; i < group_name.size(); i++) {
      if (group_name[i] == "GBOX GPIO") {
        std::vector<string> connect_info = doc.GetRow<string>(i);
        for (unsigned int j = 0; j < connect_info.size(); j++) {
          if (connect_info[j] == "Y") {
            connected_bump_pins.insert(bump_pin_name[i]);
            break;
          }
        }
      }
    }
    bool check_ok = true;
    std::set<string> reported_unconnected_bump_pins;
    for (unsigned int i = 0; i < group_name.size(); i++) {
      if (group_name[i] == "GBOX GPIO") {
        if (connected_bump_pins.find(bump_pin_name[i]) ==
                connected_bump_pins.end() &&
            reported_unconnected_bump_pins.find(bump_pin_name[i]) ==
                reported_unconnected_bump_pins.end()) {
          reported_unconnected_bump_pins.insert(bump_pin_name[i]);
          cout << "[Pin Table Check Warning] Bump pin <" << bump_pin_name[i]
               << "> is not connected to FABRIC through any bridge for user "
                  "design data IO."
               << endl;
          check_ok = false;
        }
      }
    }
    if (!check_ok) {
      //      return false;
    }
    // 2. more mode check (to be added based on GBX spec)
  }

  // print data of interest for test
#ifdef DEBUG_PIN_C
  print_csv();
#endif

  return true;
}

// file i/o
void rapidCsvReader::write_csv(string file_name) {
  // to do
  cout << "Not Implement Yet - Write content of interest to a csv file <"
       << file_name << ">" << endl;
  return;
}
void rapidCsvReader::print_csv() {
  cout << "Bump/Pin "
          "Name\tIO_tile_pin\tIO_tile_pin_x\tIO_tile_pin_y\tIO_tile_pin_z"
       << endl;
  cout << "--------------------------------------------------------------------"
          "---------"
       << endl;
  for (unsigned int i = 0; i < bump_pin_name.size(); i++) {
    cout << i << "\t" << bump_pin_name[i] << "\t" << io_tile_pin[i] << "\t"
         << io_tile_pin_x[i] << "\t" << io_tile_pin_y[i] << "\t"
         << io_tile_pin_z[i] << endl;
  }
  cout << "--------------------------------------------------------------------"
          "---------"
       << endl;
  cout << "Total Records: " << bump_pin_name.size() << endl;
  return;
}

// data query
int rapidCsvReader::get_pin_x_by_bump_name(string mode, string bump_name) {
  if (modes_map.find(mode) == modes_map.end()) {
    return -1;
  }
  std::vector<string> mode_vector = modes_map[mode];
  for (unsigned int i = 0; i < bump_pin_name.size(); i++) {
    if ((bump_pin_name[i] == bump_name) && (mode_vector[i] == "Y")) {
      return io_tile_pin_x[i];
    }
  }
  return -1;
}
int rapidCsvReader::get_pin_y_by_bump_name(string mode, string bump_name) {
  if (modes_map.find(mode) == modes_map.end()) {
    return -1;
  }
  std::vector<string> mode_vector = modes_map[mode];
  for (unsigned int i = 0; i < bump_pin_name.size(); i++) {
    if ((bump_pin_name[i] == bump_name) && (mode_vector[i] == "Y")) {
      return io_tile_pin_y[i];
    }
  }
  return -1;
}
int rapidCsvReader::get_pin_z_by_bump_name(string mode, string bump_name) {
  if (modes_map.find(mode) == modes_map.end()) {
    return -1;
  }
  std::vector<string> mode_vector = modes_map[mode];
  for (unsigned int i = 0; i < bump_pin_name.size(); i++) {
    if ((bump_pin_name[i] == bump_name) && (mode_vector[i] == "Y")) {
      return io_tile_pin_z[i];
    }
  }
  return -1;
}
int rapidCsvReader::get_pin_x_by_pin_name(string mode, string pin_name) {
  if (modes_map.find(mode) == modes_map.end()) {
    return -1;
  }
  std::vector<string> mode_vector = modes_map[mode];
  for (unsigned int i = 0; i < io_tile_pin.size(); i++) {
    if ((io_tile_pin[i] == pin_name) && (mode_vector[i] == "Y")) {
      return io_tile_pin_x[i];
    }
  }
  return -1;
}
int rapidCsvReader::get_pin_y_by_pin_name(string mode, string pin_name) {
  if (modes_map.find(mode) == modes_map.end()) {
    return -1;
  }
  std::vector<string> mode_vector = modes_map[mode];
  for (unsigned int i = 0; i < io_tile_pin.size(); i++) {
    if ((io_tile_pin[i] == pin_name) && (mode_vector[i] == "Y")) {
      return io_tile_pin_y[i];
    }
  }
  return -1;
}
int rapidCsvReader::get_pin_z_by_pin_name(string mode, string pin_name) {
  if (modes_map.find(mode) == modes_map.end()) {
    return -1;
  }
  std::vector<string> mode_vector = modes_map[mode];
  for (unsigned int i = 0; i < io_tile_pin.size(); i++) {
    if ((io_tile_pin[i] == pin_name) && (mode_vector[i] == "Y")) {
      return io_tile_pin_z[i];
    }
  }
  return -1;
}
string rapidCsvReader::get_bump_name_by_pin_name(string mode, string pin_name) {
  if (modes_map.find(mode) == modes_map.end()) {
    return string("");
  }
  std::vector<string> mode_vector = modes_map[mode];
  for (unsigned int i = 0; i < io_tile_pin.size(); i++) {
    if ((io_tile_pin[i] == pin_name) && (mode_vector[i] == "Y")) {
      return bump_pin_name[i];
    }
  }
  return string("");
}
string rapidCsvReader::get_pin_name_by_bump_name(string mode,
                                                 string bump_name) {
  if (modes_map.find(mode) == modes_map.end()) {
    return string("");
  }
  std::vector<string> mode_vector = modes_map[mode];
  for (unsigned int i = 0; i < bump_pin_name.size(); i++) {
    if ((bump_pin_name[i] == bump_name) && (mode_vector[i] == "Y")) {
      return io_tile_pin[i];
    }
  }
  return string("");
}
bool rapidCsvReader::find_io_pin(string pin_name) {
  return std::find(bump_pin_name.begin(), bump_pin_name.end(), pin_name) !=
         bump_pin_name.end();
}
int rapidCsvReader::get_pin_x_by_pin_idx(int i) { return io_tile_pin_x[i]; }
int rapidCsvReader::get_pin_y_by_pin_idx(int i) { return io_tile_pin_y[i]; }
int rapidCsvReader::get_pin_z_by_pin_idx(int i) { return io_tile_pin_z[i]; }
