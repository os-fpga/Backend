#include "rapid_csv_reader.h"

#include <algorithm>
#include <set>

#include "rapidcsv.h"

namespace pinc {

using namespace std;

RapidCsvReader::RapidCsvReader() {}

RapidCsvReader::~RapidCsvReader() {}

void RapidCsvReader::reset() noexcept
{
  start_GBOX_GPIO_row_ = 0;
  start_CustomerInternal_row_ = 0;
  bcd_AXI_.clear();
  bcd_GBGPIO_.clear();
  bcd_.clear();
  modes_map_.clear();
  col_headers_.clear();
  mode_names_.clear();
}

std::ostream& operator<<(std::ostream& os, const RapidCsvReader::BCD& b) {
  os << "(bcd "
     << "  grp:" << b.groupA_
     << "  " << b.bump_
     << "  " << b.customer_
     << "  " << b.ball_ID_
     << "  fc:" << b.fullchipName_
     << "  ci:" << b.customerInternal_
     << "  axi:" << int(b.is_axi_)
     << "  isGPIO:" << int(b.is_GPIO_)
     << "  isGB_GPIO:" << int(b.is_GBOX_GPIO_)
     << "  row:" << b.row_
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

static bool get_column(rapidcsv::Document& doc, std::ostream& logStream,
                       const string& colName, vector<string>& V) {
  V.clear();
  try {
    V = doc.GetColumn<string>(colName);
  } catch (const std::out_of_range& e) {
    logStream << "\nRapidCsvReader::read_csv() caught std::out_of_range on "
                 "doc.GetColumn()\n";
    logStream << "\t  what: " << e.what() << '\n';
    logStream << "\t  colName: " << colName << '\n';
    logStream << endl;
    return false;
  } catch (...) {
    logStream << "\nRapidCsvReader::read_csv() caught excepttion on "
                 "doc.GetColumn()\n";
    logStream << "\t  colName: " << colName << '\n';
    logStream << endl;
    return false;
  }
  return true;
}

// if 'hdr' starts with "Mode_"/"MODE_" then convert
// the prefix to upper case and return true.
bool RapidCsvReader::prepare_mode_header(string& hdr) noexcept
{
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

bool RapidCsvReader::read_csv(const string& fn, bool check) {
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2)
    ls << "RapidCsvReader::read_csv( " << fn << " )  Reading data ..." << endl;

  reset();

  rapidcsv::Document doc(
      fn,
      rapidcsv::LabelParams(),      // label params - use default, no offset
      rapidcsv::SeparatorParams(),  // separator params - use default ","
      rapidcsv::ConverterParams(
          true /*user default number for non-numberical strings*/,
          0.0 /* default float*/, -1 /*default integer*/),
      rapidcsv::LineReaderParams()
      /*skip lines with specified prefix, use default "false"*/);

  col_headers_ = doc.GetRow<string>(-1);
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

  vector<string> group_name = doc.GetColumn<string>("Group");
  size_t num_rows = group_name.size();
  assert(num_rows > 300);
  start_GBOX_GPIO_row_ = 0;
  for (uint i = 0; i < num_rows; i++) {
    if (group_name[i] == "GBOX GPIO") {
      start_GBOX_GPIO_row_ = i;
      break;
    }
  }

  if (tr >= 2) {
    ls << "  num_rows= " << num_rows << "  num_cols= " << col_headers_.size()
       << "  start_GBOX_GPIO_row_= " << start_GBOX_GPIO_row_ << '\n'
       << endl;
  }

  constexpr bool STRICT_FORMAT = false;

  bool has_Fullchip_name = false;
  vector<string> mode_data;
  string hdr_i;
  mode_names_.reserve(col_headers_.size());
  for (uint i = 0; i < col_headers_.size(); i++) {
    string col_label = label_column(i);
    const string& orig_hdr_i = col_headers_[i]; // before case conversion by prepare_mode_header()
    hdr_i = orig_hdr_i;

    if (!has_Fullchip_name && hdr_i == "Fullchip_NAME")
      has_Fullchip_name = true;

    if (!prepare_mode_header(hdr_i)) continue;

    if (tr >= 4)
      ls << "  (Mode_) " << i << '-' << col_label << "  hdr_i= " << hdr_i
         << endl;

    try {
      mode_data = doc.GetColumn<string>(orig_hdr_i);
    } catch (const std::out_of_range& e) {
      if (STRICT_FORMAT) {
        ls << "\nRapidCsvReader::read_csv() caught std::out_of_range"
           << "  i= " << i << "  hdr_i= " << hdr_i << endl;
        ls << "\t  what: " << e.what() << '\n' << endl;
        return false;
      } else {
        if (tr >= 3) {
          ls << "\nWARNING RapidCsvReader::read_csv() caught std::out_of_range"
             << "  i= " << i << "  hdr_i= " << hdr_i << endl;
          ls << "\t  what: " << e.what() << '\n' << endl;
        }
        continue;
      }
    } catch (...) {
      if (STRICT_FORMAT) {
        ls << "\nRapidCsvReader::read_csv() caught excepttion"
           << "  i= " << i << "  hdr_i= " << hdr_i << '\n'
           << endl;
        return false;
      } else {
        continue;
      }
    }
    assert(mode_data.size() <= num_rows);
    if (mode_data.size() < num_rows) {
      if (tr >= 4) ls << "!!! mode_data.size() < num_rows" << endl;
      mode_data.resize(num_rows);
    }
    modes_map_.emplace(hdr_i, mode_data);
    mode_names_.emplace_back(hdr_i);
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

  bcd_.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) {
    BCD& bcd = bcd_[i];
    bcd.row_ = i;
    bcd.groupA_ = group_name[i];
    bcd.is_GBOX_GPIO_ = (bcd.groupA_ == "GBOX GPIO");
    bcd.is_GPIO_ = (bcd.groupA_ == "GPIO");
  }

  vector<string> S_tmp;
  bool ok = false;

  S_tmp = doc.GetColumn<string>("Fullchip_NAME");
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) bcd_[i].fullchipName_ = S_tmp[i];

  ok = get_column(doc, ls, "Customer Name", S_tmp);
  if (!ok) return false;
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) bcd_[i].customer_ = S_tmp[i];

