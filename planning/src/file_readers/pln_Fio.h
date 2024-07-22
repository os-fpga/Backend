#pragma once
//  - namespace fio - File IO
// ======== 0. Fio (common base class)
// ======== 1. MMapReader ============
// ======== 2. LineReader ============
// ======== 3. CSV_Reader ============
// ======== 4. XML_Reader ============

#ifndef _pln_file_readers_Fio_H_h_8b7b14eef966_
#define _pln_file_readers_Fio_H_h_8b7b14eef966_

#include "util/pln_log.h"
#include <string_view>

namespace tinxml2 {
class XMLDocument;
class XMLElement;
class XMLAttribute;
class XMLComment;
class XMLText;
class XMLDeclaration;
class XMLUnknown;
class XMLPrinter;
}

namespace fio {

using std::string;
using std::vector;
using CStr = const char*;

inline void p_free(void* p) noexcept {
  if (p) ::free(p);
}

inline char* p_strdup(CStr p) noexcept {
  if (!p) return nullptr;
  return ::strdup(p);
}

struct Info {
  string name_, absName_;
  size_t size_ = 0;
  bool exists_ = false;
  bool accessible_ = false;
  bool absolute_ = false;

public:
  Info() noexcept = default;

  Info(CStr nm) noexcept;
  Info(const string& nm) noexcept;
  void init() noexcept;

  static string get_abs_name(const string& nm) noexcept;
  static string get_realpath(const string& nm) noexcept;
  static string get_basename(const string& nm) noexcept;
};

class Fio {
public:
  string fnm_;

  size_t sz_ = 0;         // data buffer size
  size_t fsz_ = 0;        // file size
  size_t num_lines_ = 0;  // number of lines
  size_t max_llen_ = 0;   // max line length

  vector<char*> lines_;

public:
  Fio() noexcept = default;

  const string& fileName() const noexcept { return fnm_; }

  Fio(CStr nm) noexcept { if (nm) fnm_ = nm; }
  Fio(const string& nm) noexcept { fnm_ = nm; }

  virtual ~Fio() {}

  virtual void reset(CStr nm, uint16_t tr = 0) noexcept;

  virtual bool makeLines(bool cutComments, bool cutNL) noexcept { return false; }

  bool hasLines() const noexcept {
    if (!sz_ || !num_lines_)
      return false;
    for (CStr z : lines_) {
      if (z && z[0])
        return true;
    }
    return false;
  }
  void copyLines(vector<string>& out) const noexcept;

  uint16_t trace() const noexcept { return trace_; }
  void setTrace(int t) noexcept;

  size_t numLines() const noexcept { return num_lines_; }
  size_t maxLineLen() const noexcept { return max_llen_; }

  static constexpr size_t kilo2mega(size_t kb) noexcept { return (kb + 512) / 1024; }
  static constexpr size_t bytes2mega(size_t b) noexcept { return (b + 524288) / 1048576; }

  static uint64_t getHash(CStr cs) noexcept {
    if (!cs) return 0;
    if (!cs[0]) return 1;
    std::hash<std::string_view> h;
    return h(std::string_view{cs});
  }

  static bool fileAccessible(CStr) noexcept;

  static bool fileAccessible(const string& fn) noexcept {
    return fn.empty() ? false : fileAccessible(fn.c_str());
  }

  static bool regularFileExists(CStr) noexcept;
  static bool nonEmptyFileExists(CStr) noexcept;

  static bool regularFileExists(const string& fn) noexcept {
    return fn.empty() ? false : regularFileExists(fn.c_str());
  }
  static bool nonEmptyFileExists(const string& fn) noexcept {
    return fn.empty() ? false : nonEmptyFileExists(fn.c_str());
  }

  bool fileAccessible() const noexcept { return fnm_.empty() ? false : fileAccessible(fnm_.c_str()); }
  bool fileExists() const noexcept { return fnm_.empty() ? false : regularFileExists(fnm_.c_str()); }
  bool fileExistsAccessible() const noexcept {
    if (fnm_.empty()) return false;
    return regularFileExists(fnm_.c_str()) and fileAccessible(fnm_.c_str());
  }

