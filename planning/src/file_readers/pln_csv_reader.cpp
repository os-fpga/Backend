#include "file_readers/pln_csv_reader.h"
#include "file_readers/pln_Fio.h"
#include "pin_loc/pin.h"

#include <iomanip>

namespace pln {

using namespace std;
using Tile_p = PcCsvReader::Tile*;
using BCD_p = PcCsvReader::BCD*;

static constexpr uint MAX_COLMS = PcCsvReader::MAX_PT_COLS;

PcCsvReader::PcCsvReader() {}

PcCsvReader::~PcCsvReader() { delete crd_; }

void PcCsvReader::reset() noexcept {
  start_GBOX_GPIO_row_ = 0;
  start_CustomerInternal_row_ = 0;
  start_MODE_col_ = 0;
  bcd_AXI_.clear();
  bcd_GBGPIO_.clear();
  bcd_.clear();
  col_headers_.clear();
  col_headers_lc_.clear();
  mode_names_.clear();
  rx_cols_.reset();
  tx_cols_.reset();
  gpio_cols_.reset();
  fullchipNames_.clear();

  bcd_good_.clear();

  tilePool_[0].clear();
  tilePool_[1].clear();
  tiles2_[0].clear();
  tiles2_[1].clear();
  //tiles_.clear();

  delete crd_;
  crd_ = nullptr;
}

static inline bool starts_with_mode(const char* z) noexcept {
  assert(z);
  return z[0] == 'm' and z[1] == 'o' and z[2] == 'd' and z[3] == 'e' and
         z[4] == '_';
}

static inline bool ends_with_tx_rx(const char* z, size_t len) noexcept {
  assert(z);
  if (len < 4) return false;
  return z[len - 1] == 'x' and (z[len - 2] == 't' or z[len - 2] == 'r') and
         z[len - 3] == '_';
}

bool PcCsvReader::ends_with_rx(const char* z, size_t len) noexcept {
  assert(z);
  if (len < 4) return false;
  return z[len - 1] == 'x' and z[len - 2] == 'r' and z[len - 3] == '_';
}

bool PcCsvReader::ends_with_tx(const char* z, size_t len) noexcept {
  assert(z);
  if (len < 4) return false;
  return z[len - 1] == 'x' and z[len - 2] == 't' and z[len - 3] == '_';
}

static inline bool ends_with_gpio(const char* z, size_t len) noexcept {
  assert(z);
  if (len < 5) return false;
  return z[len - 1] == 'o' and z[len - 2] == 'i' and z[len - 3] == 'p' and
         z[len - 4] == 'g' and z[len - 5] == '_';
  // _gpio
  // oipg_
}

static inline bool starts_with_A2F(const char* z) noexcept {
  assert(z);
  if (!z[0])
    return false;
  assert(::strlen(z) >= 3);
  return z[0] == 'A' and z[1] == '2' and z[2] == 'F';
}

static inline bool starts_with_F2A(const char* z) noexcept {
  assert(z);
  if (!z[0])
    return false;
  assert(::strlen(z) >= 3);
  return z[0] == 'F' and z[1] == '2' and z[2] == 'A';
}

std::bitset<MAX_COLMS> PcCsvReader::BCD::getRxModes() const noexcept {
  std::bitset<MAX_COLMS> result{0U};
  if (modes_.none())
    return result;
  uint nc = reader_.numCols();
  assert(nc > 2);
  assert(reader_.start_MODE_col_ > 1);
  for (uint col = reader_.start_MODE_col_; col < nc; col++) {
    const string& hdr = reader_.col_headers_lc_[col];
    if (modes_[col] and ends_with_rx(hdr.c_str(), hdr.length()))
      result.set(col, true);
  }
  return result;
}

std::bitset<MAX_COLMS> PcCsvReader::BCD::getTxModes() const noexcept {
  std::bitset<MAX_COLMS> result{0U};
  if (modes_.none())
    return result;
  uint nc = reader_.numCols();
  assert(nc > 2);
  assert(reader_.start_MODE_col_ > 1);
  for (uint col = reader_.start_MODE_col_; col < nc; col++) {
    const string& hdr = reader_.col_headers_lc_[col];
    if (modes_[col] and ends_with_tx(hdr.c_str(), hdr.length()))
      result.set(col, true);
  }
  return result;
}

std::bitset<MAX_COLMS> PcCsvReader::BCD::getGpioModes() const noexcept {
  std::bitset<MAX_COLMS> result{0U};
  if (modes_.none())
    return result;
  uint nc = reader_.numCols();
  assert(nc > 2);
  assert(reader_.start_MODE_col_ > 1);
  for (uint col = reader_.start_MODE_col_; col < nc; col++) {
    if (modes_[col] and reader_.isGpioCol(col))
      result.set(col, true);
  }
  return result;
}

Pin* PcCsvReader::BCD::annotatePin(const string& udes_pn,
                                      const string& device_pn,
                                      bool is_usr_inp) noexcept {
  if (ann_pin_)
    ann_pin_->reset();
  else
    ann_pin_ = new Pin;

  ann_pin_->udes_pin_name_ = udes_pn;
  ann_pin_->device_pin_name_ = device_pn;

  ann_pin_->xyz_ = xyz_;

  ann_pin_->pt_row_ = row_;

  ann_pin_->all_modes_ = modes_;

  ann_pin_->rx_modes_ = getRxModes();
  ann_pin_->tx_modes_ = getTxModes();

  ann_pin_->is_usr_input_ = is_usr_inp;
  ann_pin_->is_dev_input_ = isInput();
  ann_pin_->is_a2f_ = isA2F();
  ann_pin_->is_f2a_ = isF2A();

  return ann_pin_;
}

const char* PcCsvReader::str_Mode_dir(BCD::ModeDir t) noexcept {
  // enum ModeDir
  // {  No_dir,   Input_dir,   Output_dir,   HasBoth_dir,   AllEnabled_dir  };
  static const char* enumS[] =
     { "No_dir", "Input_dir", "Output_dir", "HasBoth_dir", "AllEnabled_dir" };
  constexpr size_t n = sizeof(enumS) / sizeof(enumS[0]);
  static_assert(n == BCD::AllEnabled_dir + 1);
  uint i = uint(t);
  assert(i < n);
  return enumS[i];
}

std::ostream& operator<<(std::ostream& os, const PcCsvReader::BCD& b) {
  os << "(bcd-" << b.row_ << ' '
     << "  grp:" << b.groupA_ << "  " << b.bump_B_ << "  " << b.customer_ << "  "
     << b.ball_ID_ << "  ITP: " << b.IO_tile_pin_ << "  XYZ: " << b.xyz_
     << "  colM:" << b.col_M_ << "  fc:" << b.fullchipName_
     << "  ci:" << b.customerInternal() << "  axi:" << int(b.is_axi_)
     << "  isGPIO:" << int(b.is_GPIO_) << "  isGB_GPIO:" << int(b.is_GBOX_GPIO_)
     << "  rxtx_dir:" << PcCsvReader::str_Mode_dir(b.rxtx_dir_)
     << "  colM_dir:" << b.str_colM_dir()
     << "  is_input:" << int(b.isInput())
     << "  #modes:" << b.numModes()
     //<< "  #modes:" << b.modes_
     << "  #rx_modes:" << b.numRxModes()
     << "  #tx_modes:" << b.numTxModes()
     << "  row:" << b.row_ << ')';
  return os;
}
void PcCsvReader::BCD::dump() const { lout() << *this << endl; }

std::ostream& operator<<(std::ostream& os, const PcCsvReader::Tile& t) {
  os << "(tile-" << t.id_ << ' '
     << "  loc: " << t.loc_
     << "  colA: " << t.colA_
     << "  colB: " << t.colB_
     << "  beg_row:" << t.beg_row_
     << "  #used=" << t.num_used_
     << "  #a2f=" << t.a2f_sites_.size()
     << "  #f2a=" << t.f2a_sites_.size()
     << "  #modes=" << t.countModes()
     << ')';
  return os;
}
void PcCsvReader::Tile::dump() const { lout() << *this << endl; }

string PcCsvReader::Tile::key1() const noexcept {
  string k = loc_.toString();
  k.push_back(' ');
  k += colB_;
  return k;
}
string PcCsvReader::Tile::key2() const noexcept {
  string k = key1();
  k += " #";
  k += std::to_string(id_);
  return k;
}

// returns spreadsheet column label ("A", "B", "BC", etc) for column index 'i'
string PcCsvReader::label_of_column(int i) noexcept {
  assert(i >= 0 && i < 1000);
  string label;

  if (i < 26) {
    label.push_back('A' + i);
    return label;
  }

  int j = i - 26;
  if (j >= 26) {
    j -= 26;
    label.push_back('B');
  } else {
    label.push_back('A');
  }
  label.push_back('A' + j);

  return label;
}

static bool get_column(const fio::CSV_Reader& crd, const string& colName,
                       vector<string>& V) noexcept {
  V = crd.getColumn(colName);
  if (V.empty()) {
    lout() << "\nERROR reading csv: failed column: " << colName << endl;
    return false;
  }
  return true;
}

struct RX_TX_val {
  string hdr_;
  string val_;
  RX_TX_val() noexcept = default;
  RX_TX_val(const string& h, const string& v) noexcept : hdr_(h), val_(v) {}

