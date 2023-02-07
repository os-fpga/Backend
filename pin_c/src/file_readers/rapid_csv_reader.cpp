#include "rapid_csv_reader.h"

#include <algorithm>
#include <set>

#include "rapidcsv.h"

namespace pinc {

using namespace std;

// returns spreadsheet column label ("A", "B", "BC", etc) for column index 'i'
static inline string label_column(int i) noexcept
{
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

RapidCsvReader::RapidCsvReader()
{
  // old mode for EDA-1057
  // use_bump_column_B_ = false; // true - old mode, false - new mode
}

RapidCsvReader::~RapidCsvReader()
{
}

bool RapidCsvReader::read_csv(const string& fn, bool check) {
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2)
    ls << "RapidCsvReader::read_csv( " << fn << " )  Reading data ..." << endl;

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
    if (tr >= 4) {
      for (uint i = 0; i < header_data.size(); i++) {
        string col_label = label_column(i);
        const string& hdr_i = header_data[i];
        ls << "--- " << i << '-' << col_label << "  hdr_i= " << hdr_i << endl;
      }
      ls << endl;
    }
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

  if (tr >= 3) {
    ls << "  num_rows= " << num_rows << "  num_cols= " << header_data.size()
       << "  start_position_= " << start_position_ << '\n' << endl;
  }

  constexpr bool STRICT_FORMAT = false;

  bool has_Fullchip_name = false;
  vector<string> mode_data;
  mode_names_.reserve(num_rows + 1);
  for (uint i = 0; i < header_data.size(); i++) {
    string col_label = label_column(i);
    const string& hdr_i = header_data[i];

    if (!has_Fullchip_name && hdr_i == "Fullchip_NAME")
      has_Fullchip_name = true;

    if (hdr_i.find("Mode_") == string::npos)
      continue;

    if (tr >= 4)
      ls << "  (Mode_) " << i << '-' << col_label << "  hdr_i= " << hdr_i << endl;

    try {
      mode_data = doc.GetColumn<string>(hdr_i);
    }
    catch (const std::out_of_range& e) {
      if (STRICT_FORMAT) {
        ls << "\nRapidCsvReader::read_csv() caught std::out_of_range"
           << "  i= " << i << "  hdr_i= " << hdr_i << endl;
        ls << "\t  what: " << e.what() << '\n' << endl;
        return false;
      } else {
        continue;
      }
    }
    catch (...) {
      if (STRICT_FORMAT) {
        ls << "\nRapidCsvReader::read_csv() caught excepttion"
           << "  i= " << i << "  hdr_i= " << hdr_i << '\n' << endl;
        return false;
      } else {
        continue;
      }
    }
    assert(mode_data.size() <= num_rows);
    if (mode_data.size() < num_rows) {
      if (tr >= 4)
        ls << "!!! mode_data.size() < num_rows" << endl;
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
    ls << "\nRapidCsvReader::read_csv(): column Fullchip_NAME not found\n" << endl;
    return false;
  }

  vector<string> S_tmp = doc.GetColumn<string>("Bump/Pin Name");
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  const vector<string>& bump_pin_name = S_tmp;

  if (tr >= 4) {
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

  bcd_.resize(num_rows);
  for (uint i = 0; i < num_rows; i++) {
    bcd_[i].bump_ = bump_pin_name[i];
    bcd_[i].row_ = i;
  }

  try {
    S_tmp = doc.GetColumn<string>("Customer Name");
  }
  catch (const std::out_of_range& e) {
    ls << "\nRapidCsvReader::read_csv() caught std::out_of_range on doc.GetColumn()\n";
    ls << "\t  what: " << e.what() << '\n' << endl;
    return false;
  }
  catch (...) {
    ls << "\nRapidCsvReader::read_csv() caught excepttion on doc.GetColumn()" << endl;
    return false;
  }

  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++)
    bcd_[i].customer_ = S_tmp[i];

  S_tmp = doc.GetColumn<string>("Ball ID");
  assert(S_tmp.size() > 1);
  assert(S_tmp.size() <= num_rows);
  S_tmp.resize(num_rows);
  for (uint i = 0; i < num_rows; i++)
    bcd_[i].ball_ID_ = S_tmp[i];

  if (tr >= 5) {
    ls << "+++ BCD dump::: Bump/Pin Name , Customer Name , Ball ID :::" << endl;
    for (uint i = 0; i < num_rows; i++) {
      const BCD& bcd = bcd_[i];
      ls << "\t " << bcd << endl;
    }
    ls << "--- BCD dump^^^" << '\n' << endl;
  }

  assert(has_Fullchip_name);
  fullchip_name_ = doc.GetColumn<string>("Fullchip_NAME");
  assert(fullchip_name_.size() > 1);
  assert(fullchip_name_.size() <= num_rows);
  fullchip_name_.resize(num_rows);

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
  if (check) {
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
                  "design data IO." << endl;
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
void RapidCsvReader::write_csv(string file_name) const {
  // to do
  lout() << "Not Implement Yet - Write content of interest to a csv file <"
         << file_name << ">" << endl;
  return;
}

void RapidCsvReader::print_csv() const
{
  lputs("print_csv()");
  auto& ls = lout();
  ls << "#row\tBump/Pin Name \t Customer Name \t Ball ID "
     << "\t IO_tile_pin\t IO_tile_pin_x\tIO_tile_pin_y\tIO_tile_pin_z\n";
  string dash = strReplicate('-', 111u);
  ls << dash << endl;

  uint num_rows = numRows();
  assert(bcd_.size() == num_rows);
  assert(io_tile_pin_xyz_.size() == num_rows);
  for (uint i = 0; i < num_rows; i++) {
    const BCD& b = bcd_[i];
    const XYZ& p = io_tile_pin_xyz_[i];
    lprintf("%-5u ", i);
    lprintf("%12s ", b.bump_.c_str());
    lprintf("%22s ", b.customer_.c_str());
    lprintf("%6s ", b.ball_ID_.c_str());
    ls << "\t " << io_tile_pin_[i] << "\t"
       << p.x_ << "\t" << p.y_ << "\t" << p.z_ << endl;
  }

  ls << dash << endl;
  ls << "Total Records: " << num_rows << endl;
}

XYZ RapidCsvReader::get_pin_xyz_by_name(
    const string& mode, const string& customerPin_or_ID,
    const string& gbox_pin_name) const {
  XYZ result;
  auto fitr = modes_map_.find(mode);
  if (fitr == modes_map_.end()) return result;

  const vector<string>& mode_vector = fitr->second;

  uint num_rows = numRows();
  assert(num_rows > 1);
  assert(mode_vector.size() == num_rows);
  assert(io_tile_pin_xyz_.size() == num_rows);
  assert(bcd_.size() == num_rows);

  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = bcd_[i];
    if (bcd.customer_ != customerPin_or_ID && bcd.ball_ID_ != customerPin_or_ID)
      continue;
    if (mode_vector[i] != "Y")
      continue;
    if (gbox_pin_name.empty() || fullchip_name_[i] == gbox_pin_name) {
      result = io_tile_pin_xyz_[i];
      assert(result.valid());
      break;
    }
  }

  return result;
}

string RapidCsvReader::bumpName2BallName(const string& bump_name) const noexcept {
  assert(!bump_name.empty());
  uint num_rows = numRows();
  assert(num_rows > 1);
  assert(io_tile_pin_xyz_.size() == num_rows);
  assert(bcd_.size() == num_rows);

  // tmp linear search
  for (uint i = 0; i < num_rows; i++) {
    const BCD& bcd = bcd_[i];
    if (bcd.bump_ == bump_name)
      return bcd.customer_;
  }

  return {};
}

bool RapidCsvReader::has_io_pin(const string& pin_name_or_ID) const noexcept {
  assert(!bcd_.empty());

  for (const BCD& x : bcd_) {
    if (x.customer_ == pin_name_or_ID || x.ball_ID_ == pin_name_or_ID)
      return true;
  }

  return false;
}

}  // namespace pinc