  static bool split_com(CStr src, vector<string>& dat) noexcept; // comma
  static bool split_spa(CStr src, vector<string>& dat) noexcept; // space
  static bool split_pun(CStr src, vector<string>& dat) noexcept; // punctuation + space

  static CStr firstSpace(CStr src) noexcept;
  static CStr firstNonSpace(CStr src) noexcept;
  static bool isEmptyLine(CStr src) noexcept;

public:
  static constexpr size_t lr_MAX_LINE_LEN = 4194304;  //  4 MiB
  static constexpr size_t lr_DEFAULT_CAP = 50331647;  // 48 MiB

protected:
  uint16_t trace_ = 0;
};  // Fio

inline bool file_accessible(CStr fn) noexcept { return Fio::fileAccessible(fn); }
inline bool file_accessible(const string& fn) noexcept { return Fio::fileAccessible(fn); }

inline bool regular_file_exists(CStr fn) noexcept { return Fio::regularFileExists(fn); }
inline bool regular_file_exists(const string& fn) noexcept { return Fio::regularFileExists(fn); }

inline bool file_exists_accessible(CStr fn) noexcept {
  return Fio::regularFileExists(fn) && Fio::fileAccessible(fn);
}
inline bool file_exists_accessible(const string& fn) noexcept {
  return Fio::regularFileExists(fn) && Fio::fileAccessible(fn);
}

namespace hashf {

// Fowler,Noll,Vo FNV-64 hash function
inline size_t hf_FNV64(CStr z) noexcept
{
  if (!z) return 0;
  if (!z[0]) return 1;
  constexpr size_t FNV_64_H0 = 14695981039346656037ULL;
  size_t h = FNV_64_H0;
  for (; *z; z++) {
    h += (h << 1) + (h << 4) + (h << 5) + (h << 7) + (h << 8) + (h << 40);
    h ^= *z;
  }
  return h;
}
inline size_t hf_FNV64(CStr z, size_t len) noexcept
{
  if (!z || !len) return 0;
  if (!z[0]) return 1;
  constexpr size_t FNV_64_H0 = 14695981039346656037ULL;
  size_t h = FNV_64_H0;
  for (size_t i = 0; i < len; i++) {
    h += (h << 1) + (h << 4) + (h << 5) + (h << 7) + (h << 8) + (h << 40);
    h ^= z[i];
  }
  return h;
}

inline size_t hf_std(CStr z) noexcept
{
  if (!z) return 0;
  if (!z[0]) return 1;
  std::hash<std::string_view> h;
  return h(std::string_view{z});
}

} // NS hashf

// ======== 1. MMapReader ============

class MMapReader : public Fio
{
public:
  static constexpr size_t MIN_SIZE_for_MMAP = 4098;  // > 1 page

  char* buf_ = nullptr;
  size_t mm_len_ = 0;
  int fd_ = -1;
  int16_t lastByte_ = -1;
  bool pad_ = false;

public:
  MMapReader() noexcept = default;

  MMapReader(CStr nm) noexcept : Fio(nm) {}

  MMapReader(const string& nm) noexcept : Fio(nm) {}

  virtual ~MMapReader();

  virtual void reset(CStr nm, uint16_t tr = 0) noexcept override;

  bool isMapped() const noexcept { return mm_len_ > 0; }

  bool read() noexcept;

  virtual bool makeLines(bool cutComments, bool cutNL) noexcept override;

  size_t escapeNL() noexcept; // by '\'

  int64_t countLines() const noexcept;

  size_t setNumLines() noexcept {
    num_lines_ = 0;
    int64_t n = countLines();
    if (n > 0)
        num_lines_ = n;
    return num_lines_;
  }

  int64_t countWC(int64_t& numWords) const noexcept;  // ~ wc command
  int64_t printWC(std::ostream& os) const noexcept;   // ~ wc command