  bool enabled() const noexcept { return val_ == "Y"; }
  bool is_rx() const noexcept {
    return PcCsvReader::ends_with_rx(hdr_.c_str(), hdr_.length());
  }
};

std::ostream& operator<<(std::ostream& os, const RX_TX_val& v) {
  os << '(' << v.hdr_ << " : " << v.val_ << ')';
  return os;
}

static bool get_row_modes(const fio::CSV_Reader& crd, uint rowNum,
                          vector<RX_TX_val>& V) noexcept {
  V.clear();
  assert(rowNum < crd.numRows());
  uint nc = crd.numCols();
  assert(nc > 2);
  const vector<string>& H = crd.lowHeader_;
  assert(H.size() == nc);

  const string* R = crd.getRow(rowNum);
  if (!R) {
    flush_out(true);
    err_puts();
    lprintf2("[Error] pin_c: ERROR reading csv: failed row: %u\n", rowNum+2);
    flush_out(true);
    return false;
  }

  string s;
  for (uint c = 1; c < nc; c++) {
    size_t len = H[c].length();
    if (len < 7)  // mode_ is at least 5 chars, plus _rx/_tx
      continue;
    const char* hs = H[c].c_str();
    if (not starts_with_mode(hs)) continue;
    if (not ends_with_tx_rx(hs, len)) continue;
    V.emplace_back(H[c], R[c]);
  }

  if (ltrace() >= 8) {
    lout() << "ROW-" << rowNum+2;
    for (uint c = 0; c < nc; c++) lout() << " | " << R[c];
    lputs(" |");
  }

  return true;
}

// if 'hdr' starts with "Mode_"/"MODE_" then convert
// the prefix to upper case and return true.
bool PcCsvReader::prepare_mode_header(string& hdr) noexcept {
  if (hdr.size() < 6) return false;
  const char* cs = hdr.c_str();
  char p[8] = {};
  for (int i = 0; i < 5; i++) {
    p[i] = tolower(cs[i]);
  }
  if (p[0] == 'm' && p[1] == 'o' && p[2] == 'd' && p[3] == 'e' && p[4] == '_') {
    hdr[0] = 'M';
    hdr[1] = 'O';
    hdr[2] = 'D';
    hdr[3] = 'E';
    return true;
  }
  return false;
}

bool PcCsvReader::initCols(const fio::CSV_Reader& crd) {
  uint16_t tr = ltrace();
  auto& ls = lout();

  rx_cols_.reset();
  tx_cols_.reset();
  gpio_cols_.reset();

  col_headers_ = crd.header_;
  assert(col_headers_.size() > 2);
  if (col_headers_.size() <= 2)
    return false;

  col_headers_lc_ = col_headers_;
  for (auto& h : col_headers_lc_)
    h = str::s2lower(h);

  uint nc = numCols();

  if (tr >= 4) {
    ls << "  col_headers_.size()= " << nc << "  ["
       << col_headers_.front() << " ... " << col_headers_.back() << ']' << endl;
    if (tr >= 5) {
      for (uint i = 0; i < nc; i++) {
        string col_label = label_of_column(i);
        const string& hdr_i = col_headers_[i];
        ls << "--- " << i << '-' << col_label << "  hdr_i= " << hdr_i << endl;
      }
      ls << endl;
    }
  }

  for (uint col = 1; col < nc; col++) {
    const string& hdr = col_headers_lc_[col];
    if (not starts_with_mode(hdr.c_str()))
      continue;
    size_t len = hdr.length();
    const char* h = hdr.c_str();
    if (ends_with_rx(h, len))
      rx_cols_.set(col, true);
    else if (ends_with_tx(h, len))
      tx_cols_.set(col, true);
    else if (ends_with_gpio(h, len))
      gpio_cols_.set(col, true);
  }

  if (tr >= 3) {
    ls << "  #RX_cols= " << rx_cols_.count()
       << "  #TX_cols= " << tx_cols_.count()
       << "  #GPIO_cols= " << gpio_cols_.count() << '\n' << endl;
  }

  return true;
}

bool PcCsvReader::initRows(const fio::CSV_Reader& crd) {
  start_GBOX_GPIO_row_ = 0;
  vector<string> group_col = crd.getColumn("Group");
  assert(!group_col.empty());
  if (group_col.empty())
    return false;

  size_t num_rows = group_col.size();
  assert(num_rows >= 10);
  if (num_rows < 10)
    return false;

  for (uint i = 0; i < num_rows; i++) {
    if (group_col[i] == "GBOX GPIO") {
      start_GBOX_GPIO_row_ = i;
      break;
    }
  }

  bcd_.resize(num_rows, nullptr);
  for (uint i = 0; i < num_rows; i++) {
    bcd_[i] = new BCD(*this, i);
    BCD& bcd = *bcd_[i];
    bcd.groupA_ = group_col[i];
    bcd.is_GBOX_GPIO_ = (bcd.groupA_ == "GBOX GPIO");
    bcd.is_GPIO_ = (bcd.groupA_ == "GPIO");
  }

  assert(num_rows == numRows());
  assert(col_headers_.size() == numCols());

  if (ltrace() >= 2) {
    lprintf("initRows:  num_rows= %u  num_cols= %u  start_GBOX_GPIO_row_= %u\n\n",
        numRows(), numCols(), start_GBOX_GPIO_row_);
  }

  return true;
}

// classify row directions BCD:: rxtx_dir_, colM_dir_
bool PcCsvReader::setDirections(const fio::CSV_Reader& crd) {
  uint16_t tr = ltrace();
  uint num_rows = numRows();
  if (num_rows < 3) return false;

  vector<RX_TX_val> RX_TX_values;
  RX_TX_values.reserve(crd.numCols());

  // 1. set BCD::rxtx_dir_
  for (uint r = 0; r < num_rows; r++) {
    bcd_[r]->rxtx_dir_ = BCD::No_dir;
    bool ok = get_row_modes(crd, r, RX_TX_values);
    if (!ok) {
      lprintf("\n [Error] pin_c csv row %u\n\n", r);
      return false;
    }
    size_t n = RX_TX_values.size();
    assert(n > 1);
    uint num_Y_rx = 0, num_Y_tx = 0;
    for (size_t j = 0; j < n; j++) {
      const RX_TX_val& rtv = RX_TX_values[j];
      if (not rtv.enabled()) continue;
      if (rtv.is_rx())
        num_Y_rx++;
      else
        num_Y_tx++;
    }
    if (!num_Y_rx && !num_Y_tx) {
      bcd_[r]->rxtx_dir_ = BCD::No_dir;
      continue;
    }
    if (!num_Y_rx) {
      // device does not receive - outputs only
      bcd_[r]->rxtx_dir_ = BCD::Output_dir;
      continue;
    }
    if (!num_Y_tx) {
      // device does not transmit - inputs only
      bcd_[r]->rxtx_dir_ = BCD::Input_dir;
      continue;
    }
    assert(num_Y_rx && num_Y_tx);
    assert(num_Y_rx + num_Y_tx <= n);
    // there are both tranmitters and receivers on this row:
    bcd_[r]->rxtx_dir_ = BCD::HasBoth_dir;
    if (num_Y_rx + num_Y_tx == n) {
      // there are no blanks on this row - all modes enabled:
      bcd_[r]->rxtx_dir_ = BCD::AllEnabled_dir;
    }
  }

  uint inp_cnt = 0, out_cnt = 0;

  // 2. set BCD::colM_dir_
  for (uint r = 0; r < num_rows; r++) {
    assert(bcd_[r]);
    BCD& bcd = *bcd_[r];
    bcd.colM_dir_ = BCD::No_dir;
    const string& col_M = bcd.col_M_;
    if (col_M.empty())
      continue;
    const char* cm = col_M.c_str();
    if (tr >= 8)
      lprintf("  row#%u  col_M_  %s\n", r, cm);
    if (starts_with_A2F(cm)) {
      bcd.colM_dir_ = BCD::Input_dir;
      inp_cnt++;
    }
    else if (starts_with_F2A(cm)) {
      bcd.colM_dir_ = BCD::Output_dir;
      out_cnt++;
    }
  }

  return true;
}

bool PcCsvReader::createTiles(bool uniq_XY) {
  flush_out(false);
  uniq_XY = false;
  static uint cr_tiles_cnt = 0;
  cr_tiles_cnt++;
  uint16_t tr = ltrace();
  auto& ls = lout();

  max_x_ = 0;
  max_y_ = 0;
  uint16_t uni = uniq_XY;
  tilePool_[uni].clear();
  tiles2_[uni].clear();

  uint num_rows = numRows();
  if (tr >= 5) {
    lprintf("pin_c createTiles()# %u  uniq_XY:%i  num_rows= %u\n",
            cr_tiles_cnt, uniq_XY, num_rows);
  }
  assert(num_rows >= 3);
  if (num_rows < 3) return false;

  // 0. filter BCDs with valid non-negative XYZs AND with at least one mode
  //    (create bcd_good_)
  bcd_good_.clear();
  bcd_good_.reserve(num_rows);
  for (uint r = 0; r < num_rows; r++) {
    BCD* bcd = bcd_[r];
    assert(bcd);
    const XYZ& loc = bcd->xyz_;
    if (loc.nonNeg() && loc.z_ >= 0 && bcd->numModes()) {
      assert(loc.x_ >= 0);
      //assert(loc.y_ >= 0);
      if (loc.y_ < 0) {
        flush_out(true);
        err_puts();
        lprintf2("[Error] pin_c:  negative location coordinate Y: %i\n", loc.y_);
        flush_out(true);
        return false;
      }
      bcd_good_.push_back(bcd);
    }
  }

  uint sz_bcd_good = bcd_good_.size();
  if (tr >= 5)
    lprintf("createTiles:  num_rows= %u  num_good_ROWs= %u\n", num_rows, sz_bcd_good);
  if (sz_bcd_good < 2) {
    flush_out(true);
    err_puts();
    lprintf2("[Error] pin_c:  NO GOOD ROWs in .csv\n");
    flush_out(true);
    return false;
  }

  vector<Tile>& tPool = tilePool_[uni];
  tPool.reserve(sz_bcd_good + 1);

  // 1. determine first_valid_k, max_x_, max_y_
  int first_valid_k = -1;
  for (uint k = 0; k < sz_bcd_good; k++) {
    assert(bcd_good_[k]);
    const XY& loc = bcd_good_[k]->xyz_;
    auto colM_dir = bcd_good_[k]->colM_dir_;
    if (colM_dir != BCD::Input_dir && colM_dir != BCD::Output_dir)
      continue;
    assert(loc.valid());
    if (!loc.valid())
      continue;
    if (first_valid_k < 0)
      first_valid_k = k;
    if (loc.x_ > max_x_)
      max_x_ = loc.x_;
    if (loc.y_ > max_y_)
      max_y_ = loc.y_;
  }

  flush_out(false);

  if (tr >= 5) {
    lprintf("createTiles:  first_valid_k= %i  max_x_= %i  max_y_= %i\n",
            first_valid_k, max_x_, max_y_);
  }

  if (first_valid_k < 0)
    return false;

  // 2. add tiles_ avoiding duplicates if possible
  tPool.emplace_back(bcd_good_[first_valid_k]->xyz_,
                      bcd_good_[first_valid_k]->groupA_,
                      bcd_good_[first_valid_k]->bump_B_,
                      first_valid_k);
  for (uint k = first_valid_k + 1; k < sz_bcd_good; k++) {
    const BCD& bcd = *bcd_good_[k];
    const XY& loc = bcd.xyz_;
    if (tPool.back().eq(loc, bcd.bump_B_))
      continue;
    tPool.emplace_back(loc, bcd.groupA_, bcd.bump_B_, k);
  }

  uint sz = tPool.size();
  if (tr >= 6) {
    ls << "  sz= " << sz << endl;
  }
  if (sz < 2)
    return false;

  // 3. uniquefy tiles
  uint num_invalidated = 0;
  if (uniq_XY) {
    if (tr >= 4)
      lputs("createTiles: in uniq_XY mode, uniq_XY = TRUE");
    for (uint i = 0; i < sz; i++) {
      const Tile& ti = tPool[i];
      if (!ti.loc_.valid())
        continue;
      for (uint j = i + 1; j < sz; j++) {
        Tile& tj = tPool[j];
        if (!tj.loc_.valid())
          continue;
        if (ti.loc_ == tj.loc_) {
          tj.loc_.inval();
          num_invalidated++;
        }
      }
    }
  }
  else {
    if (tr >= 4)
      lputs("createTiles: in NON-uniq_XY mode, uniq_XY = FALSE");
    for (uint i = 0; i < sz; i++) {
      const Tile& ti = tPool[i];
      if (!ti.loc_.valid())
        continue;
      for (uint j = i + 1; j < sz; j++) {
        Tile& tj = tPool[j];
        if (!tj.loc_.valid())
          continue;
        if (ti == tj) {
          tj.loc_.inval();
          num_invalidated++;
        }
      }
    }
  }

  flush_out(false);

  if (tr >= 6) {
    ls << "  num_invalidated= " << num_invalidated << "  sz= " << sz << endl;
  }
  if (num_invalidated) {
    for (int i = int(sz) - 1; i >= 0; i--) {
      XY xy = tPool[i].loc_;
      if (!xy.valid())
        tPool.erase(tPool.begin() + i);
    }
  }

  sz = tPool.size();
  if (tr >= 5)
    lprintf("createTiles:  tPool.size()= %u\n", sz);

  // 4. index and populate tiles
  if (sz < 2)
    return false;
  for (uint i = 0; i < sz; i++) {
    Tile& ti = tPool[i];
    assert(ti.loc_.valid());
    ti.id_ = i;
    assert(ti.beg_row_ < sz_bcd_good);
    for (uint k = ti.beg_row_; k < sz_bcd_good; k++) {
      BCD* bcd = bcd_good_[k];
      if (not ti.eq(*bcd))
        continue;
      if (bcd->isA2F())
        ti.a2f_sites_.push_back(bcd);
      else if (bcd->isF2A())
        ti.f2a_sites_.push_back(bcd);
    }
  }

  flush_out(false);

  // 5. rewrite Tile::beg_row_ as real row instead of k
  for (uint i = 0; i < sz; i++) {
    Tile& ti = tPool[i];
    assert(ti.loc_.valid());
    assert(ti.loc_.x_ >= 0);
    assert(ti.loc_.y_ >= 0);
    assert(ti.beg_row_ < sz_bcd_good);
    ti.beg_row_ = bcd_good_[ti.beg_row_]->row_;
    assert(ti.beg_row_ < num_rows);
  }

  if (tr >= 6) {
    lprintf("pin_c csv-reader: dumping Unsorted Tiles (%u)  uni:%u\n", sz, uni);
    for (uint i = 0; i < sz; i++) {
      const Tile& ti = tPool[i];
      ls << ti << endl;
    }
    lputs("--------");
  }

  flush_out(false);

  // 6. sort tiles_ by Tile::Cmp
  assert(!tPool.empty());
  vector<Tile*>& tls = tiles2_[uni];
  tls.clear();
  tls.reserve(tPool.size());
  for (Tile& t : tPool)
    tls.push_back(&t);

  //std::sort(tls.begin(), tls.end(), Tile::Cmp());

  if (0&& tr >= 6) {
    lprintf("pin_c csv-reader: dumping Sorted Tiles (%u)  uni:%u\n", sz, uni);
    for (uint i = 0; i < sz; i++) {
      const Tile& ti = *tls[i];
      ls << ti << endl;
    }
    lputs("--------");
  }

  if (tr >= 4)
    lprintf("pin_c csv-reader: created Tiles:  number of tiles = %u   uni:%u\n", sz, uni);

  flush_out(false);
  return true;
}

Tile_p PcCsvReader::getUnusedTile(bool input_dir,
                           const std::unordered_set<uint>& except,
                           uint overlap_level) noexcept {
  assert(uni_XY_ == 0 or uni_XY_ == 1);
  uint16_t uni = uni_XY_;
  //assert(tiles2_[uni].size() == tilePool_[uni].size());
  //if (tiles2_[uni].empty())
  //  return nullptr;
  assert(uni == 0);
  assert(not tiles2_[uni].empty());

  Tile* result = nullptr;
  uint sz = tiles2_[uni].size();
  for (uint i = 0; i < sz; i++) {
    Tile& ti = *tiles2_[uni][i];
    assert(ti.loc_.valid());
    assert(ti.loc_.x_ >= 0);
    assert(ti.loc_.y_ >= 0);
    if (ti.num_used_ >= overlap_level)
      continue;
    if (except.count(i))
      continue;
    if (input_dir) {
      if (ti.a2f_sites_.size()) {
        result = &ti;
        break;
      }
    }
    else {
      if (ti.f2a_sites_.size()) {
        result = &ti;
        break;
      }
    }
  }

  return result;
}

BCD_p PcCsvReader::Tile::bestInputSite() noexcept {
  if (a2f_sites_.empty())
    return nullptr;

  // 1. try skipping "bidi" sites
  for (BCD* bcd : a2f_sites_) {
    assert(bcd->isA2F());
    if (bcd->used_)
      continue;
    if (bcd->numRxModes() > 0 && bcd->numTxModes() == 0)
      return bcd;
  }

  // 2. not skipping "bidi" sites
  for (BCD* bcd : a2f_sites_) {
    if (bcd->used_)
      continue;
    if (bcd->numRxModes() > 0)
      return bcd;
  }

  // 3. try MODE_GPIO
  for (BCD* bcd : a2f_sites_) {
    if (bcd->used_)
      continue;
    if (bcd->numGpioModes() > 0)
      return bcd;
  }

  return nullptr;
}

BCD_p PcCsvReader::Tile::bestOutputSite() noexcept {
  if (f2a_sites_.empty())
    return nullptr;

  // 1. try skipping "bidi" sites
  for (BCD* bcd : f2a_sites_) {
    assert(bcd->isF2A());
    if (bcd->used_)
      continue;
    if (bcd->numTxModes() > 0 && bcd->numRxModes() == 0)
      return bcd;
  }

  // 2. not skipping "bidi" sites
  for (BCD* bcd : f2a_sites_) {
    if (bcd->used_)
      continue;
    if (bcd->numTxModes() > 0)
      return bcd;
  }

  // 3. try MODE_GPIO
  for (BCD* bcd : f2a_sites_) {
    if (bcd->used_)
      continue;
    if (bcd->numGpioModes() > 0)
      return bcd;
  }

  return nullptr;
}

bool PcCsvReader::read_csv(const string& fn, uint num_udes_pins) {
  flush_out(false);
  uint16_t tr = ltrace();
  if (tr >= 3)
    lprintf("pin_c CsvReader::read_csv( %s )  num_udes_pins= %u\n", fn.c_str(), num_udes_pins);

  reset();

  crd_ = new fio::CSV_Reader(fn);
  fio::CSV_Reader& crd = *crd_;

  flush_out(false);

  // check file accessability and format:
  crd.setTrace(tr);
  if (!crd.fileExistsAccessible()) {
    flush_out(true);
    err_puts();
    lprintf2("[Error] pin_c: ERROR reading csv: file is not accessible: %s\n", fn.c_str());
    flush_out(true);
    delete crd_;
    crd_ = nullptr;
    return false;
  }
  if (!crd.readCsv(false)) {
    flush_out(true);
    lprintf2("[Error] pin_c: reading csv: %s\n", fn.c_str());
    flush_out(true);
    delete crd_;
    crd_ = nullptr;
    return false;
  }
  if (tr >= 5) crd.dprint1();

  if (!initCols(crd)) {
    flush_out(true);
    lprintf2("[Error] pin_c: initCols() failed\n");
    flush_out(true);
    return false;
  }

  if (!initRows(crd)) {
    flush_out(true);
    lprintf2("[Error] pin_c: initRows() failed\n");
    flush_out(true);
    return false;
  }
  flush_out(false);

  uint num_rows = numRows();
  assert(num_rows >= 10);

  bool has_Fullchip_name = false;
  vector<string> mode_data;
  string hdr_i;
  mode_names_.reserve(col_headers_.size());
  start_MODE_col_ = 0;
  for (uint col = 0; col < col_headers_.size(); col++) {
    string col_label = label_of_column(col);
    const string& orig_hdr_i = col_headers_[col]; // before case conversion
    hdr_i = orig_hdr_i;

    if (!has_Fullchip_name && hdr_i == "Fullchip_NAME")
      has_Fullchip_name = true;

    if (!prepare_mode_header(hdr_i))
      continue;

    if (!start_MODE_col_)
      start_MODE_col_ = col;

    if (tr >= 4)
      lprintf("  (ModeID) %u --- %s  hdr_i= %s\n",
              col, col_label.c_str(), hdr_i.c_str());

    mode_data = crd.getColumn(orig_hdr_i);
    if (mode_data.empty()) {
      flush_out(true);
      err_puts();
      lprintf2("[Error] pin_c: ERROR reading csv: failed column: %s\n", orig_hdr_i.c_str());
      flush_out(true);
      return false;
    }

    assert(mode_data.size() <= num_rows);
    if (mode_data.size() < num_rows) {
      if (tr >= 4)
        lputs("!!! mode_data.size() < num_rows");
      mode_data.resize(num_rows);
    }
    //// modes_map_.emplace(hdr_i, mode_data);
    mode_names_.emplace_back(hdr_i);

    // set 1 in bitsets for 'Y'
    for (uint row = 0; row < num_rows; row++) {
      if (mode_data[row] == "Y") {
        bcd_[row]->modes_.set(col, true);
      }
    }
  }

  flush_out(false);

  if (tr >= 5) {
    if (tr >= 6)
      flush_out(true);
    lout() << "  mode_names_.size()= " << mode_names_.size() << endl;
    if (tr >= 6) {
      logVec(mode_names_, "  mode_names_ ");
      flush_out(true);
    }
  }

  flush_out(false);

  if (!has_Fullchip_name) {
    flush_out(true);
    err_puts();
    lprintf2("  pin_c CsvReader::read_csv(): column Fullchip_NAME not found\n");
    err_puts();
    flush_out(true);
    return false;
  }

  vector<string> S_tmp;
  bool ok = false;

  S_tmp = crd.getColumn("EFPGA_PIN");
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) {
    bcd_[i]->col_M_ = std::move(S_tmp[i]);
  }