  ok = get_column(doc, ls, "Customer Internal Name", S_tmp);
  if (!ok) return false;
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  vector<uint> custIntNameRows;
  custIntNameRows.reserve(num_rows);
  for (uint i = 0; i < num_rows; i++) {
    const string& nm = S_tmp[i];
    if (nm.length()) {
      bcd_[i].customerInternal_ = nm;
      custIntNameRows.push_back(i);
    }
  }
  if (!custIntNameRows.empty()) {
    uint firs_ri = custIntNameRows.front();
    uint last_ri = custIntNameRows.back();
    uint firs_ri_uniq = firs_ri;
    for (uint i = firs_ri; i <= last_ri; i++) {
      if (bcd_[i].isCustomerInternalUnique()) {
        firs_ri_uniq = i;
        break;
      }
    }
    uint firs_ri_only = firs_ri_uniq;
    for (uint i = firs_ri_uniq; i <= last_ri; i++) {
      if (bcd_[i].isCustomerInternalOnly()) {
        firs_ri_only = i;
        break;
      }
    }
    if (firs_ri_only && firs_ri_only < last_ri && bcd_[firs_ri_only].isCustomerInternalOnly()) {
      start_CustomerInternal_row_ = firs_ri_only;
      bcd_AXI_.clear();
      bcd_AXI_.reserve(last_ri - firs_ri_only + 1);
      for (uint i = firs_ri_only; i <= last_ri; i++) {
        BCD& bcd = bcd_[i];
        if (bcd.isCustomerInternalOnly()) {
          bcd.is_axi_ = true;
          bcd_AXI_.push_back(&bcd);
        }
      }
    }
    if (tr >= 3) {
      const auto& firs_bcd = bcd_[firs_ri];
      const auto& last_bcd = bcd_[last_ri];
      ls << " ==        first row with Customer Internal Name : " << (firs_ri+2) << "  " << firs_bcd << endl;
      ls << " == first row with unique Customer Internal Name : " << (firs_ri_uniq+2)
         << "  " << bcd_[firs_ri_uniq] << endl;
      ls << " ==   first row with only Customer Internal Name : " << (firs_ri_only+2)
         << "  " << bcd_[firs_ri_only] << endl;
      ls << " ==         last row with Customer Internal Name : " << (last_ri+2) << "  " << last_bcd << endl;
    }
  }

  bcd_GBGPIO_.reserve(bcd_.size());
  for (BCD& bcd : bcd_) {
    if (bcd.is_GBOX_GPIO_)
      bcd_GBGPIO_.push_back(&bcd);
  }

  if (tr >= 2) {
    ls << "  num_rows= " << num_rows << "  num_cols= " << col_headers_.size()
       << "  start_GBOX_GPIO_row_= " << start_GBOX_GPIO_row_
       << "  start_CustomerInternal_row_= " << start_CustomerInternal_row_
       << '\n'
       << "  bcd_GBGPIO_.size()= " << bcd_GBGPIO_.size()
       << "  bcd_AXI_.size()= " << bcd_AXI_.size()
       << '\n' << endl;
  }