  size_t printLines(std::ostream& os) noexcept;
  int64_t writeFile(const string& nm) noexcept;

  char* skipLine(char* curL) noexcept;
  bool advanceLine(char*& curL) noexcept;

  bool printMagic(std::ostream& os) const noexcept;
  bool isGoodForParsing() const noexcept;

  uint64_t hashSum() const noexcept;
  int dprint1() const noexcept;
  int dprint2() const noexcept;

private:
  bool is_text_file(string& label) const noexcept;

  vector<char*> get_backslashed_block(size_t fromLine) noexcept;

}; // MMapReader


// ======== 2. LineReader ============

struct LineReader : public Fio
{
  char* buf_ = 0;
  char* curLine_ = 0;
  mutable char* curLine2_ = 0;

  size_t cap_ = lr_DEFAULT_CAP;

public:
  LineReader(size_t iniCap = 0) noexcept;
  LineReader(CStr nm, size_t iniCap = 0) noexcept;
  LineReader(const string& nm, size_t iniCap = 0) noexcept;

  virtual ~LineReader() { p_free(buf_); }

  virtual void reset(CStr nm, uint16_t tr = 0) noexcept override;

  LineReader(const LineReader&) = delete;
  LineReader& operator=(const LineReader&) = delete;

  char* curLine() noexcept { return curLine_; }
  CStr curLine() const noexcept { return curLine_; }

  void reserve() noexcept;

  char* getFreeMem() noexcept {
    assert(buf_);
    if (cap_ - sz_ < lr_MAX_LINE_LEN + 1) reserve();
    return buf_ + sz_;
  }

  static char* skipLine(char* p) noexcept {
    while (*p) p++;
    p++;
    return p;
  }

  bool advanceLine() noexcept;
  bool advanceLine2() const noexcept;
  void reIterate() noexcept { curLine_ = buf_; }
  void reIterate2() const noexcept { curLine2_ = buf_; }

  bool readBuffer() noexcept;
  bool readStdin() noexcept;
  bool readPipe() noexcept;

  bool read(bool mkLines, bool cutComments = false) noexcept;

  virtual bool makeLines(bool cutComments, bool cutNL) noexcept override;

  vector<string> getWords() const noexcept;

  uint64_t hashSum() const noexcept;

  int dprint1() const noexcept;

  void print(std::ostream& os, bool nl) const noexcept;
  size_t printLines(std::ostream& os) const noexcept;
  void dump(bool nl) const noexcept;
};  // LineReader


// ======== 3.  CSV_Reader ============

class CSV_Reader : public MMapReader
{
public:
  CStr headLine_ = nullptr;
  size_t num_commas_ = 0;
  bool valid_csv_ = false;

  vector<string> header_;     // original header
  vector<string> lowHeader_;  // low-case header

  // number of rows and columns, without header_
  size_t nr_ = 0, nc_ = 0;

public:
  CSV_Reader() noexcept = default;

  CSV_Reader(CStr nm) noexcept : MMapReader(nm) {}

  CSV_Reader(const string& nm) noexcept : MMapReader(nm) {}

  virtual ~CSV_Reader();

  virtual void reset(CStr nm, uint16_t tr = 0) noexcept override;

  bool parse(bool cutComments = false) noexcept;

  bool isValidCsv() const noexcept;

  bool readCsv(bool cutComments = false) noexcept;

  uint findColumn(CStr colName) const noexcept;

  vector<string> getColumn(CStr colName) const noexcept;

  vector<string> getColumn(const string& colName) const noexcept {
    if (colName.empty()) return {};
    return getColumn(colName.c_str());
  }

  vector<int> getColumnInt(CStr colName) const noexcept;

  vector<int> getColumnInt(const string& colName) const noexcept {
    if (colName.empty()) return {};
    return getColumnInt(colName.c_str());
  }