  flush_out(false);

  fullchipNames_.clear();
  S_tmp = crd.getColumn("Fullchip_NAME");
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  fullchipNames_.reserve(num_rows + 2);
  for (uint i = 0; i < num_rows; i++) {
    bcd_[i]->fullchipName_ = std::move(S_tmp[i]);
    if (not bcd_[i]->fullchipName_.empty())
      fullchipNames_.insert(bcd_[i]->fullchipName_);
  }

  flush_out(false);

  ok = get_column(crd, "Customer Name", S_tmp);
  if (!ok) return false;
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) {
    bcd_[i]->customer_ = std::move(S_tmp[i]);
  }

  flush_out(false);

  ok = get_column(crd, "Customer Internal Name", S_tmp);
  if (!ok) {
    flush_out(true);
    lputs("pin_c WARNING: could not read Customer Internal Name column");
    flush_out(true);
    return false;
  }

  flush_out(false);

  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  vector<uint> custIntNameRows;
  custIntNameRows.reserve(num_rows);
  for (uint i = 0; i < num_rows; i++) {
    const string& nm = S_tmp[i];
    if (nm.length()) {
      bcd_[i]->setCustomerInternal(nm);
      custIntNameRows.push_back(i);
    }
  }
  if (!custIntNameRows.empty()) {
    uint firs_ri = custIntNameRows.front();
    uint last_ri = custIntNameRows.back();
    uint firs_ri_uniq = firs_ri;
    for (uint i = firs_ri; i <= last_ri; i++) {
      if (bcd_[i]->isCustomerInternalUnique()) {
        firs_ri_uniq = i;
        break;
      }
    }
    uint firs_ri_only = firs_ri_uniq;
    for (uint i = firs_ri_uniq; i <= last_ri; i++) {
      if (bcd_[i]->isCustomerInternalOnly()) {
        firs_ri_only = i;
        break;
      }
    }
    if (firs_ri_only < last_ri &&
        bcd_[firs_ri_only]->isCustomerInternalOnly()) {
      start_CustomerInternal_row_ = firs_ri_only;
      bcd_AXI_.clear();
      bcd_AXI_.reserve(last_ri - firs_ri_only + 1);
      for (uint i = firs_ri_only; i <= last_ri; i++) {
        BCD& bcd = *bcd_[i];
        if (bcd.isCustomerInternalOnly()) {
          bcd.is_axi_ = true;
          bcd_AXI_.push_back(&bcd);
        }
      }
    }
    if (tr >= 4) {
      flush_out(false);
      const auto& firs_bcd = *bcd_[firs_ri];
      const auto& last_bcd = *bcd_[last_ri];
      auto& ls = lout();
      ls << " ==        first row with Customer Internal Name : "
         << (firs_ri + 2) << "  " << firs_bcd << endl;
      ls << " == first row with unique Customer Internal Name : "
         << (firs_ri_uniq + 2) << "  " << *bcd_[firs_ri_uniq] << endl;
      ls << " ==   first row with only Customer Internal Name : "
         << (firs_ri_only + 2) << "  " << *bcd_[firs_ri_only] << endl;
      ls << " ==         last row with Customer Internal Name : "
         << (last_ri + 2) << "  " << last_bcd << endl;
    }
  }

  flush_out(false);

  bcd_GBGPIO_.reserve(bcd_.size());
  for (BCD* bcd : bcd_) {
    if (bcd->is_GBOX_GPIO_) bcd_GBGPIO_.push_back(bcd);
  }

  if (tr >= 4) {
    auto& ls = lout();
    ls << "  num_rows= " << num_rows << "  num_cols= " << col_headers_.size()
       << "  start_GBOX_GPIO_row_= " << start_GBOX_GPIO_row_
       << "  start_CustomerInternal_row_= " << start_CustomerInternal_row_
       << "  start_MODE_col_= " << start_MODE_col_
       << '\n'
       << "  bcd_GBGPIO_.size()= " << bcd_GBGPIO_.size()
       << "  bcd_AXI_.size()= " << bcd_AXI_.size() << '\n'
       << endl;
  }

  ok = get_column(crd, "Ball ID", S_tmp);
  if (!ok) return false;
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) bcd_[i]->ball_ID_ = S_tmp[i];

  flush_out(false);

  ok = get_column(crd, "Bump/Pin Name", S_tmp);
  if (!ok) return false;
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  const vector<string>& bump_pin_name = S_tmp;

  if (tr >= 9) {
    auto& ls = lout();
    flush_out(true);
    if (num_rows > 3000) {
      const string* A = bump_pin_name.data();
      logArray(A, 80u, "  bump_pin_name ");
      ls << " ..." << endl;
      logArray(A + 202u, 100u, "    ");
      ls << " ..." << endl;
      logArray(A + 303u, 100u, "    ");
      ls << " ..." << endl;
      logArray(A + 707u, 100u, "    ");
      ls << " ... @ 909" << endl;
      logArray(A + 909u, 100u, "    ");
      ls << " ... @ 1200" << endl;
      logArray(A + 1200u, 100u, "    ");
    } else {
      logVec(bump_pin_name, "  bump_pin_name ");
    }
    ls << "num_rows= " << num_rows << '\n' << endl;
  }

  flush_out(false);

  for (uint i = 0; i < num_rows; i++) {
    BCD& bcd = *bcd_[i];
    bcd.bump_B_ = bump_pin_name[i];
    if (bcd.bump_B_.empty()) {
      if (bcd.customerInternal().empty() && tr >= 4) {
        lprintf(" (WW) both bcd.bump_B_ and bcd.customerInternal_ are empty on row# %u\n", i);
        // assert(0);
      }
    }
    bcd.normalize();
    // assert(!bcd.bump_B_.empty()); // no-assert, could be clock: colM = F2CLK
  }

  flush_out(false);

  vector<string> io_tile_pins = crd.getColumn("IO_tile_pin");
  io_tile_pins.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) {
    bcd_[i]->IO_tile_pin_ = io_tile_pins[i];
  }

  // if (tr >= 6)
  //   print_bcd(ls);
  // if (tr >= 5)
  //   print_axi_bcd(ls);

  vector<int> tmp = crd.getColumnInt("IO_tile_pin_x");
  assert(tmp.size() == num_rows);
  for (uint i = 0; i < num_rows; i++) {
    int x = tmp[i];
    assert(x >= -1);
    assert(x < 2000);
    bcd_[i]->xyz_.x_ = x;
  }

  tmp = crd.getColumnInt("IO_tile_pin_y");
  assert(tmp.size() == num_rows);
  for (uint i = 0; i < num_rows; i++) {
    int y = tmp[i];
    assert(y >= -1);
    assert(y < 2000);
    bcd_[i]->xyz_.y_ = y;
  }

  tmp = crd.getColumnInt("IO_tile_pin_z");
  tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) {
    int z = tmp[i];
    assert(z >= -1);
    assert(z < 100);
    bcd_[i]->xyz_.z_ = z;
  }

  flush_out(false);

  bool status_ok = setDirections(crd);
  if (not status_ok) {
    flush_out(true);
    err_puts();
    lprintf2("  [Error] directions are not OK\n");
    err_puts();
    flush_out(true);
    return false;
  }

  flush_out(false);

  if (tr >= 7) {
    lputs("___ original ROW-RECORD order ___");
    print_bcd(lout());
  }

  status_ok = createTiles(false); // non-uniq XY
  if (not status_ok) {
    flush_out(true);
    err_puts();
    lprintf2("[Error] pin_c csv-reader: createTiles(false) status not OK");
    flush_out(true);
    return false;
  }
  //status_ok = createTiles(true); // uniq XY
  //if (not status_ok) {
  //  ls << '\n' << "[Warning] pin_c csv-reader: createTiles(true) status not OK" << endl;
  //  cerr << "[Warning] pin_c csv-reader: createTiles(true) status not OK" << endl;
  //}
  //uni_XY_ = 0;
  //if (status_ok and num_udes_pins < tiles2_[1].size() / 2) {
  //  uni_XY_ = 1;
  //  if (tr >= 3)
  //    lprintf("pin_c: using XY-unique tiles (%zu)\n", tiles2_[1].size());
  //}

  uint num_bidi = countBidiRows();
  if (tr >= 4) lprintf("num_bidi= %u\n", num_bidi);
  bool no_reorder = true;  //::getenv("pinc_dont_reorder_bcd");
  if (!no_reorder) {
    if (num_bidi) {
      if (tr >= 4) {
        lputs("re-ordering bcd...");
        lprintf("..moving bidi rows (%u) to the back of bcd_\n", num_bidi);
      }
      std::stable_partition(bcd_.begin(), bcd_.end(),
                            [](BCD* p) { return p->isInputRxTx(); });
      std::stable_partition(bcd_.begin(), bcd_.end(),
                            [](BCD* p) { return p->isNotBidiRxTx(); });
      if (tr >= 6) {
        lputs("___ new ROW-RECORD order ___");
        print_bcd(lout());
      }
      flush_out(true);
    }
  }

  bool always_print = ::getenv("pinc_always_print_csv");
  bool write_debug = (tr >= 8 or ::getenv("pinc_write_debug_csv"));
  bool copy_to_CWD = always_print or write_debug;
  if (!copy_to_CWD)
    copy_to_CWD = ::getenv("pinc_copy_input");

  if (tr >= 6) {
    flush_out(false);
    print_bcd_stats(lout());
    flush_out(true);
    print_axi_bcd(lout());
    flush_out(false);
    if (tr >= 7 or always_print) {
      print_csv();
    }
  } else if (always_print) {
      print_csv();
  }

  if (write_debug)
    write_debug_csv();

  if (copy_to_CWD) {
    string cwd = get_CWD();
    string bn = fio::Info::get_basename(fn);
    string cn = str::concat("cp_", bn); 

    if (tr >= 4) {
      flush_out(true);
      lprintf("read_csv: copy_to_CWD in work_dir = %s\n", cwd.c_str());
      lprintf("  input csv: %s\n", fn.c_str());
      lprintf("   basename: %s\n", bn.c_str());
      lprintf("   copyname: %s\n", cn.c_str());
      flush_out(true);
    }

    int64_t wr_status = crd.writeFile(cn);
    if (wr_status > 0) {
      if(tr >= 4)
        lprintf("read_csv: copy_to_CWD OK  wr_status= %zu\n", size_t(wr_status));
    } else {
      if(tr >= 3)
        lprintf("read_csv: copy_to_CWD FAILED\n");
    }
    flush_out(true);
  }

  flush_out(false);
  return true;
}

