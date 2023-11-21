#include "pinc_Fio.h"
#include <set>

namespace fio {

using namespace pinc;
using namespace std;

// ======== 3. CSV_Reader ==============================================

CSV_Reader::~CSV_Reader() {
  free_num_matrix();
  free_str_matrix();
}

void CSV_Reader::reset(const char* nm, uint16_t tr) noexcept {
  MMapReader::reset(nm, tr);

  free_num_matrix();
  free_str_matrix();

  num_lines_ = 0;
  num_commas_ = 0;
  valid_csv_ = false;
  headLine_ = nullptr;
  header_.clear();
  lowHeader_.clear();
  nr_ = nc_ = 0;
}

bool CSV_Reader::readCsv(bool cutComments) noexcept {
  bool ok = read();
  if (!ok) return false;

  ok = parse(cutComments);
  return ok;
}

bool CSV_Reader::writeCsv(const string& fn, uint minRow, uint maxRow) const noexcept
{
  uint16_t tr = trace();
  if (fn.empty())
    return false;
  if (not isValidCsv())
    return false;
  if (!smat_ || !nmat_)
    return false;
  if (nr_ < 2 || nc_ < 2)
    return false;
  if (header_.empty())
    return false;

  bool ok = true;
  std::ofstream fos(fn);

  if (fos.is_open()) {
    ok = printCsv(fos, minRow, maxRow);
    if (tr >= 2)
      lprintf("writeCsv:  written %s  ok:%i\n", fn.c_str(), ok);
  } else {
    if (tr >= 2)
      lprintf("\nwriteCsv:  could not open file for writing: %s\n\n", fn.c_str());
    ok = false;
  }

  return ok;
}

bool CSV_Reader::printCsv(std::ostream& os, uint minRow, uint maxRow) const noexcept
{
  if (not isValidCsv())
    return false;
  if (!smat_ || !nmat_)
    return false;
  if (nr_ < 2 || nc_ < 2)
    return false;
  if (header_.empty())
    return false;
  assert(header_.size() == nc_);
  assert(maxRow > 1);
  assert(minRow <= maxRow);

  static std::set<string> skip_cols = {
    "Remark", "Voltage2", "Discription", "Power Pad", "Voltage", "Mbist Mode",
    "Scan Mode", "Debug Mode", "ALT Function", "MODE_ETH", "MODE_USB",
    "MODE_UART0", "MODE_UART1", "MODE_I2C", "MODE_SPI0",
    "MODE_PWM", "MODE_DDR", "Ref clock", "IS_FPGA_GPIO", "Main Function",
    "Identifier", "Direction", "Type", "BANK", "MODE_MIPI"
  };
  uint skipped_cnt = 0;

  os << header_[0];
  for (size_t c = 1; c < nc_; c++) {
    if (nc_ > 2 && skip_cols.count(header_[c])) {
      skipped_cnt++;
      continue;
    }
    os << ',' << header_[c];
  }
  os << endl;

  if (minRow > maxRow)
    minRow = 0;

  for (size_t r = minRow; r < nr_; r++) {
    const string* row = smat_[r];
    assert(row);
    os << row[0];
    for (size_t c = 1; c < nc_; c++) {
      if (nc_ > 2 && skip_cols.count(header_[c]))
        continue;
      os << ',' << row[c];
    }
    os << endl;
    if (r > maxRow)
      break;
  }

  if (skipped_cnt && trace() >= 5) {
    lprintf("printCsv() skipped %u columns\n", skipped_cnt);
  }

  return true;
}

bool CSV_Reader::parse(bool cutComments) noexcept {
  if (!sz_ || !fsz_) return false;
  if (!buf_) return false;
  if (sz_ < 4) return false;

  num_lines_ = 0;
  num_commas_ = 0;
  valid_csv_ = false;
  headLine_ = nullptr;
  header_.clear();
  lowHeader_.clear();
  nr_ = nc_ = 0;
  smat_ = nullptr;
  nmat_ = nullptr;

  // 1. replace '\n' by 0 and count lines and commas
  for (size_t i = 0; i < sz_; i++) {
    if (buf_[i] == '\n') {
      buf_[i] = 0;
      num_lines_++;
    }
    if (buf_[i] == ',') num_commas_++;
  }

  if (num_lines_ < 2 || num_commas_ < 2) return false;

  // 2. cut possible '\r' at end of lines (DOS/windows CR)
  for (size_t i = 1; i < sz_; i++) {
    if (buf_[i] == 0 && buf_[i-1] == '\r') {
      buf_[i-1] = 0;
    }
  }

  // 2. make lines_
  bool ok = makeLines(cutComments, false);
  if (!ok) return false;

  // 3. count non-empty lines
  size_t nel = 0;
  headLine_ = nullptr;
  for (size_t li = 1; li < lines_.size(); li++) {
    char* line = lines_[li];
    if (isEmptyLine(line)) continue;
    nel++;
    if (!headLine_) headLine_ = line;
  }
  if (nel < 2) return false;
  if (!headLine_) return false;

  if (trace() >= 5) {
    lprintf("_csv:  #nonEmptyLines= %zu  len(headLine_)= %zu\n  headLine_: %s\n",
            nel, ::strlen(headLine_), headLine_);
  }

  // 4. split headLine_, make header_
  ok = split_com(headLine_, header_);
  if (!ok) return false;

  // 5. header_ size() is the number of columns.
  //    data rows should now have more columns than the header.
  if (header_.size() < 2 || header_.size() > size_t(INT_MAX))
    return false;
  nc_ = header_.size();
  if (nc_ < 2)
    return false;
  lowHeader_.resize(nc_);
  for (size_t c = 0; c < nc_; c++)
    lowHeader_[c] = str::sToLower(header_[c]);

  nr_ = nel - 1;
  if (nr_ < 1) return false;
  if (trace() >= 3) {
    lprintf("CReader::parse()  nr_= %zu  nc_= %zu\n", nr_, nc_);
  }
  for (size_t li = 1; li <= num_lines_; li++) {
    char* line = lines_[li];
    if (isEmptyLine(line)) continue;
    size_t nCom = countCommas(line);
    if (!nCom || nCom >= nc_) {
      if (trace() >= 2) {
        lprintf("CReader::parse() ERROR: unexpected #commas(%zu) at line %zu of %s\n", nCom, li,
                fnm_.c_str());
      }
      return false;
    }
  }

  // 6. allocate matrixes
  //
  alloc_num_matrix();
  assert(nmat_);
  //
  alloc_str_matrix();
  assert(smat_);

  // 6. populate string matrix
  //
  vector<string> V;
  V.reserve(nc_ + 1);
  size_t lcnt = 0;
  for (size_t li = 1; li < lines_.size(); li++) {
    char* line = lines_[li];
    if (isEmptyLine(line)) continue;
    if (line == headLine_) continue;
    V.clear();
    ok = split_com(line, V);
    if (!ok) {
      return false;
    }
    assert(!V.empty());
    assert(V.size() <= nc_);
    if (V.size() < nc_) V.resize(nc_);
    string* row = smat_[lcnt];
    assert(row);
    for (size_t i = 0; i < nc_; i++) row[i] = V[i];
    lcnt++;
  }
  if (trace() >= 5) lprintf("lcnt= %zu\n", lcnt);
  assert(lcnt == nr_);

  // 7. populate number-martrix
  //
  V.clear();
  lcnt = 0;
  for (size_t li = 1; li < lines_.size(); li++) {
    char* line = lines_[li];
    if (isEmptyLine(line)) continue;
    if (line == headLine_) continue;
    V.clear();
    ok = split_com(line, V);
    if (!ok) {
      return false;
    }
    assert(!V.empty());
    assert(V.size() <= nc_);
    if (V.size() < nc_) V.resize(nc_);
    int* row = nmat_[lcnt];
    assert(row);
    for (size_t i = 0; i < nc_; i++) {
      if (V[i].empty()) {
        row[i] = -1;
        continue;
      }
      const char* cs = V[i].c_str();
      if (!is_integer(cs)) {
        row[i] = -1;
        continue;
      }
      row[i] = ::atoi(cs);
    }
    lcnt++;
  }
  if (trace() >= 5) lprintf("lcnt= %zu\n", lcnt);
  assert(lcnt == nr_);

  valid_csv_ = true;
  return true;
}

void CSV_Reader::alloc_num_matrix() noexcept {
  assert(!nmat_);
  assert(nr_ > 0 && nc_ > 1);

  nmat_ = new int*[nr_ + 2];
  for (size_t r = 0; r < nr_ + 2; r++) {
    nmat_[r] = new int[nc_ + 2];
    ::memset(nmat_[r], 0, (nc_ + 2) * sizeof(int));
  }
}

void CSV_Reader::alloc_str_matrix() noexcept {
  assert(!smat_);
  assert(nr_ > 0 && nc_ > 1);

  smat_ = new string*[nr_ + 2];
  for (size_t r = 0; r < nr_ + 2; r++) {
    smat_[r] = new string[nc_ + 2];
  }
}

void CSV_Reader::free_num_matrix() noexcept {
  if (!nmat_) {
    nr_ = nc_ = 0;
    return;
  }
  if (nr_ < 2) {
    nmat_ = nullptr;
    return;
  }

  for (size_t r = 0; r < nr_ + 2; r++) delete[] nmat_[r];

  delete[] nmat_;
  nmat_ = nullptr;
}

void CSV_Reader::free_str_matrix() noexcept {
  if (!smat_) {
    nr_ = nc_ = 0;
    return;
  }
  if (nr_ < 2) {
    smat_ = nullptr;
    return;
  }

  for (size_t r = 0; r < nr_ + 2; r++) delete[] smat_[r];

  delete[] smat_;
  smat_ = nullptr;
}

bool CSV_Reader::isValidCsv() const noexcept {
  if (!sz_ || !fsz_) return false;
  if (!buf_) return false;

  if (num_lines_ < 2 || num_commas_ < 2) return false;

  return valid_csv_;
}

uint CSV_Reader::findColumn(const char* colName) const noexcept {
  if (!colName || !colName[0]) return UINT_MAX;
  if (!isValidCsv()) return UINT_MAX;
  if (!smat_ || nr_ < 2 || nc_ < 2 || header_.empty()) return UINT_MAX;

  assert(nc_ == header_.size());
  assert(nc_ < UINT_MAX);

  uint idx = UINT_MAX;
  for (size_t i = 0; i < nc_; i++) {
    if (header_[i] == colName) {
      idx = i;
      break;
    }
  }
  return idx;
}

vector<string> CSV_Reader::getColumn(const char* colName) const noexcept {
  assert(nmat_);
  uint idx = findColumn(colName);
  if (idx == UINT_MAX) {
    if (trace() >= 3) lprintf("CSV_Reader: column not found: %s\n", colName);
    return {};
  }

  vector<string> result;
  result.resize(nr_);
  for (size_t r = 0; r < nr_; r++) {
    assert(smat_[r]);
    result[r] = smat_[r][idx];
  }

  return result;
}

vector<int> CSV_Reader::getColumnInt(const char* colName) const noexcept {
  assert(nmat_);
  uint idx = findColumn(colName);
  if (idx == UINT_MAX) {
    if (trace() >= 3) lprintf("CSV_Reader: column not found: %s\n", colName);
    return {};
  }

  vector<int> result;
  result.resize(nr_);
  for (size_t r = 0; r < nr_; r++) {
    assert(nmat_[r]);
    result[r] = nmat_[r][idx];
  }

  return result;
}

int CSV_Reader::dprint1() const noexcept {
  lprintf("  fname: %s\n", fnm_.c_str());
  lprintf("    fsz_ %zu  sz_= %zu  num_lines_= %zu  num_commas_=%zu  nr_=%zu  nc_=%zu\n", fsz_, sz_,
          num_lines_, num_commas_, nr_, nc_);

  lprintf("    valid_csv_: %i\n", valid_csv_);
  lprintf("    headLine_: %s\n", headLine_ ? headLine_ : "(NULL)");
  lprintf("    header_.size()= %zu\n", header_.size());

  logVec(header_, "    header_:");

  lputs();
  logVec(lowHeader_, "    lowHeader_:");
  lputs();

  lprintf("    lines_.size()= %zu\n", lines_.size());
  if (lines_.size() > 3 && lines_[2] && nc_ < 400) lprintf("    lines_[2]  %s\n", lines_[2]);

  return sz_;
}

size_t CSV_Reader::countCommas(const char* src) noexcept {
  if (!src || !src[0]) return 0;
  size_t cnt = 0;
  for (const char* p = src; *p; p++) {
    if (*p == ',') cnt++;
  }
  return cnt;
}

}  // NS fio

