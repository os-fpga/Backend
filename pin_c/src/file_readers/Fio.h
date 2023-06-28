#pragma once
//  - namespace fio - File IO
// ======== 0. Fio (common base class)
// ======== 1. MMapReader ============
// ======== 2. LineReader ============
// ======== 3. CSV_Reader ============
// ======== 4. XML_Reader ============

#ifndef __rs_file_readers_Fio_H_h_
#define __rs_file_readers_Fio_H_h_

#include "util/pinc_log.h"
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

inline void p_free(void* p) noexcept {
  if (p) ::free(p);
}

inline char* p_strdup(const char* p) noexcept {
  if (!p) return nullptr;
  return ::strdup(p);
}

using std::string;
using std::vector;

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

  Fio(const char* nm) noexcept { if (nm) fnm_ = nm; }
  Fio(const string& nm) noexcept { fnm_ = nm; }

  virtual ~Fio() {}

  virtual void reset(const char* nm, uint16_t tr = 0) noexcept;

  virtual bool makeLines(bool cutComments, bool cutNL) noexcept { return false; }

  uint16_t trace() const noexcept { return trace_; }
  void setTrace(int t) noexcept;

  size_t numLines() const noexcept { return num_lines_; }
  size_t maxLineLen() const noexcept { return max_llen_; }

  static constexpr size_t kilo2mega(size_t kb) noexcept { return (kb + 512) / 1024; }
  static constexpr size_t bytes2mega(size_t b) noexcept { return (b + 524288) / 1048576; }

  static uint64_t getHash(const char* cs) noexcept {
    if (!cs) return 0;
    if (!cs[0]) return 1;
    std::hash<std::string_view> h;
    return h(std::string_view{cs});
  }

  static bool fileAccessible(const char*) noexcept;

  static bool fileAccessible(const string& fn) noexcept {
    return fn.empty() ? false : fileAccessible(fn.c_str());
  }

  static bool regularFileExists(const char*) noexcept;

  static bool regularFileExists(const string& fn) noexcept {
    return fn.empty() ? false : regularFileExists(fn.c_str());
  }

  bool fileAccessible() const noexcept { return fnm_.empty() ? false : fileAccessible(fnm_.c_str()); }
  bool fileExists() const noexcept { return fnm_.empty() ? false : regularFileExists(fnm_.c_str()); }
  bool fileExistsAccessible() const noexcept {
    if (fnm_.empty()) return false;
    return regularFileExists(fnm_.c_str()) and fileAccessible(fnm_.c_str());
  }

  static bool split_com(const char* src, vector<string>& dat) noexcept; // comma
  static bool split_spa(const char* src, vector<string>& dat) noexcept; // space
  static bool split_pun(const char* src, vector<string>& dat) noexcept; // punctuation + space

  static const char* firstSpace(const char* src) noexcept;
  static const char* firstNonSpace(const char* src) noexcept;
  static bool isEmptyLine(const char* src) noexcept;

public:
  static constexpr size_t lr_MAX_LINE_LEN = 4194304;  //  4 MiB
  static constexpr size_t lr_DEFAULT_CAP = 50331647;  // 48 MiB

protected:
  uint16_t trace_ = 0;
};  // Fio

inline bool file_accessible(const char* fn) noexcept { return Fio::fileAccessible(fn); }
inline bool file_accessible(const string& fn) noexcept { return Fio::fileAccessible(fn); }

inline bool regular_file_exists(const char* fn) noexcept { return Fio::regularFileExists(fn); }
inline bool regular_file_exists(const string& fn) noexcept { return Fio::regularFileExists(fn); }

inline bool file_exists_accessible(const char* fn) noexcept {
  return Fio::regularFileExists(fn) && Fio::fileAccessible(fn);
}
inline bool file_exists_accessible(const string& fn) noexcept {
  return Fio::regularFileExists(fn) && Fio::fileAccessible(fn);
}