bool PcCsvReader::write_csv(const string& fn, uint minRow,
                               uint maxRow) const {
  flush_out(false);
  uint16_t tr = ltrace();
  if (tr >= 2) {
    lprintf("PcCsvReader::write_csv( %s )  minRow= %u  maxRow= %u\n",
            fn.c_str(), minRow, maxRow);
  }

  if (fn.empty()) return false;
  if (!crd_) return false;

  if (maxRow == 0) maxRow = UINT_MAX;

  bool ok = crd_->writeCsv(fn, minRow, maxRow);
  if (tr >= 2)
    lprintf("done PcCsvReader::write_csv( %s )  ok:%i\n", fn.c_str(), ok);

  return ok;
}

uint PcCsvReader::countBidiRows() const noexcept {
  uint nr = numRows();
  uint cnt = 0;
  for (uint i = 0; i < nr; i++) {
    const BCD& bcd = *bcd_[i];
    if (bcd.isBidiRxTx()) cnt++;
  }
  return cnt;
}

uint PcCsvReader::print_bcd_stats(std::ostream& os) const noexcept {
  flush_out(false);
  uint nr = numRows();
  if (!nr) return 0;

  constexpr uint N_dirs = BCD::AllEnabled_dir + 1;
  uint dir_counters[N_dirs] = {};
  uint axi_cnt = 0, gbox_gpio_cnt = 0, gpio_cnt = 0;
  uint inp_colm_cnt = 0, out_colm_cnt = 0;

  for (uint i = 0; i < nr; i++) {
    const BCD& bcd = *bcd_[i];
    dir_counters[bcd.rxtx_dir_]++;
    axi_cnt += int(bcd.is_axi_);
    gbox_gpio_cnt += int(bcd.is_GBOX_GPIO_);
    gpio_cnt += int(bcd.is_GPIO_);
    inp_colm_cnt += int(bcd.isA2F());
    out_colm_cnt += int(bcd.isF2A());
  }

  os << "ROW-RECORD stats ( numRows= " << nr << " )\n";
  for (uint i = 0; i < N_dirs; i++) {
    os << std::setw(20) << str_Mode_dir(BCD::ModeDir(i)) << " : "
       << dir_counters[i] << '\n';
  }

  os << "        #AXI = " << axi_cnt << '\n';
  os << "       #GPIO = " << gpio_cnt << '\n';
  os << "  #GBOX_GPIO = " << gbox_gpio_cnt << '\n';
  os << "   #inp_colm A2F = " << inp_colm_cnt << '\n';
  os << "   #out_colm F2A = " << out_colm_cnt << endl;

  flush_out(false);
  return nr;
}

