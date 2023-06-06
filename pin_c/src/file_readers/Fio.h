//  - namespace fio - File IO
// ======== 0. Fio (common base class)
// ======== 1. MMapReader ============
// ======== 2. LineReader ============
// ======== 3. CSV_Reader ============
#pragma once
#ifndef __rs_file_readers_Fio_H_h_
#define __rs_file_readers_Fio_H_h_

#include "util/pinc_log.h"

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

  vector<char*> lines_;

public:
  Fio() noexcept = default;

  Fio(const char* nm) noexcept { if (nm) fnm_ = nm; }
  Fio(const string& nm) noexcept { fnm_ = nm; }

  virtual ~Fio() {}

  virtual void reset(const char* nm, uint16_t tr = 0) noexcept;

  uint16_t trace() const noexcept { return trace_; }
  void setTrace(int t) noexcept;

  size_t numLines() const noexcept { return num_lines_; }

  static constexpr size_t kilo2mega(size_t kb) noexcept { return (kb + 512) / 1024; }
  static constexpr size_t bytes2mega(size_t b) noexcept { return (b + 524288) / 1048576; }

  static uint64_t getHash(const char* cs) noexcept {
    if (!cs) return 0;
    if (!cs[0]) return 1;
    std::hash<std::string_view> h;
    return h(std::string_view{cs});
  }

  static constexpr uint64_t hashCombine(uint64_t a, uint64_t b) noexcept {
    constexpr uint64_t m = 0xc6a4a7935bd1e995;
    return (a ^ (b * m + (a << 6) + (a >> 2))) + 0xe6546b64;
  }

  static bool fileAccessible(const char*) noexcept;

  static bool fileAccessible(const string& fn) noexcept {
    return fn.empty() ? false : fileAccessible(fn.c_str());
  }

  static bool regularFileExists(const char*) noexcept;

  static bool regularFileExists(const string& fn) noexcept {
    return fn.empty() ? false : regularFileExists(fn.c_str());
  }

  bool fileAccessible() const noexcept {
    return fnm_.empty() ? false : fileAccessible(fnm_.c_str());
  }
  bool fileExists() const noexcept {
    return fnm_.empty() ? false : regularFileExists(fnm_.c_str());
  }
  bool fileExistsAccessible() const noexcept {
    if (fnm_.empty()) return false;
    return regularFileExists(fnm_.c_str()) and fileAccessible(fnm_.c_str());
  }

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


// ======== 1. MMapReader ============

class MMapReader : public Fio
{
public:
  //static constexpr size_t  MIN_SIZE_for_MMAP = 2; // always mmap
  static constexpr size_t MIN_SIZE_for_MMAP = 2048;  // half-page
  //static constexpr size_t  MIN_SIZE_for_MMAP = 4096; // one page
  //static constexpr size_t  MIN_SIZE_for_MMAP = lr_DEFAULT_CAP; // temporary turn off mmap

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

  bool makeLines(bool cutComments = false) noexcept;

  int64_t countLines() const noexcept;

  int64_t countWC(int64_t& numWords) const noexcept;  // ~ wc command
  int64_t printWC(std::ostream& os) const noexcept;   // ~ wc command

  char* skipLine(char* curL) noexcept;
  bool advanceLine(char*& curL) noexcept;

  uint64_t hashSum() const noexcept;
  int dprint() const noexcept;

};  // MMapReader


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

  //bool readPipe() noexcept;

  bool makeLines(bool cutComments = false) noexcept;

  vector<string> getWords() const noexcept;

  uint64_t hashSum() const noexcept;

  int dprint() const noexcept;

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

  vector<string> getColumn(const char* colName) const noexcept;

  vector<string> getColumn(const string& colName) const noexcept {
    if (colName.empty())
      return {};
    return getColumn(colName.c_str());
  }

  int dprint1() const noexcept;

  static bool split(const char* src, vector<string>& dat) noexcept;

  static const char* firstSpace(const char* src) noexcept;
  static const char* firstNonSpace(const char* src) noexcept;
  static bool isEmptyLine(const char* src) noexcept;
  static size_t countCommas(const char* src) noexcept;

private:
  void alloc_num_matrix() noexcept;
  void alloc_str_matrix() noexcept;
  void free_num_matrix() noexcept;
  void free_str_matrix() noexcept;

  string** smat_ = nullptr;  // string matrix
  double** nmat_ = nullptr;  // number matrix

};  // CSV_Reader

bool addIncludeGuards(LineReader& lr) noexcept;

}  // NS fio

#endif

