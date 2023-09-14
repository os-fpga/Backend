#include "file_readers/rapid_csv_reader.h"
#include "file_readers/Fio.h"
#include "pin_loc/pin.h"

#include <iomanip>

namespace pinc {

using namespace std;

static constexpr uint MAX_COLMS = RapidCsvReader::MAX_PT_COLS;

RapidCsvReader::RapidCsvReader() {}

RapidCsvReader::~RapidCsvReader() { delete crd_; }

void RapidCsvReader::reset() noexcept {
  start_GBOX_GPIO_row_ = 0;
  start_CustomerInternal_row_ = 0;
  start_MODE_col_ = 0;
  bcd_AXI_.clear();
  bcd_GBGPIO_.clear();
  bcd_.clear();
  modes_map_.clear();
  col_headers_.clear();
  col_headers_lc_.clear();
  mode_names_.clear();

  bcd_inp_Q_.clear();
  bcd_out_Q_.clear();

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

static inline bool ends_with_rx(const char* z, size_t len) noexcept {
  assert(z);
  if (len < 4) return false;
  return z[len - 1] == 'x' and z[len - 2] == 'r' and z[len - 3] == '_';
}

static inline bool ends_with_tx(const char* z, size_t len) noexcept {
  assert(z);
  if (len < 4) return false;
  return z[len - 1] == 'x' and z[len - 2] == 't' and z[len - 3] == '_';
}

static inline bool starts_with_A2F(const char* z) noexcept {
  assert(z);
  return z[0] == 'A' and z[1] == '2' and z[2] == 'F';
}

static inline bool starts_with_F2A(const char* z) noexcept {
  assert(z);
  return z[0] == 'F' and z[1] == '2' and z[2] == 'A';
}

std::bitset<MAX_COLMS> RapidCsvReader::BCD::getRxModes() const noexcept {
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

std::bitset<MAX_COLMS> RapidCsvReader::BCD::getTxModes() const noexcept {
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

Pin* RapidCsvReader::BCD::annotatePin(const string& udes_pn,
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

const char* RapidCsvReader::str_Mode_dir(BCD::ModeDir t) noexcept {
  // enum ModeDir
  // {  No_dir,   Input_dir,   Output_dir, HasBoth_dir,   AllEnabled_dir  };
  static const char* enumS[] =
    {"No_dir", "Input_dir", "Output_dir", "HasBoth_dir", "AllEnabled_dir"};
  constexpr size_t n = sizeof(enumS) / sizeof(enumS[0]);
  static_assert(n == BCD::AllEnabled_dir + 1);
  uint i = uint(t);
  assert(i < n);
  return enumS[i];
}

std::ostream& operator<<(std::ostream& os, const RapidCsvReader::BCD& b) {
  os << "(bcd-" << b.row_ << ' '
     << "  grp:" << b.groupA_ << "  " << b.bump_ << "  " << b.customer_ << "  "
     << b.ball_ID_ << "  ITP: " << b.IO_tile_pin_ << "  XYZ: " << b.xyz_
     << "  colM:" << b.col_M_ << "  fc:" << b.fullchipName_
     << "  ci:" << b.customerInternal() << "  axi:" << int(b.is_axi_)
     << "  isGPIO:" << int(b.is_GPIO_) << "  isGB_GPIO:" << int(b.is_GBOX_GPIO_)
     << "  rxtx_dir:" << RapidCsvReader::str_Mode_dir(b.rxtx_dir_)
     << "  colM_dir:" << b.str_colM_dir()
     << "  is_input:" << int(b.isInput())
     << "  #modes:" << b.numModes()
     //<< "  #modes:" << b.modes_
     << "  #rx_modes:" << b.numRxModes()
     << "  #tx_modes:" << b.numTxModes()
     << "  row:" << b.row_ << ')';
  return os;
}

std::ostream& operator<<(std::ostream& os, const RapidCsvReader::Tile& t) {
  os << "(tile-" << t.id_ << ' '
     << "  loc: " << t.loc_
     << "  beg_row:" << t.beg_row_
     << "  #used=" << t.num_used_
     << "  #a2f=" << t.a2f_sites_.size()
     << "  #f2a=" << t.f2a_sites_.size()
     << ')';
  return os;
}

// returns spreadsheet column label ("A", "B", "BC", etc) for column index 'i'
static inline string label_column(int i) noexcept {
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
    return ends_with_rx(hdr_.c_str(), hdr_.length());
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
    lout() << "\nERROR reading csv: failed row: " << rowNum << endl;
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
    lout() << "ROW-" << rowNum;
    for (uint c = 0; c < nc; c++) lout() << " | " << R[c];
    lputs(" |");
  }

  return true;
}

// if 'hdr' starts with "Mode_"/"MODE_" then convert
// the prefix to upper case and return true.
bool RapidCsvReader::prepare_mode_header(string& hdr) noexcept {
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

bool RapidCsvReader::initRows(const fio::CSV_Reader& crd) {
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
bool RapidCsvReader::setDirections(const fio::CSV_Reader& crd) {
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
    // lputs9();
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

  // 3. populate Qs in reverse order
  bcd_inp_Q_.clear();
  bcd_out_Q_.clear();
  bcd_inp_Q_.reserve(inp_cnt + 1);
  bcd_out_Q_.reserve(out_cnt + 1);
  for (int r = int(num_rows) - 1; r >= 0; r--) {
    assert(bcd_[r]);
    BCD* bcd = bcd_[r];
    if (bcd->colM_dir_ == BCD::Input_dir)
      bcd_inp_Q_.push_back(bcd);
    else if (bcd->colM_dir_ == BCD::Output_dir)
      bcd_out_Q_.push_back(bcd);
  }

  if (tr >= 5) {
    lprintf("setDirections:  bcd_inp_Q_.size()= %zu  bcd_out_Q_.size()= %zu\n",
            bcd_inp_Q_.size(), bcd_out_Q_.size());
  }

  return true;
}

bool RapidCsvReader::createTiles()
{
  uint16_t tr = ltrace();
  auto& ls = lout();

  max_x_ = 0;
  max_y_ = 0;
  tiles_.clear();

  uint num_rows = numRows();
  assert(num_rows >= 3);
  if (num_rows < 3) return false;

  tiles_.reserve(num_rows);

  // 1. determine first_valid_row, max_x_, max_y_
  int first_valid_row = -1;
  for (uint r = 0; r < num_rows; r++) {
    assert(bcd_[r]);
    const XY& loc = bcd_[r]->xyz_;
    auto colM_dir = bcd_[r]->colM_dir_;
    if (colM_dir != BCD::Input_dir && colM_dir != BCD::Output_dir)
      continue;
    assert(loc.valid());
    if (!loc.valid())
      continue;
    if (first_valid_row < 0)
      first_valid_row = r;
    if (loc.x_ > max_x_)
      max_x_ = loc.x_;
    if (loc.y_ > max_y_)
      max_y_ = loc.y_;
  }

  if (tr >= 5) {
    lprintf("createTiles:  first_valid_row= %i  max_x_= %i  max_y_= %i\n",
            first_valid_row, max_x_, max_y_);
  }

  if (first_valid_row < 0)
    return false;

  // 2. add tiles_ avoiding xy-duplicates if possible
  tiles_.emplace_back(bcd_[first_valid_row]->xyz_, first_valid_row);
  for (uint r = first_valid_row + 1; r < num_rows; r++) {
    const XY& loc = bcd_[r]->xyz_;
    if (tiles_.back().loc_ == loc)
      continue;
    tiles_.emplace_back(loc, r);
  }

  uint sz = tiles_.size();
  if (sz < 2)
    return false;

  // 3. uniquefy tiles
  uint num_invalidated = 0;
  for (uint i = 0; i < sz; i++) {
    const Tile& ti = tiles_[i];
    if (!ti.loc_.valid())
      continue;
    for (uint j = i + 1; j < sz; j++) {
      Tile& tj = tiles_[j];
      if (!tj.loc_.valid())
        continue;
      if (ti.loc_ == tj.loc_) {
        tj.loc_.invalidate();
        num_invalidated++;
      }
    }
  }
  if (tr >= 6) {
    ls << "  num_invalidated= " << num_invalidated << "  sz= " << sz << endl;
  }
  if (num_invalidated) {
    for (int i = int(sz) - 1; i >= 0; i--) {
      XY xy = tiles_[i].loc_;
      if (!xy.valid())
        tiles_.erase(tiles_.begin() + i);
    }
  }

  if (tr >= 5) {
    lprintf("createTiles:  tiles_.size()= %zu\n", tiles_.size());
  }

  // 4. index and populate tiles
  sz = tiles_.size();
  if (sz < 2)
    return false;
  for (uint i = 0; i < sz; i++) {
    Tile& ti = tiles_[i];
    assert(ti.loc_.valid());
    ti.id_ = i;
    assert(ti.beg_row_ < num_rows);
    for (uint r = ti.beg_row_; r < num_rows; r++) {
      BCD* bcd = bcd_[r];
      if (ti.loc_ != bcd->xyz_)
        continue;
      if (bcd->isA2F())
        ti.a2f_sites_.push_back(bcd);
      else if (bcd->isF2A())
        ti.f2a_sites_.push_back(bcd);
    }
  }

  if (tr >= 6) {
    for (uint i = 0; i < sz; i++) {
      const Tile& ti = tiles_[i];
      ls << ti << endl;
    }
  }

  return true;
}

bool RapidCsvReader::read_csv(const string& fn, bool check) {
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2)
    ls << "RapidCsvReader::read_csv( " << fn << " )  check:" << int(check) << endl;

  reset();

  crd_ = new fio::CSV_Reader(fn);
  fio::CSV_Reader& crd = *crd_;

  // check file accessability and format:
  crd.setTrace(tr);
  if (!crd.fileExistsAccessible()) {
    ls << "\nERROR reading csv: file is not accessible: " << fn << '\n' << endl;
    delete crd_;
    crd_ = nullptr;
    return false;
  }
  if (!crd.readCsv(false)) {
    ls << "\nERROR reading csv: wrong format: " << fn << '\n' << endl;
    delete crd_;
    crd_ = nullptr;
    return false;
  }
  if (tr >= 4) crd.dprint1();

  col_headers_ = crd.header_;
  col_headers_lc_ = col_headers_;
  for (auto& h : col_headers_lc_)
    h = str::sToLower(h);

  if (tr >= 3) {
    assert(col_headers_.size() > 2);
    ls << "  col_headers_.size()= " << col_headers_.size() << "  ["
       << col_headers_.front() << " ... " << col_headers_.back() << ']' << endl;
    if (tr >= 4) {
      for (uint i = 0; i < col_headers_.size(); i++) {
        string col_label = label_column(i);
        const string& hdr_i = col_headers_[i];
        ls << "--- " << i << '-' << col_label << "  hdr_i= " << hdr_i << endl;
      }
      ls << endl;
    }
  }

  if (!initRows(crd)) {
    ls << "\nERROR initRows() failed\n" << endl;
    return false;
  }
  uint num_rows = numRows();
  assert(num_rows >= 10);

  bool has_Fullchip_name = false;
  vector<string> mode_data;
  string hdr_i;
  mode_names_.reserve(col_headers_.size());
  start_MODE_col_ = 0;
  for (uint col = 0; col < col_headers_.size(); col++) {
    string col_label = label_column(col);
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
      ls << "\nERROR reading csv: failed column: " << orig_hdr_i << endl;
      return false;
    }

    assert(mode_data.size() <= num_rows);
    if (mode_data.size() < num_rows) {
      if (tr >= 4) ls << "!!! mode_data.size() < num_rows" << endl;
      mode_data.resize(num_rows);
    }
    modes_map_.emplace(hdr_i, mode_data);
    mode_names_.emplace_back(hdr_i);

    // set 1 in bitsets for 'Y'
    for (uint row = 0; row < num_rows; row++) {
      if (mode_data[row] == "Y") {
        bcd_[row]->modes_[col] = true;
      }
    }
  }

  if (tr >= 2) {
    if (tr >= 4) ls << endl;
    ls << "  mode_names_.size()= " << mode_names_.size() << endl;
    if (tr >= 3) {
      logVec(mode_names_, "  mode_names_ ");
      ls << endl;
    }
  }

  if (!has_Fullchip_name) {
    lputs("\nRapidCsvReader::read_csv(): column Fullchip_NAME not found\n");
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

  S_tmp = crd.getColumn("Fullchip_NAME");
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) {
    bcd_[i]->fullchipName_ = std::move(S_tmp[i]);
  }

  ok = get_column(crd, "Customer Name", S_tmp);
  if (!ok) return false;
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) {
    bcd_[i]->customer_ = std::move(S_tmp[i]);
  }

  ok = get_column(crd, "Customer Internal Name", S_tmp);
  if (!ok) {
    lputs("\n pin_c WARNING: could not read Customer Internal Name column\n");
    return false;
  }

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
    if (tr >= 3) {
      const auto& firs_bcd = *bcd_[firs_ri];
      const auto& last_bcd = *bcd_[last_ri];
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

  bcd_GBGPIO_.reserve(bcd_.size());
  for (BCD* bcd : bcd_) {
    if (bcd->is_GBOX_GPIO_) bcd_GBGPIO_.push_back(bcd);
  }

  if (tr >= 2) {
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

  ok = get_column(crd, "Bump/Pin Name", S_tmp);
  if (!ok) return false;
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  const vector<string>& bump_pin_name = S_tmp;

  if (tr >= 9) {
    ls << endl;
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

  for (uint i = 0; i < num_rows; i++) {
    BCD& bcd = *bcd_[i];
    bcd.bump_ = bump_pin_name[i];
    if (bcd.bump_.empty()) {
      if (bcd.customerInternal().empty() && tr >= 3) {
        ls << " (WW) both bcd.bump_ and bcd.customerInternal_ are empty"
           << " on row# " << i << endl;
        // assert(0);
      }
    }
    bcd.normalize();
    // assert(!bcd.bump_.empty()); // no-assert, could be clock: colM = F2CLK
  }

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

  bool status_ok = setDirections(crd);
  if (not status_ok) {
    ls << '\n' << "[Error] directions are not OK" << endl;
    std::cerr << "[Error] directions are not OK" << endl;
    return false;
  }

  if (tr >= 6) {
    ls << "___ original BCD order ___" << endl;
    print_bcd(ls);
  }

  status_ok = createTiles();
  if (not status_ok) {
    ls << '\n' << "[Error] status not OK" << endl;
    std::cerr << "[Error] status not OK" << endl;
    return false;
  }

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
        ls << "___ new BCD order ___" << endl;
        print_bcd(ls);
      }
      lputs9();
    }
  }

  if (tr >= 5) {
    print_bcd_stats(ls);
    lputs();
    print_axi_bcd(ls);
  }
  if (tr >= 5) print_csv();

  return true;
}

bool RapidCsvReader::write_csv(const string& fn, uint minRow,
                               uint maxRow) const {
  uint16_t tr = ltrace();
  if (tr >= 2) {
    lprintf("RapidCsvReader::write_csv( %s )  minRow= %u  maxRow= %u\n",
            fn.c_str(), minRow, maxRow);
  }

  if (fn.empty()) return false;
  if (!crd_) return false;

  if (maxRow == 0) maxRow = UINT_MAX;

  bool ok = crd_->writeCsv(fn, minRow, maxRow);
  if (tr >= 2)
    lprintf("done RapidCsvReader::write_csv( %s )  ok:%i\n", fn.c_str(), ok);

  return ok;
}

uint RapidCsvReader::countBidiRows() const noexcept {
  uint nr = numRows();
  uint cnt = 0;
  for (uint i = 0; i < nr; i++) {
    const BCD& bcd = *bcd_[i];
    if (bcd.isBidiRxTx()) cnt++;
  }
  return cnt;
}

uint RapidCsvReader::print_bcd_stats(std::ostream& os) const noexcept {
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

  os << "bcd_stats( numRows= " << nr << " )\n";
  for (uint i = 0; i < N_dirs; i++) {
    os << std::setw(20) << str_Mode_dir(BCD::ModeDir(i)) << " : "
       << dir_counters[i] << '\n';
  }

  os << "        #AXI = " << axi_cnt << '\n';
  os << "       #GPIO = " << gpio_cnt << '\n';
  os << "  #GBOX_GPIO = " << gbox_gpio_cnt << '\n';
  os << "   #inp_colm A2F = " << inp_colm_cnt << '\n';
  os << "   #out_colm F2A = " << out_colm_cnt << endl;

  return nr;
}

uint RapidCsvReader::print_bcd(std::ostream& os) const noexcept {
  uint nr = numRows();
  os << "+++ BCD dump::: Bump/Pin Name , Customer Name , Ball ID , IO_tile_pin "
        ":::"
     << endl;
  for (uint i = 0; i < nr; i++) {
    const BCD& bcd = *bcd_[i];
    os << "   " << bcd << endl;
  }
  os << "--- BCD dump ^^^ (nr=" << nr << ")\n" << endl;
  return nr;
}

uint RapidCsvReader::print_axi_bcd(std::ostream& os) const noexcept {
  uint n = bcd_AXI_.size();
  os << "+++ bcd_AXI_ dump (" << n
     << ") ::: Bump/Pin Name , Customer Name , Ball ID , IO_tile_pin :::"
     << endl;
  for (const BCD* bcd_p : bcd_AXI_) {
    assert(bcd_p);
    os << "\t " << *bcd_p << endl;
  }
  os << "--- bcd_AXI_ dump (n=" << n << ")\n" << endl;
  return n;
}

void RapidCsvReader::print_csv() const {
  lputs("print_csv()");
  auto& ls = lout();
  ls << "#row\tBump/Pin Name \t Customer Name \t Ball ID "
     << "\t IO_tile_pin\t IO_tile_pin_x\tIO_tile_pin_y\tIO_tile_pin_z"
     << " \t Customer Internal Name" << endl;
  string dash = str::sReplicate('-', 139u);
  ls << dash << endl;

  uint num_rows = numRows();
  assert(bcd_.size() == num_rows);
  for (uint i = 0; i < num_rows; i++) {
    const BCD& b = *bcd_[i];
    const XYZ& p = b.xyz_;
    lprintf("%-5u ", i + 2);
    lprintf(" %12s ", b.bump_.c_str());
    lprintf(" %22s ", b.customer_.c_str());
    lprintf(" %6s ", b.ball_ID_.c_str());
    ls << "\t " << b.IO_tile_pin_ << "\t " << p.x_ << " " << p.y_ << " "
       << p.z_;
    lprintf(" %22s ", b.customerInternal().c_str());
    ls << endl;
  }

  ls << dash << endl;
  ls << "Total Records: " << num_rows << endl;
}

XYZ RapidCsvReader::get_axi_xyz_by_name(const string& axi_name,
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

XYZ RapidCsvReader::get_pin_xyz_by_name(const string& mode,
                                        const string& customerPin_or_ID,
                                        const string& gbox_pin_name,
                                        uint& pt_row) const noexcept {
  pt_row = 0;

  // 1. if customerPin_or_ID is an AXI-pin, then skip the mode="Y" check
  XYZ result = get_axi_xyz_by_name(customerPin_or_ID, pt_row);
  if (result.valid()) return result;

  // 2.
  auto fitr = modes_map_.find(mode);
  if (fitr == modes_map_.end()) return result;

  const vector<string>& mode_vector = fitr->second;

  uint num_rows = numRows();
  assert(num_rows > 1);
  assert(mode_vector.size() == num_rows);
  assert(bcd_.size() == num_rows);

  // 3.
  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = *bcd_[i];
    uint realRow = bcd.row_;
    if (!bcd.match(customerPin_or_ID)) continue;
    if (mode_vector[realRow] != "Y") continue;
    if (gbox_pin_name.empty() || bcd.fullchipName_ == gbox_pin_name) {
      result = bcd.xyz_;
      pt_row = realRow;
      assert(result.valid());
      break;
    }
  }

  return result;
}

uint RapidCsvReader::printModeKeys() const {
  uint n = modes_map_.size();
  lprintf("modes_map_.size()= %u\n", n);
  if (modes_map_.empty()) return 0;

  for (const auto& I : modes_map_) lprintf("\t  %s\n", I.first.c_str());

  return n;
}

string RapidCsvReader::bumpName2CustomerName(
    const string& bump_nm) const noexcept {
  assert(!bump_nm.empty());
  uint num_rows = numRows();
  assert(num_rows > 1);
  assert(bcd_.size() == num_rows);

  // tmp linear search
  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = *bcd_[i];
    if (bcd.bump_ == bump_nm) return bcd.customer_;
  }

  return {};
}

bool RapidCsvReader::has_io_pin(const string& pin_name_or_ID) const noexcept {
  assert(!bcd_.empty());

  for (const BCD* x : bcd_) {
    if (x->match(pin_name_or_ID)) return true;
  }
  return false;
}

bool RapidCsvReader::hasCustomerInternalName(const string& nm) const noexcept {
  assert(!bcd_.empty());
  assert(!nm.empty());

  for (const BCD* x : bcd_AXI_) {
    assert(x);
    if (x->customerInternal() == nm) return true;
  }
  return false;
}

vector<string> RapidCsvReader::get_AXI_inputs() const {
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

vector<string> RapidCsvReader::get_AXI_outputs() const {
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

}  // namespace pinc