uint PcCsvReader::print_bcd(std::ostream& os) const noexcept {
  flush_out(false);
  uint nr = numRows();
  os << "+++ BCD dump::: Bump/Pin Name , Customer Name , Ball ID , IO_tile_pin "
        ":::"
     << endl;
  for (uint i = 0; i < nr; i++) {
    const BCD& bcd = *bcd_[i];
    os << "   " << bcd << endl;
  }
  os << "--- BCD dump ^^^ (nr=" << nr << ")\n" << endl;
  flush_out(false);
  return nr;
}

uint PcCsvReader::print_axi_bcd(std::ostream& os) const noexcept {
  flush_out(false);
  uint n = bcd_AXI_.size();
  os << "+++ bcd_AXI_ dump (" << n
     << ") ::: Bump/Pin Name , Customer Name , Ball ID , IO_tile_pin :::"
     << endl;
  for (const BCD* bcd_p : bcd_AXI_) {
    assert(bcd_p);
    os << "\t " << *bcd_p << endl;
  }
  os << "--- bcd_AXI_ dump (n=" << n << ")\n" << endl;
  flush_out(false);
  return n;
}

void PcCsvReader::print_csv() const {
  flush_out(true);
  lputs("print_csv()");
  auto& ls = lout();
  ls << "#row\tBump/Pin Name \t Customer Name \t Ball ID "
     << "   IO_tile_pin\t X   Y   Z"
     << " \t column-M"
     << "    rxtx_dir"
     << " \t Customer Internal Name" << endl;
  string dash = str::sReplicate('-', 131u);
  ls << dash << endl;

  uint num_rows = numRows();
  assert(bcd_.size() == num_rows);
  for (uint i = 0; i < num_rows; i++) {
    const BCD& b = *bcd_[i];
    const XYZ& p = b.xyz_;
    lprintf("%-5u ", i + 2);
    lprintf(" %12s ", b.bump_B_.c_str());
    lprintf(" %22s ", b.customer_.c_str());
    lprintf(" %6s ", b.ball_ID_.c_str());
    ls << "\t " << b.IO_tile_pin_ << "\t " << p.x_ << " " << p.y_ << " "
       << p.z_ << " ";
    lprintf(" %10s ", b.col_M_.c_str());
    lprintf(" %16s ", str_Mode_dir(b.rxtx_dir_));
    if (b.dirContradiction()) {
      ls << " DIR_CONTRADICTION  ";
    }
    lprintf(" %22s ", b.customerInternal().c_str());
    ls << endl;
  }

  ls << dash << endl;
  lprintf("Total Records: %u\n", num_rows);
  flush_out(true);
}