  const string* getRow(uint r) const noexcept {
    assert(isValidCsv());
    assert(smat_);
    assert(nr_ > 0);
    assert(r < nr_);
    if (!isValidCsv() || !smat_)
      return nullptr;
    assert(smat_[r]);
    return smat_[r];
  }

  size_t numRows() const noexcept { return nr_; }
  size_t numCols() const noexcept { return nc_; }

  int dprint1() const noexcept;

  bool writeCsv(const string& fn, uint minRow, uint maxRow) const noexcept;
  bool printCsv(std::ostream& os, uint minRow, uint maxRow) const noexcept;

  static size_t countCommas(CStr src) noexcept;

private:
  void alloc_num_matrix() noexcept;
  void alloc_str_matrix() noexcept;
  void free_num_matrix() noexcept;
  void free_str_matrix() noexcept;

  string** smat_ = nullptr;  // string matrix
  int** nmat_ = nullptr;     // number matrix

};  // CSV_Reader


// ======== 4.  XML_Reader ============

class XML_Reader : public MMapReader
{
public:
  using XMLDocument = ::tinxml2::XMLDocument;
  using Element = ::tinxml2::XMLElement;
  using Text = ::tinxml2::XMLText;

  // attribute name=value pair
  using APair = std::pair<CStr, CStr>;

  struct Visitor;  // : public  tinxml2::XMLVisitor

  XMLDocument*            doc_ = nullptr;
  vector<const Element*>  elems_;
  vector<const Text*>     texts_;

  CStr headLine_ = nullptr;
  bool valid_xml_ = false;
  bool has_device_list_ = false;

  // number of rows and columns in device xml
  size_t nr_ = 0, nc_ = 0;

public:
  XML_Reader() noexcept;
  XML_Reader(CStr nm) noexcept;
  XML_Reader(const string& nm) noexcept;

  virtual ~XML_Reader();

  virtual void reset(CStr nm, uint16_t tr = 0) noexcept override;

  bool parse() noexcept;

  bool isValidXml() const noexcept;

  bool readXml() noexcept;

  int64_t countLeaves() const noexcept;

  int dprint1() const noexcept;
  int printNodes() const noexcept;
  int printLeaves() const noexcept;

private:
  static vector<APair> get_attrs(const Element& elem) noexcept;

};  // XML_Reader

bool addIncludeGuards(LineReader& lr) noexcept;

inline bool c_is_digit(char c) noexcept { return uint32_t(c) - '0' < 10u; }
inline bool c_is_dot_digit(char c) noexcept { return uint32_t(c) - '0' < 10u or c == '.'; }

inline bool has_digit(CStr z) noexcept {
  if (!z || !z[0]) return false;

  for (; *z; z++) {
    bool is_digit = (uint32_t(*z) - '0' < 10u);
    if (is_digit) return true;
  }
  return false;
}

inline bool has_digit(CStr s, size_t len) noexcept {
  if (!s or !len) return false;

  for (size_t i = 0; i < len; i++) {
    bool is_digit = (uint32_t(s[i]) - '0' < 10u);
    if (is_digit) return true;
  }
  return false;
}

inline size_t count_digits(CStr z) noexcept {
  if (!z || !z[0]) return 0;

  size_t cnt = 0;
  for (; *z; z++) {
    bool is_digit = (uint32_t(*z) - '0' < 10u);
    if (is_digit)
      cnt++;
  }
  return cnt;
}

inline bool is_integer(CStr z) noexcept {
  if (!z || !z[0]) return false;
  if (*z == '-' || *z == '+') z++;
  if (not *z) return false;

  for (; *z; z++) {
    bool is_digit = (uint32_t(*z) - '0' < 10u);
    if (!is_digit) return false;
  }
  return true;
}

inline bool is_uint(CStr z) noexcept {
  if (!z || !z[0]) return false;
  if (*z == '+') z++;
  if (not *z) return false;

  for (; *z; z++) {
    bool is_digit = (uint32_t(*z) - '0' < 10u);
    if (!is_digit) return false;
  }
  return true;
}

}  // NS fio

#endif
