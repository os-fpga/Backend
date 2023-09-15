#pragma once

#include <map>
#include <set>
#include <bitset>

#include "util/geo/xyz.h"
#include "util/pinc_log.h"

namespace fio {
class CSV_Reader;
}

namespace pinc {

using std::string;
using std::vector;

class PinPlacer;
struct Pin;

class RapidCsvReader {
public:

  static constexpr uint MAX_PT_COLS = 128;

  // BCD is a "reduced row record" RRR (subset of important columns)
  struct BCD {

    const RapidCsvReader& reader_;
    Pin* ann_pin_ = nullptr; // annotated Pin

    string customerInternal_; // 72-BU  Customer Internal Name

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

    string col_M_;        // 12-M  EFPGA_PIN
    string fullchipName_; // 13-N  Fullchip_NAME

    uint row_ = 0;
    ModeDir rxtx_dir_ = No_dir; // dir_
    ModeDir colM_dir_ = No_dir; // based on column-M EFPGA_PIN

    string  IO_tile_pin_; // column I
    XYZ     xyz_;         // columns J,K,L

    std::bitset<MAX_PT_COLS> modes_; // indexed by PT-columns, 1 is 'Y'

    bool is_axi_ = false;
    bool is_GBOX_GPIO_ = false;
    bool is_GPIO_ = false; // based on column A

    bool used_ = false; // pin_c already assigned this XYZ

    BCD(const RapidCsvReader& rdr, uint ro = 0) noexcept
      : reader_(rdr), row_(ro) { modes_.reset(); }

    const XY& xy() const noexcept { return xyz_; }

    void set_used() noexcept { used_ = true; }

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

    bool isInputRxTx() const noexcept { return rxtx_dir_ == Input_dir; }
    bool isOutputRxTx() const noexcept { return rxtx_dir_ == Output_dir; }
    bool isBidiRxTx() const noexcept { return rxtx_dir_ == HasBoth_dir or rxtx_dir_ == AllEnabled_dir; }
    bool isNotBidiRxTx() const noexcept { return rxtx_dir_ != HasBoth_dir and rxtx_dir_ != AllEnabled_dir; }
    bool allModesEnabledRxTx() const noexcept { return rxtx_dir_ == AllEnabled_dir; }

    std::bitset<MAX_PT_COLS> getRxModes() const noexcept;
    std::bitset<MAX_PT_COLS> getTxModes() const noexcept;
    std::bitset<MAX_PT_COLS> getGpioModes() const noexcept;

    uint numModes() const noexcept { return modes_.count(); }
    uint numRxModes() const noexcept { return getRxModes().count(); }
    uint numTxModes() const noexcept { return getTxModes().count(); }
    uint numGpioModes() const noexcept { return getGpioModes().count(); }

    bool isA2F() const noexcept { return colM_dir_ == Input_dir; }
    bool isF2A() const noexcept { return colM_dir_ == Output_dir; }

    inline bool isInput() const noexcept;
    inline const char* str_colM_dir() const noexcept;

    Pin* annotatePin(const string& udes_pn, const string& device_pn,
                     bool is_usr_inp) noexcept;

    void dump() const;
  }; // BCD

  struct Tile {

    // (loc_, colB_) - determines Tile uniqueness
    XY loc_;
    string colB_; // column B: Bump/Pin Name

    uint beg_row_ = 0;    // row at which this Tile is 1st encountered
    uint id_ = UINT_MAX;  // index in tiles_
    uint num_used_ = 0;
    vector<BCD*> a2f_sites_;
    vector<BCD*> f2a_sites_;

    Tile(XY loc, const string& colb, uint beg_r) noexcept
      : loc_(loc), colB_(colb), beg_row_(beg_r)
    {}

    bool operator==(const Tile& t) const noexcept { return loc_ == t.loc_ && colB_ == t.colB_; }
    bool operator!=(const Tile& t) const noexcept { return not operator==(t); }
    bool eq(const XY& loc, const string& colb) const noexcept { return loc_ == loc && colB_ == colb; }
    bool eq(const BCD& bcd) const noexcept { return eq(bcd.xyz_, bcd.bump_); }

    BCD* bestInputSite() noexcept;
    BCD* bestOutputSite() noexcept;

    void dump() const;
  }; // Tile

  RapidCsvReader();
  ~RapidCsvReader();

  void reset() noexcept;

  bool read_csv(const string& fn, bool check);

  bool write_csv(const string& fn, uint minRow, uint maxRow) const;

  void print_csv() const;

  uint countBidiRows() const noexcept;