void PcCsvReader::write_debug_csv() const {
  flush_out(true);
  string cwd = get_CWD();
  lprintf("write_debug_csv() in work_dir = %s\n", cwd.c_str());

  CStr fn = "DEBUG_PINC_PT.csv";
  std::ofstream fos(fn);
  if (not fos.is_open()) {
    flush_out(true);
    lprintf2("ERROR write_debug_csv() could not open file for writing: %s\n", fn);
    flush_out(true);
    return;
  }

  lprintf("  writing %s ...\n", fn);

  fos << "#row\tBump/Pin Name \t Customer Name \t Ball ID "
      << "   IO_tile_pin\t X   Y   Z"
      << " \t column-M"
      << "    rxtx_dir"
      << " \t Customer Internal Name" << endl;
  string dash = str::sReplicate('-', 131u);
  fos << dash << endl;

  uint num_rows = numRows();
  assert(bcd_.size() == num_rows);
  for (uint i = 0; i < num_rows; i++) {
    const BCD& b = *bcd_[i];
    const XYZ& p = b.xyz_;
    os_printf(fos, "%-5u ", i + 2);
    os_printf(fos, " %12s ", b.bump_B_.c_str());
    os_printf(fos, " %22s ", b.customer_.c_str());
    os_printf(fos, " %6s ", b.ball_ID_.c_str());
    fos << "\t " << b.IO_tile_pin_ << "\t " << p.x_ << " " << p.y_ << " "
       << p.z_ << " ";
    os_printf(fos, " %10s ", b.col_M_.c_str());
    os_printf(fos, " %16s ", str_Mode_dir(b.rxtx_dir_));
    if (b.dirContradiction()) {
      os_printf(fos, " DIR_CONTRADICTION  ");
    }
    os_printf(fos, " %22s ", b.customerInternal().c_str());
    fos << endl;
  }

  fos << dash << endl;

  lprintf("    wrote %s\n", fn);
  flush_out(true);
}

