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
    string bump_, ball_, ball_ID_;
    int row_ = 0;

    BCD() noexcept = default;
  };

  RapidCsvReader();
  ~RapidCsvReader();

  // file i/o
  bool read_csv(const std::string& f, bool check);

  void write_csv(string csv_file_name) const;
  void print_csv() const;
  bool sanity_check(const rapidcsv::Document& doc) const;

  // data query
  XYZ get_pin_xyz_by_name(const string& mode,
                          const string& bump_ball_or_ID,
                          const string& gbox_pin_name) const;

  uint numRows() const noexcept {
    assert(bcd_.size() == gbox_name_.size());
    assert(bcd_.size() == io_tile_pin_xyz_.size());
    return bcd_.size();
  }

  bool has_io_pin(const string& pin_name_or_ID) const noexcept;

  const string& bumpPinName(uint row) const noexcept {
    assert(row < bcd_.size());
    return bcd_[row].bump_;
  }

  const string& ballPinName(uint row) const noexcept {
    assert(row < bcd_.size());
    return bcd_[row].ball_;
  }

  const vector<string>* getModeData(const string& mode_name) const noexcept {
    assert(!mode_name.empty());
    if (mode_name.empty())
      return nullptr;
    auto fitr = modes_map_.find(mode_name);
    if (fitr == modes_map_.end())
      return nullptr;
    return &(fitr->second);
  }

  string bumpName2BallName(const string& bump_name) const noexcept;

 private:
  std::map<string, vector<string>> modes_map_;

  vector<string> mode_names_; // column labels that contain "Mode_"

  // below vectors are indexed by csv row, size() == #rows

  // vector<string> bump_pin_name_;  // "Bump/Pin Name" - column B

  vector<BCD> bcd_; // "Bump/Pin Name", "Ball Name", "Ball ID" - columns B, C, D

  vector<string> gbox_name_;      // "GBOX_NAME"

  vector<string> io_tile_pin_;    // "IO_tile_pin"

  vector<XYZ> io_tile_pin_xyz_;  // "IO_tile_pin_x", "_y", "_z"

  int start_position_ = 0;  // "GBX GPIO" group start position in pin table row

  friend class pin_location;
};

inline std::ostream& operator<<(std::ostream& os, const RapidCsvReader::BCD& b) {
  os << "(bcd  " << b.bump_ << "  " << b.ball_
     << "  " << b.ball_ID_ << "  row:" << b.row_ << ')';
  return os;
}

}  // namespace pinc