  ok = get_column(doc, ls, "Ball ID", S_tmp);
  if (!ok) return false;
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) bcd_[i].ball_ID_ = S_tmp[i];

  ok = get_column(doc, ls, "Bump/Pin Name", S_tmp);
  if (!ok) return false;
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  const vector<string>& bump_pin_name = S_tmp;

  if (tr >= 6) {
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
    BCD& bcd = bcd_[i];
    bcd.bump_ = bump_pin_name[i];
    if (bcd.bump_.empty()) {
      if (bcd.customerInternal_.empty() && tr >= 2) {
        ls << " (WW) both bcd.bump_ and bcd.customerInternal_ are empty"
           << " on row# " << i << endl;
      }
    }
    bcd.normalize();
    assert(!bcd.bump_.empty());
  }

  if (tr >= 6) {
    ls << "+++ BCD dump::: Bump/Pin Name , Customer Name , Ball ID :::" << endl;
    for (uint i = 0; i < num_rows; i++) {
      const BCD& bcd = bcd_[i];
      ls << "\t " << bcd << endl;
    }
    ls << "--- BCD dump^^^" << '\n' << endl;
  }
  if (tr >= 5) {
    ls << "+++ bcd_AXI_ dump (" << bcd_AXI_.size()
       << ") ::: Bump/Pin Name , Customer Name , Ball ID :::" << endl;
    for (const BCD* bcd_p : bcd_AXI_) {
      assert(bcd_p);
      ls << "\t " << *bcd_p << endl;
    }
    ls << "--- bcd_AXI_ dump^^^" << '\n' << endl;
  }

  io_tile_pin_ = doc.GetColumn<string>("IO_tile_pin");
  io_tile_pin_.resize(num_rows);

  vector<int> tmp = doc.GetColumn<int>("IO_tile_pin_x");
  tmp.resize(num_rows);
  io_tile_pin_xyz_.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) io_tile_pin_xyz_[i].x_ = tmp[i];

  tmp = doc.GetColumn<int>("IO_tile_pin_y");
  tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) io_tile_pin_xyz_[i].y_ = tmp[i];

  tmp = doc.GetColumn<int>("IO_tile_pin_z");
  tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) io_tile_pin_xyz_[i].z_ = tmp[i];

  // do a sanity check
  if (0&& check) {
    bool check_ok = sanity_check(doc);
    if (!check_ok) {
      ls << "\nWARNING: !check_ok" << endl;
      //      return false;
    }
  }

  // print data of interest for test
  if (tr >= 4) print_csv();

  return true;
}