vector<uint> PcCsvReader::get_enabled_rows_for_mode(const string& mode) const noexcept {
  if (mode.empty())
    return {};
  uint col_idx = getModeCol(mode);
  if (!col_idx)
    return {};
  vector<string> col = crd_->getColumn(mode);
  if (col.empty())
    return {};

  vector<uint> result;
  result.reserve(col.size());
  for (uint r = 0; r < col.size(); r++) {
    if (col[r] == "Y")
      result.push_back(r);
  }

  return result;
}

XYZ PcCsvReader::get_axi_xyz_by_name(const string& axi_name,
                                        uint& pt_row) const noexcept {
  pt_row = 0;
  XYZ result;
  if (bcd_AXI_.empty()) {
    if (ltrace() >= 7) lputs("pin_c NOTE: bcd_AXI_.empty()");
    return result;
  }

  for (const BCD* p : bcd_AXI_) {
    assert(p);
    if (p->customerInternal() == axi_name) {
      result = p->xyz_;
      pt_row = p->row_;
      break;
    }
  }

  return result;
}

uint PcCsvReader::getModeCol(const string& mode) const noexcept {
  if (mode.length() <= 1)
    return 0;
  string mode_lc = str::s2lower(mode);
  uint modeCol = 0, nc = numCols();
  assert(nc > 2);
  for (uint c = start_MODE_col_; c < nc; c++) {
    if (mode_lc == col_headers_lc_[c]) {
      modeCol = c;
      break;
    }
  }
  return modeCol;
}

