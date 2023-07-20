#pragma once

#include <map>
#include <set>
#include <unordered_map>

#include "util/geo/xyz.h"
#include "util/pinc_log.h"

namespace fio {
class CSV_Reader;
}

namespace pinc {

using std::string;
using std::vector;

class PinPlacer;

class RapidCsvReader {
public:

  // BCD is a "reduced row record" RRR (subset of important columns)
  class BCD {

    string customerInternal_; // 72-BU  Customer Internal Name
  
  public:

    enum ModeDir {
        No_dir       = 0,    // row has no Ys in RX/TX cols
        Input_dir    = 1,    // row has Ys in RX cols
        Output_dir   = 2,    // row has Ys in TX cols
        HasBoth_dir  = 3,    // row has at least one Y in both RX and TX cols, and some blanks
        AllEnabled_dir  = 4  // row has no blanks - Ys in all RX/TX cols (i.e. all modes enabled)
    };

    string groupA_; // 0-A  Group

    // columns: B, C, D, I, J, K, L, BU(#72)
    // --- 1-B    Bump/Pin Name
    // --- 2-C    Customer Name
    // --- 3-D    Ball ID
    // --- 8-I    IO_tile_pin
    // --- 9-J    IO_tile_pin_x
    // --- 10-K   IO_tile_pin_y
    // --- 11-L   IO_tile_pin_z
    // --- 72-BU  Customer Internal Name
    string bump_,     // 1-B
           customer_, // 2-C Customer Name
           ball_ID_;  // 3-D

    string fullchipName_; // 13-N  Fullchip_NAME

    int row_ = 0;
    ModeDir dir_ = No_dir;

    string  IO_tile_pin_; // column I
    XYZ     xyz_;         // columns J,K,L

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

    const string& customerInternal() const noexcept { return customerInternal_; }
    bool isCustomerInternal() const noexcept { return !customerInternal_.empty(); }
    void setCustomerInternal(const string& nm) noexcept { customerInternal_ = nm; }

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

  bool read_csv(const string& fn, bool check);

  bool write_csv(const string& fn, uint minRow, uint maxRow) const;

  void print_csv() const;

  uint print_bcd_stats(std::ostream& os) const noexcept;
  uint print_bcd(std::ostream& os) const noexcept;
  uint print_axi_bcd(std::ostream& os) const noexcept;

  bool sanity_check() const;

  static bool prepare_mode_header(string& hdr) noexcept;

  // data query
  XYZ get_pin_xyz_by_name(const string& mode,
                          const string& customerPin_or_ID,
                          const string& gbox_pin_name, uint& pt_row) const noexcept;

  XYZ get_axi_xyz_by_name(const string& axi_name, uint& pt_row) const noexcept;

  uint numRows() const noexcept { return bcd_.size(); }

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
    return bcd_[row].customerInternal();
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

  uint row0_GBOX_GPIO() const noexcept { return start_GBOX_GPIO_row_; }
  uint row0_CustomerInternal() const noexcept { return start_CustomerInternal_row_; }

  static const char* str_Mode_dir(BCD::ModeDir t) noexcept;

private:

  fio::CSV_Reader* crd_ = nullptr;

  std::map<string, vector<string>> modes_map_;

  vector<string> col_headers_; // all column headers
  vector<string> mode_names_;  // column headers that contain "Mode_/MODE_"

  vector<BCD> bcd_; // all BCD records, indexed by csv row

  vector<BCD*> bcd_AXI_; // BCD records with .isCustomerInternalOnly() predicate (AXI pins)

  vector<BCD*> bcd_GBGPIO_;  // BCD records with .is_GBOX_GPIO_ predicate

  uint start_GBOX_GPIO_row_ = 0;   // "GBOX GPIO" group start row in PT

  uint start_CustomerInternal_row_ = 0;

  friend class PinPlacer;
};

std::ostream& operator<<(std::ostream& os, const RapidCsvReader::BCD& b);

}  // namespace pinc
