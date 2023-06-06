#pragma once

#include <map>
#include <set>
#include <unordered_map>

#include "util/geo/xyz.h"
#include "util/pinc_log.h"
#include "file_readers/rapidcsv.h"

namespace pinc {

using std::string;
using std::vector;

class PinPlacer;

class RapidCsvReader {
 public:

  // BCD is a "reduced row record" RRR
  struct BCD {

    string groupA_; // 0-A  Group

    // columns B, C, D, BU(#72) of the table
    // --- 1-B    Bump/Pin Name
    // --- 2-C    Customer Name
    // --- 3-D    Ball ID
    // --- 72-BU  Customer Internal Name
    string bump_,
           customer_, // 2-C Customer Name
           ball_ID_;

    string fullchipName_; // 13-N  Fullchip_NAME

    string customerInternal_; // 72-BU  Customer Internal Name

    int row_ = 0;

    bool is_axi_ = false;
    bool is_GBOX_GPIO_ = false;
    bool is_GPIO_ = false; // based on column A

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

    bool isCustomerInternal() const noexcept { return !customerInternal_.empty(); }

    bool isCustomerInternalUnique() const noexcept {
      return !customerInternal_.empty() && customerInternal_ != customer_; }

    bool isCustomerInternalOnly() const noexcept {
      return !customerInternal_.empty() && customer_.empty(); }

    void normalize() noexcept {
      if (customerInternal_.empty())
        return;
      if (bump_.empty())
        bump_ = customerInternal_;
      if (customer_.empty())
        customer_ = customerInternal_;
    }

  };

  RapidCsvReader();
  ~RapidCsvReader();

  void reset() noexcept;

  // file i/o
  bool read_csv(const std::string& f, bool check);

  void write_csv(string csv_file_name) const;
  void print_csv() const;
  bool sanity_check() const;

  static bool prepare_mode_header(string& hdr) noexcept;

  // data query
  XYZ get_pin_xyz_by_name(const string& mode,
                          const string& customerPin_or_ID,
                          const string& gbox_pin_name) const;

  XYZ get_axi_xyz_by_name(const string& axi_name) const noexcept;

  uint numRows() const noexcept {
    assert(bcd_.size() == io_tile_pin_xyz_.size());
    return bcd_.size();
  }

  bool has_io_pin(const string& pin_name_or_ID) const noexcept;

  bool hasCustomerInternalName(const string& nm) const noexcept;

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

  bool hasMode(const string& key) const noexcept { return modes_map_.count(key); }

  const vector<string>* getModeData(const string& mode_name) const noexcept {
    assert(!mode_name.empty());
    if (mode_name.empty())
      return nullptr;
    auto fitr = modes_map_.find(mode_name);
    if (fitr == modes_map_.end())
      return nullptr;
    return &(fitr->second);
  }

  uint printModeKeys() const;

  string bumpName2CustomerName(const string& bump_name) const noexcept;

  vector<string> get_AXI_inputs() const;
  vector<string> get_AXI_outputs() const;

 private:
  std::map<string, vector<string>> modes_map_;

  vector<string> col_headers_; // all column headers
  vector<string> mode_names_;  // column headers that contain "Mode_/MODE_"

  // below vectors are indexed by csv row, size() == #rows

  vector<BCD> bcd_; // all BCD records

  vector<BCD*> bcd_AXI_; // BCD records with .isCustomerInternalOnly() predicate (AXI pins)

  vector<BCD*> bcd_GBGPIO_;  // BCD records with .is_GBOX_GPIO_ predicate

  vector<string> io_tile_pin_;    // "IO_tile_pin"

  vector<XYZ> io_tile_pin_xyz_;   // "IO_tile_pin_x", "_y", "_z"

  int start_GBOX_GPIO_row_ = 0;   // "GBOX GPIO" group start row in PT

  int start_CustomerInternal_row_ = 0;

  friend class PinPlacer;
};

std::ostream& operator<<(std::ostream& os, const RapidCsvReader::BCD& b);

}  // namespace pinc
