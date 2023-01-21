#pragma once

#include <map>
#include <unordered_map>

#include "geo/xyz.h"
#include "pinc_log.h"
#include "rapidcsv.h"

namespace pinc {

using std::string;
using std::vector;

class pin_location;

class RapidCsvReader {
 public:

  struct BCD {
    // columns B, C, D of the table
    // --- 1-B  Bump/Pin Name
    // --- 2-C  Ball Name
    // --- 3-D  Ball ID
    string bumpB_, ballNameC_, ball_ID_;
    int row_ = 0;

    BCD() noexcept = default;
  };

  RapidCsvReader() = default;

  // file i/o
  bool read_csv(const std::string& f, bool check);

  void write_csv(string csv_file_name) const;
  void print_csv() const;
  bool sanity_check(const rapidcsv::Document& doc) const;

  // data query
  XYZ get_pin_xyz_by_bump_name(const string& mode, const string& bump_name,
                               const string& gbox_pin_name) const;

  bool has_io_pin(const string& pin_name) const noexcept {
    auto I = std::find(bump_pin_name_.begin(), bump_pin_name_.end(), pin_name);
    return I != bump_pin_name_.end();
  }

  int get_pin_x_by_pin_idx(uint i) const noexcept {
    assert(i < io_tile_pin_xyz_.size());
    return io_tile_pin_xyz_[i].x_;
  }

  int get_pin_y_by_pin_idx(uint i) const noexcept {
    assert(i < io_tile_pin_xyz_.size());
    return io_tile_pin_xyz_[i].y_;
  }

  int get_pin_z_by_pin_idx(uint i) const noexcept {
    assert(i < io_tile_pin_xyz_.size());
    return io_tile_pin_xyz_[i].z_;
  }

 private:
  std::map<string, vector<string>> modes_map_;
  vector<string> mode_names_;

  // vectors are indexed by csv row, size() == #rows

  vector<string> bump_pin_name_;  // "Bump/Pin Name" - column B

  vector<BCD> bcd_; // "Bump/Pin Name", "Ball Name", "Ball ID" - columns B, C, D

  vector<string> gbox_name_;      // "GBOX_NAME"

  vector<string> io_tile_pin_;    // "IO_tile_pin"

  vector<XYZ> io_tile_pin_xyz_;  // "IO_tile_pin_x", "_y", "_z"

  int start_position_ = 0;  // "GBX GPIO" group start position in pin table row

  friend class pin_location;
};

inline std::ostream& operator<<(std::ostream& os, const RapidCsvReader::BCD& p) {
  os << "(bcd  " << p.bumpB_ << "  " << p.ballNameC_
     << "  " << p.ball_ID_ << "  row:" << p.row_ << ')';
  return os;
}

}  // namespace pinc