namespace hashf {

constexpr uint64_t hashCombine(uint64_t a, uint64_t b) noexcept {
  constexpr uint64_t m = 0xc6a4a7935bd1e995;
  return (a ^ (b * m + (a << 6) + (a >> 2))) + 0xe6546b64;
}

// Fowler,Noll,Vo FNV-64 hash function
inline size_t hf_FNV64(const char* z) noexcept
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
inline size_t hf_FNV64(const char* z, size_t len) noexcept
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

inline size_t hf_std(const char* z) noexcept
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
  static constexpr size_t MIN_SIZE_for_MMAP = 2048;  // half-page

  char* buf_ = nullptr;
  int fd_ = -1;
  size_t mm_len_ = 0;

public:
  MMapReader() noexcept = default;

  MMapReader(const char* nm) noexcept : Fio(nm) {}

  MMapReader(const string& nm) noexcept : Fio(nm) {}

  virtual ~MMapReader();

  virtual void reset(const char* nm, uint16_t tr = 0) noexcept override;

  bool isMapped() const noexcept { return mm_len_ > 0; }

  bool read() noexcept;

  virtual bool makeLines(bool cutComments, bool cutNL) noexcept override;

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

  char* skipLine(char* curL) noexcept;
  bool advanceLine(char*& curL) noexcept;

  uint64_t hashSum() const noexcept;
  int dprint1() const noexcept;
  int dprint2() const noexcept;
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
  LineReader(const char* nm, size_t iniCap = 0) noexcept;
  LineReader(const string& nm, size_t iniCap = 0) noexcept;

  virtual ~LineReader() { p_free(buf_); }

  virtual void reset(const char* nm, uint16_t tr = 0) noexcept override;

  LineReader(const LineReader&) = delete;
  LineReader& operator=(const LineReader&) = delete;

  char* curLine() noexcept { return curLine_; }
  const char* curLine() const noexcept { return curLine_; }

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
  const char* headLine_ = nullptr;
  size_t num_commas_ = 0;
  bool valid_csv_ = false;

  // data matrix:
  vector<string> header_;

  // number of rows and columns, without header_
  size_t nr_ = 0, nc_ = 0;

public:
  CSV_Reader() noexcept = default;

  CSV_Reader(const char* nm) noexcept : MMapReader(nm) {}

  CSV_Reader(const string& nm) noexcept : MMapReader(nm) {}

  virtual ~CSV_Reader();

  virtual void reset(const char* nm, uint16_t tr = 0) noexcept override;

  bool parse(bool cutComments = false) noexcept;

  bool isValidCsv() const noexcept;

  bool readCsv(bool cutComments = false) noexcept;

  uint findColumn(const char* colName) const noexcept;

  vector<string> getColumn(const char* colName) const noexcept;

  vector<string> getColumn(const string& colName) const noexcept {
    if (colName.empty()) return {};
    return getColumn(colName.c_str());
  }

  vector<int> getColumnInt(const char* colName) const noexcept;

  vector<int> getColumnInt(const string& colName) const noexcept {
    if (colName.empty()) return {};
    return getColumnInt(colName.c_str());
  }

  int dprint1() const noexcept;

  static size_t countCommas(const char* src) noexcept;

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
  using APair = std::pair<const char*, const char*>;

  struct Visitor;  // : public  tinxml2::XMLVisitor

  XMLDocument*            doc_ = nullptr;
  vector<const Element*>  elems_;
  vector<const Text*>     texts_;

  const char* headLine_ = nullptr;
  bool valid_xml_ = false;
  bool has_device_list_ = false;

  // number of rows and columns in device xml
  size_t nr_ = 0, nc_ = 0;

public:
  XML_Reader() noexcept;
  XML_Reader(const char* nm) noexcept;
  XML_Reader(const string& nm) noexcept;

  virtual ~XML_Reader();

  virtual void reset(const char* nm, uint16_t tr = 0) noexcept override;

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

inline bool is_integer(const char* z) noexcept {
  if (!z || !z[0]) return false;
  if (*z == '-' || *z == '+') z++;
  if (not *z) return false;

  for (; *z; z++) {
    bool is_digit = (uint32_t(*z) - '0' < 10u);
    if (!is_digit) return false;
  }

  return true;
}

inline bool is_uint(const char* z) noexcept {
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
