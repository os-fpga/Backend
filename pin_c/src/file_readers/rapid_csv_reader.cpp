#include "rapid_csv_reader.h"

#include <algorithm>
#include <set>

#include "rapidcsv.h"

namespace pinc {

using namespace std;

bool rapidCsvReader::read_csv(const string& fn, bool check) {
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2)
    ls << "rapidCsvReader::read_csv( " << fn << " )  Reading data ..." << endl;

  rapidcsv::Document doc(
      fn,
      rapidcsv::LabelParams(),      // label params - use default, no offset
      rapidcsv::SeparatorParams(),  // separator params - use default ","
      rapidcsv::ConverterParams(
          true /*user default number for non-numberical strings*/,
          0.0 /* default float*/, -1 /*default integer*/),
      rapidcsv::LineReaderParams()
      /*skip lines with specified prefix, use default "false"*/);

  vector<string> header_data = doc.GetRow<string>(-1);
  if (tr >= 3) {
    assert(header_data.size() > 2);
    ls << "  header_data.size()= " << header_data.size() << "  ["
       << header_data.front() << " ... " << header_data.back() << ']' << endl;
  }

  vector<string> group_name = doc.GetColumn<string>("Group");
  size_t num_rows = group_name.size();
  assert(num_rows > 1);
  start_position_ = 0;
  for (uint i = 0; i < num_rows; i++) {
    if (group_name[i] == "GBOX GPIO") {
      start_position_ = i;
      break;
    }
  }

  bool has_gbox_name = false;
  vector<string> mode_data;
  for (uint i = 0; i < header_data.size(); i++) {
    const string& hdr_i = header_data[i];
    if (hdr_i.find("Mode_") != string::npos) {
      mode_data = doc.GetColumn<string>(hdr_i);
      assert(mode_data.size() == num_rows);
      modes_map_.emplace(hdr_i, mode_data);
      mode_names_.emplace_back(hdr_i);
    }
    if (!has_gbox_name) {
      if (hdr_i == "GBOX_NAME") has_gbox_name = true;
    }
  }

  bump_pin_name_ = doc.GetColumn<string>("Bump/Pin Name");
  assert(bump_pin_name_.size() == num_rows);
  if (has_gbox_name) {
    gbox_name_ = doc.GetColumn<string>("GBOX_NAME");
    assert(gbox_name_.size() == num_rows);
  }

  io_tile_pin_ = doc.GetColumn<string>("IO_tile_pin");
  assert(io_tile_pin_.size() == num_rows);

  vector<int> tmp = doc.GetColumn<int>("IO_tile_pin_x");
  assert(tmp.size() == num_rows);
  io_tile_pin_xyz_.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) io_tile_pin_xyz_[i].x_ = tmp[i];

  tmp = doc.GetColumn<int>("IO_tile_pin_y");
  assert(tmp.size() == num_rows);
  for (uint i = 0; i < num_rows; i++) io_tile_pin_xyz_[i].y_ = tmp[i];

  tmp = doc.GetColumn<int>("IO_tile_pin_z");
  assert(tmp.size() == num_rows);
  for (uint i = 0; i < num_rows; i++) io_tile_pin_xyz_[i].z_ = tmp[i];

  assert(bump_pin_name_.size() == io_tile_pin_.size() &&
         io_tile_pin_.size() == num_rows);

  // do a sanity check
  if (check) {
    bool check_ok = sanity_check(doc);
    if (!check_ok) {
      cout << "\nWARNING: !check_ok" << endl;
      //      return false;
    }
  }

  // print data of interest for test
  if (tr >= 4) print_csv();

  return true;
}

bool rapidCsvReader::sanity_check(const rapidcsv::Document& doc) const {
  // 1. check IO bump dangling (unused package pin)
  vector<string> group_name = doc.GetColumn<string>("Group");
  uint num_rows = group_name.size();
  assert(num_rows);
  std::set<string> connected_bump_pins;
  for (uint i = 0; i < num_rows; i++) {
    if (group_name[i] == "GBOX GPIO") {
      vector<string> connect_info = doc.GetRow<string>(i);
      for (uint j = 0; j < connect_info.size(); j++) {
        if (connect_info[j] == "Y") {
          connected_bump_pins.insert(bump_pin_name_[i]);
          break;
        }
      }
    }
  }

  bool check_ok = true;

  std::set<string> reported_unconnected_bump_pins;
  for (uint i = 0; i < group_name.size(); i++) {
    if (group_name[i] == "GBOX GPIO") {
      if (connected_bump_pins.find(bump_pin_name_[i]) ==
              connected_bump_pins.end() &&
          reported_unconnected_bump_pins.find(bump_pin_name_[i]) ==
              reported_unconnected_bump_pins.end()) {
        reported_unconnected_bump_pins.insert(bump_pin_name_[i]);
        cout << "[Pin Table Check Warning] Bump pin <" << bump_pin_name_[i]
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

  return true;
}

// file i/o
void rapidCsvReader::write_csv(string file_name) const {
  // to do
  cout << "Not Implement Yet - Write content of interest to a csv file <"
       << file_name << ">" << endl;
  return;
}

void rapidCsvReader::print_csv() const {
  cout << "Bump/Pin "
          "Name\tIO_tile_pin\tIO_tile_pin_x\tIO_tile_pin_y\tIO_tile_pin_z"
       << endl;
  cout << "--------------------------------------------------------------------"
          "---------"
       << endl;
  for (uint i = 0; i < bump_pin_name_.size(); i++) {
    const XYZ& p = io_tile_pin_xyz_[i];
    cout << i << "\t" << bump_pin_name_[i] << "\t" << io_tile_pin_[i] << "\t"
         << p.x_ << "\t" << p.y_ << "\t" << p.z_ << endl;
  }
  cout << "--------------------------------------------------------------------"
          "---------"
       << endl;
  cout << "Total Records: " << bump_pin_name_.size() << endl;
}

XYZ rapidCsvReader::get_pin_xyz_by_bump_name(
    const string& mode, const string& bump_name,
    const string& gbox_pin_name) const {
  XYZ result;
  auto fitr = modes_map_.find(mode);
  if (fitr == modes_map_.end()) return result;

  const vector<string>& mode_vector = fitr->second;

  uint num_rows = bump_pin_name_.size();
  assert(num_rows > 1);
  assert(mode_vector.size() == num_rows);
  assert(io_tile_pin_xyz_.size() == num_rows);

  for (uint i = 0; i < num_rows; i++) {
    if ((bump_pin_name_[i] == bump_name) && (mode_vector[i] == "Y")) {
      if (gbox_pin_name.length() == 0 ||
          ((gbox_pin_name.length() > 0) && (gbox_name_[i] == gbox_pin_name))) {
        // result.set3(io_tile_pin_x_[i], io_tile_pin_y_[i], io_tile_pin_z_[i]);
        result = io_tile_pin_xyz_[i];
        assert(result.valid());
        break;
      }
    }
  }

  return result;
}

}  // namespace pinc