  uint print_bcd_stats(std::ostream& os) const noexcept;
  uint print_bcd(std::ostream& os) const noexcept;
  uint print_axi_bcd(std::ostream& os) const noexcept;

  // data query
  XYZ get_pin_xyz_by_name(const string& mode,
                          const string& customerPin_or_ID,
                          const string& gbox_pin_name, uint& pt_row) const noexcept;

  XYZ get_axi_xyz_by_name(const string& axi_name, uint& pt_row) const noexcept;

  uint numRows() const noexcept {
    assert(col_headers_.size() == col_headers_lc_.size());
    return bcd_.size();
  }
  uint numCols() const noexcept {
    assert(col_headers_.size() == col_headers_lc_.size());
    return col_headers_.size();
  }

  bool has_io_pin(const string& pin_name_or_ID) const noexcept;

  bool hasCustomerInternalName(const string& nm) const noexcept;

  const string& bumpPinName(uint row) const noexcept {
    assert(row < bcd_.size());
    return bcd_[row]->bump_;
  }

  const string& customerPinName(uint row) const noexcept {
    assert(row < bcd_.size());
    return bcd_[row]->customer_;
  }
  const string& customerInternalName(uint row) const noexcept {
    assert(row < bcd_.size());
    return bcd_[row]->customerInternal();
  }

  const BCD& getBCD(uint row) const noexcept {
    assert(row < numRows());
    assert(bcd_[row]->row_ == row);
    return *bcd_[row];
  }
  BCD& getBCD(uint row) noexcept {
    assert(row < numRows());
    assert(bcd_[row]->row_ == row);
    return *bcd_[row];
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

  Tile* getUnusedTile(bool input_dir) noexcept;

  bool isRxCol(uint col) const noexcept {
    assert(col < numCols());
    return rx_cols_[col];
  }
  bool isTxCol(uint col) const noexcept {
    assert(col < numCols());
    return tx_cols_[col];
  }
  bool isGpioCol(uint col) const noexcept {
    assert(col < numCols());
    return gpio_cols_[col];
  }

  static const char* str_Mode_dir(BCD::ModeDir t) noexcept;

private:

  bool initCols(const fio::CSV_Reader& crd);
  bool initRows(const fio::CSV_Reader& crd);
  bool setDirections(const fio::CSV_Reader& crd);
  bool createTiles();

  static bool prepare_mode_header(string& hdr) noexcept;

private:

  fio::CSV_Reader* crd_ = nullptr;

  std::map<string, vector<string>> modes_map_; // mode name --> column of strings

  vector<string> col_headers_;    // Original column headers
  vector<string> col_headers_lc_; // Lower-Case column headers
  vector<string> mode_names_;     // column headers that contain "Mode_/MODE_"
  std::bitset<MAX_PT_COLS> rx_cols_;    // which columns are "Mode_..._RX"
  std::bitset<MAX_PT_COLS> tx_cols_;    // which columns are "Mode_..._TX"
  std::bitset<MAX_PT_COLS> gpio_cols_;  // which columns are "Mode_..._GPIO"

  vector<BCD*> bcd_;         // all BCD records, indexed by csv row

  vector<BCD*> bcd_AXI_;     // BCD records with .isCustomerInternalOnly() predicate (AXI pins)

  vector<BCD*> bcd_GBGPIO_;  // BCD records with .is_GBOX_GPIO_ predicate

  vector<BCD*> bcd_XY_;      // BCDs with valid non-negative XYs

  //vector<BCD*>  bcd_inp_Q_;   // Q of available input BCDs
  //vector<BCD*>  bcd_out_Q_;   // Q of available output BCDs

  // XY-tiles (i.e. XY-groups)
  int max_x_ = 0, max_y_ = 0;
  vector<Tile> tiles_;        // XY translates to tile_index

  uint start_GBOX_GPIO_row_ = 0;   // "GBOX GPIO" group start-row in PT

  uint start_CustomerInternal_row_ = 0;

  uint start_MODE_col_ = 0;   // first column titled "Mode_/MODE_"

  friend class PinPlacer;
};

std::ostream& operator<<(std::ostream& os, const RapidCsvReader::BCD& b);
std::ostream& operator<<(std::ostream& os, const RapidCsvReader::Tile& t);

inline bool RapidCsvReader::BCD::isInput() const noexcept {
  if (isA2F())
    return true;
  if (isF2A())
    return false;
  return isInputRxTx();
}

inline const char* RapidCsvReader::BCD::str_colM_dir() const noexcept {
  return RapidCsvReader::str_Mode_dir(colM_dir_);
}

}  // namespace pinc