bool RapidCsvReader::sanity_check(const rapidcsv::Document& doc) const {
#if 0
  // 1. check IO bump dangling (unused package pin)
  vector<string> group_name = doc.GetColumn<string>("Group");
  uint num_rows = group_name.size();
  assert(num_rows);
  std::set<string> connected_bump_pins;
  vector<string> connect_info;
  for (uint i = 0; i < num_rows; i++) {
    if (group_name[i] == "GBOX GPIO") {
      connect_info = doc.GetRow<string>(i);
      for (uint j = 0; j < connect_info.size(); j++) {
        if (connect_info[j] == "Y") {
          connected_bump_pins.insert(bumpPinName(i));
          break;
        }
      }
    }
  }

  bool check_ok = true;

  std::set<string> reported_unconnected_bump_pins;
  for (uint i = 0; i < group_name.size(); i++) {
    if (group_name[i] == "GBOX GPIO") {
      if (connected_bump_pins.find(bumpPinName(i)) ==
              connected_bump_pins.end() &&
          reported_unconnected_bump_pins.find(bumpPinName(i)) ==
              reported_unconnected_bump_pins.end()) {
        reported_unconnected_bump_pins.insert(bumpPinName(i));
        lout() << "[Pin Table Check Warning] Bump pin <" << bumpPinName(i)
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
#endif ////0000

  return true;
}

// file i/o
void RapidCsvReader::write_csv(string file_name) const {
  // to do
  lout() << "Not Implement Yet - Write content of interest to a csv file <"
         << file_name << ">" << endl;
  return;
}

void RapidCsvReader::print_csv() const {
  lputs("print_csv()");
  auto& ls = lout();
  ls << "#row\tBump/Pin Name \t Customer Name \t Ball ID "
     << "\t IO_tile_pin\t IO_tile_pin_x\tIO_tile_pin_y\tIO_tile_pin_z"
     << " \t Customer Internal Name" << endl;
  string dash = strReplicate('-', 139u);
  ls << dash << endl;

  uint num_rows = numRows();
  assert(bcd_.size() == num_rows);
  assert(io_tile_pin_xyz_.size() == num_rows);
  for (uint i = 0; i < num_rows; i++) {
    const BCD& b = bcd_[i];
    const XYZ& p = io_tile_pin_xyz_[i];
    lprintf("%-5u ", i+2);
    lprintf(" %12s ", b.bump_.c_str());
    lprintf(" %22s ", b.customer_.c_str());
    lprintf(" %6s ", b.ball_ID_.c_str());
    ls << "\t " << io_tile_pin_[i] << "\t" << p.x_ << "\t" << p.y_ << "\t"
       << p.z_;
    lprintf(" %22s ", b.customerInternal_.c_str());
    ls << endl;
  }

  ls << dash << endl;
  ls << "Total Records: " << num_rows << endl;
}

XYZ RapidCsvReader::get_axi_xyz_by_name(const string& axi_name) const noexcept {
  assert(!bcd_AXI_.empty());
  XYZ result;

  for (const BCD* x : bcd_AXI_) {
    assert(x);
    if (x->customerInternal_ == axi_name) {
      uint row = x->row_;
      assert(row < io_tile_pin_xyz_.size());
      result = io_tile_pin_xyz_[row];
      break;
    }
  }

  return result;
}

XYZ RapidCsvReader::get_pin_xyz_by_name(const string& mode,
                                        const string& customerPin_or_ID,
                                        const string& gbox_pin_name) const {
  // 1. if customerPin_or_ID is an AXI-pin, then skip the mode="Y" check
  XYZ result = get_axi_xyz_by_name(customerPin_or_ID);
  if (result.valid())
    return result;

  // 2.
  auto fitr = modes_map_.find(mode);
  if (fitr == modes_map_.end()) return result;

  const vector<string>& mode_vector = fitr->second;

  uint num_rows = numRows();
  assert(num_rows > 1);
  assert(mode_vector.size() == num_rows);
  assert(io_tile_pin_xyz_.size() == num_rows);
  assert(bcd_.size() == num_rows);

  // 3.
  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = bcd_[i];
    if (!bcd.match(customerPin_or_ID)) continue;
    if (mode_vector[i] != "Y") continue;
    if (gbox_pin_name.empty() || bcd.fullchipName_ == gbox_pin_name) {
      result = io_tile_pin_xyz_[i];
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

  for (const auto& I : modes_map_)
    lprintf("\t  %s\n", I.first.c_str());

  return n;
}

string
RapidCsvReader::bumpName2CustomerName(const string& bump_nm) const noexcept {
  assert(!bump_nm.empty());
  uint num_rows = numRows();
  assert(num_rows > 1);
  assert(io_tile_pin_xyz_.size() == num_rows);
  assert(bcd_.size() == num_rows);

  // tmp linear search
  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = bcd_[i];
    if (bcd.bump_ == bump_nm) return bcd.customer_;
  }

  return {};
}

bool RapidCsvReader::has_io_pin(const string& pin_name_or_ID) const noexcept {
  assert(!bcd_.empty());

  for (const BCD& x : bcd_) {
    if (x.match(pin_name_or_ID)) return true;
  }
  return false;
}

bool RapidCsvReader::hasCustomerInternalName(const string& nm) const noexcept {
  assert(!bcd_.empty());
  assert(!nm.empty());

  for (const BCD* x : bcd_AXI_) {
    assert(x);
    if (x->customerInternal_ == nm)
      return true;
  }
  return false;
}

vector<string> RapidCsvReader::get_AXI_inputs() const
{
  assert(!bcd_.empty());
  assert(!bcd_AXI_.empty());

  vector<string> result;
  result.reserve(bcd_AXI_.size());

  auto is_axi_inp = [](const string& s) -> bool
  {
    size_t len = s.length();
    if (len < 4 || len > 300)
      return false;
    return (s[len-1] == 'i' &&  s[len-2] == '_'); // ends with "_i"
  };

  for (const BCD* p : bcd_AXI_) {
    assert(p);
    if (is_axi_inp(p->customerInternal_))
      result.emplace_back(p->customerInternal_);
  }

  return result;
}

vector<string> RapidCsvReader::get_AXI_outputs() const
{
  assert(!bcd_.empty());
  assert(!bcd_AXI_.empty());

  vector<string> result;
  result.reserve(bcd_AXI_.size());

  auto is_axi_out = [](const string& s) -> bool
  {
    size_t len = s.length();
    if (len < 4 || len > 300)
      return false;
    return (s[len-1] == 'o' &&  s[len-2] == '_'); // ends with "_o"
  };

  for (const BCD* p : bcd_AXI_) {
    assert(p);
    if (is_axi_out(p->customerInternal_))
      result.emplace_back(p->customerInternal_);
  }

  return result;
}

}  // namespace pinc
