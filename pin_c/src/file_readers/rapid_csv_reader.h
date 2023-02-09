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
    // columns B, C, D, BU(#72) of the table
    // --- 1-B    Bump/Pin Name
    // --- 2-C    Customer Name
    // --- 3-D    Ball ID
    // --- 72-BU  Customer Internal Name
    string bump_,
           customer_, // 2-C Customer Name
           ball_ID_,
           customerInternal_; // 72-BU Customer Internal Name

    int row_ = 0;

    BCD() noexcept = default;

    bool match(const string& customerPin_or_ID) const noexcept {
      if (customer_ == customerPin_or_ID)
        return true;
      if (ball_ID_ == customerPin_or_ID)
        return true;
      if (customerInternal_ == customerPin_or_ID)
        return true;
      return false;
    }
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
                          const string& customerPin_or_ID,
                          const string& gbox_pin_name) const;

  uint numRows() const noexcept {
    assert(bcd_.size() == fullchip_name_.size());
    assert(bcd_.size() == io_tile_pin_xyz_.size());
    return bcd_.size();
  }

  bool has_io_pin(const string& pin_name_or_ID) const noexcept;

  const string& bumpPinName(uint row) const noexcept {
    assert(row < bcd_.size());
    return bcd_[row].bump_;
  }

  const string& customerPinName(uint row) const noexcept {
    assert(row < bcd_.size());
    return bcd_[row].customer_;
  }
  const string& customerInternalName(uint row) const noexcept {
    assert(row < bcd_.size());
    return bcd_[row].customerInternal_;
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

  string bumpName2CustomerName(const string& bump_name) const noexcept;

 private:
  std::map<string, vector<string>> modes_map_;

  vector<string> mode_names_; // column labels that contain "Mode_"

  // below vectors are indexed by csv row, size() == #rows

  vector<BCD> bcd_; // "Bump/Pin Name", "Customer Name", "Ball ID" - columns B, C, D

  vector<string> fullchip_name_;  // "Fullchip_NAME", column N

  vector<string> io_tile_pin_;    // "IO_tile_pin"

  vector<XYZ> io_tile_pin_xyz_;   // "IO_tile_pin_x", "_y", "_z"

  int start_position_ = 0;  // "GBX GPIO" group start position in pin table row

  friend class pin_location;
};

inline std::ostream& operator<<(std::ostream& os, const RapidCsvReader::BCD& b) {
  os << "(bcd  " << b.bump_ 
     << "  " << b.customer_
     << "  " << b.ball_ID_
     << "  ci:" << b.customerInternal_
     << "  row:" << b.row_ << ')';
  return os;
}

}  // namespace pinc