XYZ PcCsvReader::get_ipin_xyz_by_name(const string& mode,
                                         const string& customerPin_or_ID,
                                         const string& gbox_pin_name,
                                         const std::set<XYZ>& except,
                                         uint& pt_row) const noexcept {
  pt_row = 0;

  // 1. if customerPin_or_ID is an AXI-pin, then skip the mode="Y" check
  XYZ result = get_axi_xyz_by_name(customerPin_or_ID, pt_row);
  if (result.valid()) return result;

  assert(mode.length() > 1);
  if (mode.length() <= 1)
    return result;

  // 2.
  uint modeCol = getModeCol(mode);
  if (!modeCol)
    return result; // 'mode' not found

  uint num_rows = numRows();
  assert(num_rows > 1);

  // 3. try without GPIO
  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = *bcd_[i];
    if (not bcd.isInput())
      continue;
    uint realRow = bcd.row_;
    assert(realRow == i);
    if (!bcd.match(customerPin_or_ID))
      continue;
    if (!bcd.modes_[modeCol])
      continue;
    if (gbox_pin_name.empty() || bcd.fullchipName_ == gbox_pin_name) {
      if (not except.count(bcd.xyz_)) {
        result = bcd.xyz_;
        pt_row = realRow;
        assert(result.valid());
        goto ret;
      }
    }
  }

  // 4. try with GPIO
  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = *bcd_[i];
    if (not bcd.isInput())
      continue;
    uint realRow = bcd.row_;
    assert(realRow == i);
    if (!bcd.match(customerPin_or_ID))
      continue;
    if (!bcd.modes_[modeCol] && !bcd.numGpioModes())
      continue;
    if (gbox_pin_name.empty() || bcd.fullchipName_ == gbox_pin_name) {
      if (not except.count(bcd.xyz_)) {
        result = bcd.xyz_;
        pt_row = realRow;
        assert(result.valid());
        goto ret;
      }
    }
  }

  // 5. if failed, annotate a partially matching pt_row for debugging:
  if (!result.valid()) {
    for (uint i = 0; i < num_rows; i++) {
      const BCD& bcd = *bcd_[i];
      if (not bcd.isInput())
        continue;
      if (!bcd.match(customerPin_or_ID))
        continue;
      if (gbox_pin_name.empty() || bcd.fullchipName_ == gbox_pin_name) {
        pt_row = i;
        break;
      }
    }
  }

ret:
  return result;
}

XYZ PcCsvReader::get_opin_xyz_by_name(const string& mode,
                                         const string& customerPin_or_ID,
                                         const string& gbox_pin_name,
                                         const std::set<XYZ>& except,
                                         uint& pt_row) const noexcept {
  pt_row = 0;

  // 1. if customerPin_or_ID is an AXI-pin, then skip the mode="Y" check
  XYZ result = get_axi_xyz_by_name(customerPin_or_ID, pt_row);
  if (result.valid()) return result;

  assert(mode.length() > 1);
  if (mode.length() <= 1)
    return result;

  // 2.
  uint modeCol = getModeCol(mode);
  if (!modeCol)
    return result; // 'mode' not found

  uint num_rows = numRows();
  assert(num_rows > 1);

  // 3. try without GPIO
  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = *bcd_[i];
    if (bcd.isInput())
      continue;
    uint realRow = bcd.row_;
    assert(realRow == i);
    if (!bcd.match(customerPin_or_ID))
      continue;
    if (!bcd.modes_[modeCol])
      continue;
    if (gbox_pin_name.empty() || bcd.fullchipName_ == gbox_pin_name) {
      if (not except.count(bcd.xyz_)) {
        result = bcd.xyz_;
        pt_row = realRow;
        assert(result.valid());
        goto ret;
      }
    }
  }

  // 4. try with GPIO
  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = *bcd_[i];
    if (bcd.isInput())
      continue;
    uint realRow = bcd.row_;
    assert(realRow == i);
    if (!bcd.match(customerPin_or_ID))
      continue;
    if (!bcd.modes_[modeCol] && !bcd.numGpioModes())
      continue;
    if (gbox_pin_name.empty() || bcd.fullchipName_ == gbox_pin_name) {
      if (not except.count(bcd.xyz_)) {
        result = bcd.xyz_;
        pt_row = realRow;
        assert(result.valid());
        goto ret;
      }
    }
  }

  // 5. if failed, annotate a partially matching pt_row for debugging:
  if (!result.valid()) {
    for (uint i = 0; i < num_rows; i++) {
      const BCD& bcd = *bcd_[i];
      if (bcd.isInput())
        continue;
      if (!bcd.match(customerPin_or_ID))
        continue;
      if (gbox_pin_name.empty() || bcd.fullchipName_ == gbox_pin_name) {
        pt_row = i;
        break;
      }
    }
  }

ret:
  return result;
}

uint PcCsvReader::printModeNames() const {
  uint n = mode_names_.size();
  lprintf("mode_names_.size()= %u\n", n);
  if (mode_names_.empty()) return 0;

  for (const string& nm : mode_names_)
    lprintf("\t  %s\n", nm.c_str());

  return n;
}

string PcCsvReader::bumpName2CustomerName(
    const string& bump_nm) const noexcept {
  assert(!bump_nm.empty());
  uint num_rows = numRows();
  assert(num_rows > 1);
  assert(bcd_.size() == num_rows);

  // tmp linear search
  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = *bcd_[i];
    if (bcd.bump_B_ == bump_nm) return bcd.customer_;
  }

  return {};
}

bool PcCsvReader::has_io_pin(const string& pin_name_or_ID) const noexcept {
  assert(!bcd_.empty());

  for (const BCD* x : bcd_) {
    if (x->match(pin_name_or_ID)) return true;
  }
  return false;
}

vector<uint> PcCsvReader::get_gbox_rows(const string& device_gbox_name) const noexcept {
  assert(!bcd_.empty());
  assert(!device_gbox_name.empty());
  if (device_gbox_name.empty())
    return {};

  vector<uint> result;

  // tmp linear search
  uint num_rows = numRows();
  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = *bcd_[i];
    if (bcd.match(device_gbox_name)) {
      result.push_back(i);
    }
  }

  return result;
}

bool PcCsvReader::hasCustomerInternalName(const string& nm) const noexcept {
  assert(!bcd_.empty());
  assert(!nm.empty());

  for (const BCD* x : bcd_AXI_) {
    assert(x);
    if (x->customerInternal() == nm) return true;
  }
  return false;
}

vector<string> PcCsvReader::get_AXI_inputs() const {
  assert(!bcd_.empty());
  // DO NOT uncomment:  assert(!bcd_AXI_.empty());

  vector<string> result;
  result.reserve(bcd_AXI_.size());

  auto is_axi_inp = [](const string& s) -> bool {
    size_t len = s.length();
    if (len < 4 || len > 300) return false;
    return (s[len - 1] == 'i' && s[len - 2] == '_');  // ends with "_i"
  };

  for (const BCD* p : bcd_AXI_) {
    assert(p);
    if (is_axi_inp(p->customerInternal()))
      result.emplace_back(p->customerInternal());
  }

  return result;
}

vector<string> PcCsvReader::get_AXI_outputs() const {
  assert(!bcd_.empty());
  // DO NOT uncomment: assert(!bcd_AXI_.empty());

  vector<string> result;
  result.reserve(bcd_AXI_.size());

  auto is_axi_out = [](const string& s) -> bool {
    size_t len = s.length();
    if (len < 4 || len > 300) return false;
    return (s[len - 1] == 'o' && s[len - 2] == '_');  // ends with "_o"
  };

  for (const BCD* p : bcd_AXI_) {
    assert(p);
    if (is_axi_out(p->customerInternal()))
      result.emplace_back(p->customerInternal());
  }

  return result;
}

}  // namespace pln
