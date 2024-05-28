// File IO - namespace fio
#include "file_readers/pinc_Fio.h"
#include "file_readers/pinc_tinyxml2.h"

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <libgen.h>
#include <filesystem>

namespace fio {

using namespace pln;
using namespace std;

static constexpr uint32_t fio_MAX_STACK_USE = 1048576;  // 1 MiB

Info::Info(CStr nm) noexcept {
  if (!nm) return;
  name_ = nm;
  if (name_.empty()) return;
  init();
}

Info::Info(const string& nm) noexcept {
  name_ = nm;
  if (name_.empty()) return;
  init();
}

void Info::init() noexcept {
  namespace fs = std::filesystem;
  absName_.clear();
  size_ = 0;
  exists_ = false;
  accessible_ = false;
  absolute_ = false;
  if (name_.empty()) return;

  exists_ = regular_file_exists(name_);
  if (not exists_) return;

  accessible_ = file_accessible(name_);
  if (not accessible_) return;

  try {
    fs::path p{name_};
    size_ = fs::file_size(p);
    p = p.lexically_normal();
    absolute_ = p.is_absolute();
    p = fs::absolute(p);
    absName_ = p.string();
  } catch (...) {
    // noexcept
  }
}

string Info::get_abs_name(const string& nm) noexcept {
  namespace fs = std::filesystem;
  if (nm.empty()) return {};
  try {
    fs::path p{nm};
    p = p.lexically_normal();
    p = fs::absolute(p);
    return p.string();
  } catch (...) {
    // noexcept
  }
  return {};
}

string Info::get_realpath(const string& nm) noexcept {
  string abs_nm = get_abs_name(nm);
  if (abs_nm.empty())
    return {};

  char buf[8192];
  buf[0] = 0;
  buf[1] = 0;
  buf[8190] = 0;
  buf[8191] = 0;

  if (::realpath(abs_nm.c_str(), buf)) {
    return buf;
  } else {
    ::perror("realpath()");
    return abs_nm;
  }
}

string Info::get_basename(const string& nm) noexcept {
  size_t len = nm.length();
  if (!len or len > 8192)
    return {};

  char buf[len + 2] = {};

  ::strncpy(buf, nm.c_str(), len);

  CStr bn = ::basename(buf);
  if (!bn) bn = "";
  return bn;
}

void Fio::setTrace(int t) noexcept {
  if (t <= 0) {
    trace_ = 0;
    return;
  }
  if (t >= USHRT_MAX) {
    trace_ = USHRT_MAX;
    return;
  }
  trace_ = t;
}

bool Fio::fileAccessible(CStr fn) noexcept {
  if (!fn) return false;
  int status = ::access(fn, F_OK);
  if (status != 0) return false;
  status = ::access(fn, R_OK);
  if (status != 0) return false;
  return true;
}

bool Fio::regularFileExists(CStr fn) noexcept {
  if (!fn or !fn[0])
    return false;
  struct stat sb;
  if (::stat(fn, &sb)) return false;

  if (not(S_IFREG & sb.st_mode)) return false;

  return true;
}

bool Fio::nonEmptyFileExists(CStr fn) noexcept {
  if (!fn or !fn[0])
    return false;
  struct stat sb;
  if (::stat(fn, &sb)) return false;

  if (not(S_IFREG & sb.st_mode)) return false;

  FILE* f = ::fopen(fn, "r");
  if (!f)
    return false;

  int64_t sz = 0;
  int err = ::fseek(f, 0, SEEK_END);
  if (not err)
    sz = ::ftell(f);

  ::fclose(f);
  return sz > 1;
}

void Fio::reset(CStr nm, uint16_t tr) noexcept {
  sz_ = fsz_ = 0;
  num_lines_ = 0;
  max_llen_ = 0;
  fnm_.clear();
  setTrace(tr);
  if (nm) fnm_ = nm;
}

static inline CStr trim_front(CStr z) noexcept {
  if (z && *z) {
    while (std::isspace(*z)) z++;
  }
  return z;
}

static inline void tokenize_A(char* A, size_t len, vector<string>& dat) {
  assert(A && len);
  if (!A or !len) return;

  CStr tk = A;
  for (size_t i = 0; i <= len; i++) {
    if (A[i]) continue;
    dat.emplace_back(tk);
    size_t j = i + 1;
    for (; j <= len; j++)
      if (A[j]) break;
    if (j == len) break;
    i = j;
    tk = A + i;
  }
}

bool Fio::split_com(CStr src, vector<string>& dat) noexcept {
  dat.clear();
  assert(src);
  if (!src || !src[0]) return false;

  src = trim_front(src);

  if (!src || !src[0]) return false;

  size_t len = ::strlen(src);
  if (len < 2 || len > fio_MAX_STACK_USE) return false;

  if (!::strchr(src, ',')) return false;

  size_t cap = 2 * len + 4;
  char scratch[cap];
  scratch[len] = 0;
  scratch[len + 1] = 0;
  scratch[cap - 1] = 0;
  scratch[cap - 2] = 0;

  // copy 'src' to 'scratch', inserting space between double-commas ",,".
  char* dest = scratch;
  for (size_t i = 0; i < len - 1; i++) {
    char c = src[i];
    *dest++ = c;
    if (c == ',' && src[i + 1] == ',') *dest++ = ' ';
  }
  *dest++ = src[len - 1];
  *dest++ = '\0';
  *dest++ = '\0';
  len = ::strlen(scratch);

  // tokenize using commas as delimiter:
  for (char* p = scratch; *p; p++)
    if (*p == ',') *p = '\0';

  tokenize_A(scratch, len, dat);

  if (dat.empty()) return false;

  // single-space strings to empty strings:
  for (auto& s : dat) {
    if (s == " ") s.clear();
  }

  return true;
}

bool Fio::split_spa(CStr src, vector<string>& dat) noexcept {
  dat.clear();
  assert(src);
  if (!src || !src[0]) return false;

  src = trim_front(src);

  if (!src || !src[0]) return false;

  size_t len = ::strlen(src);
  if (len < 2 || len > fio_MAX_STACK_USE) return false;

  char scratch[len + 4];
  scratch[len] = 0;
  scratch[len + 1] = 0;

  ::strcpy(scratch, src);

  // write '\0' at delimiters
  for (char* p = scratch; *p; p++) {
    char c = *p;
    if (std::isspace(c)) {
      *p = '\0';
    }
  }

  tokenize_A(scratch, len, dat);

  if (dat.empty()) return false;

  // single-space strings to empty strings:
  for (auto& s : dat) {
    if (s == " ") s.clear();
  }

  return true;
}

bool Fio::split_pun(CStr src, vector<string>& dat) noexcept {
  dat.clear();
  assert(src);
  if (!src || !src[0]) return false;

  src = trim_front(src);

  if (!src || !src[0]) return false;

  size_t len = ::strlen(src);
  if (len < 2 || len > fio_MAX_STACK_USE) return false;

  char scratch[len + 4];
  scratch[len] = 0;
  scratch[len + 1] = 0;

  ::strcpy(scratch, src);

  // write '\0' at delimiters
  for (char* p = scratch; *p; p++) {
    char c = *p;
    if (c == ',' || std::isspace(c) || std::ispunct(c) || !std::isalnum(c)) {
      *p = '\0';
    }
  }

  tokenize_A(scratch, len, dat);

  if (dat.empty()) return false;

  // single-space strings to empty strings:
  for (auto& s : dat) {
    if (s == " ") s.clear();
  }

  return true;
}

CStr Fio::firstSpace(CStr src) noexcept {
  if (!src || !src[0]) return nullptr;
  for (CStr p = src; *p; p++)
    if (std::isspace(*p)) return p;
  return nullptr;
}

CStr Fio::firstNonSpace(CStr src) noexcept {
  if (!src || !src[0]) return nullptr;
  for (CStr p = src; *p; p++)
    if (!std::isspace(*p)) return p;
  return nullptr;
}

bool Fio::isEmptyLine(CStr src) noexcept {
  if (!src || !src[0]) return true;
  if (!firstNonSpace(src)) return true;
  size_t len = ::strnlen(src, 4);
  return len < 2;
}

void Fio::copyLines(vector<string>& out) const noexcept {
  out.clear();
  if (!hasLines() || lines_.empty()) return;

  out.reserve(lines_.size());
  for (size_t li = 1; li < lines_.size(); li++) {
    CStr line = lines_[li];
    if (line) out.emplace_back(line);
  }
}

// ======== 1. MMapReader ==============================================

MMapReader::~MMapReader() { reset(nullptr, 0); }

void MMapReader::reset(CStr nm, uint16_t tr) noexcept {
  string fnm = fnm_;
  Fio::reset(nm, tr);

  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }

  if (!buf_) {
    mm_len_ = 0;
    return;
  }

  if (!isMapped()) {
    ::free(buf_);
    buf_ = nullptr;
    return;
  }

  if (::munmap(buf_, mm_len_) == -1) {
    if (trace()) {
      ::perror("munmap");
      lout() << "\n  UNEXPECTED munmap() failure: " << fnm << '\n' << endl;
    }
  }

  buf_ = nullptr;
  mm_len_ = 0;
}

bool MMapReader::read() noexcept {
  assert(!fnm_.empty());
  uint16_t tr = trace();

  sz_ = fsz_ = 0;
  buf_ = nullptr;
  fd_ = -1;
  mm_len_ = 0;

  struct stat sb;
  if (::stat(fnm_.c_str(), &sb)) {
    if (tr >= 2) ::perror("stat");
    return false;
  }

  if (not(S_IFREG & sb.st_mode)) {
    if (tr >= 2) lout() << "not a regular file: " << fnm_ << endl;
    return false;
  }

  fsz_ = sb.st_size;
  sz_ = fsz_;
  if (sz_ == 0) {  // zero-size corner case
    buf_ = (char*)::calloc(8, 1);
    return true;
  }

  fd_ = ::open(fnm_.c_str(), O_RDONLY);
  if (fd_ < 0) {
    if (tr >= 2) {
      ::perror("open");
      lout() << "can't open file: " << fnm_ << endl;
    }
    return false;
  }

  if (fsz_ < MIN_SIZE_for_MMAP) {
    mm_len_ = 0;
    // use read() syscall instead of mmap()
    buf_ = (char*)::calloc(fsz_ + 4, 1);
    assert(buf_);
    buf_[fsz_] = 0;
    char* buf = buf_;
    int64_t ret = 0, len = fsz_;
    while (len > 0) {
      ret = ::read(fd_, buf, len);
      if (ret == 0) break;
      if (ret < 0) {
        if (errno == EINTR) continue;
        if (tr) {
          ::perror("read");
          lout() << "::read() failed: " << fnm_ << endl;
        }
        break;
      }
      len -= ret;
      buf += ret;
    }
    if (ret < 0 || len == int64_t(fsz_)) {
      if (tr) lout() << "MMapReader::read() failed: " << fnm_ << endl;
      return false;
    }
    return true;
  }

  mm_len_ = fsz_ + 4;
  void* addr = ::mmap(nullptr, mm_len_, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd_, 0);
  if (!addr || addr == MAP_FAILED) {
    if (tr) {
      if (tr >= 2) ::perror("mmap");
      lout() << "mmap() failed: " << fnm_ << endl;
    }
    ::close(fd_);
    fd_ = -1;
    return false;
  }

  buf_ = (char*)addr;
  buf_[fsz_ + 1] = 0;
  buf_[fsz_] = 0;
  assert(isMapped());

  return true;
}

int64_t MMapReader::countLines() const noexcept {
  assert(fsz_ == sz_);
  if (!fsz_ || !sz_ || !buf_) return -1;

  int64_t cnt = 0;
  for (size_t i = 0; i < sz_; i++)
    if (buf_[i] == '\n') cnt++;

  return cnt;
}

static size_t count_words(CStr line) noexcept {
  if (!line || !line[0]) return 0;
  size_t len = ::strlen(line);

  const size_t z = len + 1;
  void* mem = (z <= fio_MAX_STACK_USE ? alloca(z) : malloc(z));
  char* s = (char*)mem;
  ::memcpy(s, line, z);
  s[z] = '\0';
  for (size_t i = 0; i < z; i++) {
    if (!s[z]) s[z] = ' ';
  }
  CStr delim = " \t\n";

  size_t nw = 0;
  char* tk = strtok(s, delim);
  while (tk) {
    nw++;
    tk = strtok(nullptr, delim);
  }

  if (z > fio_MAX_STACK_USE) ::free(mem);

  return nw;
}

int64_t MMapReader::countWC(int64_t& numWords) const noexcept {
  assert(fsz_ == sz_);
  numWords = 0;
  if (!fsz_ || !sz_ || !buf_) return -1;

  if (num_lines_ && lines_.size() > 2) {
    for (size_t li = 1; li < lines_.size(); li++) {
      char* line = lines_[li];
      if (!line) continue;
      numWords += count_words(line);
    }
    return num_lines_;
  }

  const size_t z = sz_ + 1;
  void* mem = (z <= fio_MAX_STACK_USE ? alloca(z) : malloc(z));
  char* s = (char*)mem;
  ::memcpy(s, buf_, z);
  s[z] = '\0';
  for (size_t i = 0; i < z; i++) {
    if (!s[z]) s[z] = ' ';
  }
  CStr delim = " \t\n";

  char* tk = strtok(s, delim);
  while (tk) {
    numWords++;
    tk = strtok(nullptr, delim);
  }

  if (z > fio_MAX_STACK_USE) ::free(mem);

  return countLines();
}

// unix command 'wc'
int64_t MMapReader::printWC(std::ostream& os) const noexcept {
  assert(fsz_ == sz_);
  if (!fsz_ || !sz_ || !buf_ || fnm_.empty()) return -1;

  int64_t numLines, numWords = 0;
  numLines = countWC(numWords);

  os << numLines << ' ' << numWords << ' ' << sz_ << ' ' << fnm_ << endl;

  return numLines;
}

static void print_byte(std::ostream& os, CStr prefix, int b) noexcept {
  bool printable = std::ispunct(b) or std::isalnum(b);
  os_printf(os, "%s decimal= %d  hex= %x",
            prefix ? prefix : "", b, b);
  if (printable)
    os_printf(os, "  char= %c", b);
  os << endl;
  os.flush();
}

bool MMapReader::is_text_file(string& label) const noexcept {
  label.clear();
  if (!buf_ or sz_ < 4)
    return false;

  const uint* sig_p = (uint*) buf_;
  uint sig = *sig_p;

  // lprintf("\t ....... sig= %u   hex %x\n", sig, sig);

  if (sig == 0x464c457f) {
    label = "ELF";
    return false;
  }
  if (sig == 0x21726152) {
    label = "RAR";
    return false;
  }
  if (sig == 0x587a37fd) {
    label = "XZ";
    return false;
  }
  if (sig == 0xfd2fb528) {
    label = "ZST";
    return false;
  }
  if (sig == 0x425a6839) {
    label = "BZ2";
    return false;
  }
  if (sig == 0x1f8b0808) {
    label = "GZ";
    return false;
  }
  if (sig == 0x46445025) {
    label = "PDF";
    return false;
  }
  if (sig == 0x213c6172) {
    label = "AR";
    return false;
  }

  return true;
}

bool MMapReader::printMagic(std::ostream& os) const noexcept {
  assert(fsz_ == sz_);
  if (!fsz_ || !sz_ || !buf_ || fnm_.empty()) {
    os << "(empty)" << endl;
    return false;
  }
  if (sz_ == 1) {
    print_byte(os, "(single-byte)", buf_[0]);
    return false;
  }
  if (sz_ < 5) {
    os << "(tiny) size= " << sz_ << endl;
    char prefix[32] = {};
    for (uint i = 0; i < sz_; i++) {
      ::sprintf(prefix, "    byte#%u  ", i);
      print_byte(os, prefix, buf_[i]);
    }
    return false;
  }
  if (sz_ < UINT_MAX) {
    os << "(normal) size= " << sz_ << endl;
    char prefix[32] = {};
    for (uint i = 0; i < sz_; i++) {
      ::sprintf(prefix, "    byte#%u  ", i);
      print_byte(os, prefix, buf_[i]);
      if (i > 8)
        break;
    }
    string label;
    bool is_txt = is_text_file(label);
    if (is_txt) {
      os << "(guessed TEXT)" << endl;
    } else if (!label.empty()) {
      os << "(guessed BINARY) : " << label << endl;
    }
    os.flush();
    return true;
  }

  os << "(huge) size= " << sz_ << endl;
  return false;
}

bool MMapReader::isGoodForParsing() const noexcept {
  assert(fsz_ == sz_);
  if (!fsz_ || !sz_ || !buf_ || fnm_.empty())
    return false;
  if (sz_ < 5)
    return false;

  if (sz_ < UINT_MAX) {
    string label;
    bool is_txt = is_text_file(label);
    return is_txt;
  }

  // huge size
  return false;
}

size_t MMapReader::printLines(std::ostream& os) noexcept {
  if (!fsz_ || !sz_ || !buf_ || fnm_.empty()) return 0;
  bool hasl = hasLines();
  if (!hasl) {
    if (makeLines(false, true)) hasl = hasLines();
  }
  if (!hasl) return 0;

  size_t cnt = 0, sz = lines_.size();
  for (size_t i = 0; i < sz; i++) {
    CStr cs = lines_[i];
    if (!cs || !cs[0]) continue;
    cnt++;
    os << i << ": " << cs << endl;
  }
  return cnt;
}

int64_t MMapReader::writeFile(const string& nm) noexcept {
  if (nm.empty())
    return -1;
  if (!fsz_ || !sz_ || !buf_ || fnm_.empty())
    return -1;
  if (nm == fnm_)
    return -1;

  auto tr = trace();
  CStr cnm = nm.c_str();
  FILE* f = ::fopen(cnm, "w");
  if (!f) {
    if (tr >= 3) {
      flush_out(true);
      lprintf("ERROR writeFile() could not open file for writing: %s\n", cnm);
      flush_out(true);
    }
    return -1;
  }

  if (hasLines()) {
    // write line-by-line
    size_t cnt = 0, n = lines_.size();
    for (size_t i = 0; i < n; i++) {
      CStr cs = lines_[i];
      if (!cs || !cs[0]) continue;
      ::fputs(cs, f);
      ::fputc('\n', f);
      if (::ferror(f)) {
        if (tr >= 3) {
          flush_out(true);
          lprintf("ERROR writeFile() error during writing: %s\n", cnm);
          flush_out(true);
        }
        break;
      }
      cnt++;
    }
    ::fclose(f);
    return cnt;
  }

  // no lines, write raw buffer
  int64_t ret = sz_;
  ::fwrite(buf_, sz_, 1, f);
  if (::ferror(f)) {
    if (tr >= 3) {
      flush_out(true);
      lprintf("ERROR writeFile() error during writing: %s\n", cnm);
      flush_out(true);
    }
    ret = -1;
  }

  ::fclose(f);
  return ret;
}

uint64_t MMapReader::hashSum() const noexcept {
  assert(fsz_ == sz_);
  if (!fsz_ || !sz_ || !buf_) return 0;

  if (sz_ <= 16) {
    return hashf::hf_FNV64(buf_, sz_);
  }

  constexpr size_t i8 = sizeof(uint64_t);  // 8

  uint64_t sum = 0;
  size_t k = sz_ / i8;
  size_t r = sz_ - k * i8;

  for (size_t i = 0; i < k; i += i8) {
    char* p = buf_ + i;
    uint64_t a = *((uint64_t*)p);
    sum = hashf::hashCombine(a, sum);
  }

  while (r) {
    sum = hashf::hashCombine(buf_[sz_ - r], sum);
    r--;
  }

  return sum;
}

int MMapReader::dprint1() const noexcept {
  lprintf(" fname: %s\n", fnm_.c_str());
  lprintf("  fsz_ %zu  sz_= %zu  num_lines_= %zu\n", fsz_, sz_, num_lines_);
  lprintf("  mapped:%i\n", isMapped());

  return sz_;
}

int MMapReader::dprint2() const noexcept {
  dprint1();
  if (!sz_) return 0;
  if (!num_lines_ || lines_.empty()) return 0;

  lout() << "  hashSum= " << hashSum() << endl;
  lprintf("  num_lines_= %zu  lines_.size()= %zu  max_llen_= %zu\n", num_lines_, lines_.size(), max_llen_);

  int64_t numLines, numWords = 0;
  numLines = countWC(numWords);

  lout() << "[WC]  L: " << numLines << "  W: " << numWords << "  C: " << sz_ << "  " << fnm_ << endl;

  return sz_;
}

bool MMapReader::makeLines(bool cutComments, bool cutNL) noexcept {
  lines_.clear();
  max_llen_ = 0;

  if (!sz_ || !fsz_) return false;
  if (!buf_) return false;

  if (cutNL) {
    num_lines_ = 0;
    // count lines
    for (size_t i = 0; i < sz_; i++) {
      if (buf_[i] == '\n') num_lines_++;
    }
    if (!num_lines_) {
      if (trace() >= 4) {
        lputs("MReader::makeLines-1 not_OK: num_lines_ == 0");
      }
      return false;
    }
    // replace '\n' by 0
    for (size_t i = 0; i < sz_; i++) {
      if (buf_[i] == '\n') buf_[i] = 0;
    }
  } else {
    if (!num_lines_) {
      if (trace() >= 4) {
        lputs("MReader::makeLines-2 not_OK: num_lines_ == 0");
      }
      return false;
    }
  }

  assert(num_lines_ > 0);
  lines_.reserve(num_lines_ + 4);
  lines_.push_back(nullptr);

  char* curLine = buf_;

  size_t numL = 0;
  do {
    if (!curLine) break;
    lines_.push_back(curLine);
    numL++;
    size_t len = ::strlen(curLine);
    if (len > max_llen_) max_llen_ = len;
  } while (advanceLine(curLine));

  if (!numL) return false;

  lines_.push_back(nullptr);

  if (cutComments) {
    for (size_t li = 1; li < lines_.size(); li++) {
      char* line = lines_[li];
      if (!line) continue;
      size_t len = ::strlen(line);
      for (size_t i = 0; i <= len; i++) {
        if (line[i] == '#') line[i] = 0;
      }
    }
  }

  if (trace() >= 4) {
    lprintf("MReader::makeLines() OK:  lines_.size()= %zu  num_lines_= %zu\n", lines_.size(), num_lines_);
  }

  return true;
}

vector<char*> MMapReader::get_backslashed_block(size_t fromLine) noexcept {
  if (!sz_ || !fsz_) return {};
  if (!buf_) return {};
  if (!hasLines()) return {};

  size_t numLines = lines_.size();
  if (numLines <= 3)
    return {};
  if (fromLine >= numLines - 3)
    return {};

  vector<char*> V;
  size_t li = 0;

  for (li = fromLine; li < numLines - 3; li++) {
    char* line = lines_[li];
    if (!line)
      break;
    size_t len = ::strlen(line);
    if (len <= 2)
      break;
    if (line[len - 1] != '\\')
      break;

    line[len - 1] = ' '; // erase backslash
    V.push_back(line);
  }

  // add non-backslashed line at the end of block:
  if (li < numLines - 1) {
    char* line = lines_[li];
    if (line and line[0])
      V.push_back(line);
  }

  return V;
}

// '\' screens new-line '\n' like in tcl and cpp.
// returns number of processed '\' symbols.
size_t MMapReader::escapeNL() noexcept {
  if (!sz_ || !fsz_) return 0;
  if (!buf_) return 0;
  if (!hasLines()) return 0;

  size_t numLines = lines_.size();
  if (numLines <= 3)
    return 0;

  size_t cnt = 0;
  vector<char*> bs_block;

  for (size_t li = 1; li < numLines - 1; li++) {
    char* line = lines_[li];
    if (!line)
      continue;
    size_t len = ::strlen(line);
    if (len <= 2)
      continue;
    if (line[len - 1] != '\\')
      continue;
    line[len - 1] = ' '; // erase backslash

    char* nextLine = lines_[li + 1];
    if (!nextLine)
      continue;
    size_t nextLen = ::strlen(nextLine);
    if (nextLen <= 1)
      continue;

    bs_block = get_backslashed_block(li + 1);
    if (bs_block.empty())
      continue;

    // lprintf("\t    ............. bs_block.size()= %zu\n", bs_block.size());

    // replace 'line' by allocated and contatenated string:
    size_t newSize = len;
    for (char* s : bs_block) {
      assert(s);
      newSize += ::strlen(s);
    }
    char* replacement = (char*) ::calloc(newSize + 2, 1);

    void* next = ::mempcpy(replacement, line, len);
    for (char* s : bs_block) {
      size_t slen = ::strlen(s);
      next = ::mempcpy(next, s, slen);
      cnt++;
    }

    // blank bs_block lines:
    for (char* s : bs_block) {
      s[0] = 0;
    }

    lines_[li] = replacement;
  }

  return cnt;
}

char* MMapReader::skipLine(char* curL) noexcept {
  assert(sz_);
  assert(buf_);
  assert(curL);
  assert(curL >= buf_);

  size_t i = curL - buf_;
  char* p = curL;
  for (; *p; i++) p++;
  if (i < sz_) {
    p++;
    return p;
  }
  return nullptr;
}

bool MMapReader::advanceLine(char*& curL) noexcept {
  if (!curL) return false;

  char* p = skipLine(curL);
  if (!p) return false;
  assert(p > buf_);
  curL = p;
  return true;
}


// ======== 2. LineReader ==============================================

static constexpr size_t lr_MINIMUM_CAP = 3 * (LineReader::lr_MAX_LINE_LEN + 1);

LineReader::LineReader(size_t iniCap) noexcept : cap_(lr_DEFAULT_CAP) {
  if (iniCap) cap_ = std::max(iniCap, lr_MINIMUM_CAP);
}

LineReader::LineReader(CStr nm, size_t iniCap) noexcept : Fio(nm), cap_(lr_DEFAULT_CAP) {
  if (iniCap) cap_ = std::max(iniCap, lr_MINIMUM_CAP);
}

LineReader::LineReader(const std::string& nm, size_t iniCap) noexcept : Fio(nm), cap_(lr_DEFAULT_CAP) {
  if (iniCap) cap_ = std::max(iniCap, lr_MINIMUM_CAP);
}

void LineReader::reset(CStr nm, uint16_t tr) noexcept {
  Fio::reset(nm, tr);

  lines_.clear();

  if (buf_) ::free(buf_);
  buf_ = nullptr;
  curLine_ = nullptr;
  curLine2_ = nullptr;
}

void LineReader::reserve() noexcept {
  cap_ *= 2;

  char* newData = (char*)::malloc(cap_ + 4);
  ::memcpy(newData, buf_, sz_);
  ::free(buf_);
  buf_ = newData;
  buf_[cap_] = 0;
}

bool LineReader::readBuffer() noexcept {
  assert(!fnm_.empty());
  uint16_t tr = trace();

  struct stat sb;
  if (::stat(fnm_.c_str(), &sb)) {
    if (tr >= 2) ::perror("stat");
    return false;
  }

  if (not(S_IFREG & sb.st_mode)) {
    if (tr >= 2) lout() << "not a regular file: " << fnm_ << endl;
    return false;
  }

  fsz_ = sb.st_size;

  if (!buf_) {
    buf_ = (char*)::malloc(cap_ + 4);
    buf_[cap_] = 0;
  }

  FILE* f = ::fopen(fnm_.c_str(), "r");
  if (!f) {
    if (tr) {
      if (tr >= 2) ::perror("fopen");
      lout() << "can't open file: " << fnm_ << endl;
    }
    return false;
  }

  while (!feof(f)) {
    char* mem = getFreeMem();
    char* line = ::fgets(mem, lr_MAX_LINE_LEN, f);
    if (!line) break;
    num_lines_++;
    char* nextMem = skipLine(mem);
    sz_ += (nextMem - mem);
  }

  ::fclose(f);

  curLine_ = buf_;
  curLine2_ = buf_;
  return true;
}

bool LineReader::readStdin() noexcept {
  if (!buf_) {
    buf_ = (char*)::malloc(cap_ + 4);
    buf_[cap_] = 0;
  }

  while (!feof(stdin)) {
    char* mem = getFreeMem();
    char* line = ::fgets(mem, lr_MAX_LINE_LEN, stdin);
    if (!line) break;
    num_lines_++;
    char* nextMem = skipLine(mem);
    sz_ += (nextMem - mem);
  }

  curLine_ = buf_;
  curLine2_ = buf_;
  return true;
}

bool LineReader::readPipe() noexcept {
  assert(!fnm_.empty());
  if (fnm_.empty())
    return false;
  if (!buf_) {
    buf_ = (char*)::malloc(cap_ + 4);
    buf_[cap_] = 0;
  }

  CStr cfnm = fnm_.c_str();
  uint16_t tr = trace();
  if (tr >= 3)
    lprintf("::popen( %s )\n", cfnm);

  FILE* f = ::popen(cfnm, "r");
  if (!f) {
    if (tr) {
      ::perror("popen");
      lprintf("\n [Error] popen() error for cmd: %s\n", cfnm);
    }
    return false;
  }

  while (!feof(f)) {
    char* mem = getFreeMem();
    char* line = ::fgets(mem, lr_MAX_LINE_LEN, f);
    if (!line) break;
    num_lines_++;
    if (tr >= 4)
      lprintf("pipe line #%zu : %s\n", num_lines_, line);
    char* nextMem = skipLine(mem);
    sz_ += (nextMem - mem);
  }

  int pclose_rc = ::pclose(f);
  if (tr >= 3)
    lprintf("\t pclose_rc:%i\n", pclose_rc);

  curLine_ = buf_;

  return num_lines_ > 0;
}

bool LineReader::makeLines(bool cutComments, bool cutNL) noexcept {
  lines_.clear();

  if (fnm_.empty()) return false;
  if (!curLine_ || !buf_) return false;

  lines_.reserve(num_lines_ + 4);
  lines_.push_back(nullptr);

  size_t nL = 0;
  reIterate();
  do {
    if (!curLine_) break;
    lines_.push_back(curLine_);
    nL++;
  } while (advanceLine());

  if (nL) {
    for (size_t li = 1; li < lines_.size(); li++) {
      char* line = lines_[li];
      if (!line) continue;
      size_t len = ::strlen(line);
      if (len > lr_MAX_LINE_LEN) len = lr_MAX_LINE_LEN;
      line[len] = 0;
      for (size_t i = 0; i <= len; i++) {
        if (line[i] == '\n') line[i] = 0;
      }
      if (cutComments) {
        len = ::strlen(line);
        for (size_t i = 0; i <= len; i++) {
          if (line[i] == '#') {
            line[i] = 0;
            break;
          }
        }
      }
    }
    lines_.push_back(nullptr);
    reIterate();

    if (trace() >= 5) {
      lprintf("LR::makeLines() OK:  lines_.size()= %zu  num_lines_= %zu\n",
               lines_.size(), num_lines_);
    }
    return true;
  }

  lines_.clear();
  reIterate();

  return false;
}

static constexpr size_t MAX_BUF_sz = 67108860;  // 64 MB

static inline CStr trim_left(CStr p) noexcept {
  if (p && *p)
    while (std::isspace(*p)) p++;
  return p;
}

static void add_to_words(CStr cs, std::vector<std::string>& words, char* buf) noexcept {
  assert(buf);
  if (!cs || !cs[0]) return;

  cs = trim_left(cs);
  size_t len = ::strlen(cs);
  if (!len) return;

  ::strncpy(buf, cs, MAX_BUF_sz);

  for (char* p = buf; *p; p++)
    if (std::isspace(*p)) *p = 0;

  CStr tk = buf;
  for (size_t i = 0; i <= len; i++) {
    if (buf[i]) continue;
    words.emplace_back(tk);
    size_t j = i + 1;
    for (; j <= len; j++)
      if (buf[j]) break;
    if (j == len) break;
    i = j;
    tk = buf + i;
  }
}

std::vector<std::string> LineReader::getWords() const noexcept {
  if (!curLine_ || !buf_ || lines_.empty()) return {};

  std::vector<std::string> words;

  char* buf = (char*)::malloc(MAX_BUF_sz + 2);  // fixed buffer, TMP, TODO,
                                                // the buffer should be dynamic.
  assert(buf);

  for (CStr cs : lines_) {
    if (!cs || !cs[0]) continue;
    add_to_words(cs, words, buf);
  }

  ::free(buf);

  return words;
}

bool LineReader::read(bool mkLines, bool cutComments) noexcept {
  bool read_ok = readBuffer();
  if (!read_ok) return false;

  assert(curLine_ && curLine2_);
  assert(curLine_ == curLine2_);

  if (mkLines) read_ok = makeLines(cutComments, true);

  assert(curLine_ && curLine2_);
  assert(curLine_ == curLine2_);

  return read_ok;
}

bool LineReader::advanceLine() noexcept {
  if (!curLine_) return false;
  char* p = skipLine(curLine_);
  assert(p > buf_);
  if (uint64_t(p - buf_) >= sz_) return false;
  curLine_ = p;
  return true;
}

bool LineReader::advanceLine2() const noexcept {
  if (!curLine2_) return false;
  assert(curLine_);
  char* p = skipLine(curLine2_);
  assert(p > buf_);
  if (uint64_t(p - buf_) >= sz_) return false;
  curLine2_ = p;
  return true;
}

uint64_t LineReader::hashSum() const noexcept {
  assert(sz_ >= fsz_);
  if (!fsz_ || !sz_ || !buf_) return 0;

  if (fsz_ <= 16) {
    return hashf::hf_FNV64(buf_, sz_);
  }

  uint64_t sum = 0;
  reIterate2();
  do {
    sum = hashf::hashCombine(getHash(curLine2_), sum);
  } while (advanceLine2());
  return sum;
}

int LineReader::dprint1() const noexcept {
  lprintf(" fname: %s\n", fnm_.c_str());
  lprintf("  fsz_ %zu  sz_= %zu  num_lines_= %zu  lines_.size()=  %zu\n", fsz_, sz_, num_lines_,
          lines_.size());
  lprintf("  cap_= %zu\n", cap_);

  return sz_;
}

void LineReader::print(std::ostream& os, bool nl) const noexcept {
  reIterate2();
  do {
    os << curLine2_;
    if (nl) os << std::endl;
  } while (advanceLine2());
}

size_t LineReader::printLines(std::ostream& os) const noexcept {
  if (!curLine_ || !buf_ || lines_.empty()) return 0;

  size_t cnt = 0, sz = lines_.size();
  for (size_t i = 0; i < sz; i++) {
    CStr cs = lines_[i];
    if (!cs || !cs[0]) continue;
    cnt++;
    os << i << ": " << cs << endl;
  }

  return cnt;
}

void LineReader::dump(bool nl) const noexcept { print(lout(), nl); }

bool addIncludeGuards(LineReader& lr) noexcept {
  if (lr.sz_ < 3) return false;
  if (!lr.fileAccessible()) return false;

  std::ofstream f(lr.fnm_);
  if (!f.is_open()) return false;

  assert(lr.fnm_.length() > 1);
  assert(lr.fnm_.length() < 4000);

  char guard[4014] = "__rsbe__";
  guard[4000] = 0;
  guard[4001] = 0;
  guard[4002] = 0;
  guard[4003] = 0;
  strncat(guard, lr.fnm_.c_str(), 4000);
  char* p;
  for (p = guard; *p; p++)
    if (*p == '.') *p = '_';
  strcat(guard, "_H_");

  for (p = guard; *p; p++)
    ;

  sprintf(p, "%zx_", lr.hashSum());

  f << "#ifndef " << guard << endl;
  f << "#define " << guard << endl;
  f << endl;

  lr.print(f, false);

  f << endl;
  f << "#endif\n" << endl;

  return true;
}


// ======== 4. XML_Reader ==============================================

using namespace tinxml2;

static constexpr size_t X_nodes_cap0 = 30;

XML_Reader::XML_Reader() noexcept {
  doc_ = new XMLDocument;
  elems_.reserve(X_nodes_cap0);
}

XML_Reader::XML_Reader(CStr nm) noexcept : MMapReader(nm) {
  doc_ = new XMLDocument;
  elems_.reserve(X_nodes_cap0);
}

XML_Reader::XML_Reader(const string& nm) noexcept : MMapReader(nm) {
  doc_ = new XMLDocument;
  elems_.reserve(X_nodes_cap0);
}

XML_Reader::~XML_Reader() { delete doc_; }

void XML_Reader::reset(CStr nm, uint16_t tr) noexcept {
  elems_.clear();
  delete doc_;
  doc_ = new XMLDocument;

  MMapReader::reset(nm, tr);

  valid_xml_ = false;
  has_device_list_ = false;
  headLine_ = nullptr;
  nr_ = nc_ = 0;
}

bool XML_Reader::readXml() noexcept {
  bool ok = read();
  if (!ok) return false;

  ok = parse();
  return ok;
}

struct XML_Reader::Visitor : public XMLVisitor {
  XML_Reader& reader_;

  Visitor(XML_Reader& rdr) noexcept : reader_(rdr) {}

  uint16_t trace() const noexcept { return reader_.trace(); }

  virtual ~Visitor() {}

  virtual bool VisitEnter(const XMLDocument& doc) noexcept {
    if (trace() >= 7) lputs(" VisitEnter - Document");
    return true;
  }
  virtual bool VisitExit(const XMLDocument& doc) noexcept {
    if (trace() >= 7) lputs("  VisitExit - Document");
    return true;
  }

  virtual bool VisitEnter(const XMLElement& el, const XMLAttribute* firstAttr) noexcept {
    if (trace() >= 7) lputs("   VisitEnter - Element");
    reader_.elems_.push_back(&el);
    return true;
  }
  virtual bool VisitExit(const XMLElement& el) noexcept {
    if (trace() >= 7) lputs("    VisitExit - Element");
    return true;
  }

  virtual bool Visit(const XMLDeclaration& declaration) noexcept {
    if (trace() >= 7) lputs("        Visit - declaration");
    return true;
  }

  virtual bool Visit(const XMLText& text) noexcept {
    if (trace() >= 7) lputs("        Visit - text");
    reader_.texts_.push_back(&text);
    return true;
  }

  virtual bool Visit(const XMLComment& comment) noexcept { return true; }
  virtual bool Visit(const XMLUnknown& unknown) noexcept { return true; }
};

bool XML_Reader::parse() noexcept {
  valid_xml_ = false;
  has_device_list_ = false;
  if (!sz_ || !fsz_) return false;
  if (!buf_) return false;
  if (sz_ < 4) return false;

  assert(doc_);

  if (trace() >= 2) lputs("XML_Reader::parse()");

  num_lines_ = 0;
  for (size_t i = 0; i < sz_; i++) {
    if (buf_[i] == '\n') num_lines_++;
  }
  if (num_lines_ < 2) return false;

  XMLError status = doc_->ParseExternal(buf_, sz_);
  if (status != XML_SUCCESS) return false;

  Visitor vis(*this);

  doc_->Accept(&vis);

  valid_xml_ = not elems_.empty();
  if (!valid_xml_) return false;

  // -- check if it contains 'device_list' element:
  string device_list_s = "device_list";
  for (const Element* nd : elems_) {
    assert(nd);
    CStr nm = nd->Name();
    if (nm && device_list_s == nm) {
      has_device_list_ = true;
      break;
    }
  }

  return valid_xml_;
}

bool XML_Reader::isValidXml() const noexcept {
  if (!sz_ || !fsz_) return false;
  if (!buf_) return false;

  return valid_xml_;
}

int XML_Reader::dprint1() const noexcept {
  lprintf("  fname: %s\n", fnm_.c_str());
  lprintf("    fsz_ %zu  sz_= %zu  num_lines_= %zu  nr_=%zu  nc_=%zu\n", fsz_, sz_, num_lines_, nr_, nc_);

  lprintf("        valid_xml_: %i\n", valid_xml_);
  lprintf("  has_device_list_: %i\n", has_device_list_);
  lprintf("     elems_.size(): %zu\n", elems_.size());
  lprintf("     texts_.size(): %zu\n", texts_.size());

  return sz_;
}

int XML_Reader::printNodes() const noexcept {
  size_t nsz = elems_.size();
  int64_t nLeaves = countLeaves();
  lprintf("    valid_xml_: %i\n", valid_xml_);
  lprintf(" elems_.size(): %zu\n", nsz);
  lprintf("       #leaves: %zi\n", nLeaves);
  lprintf(" texts_.size(): %zu\n", texts_.size());
  if (!valid_xml_) return -1;
  if (elems_.empty()) return 0;

  for (size_t i = 0; i < nsz; i++) {
    const Element* nd = elems_[i];
    assert(nd);
    CStr nm = nd->Name();
    if (!nm) nm = "";
    CStr ts = nd->GetText();
    if (!ts) ts = "";
    bool is_leaf = not nd->FirstChildElement();
    lprintf("%zu:  name= %s  text= %s  %s\n", i, nm, ts, is_leaf ? "(leaf)" : "");
  }

  return nsz;
}

vector<XML_Reader::APair> XML_Reader::get_attrs(const Element& elem) noexcept {
  static CStr nul = "(NULL)";
  vector<APair> result;

  const XMLAttribute* a1 = elem.FirstAttribute();

  while (a1) {
    CStr nam = a1->Name();
    if (!nam) nam = nul;
    CStr val = a1->Value();
    if (!val) val = nul;
    result.emplace_back(nam, val);
    a1 = a1->Next();
  }

  return result;
}

int XML_Reader::printLeaves() const noexcept {
  size_t nsz = elems_.size();
  int64_t nLeaves = countLeaves();
  lprintf("    valid_xml_: %i\n", valid_xml_);
  lprintf(" elems_.size(): %zu\n", nsz);
  lprintf("       #leaves: %zi\n", nLeaves);
  lprintf(" texts_.size(): %zu\n", texts_.size());
  if (!valid_xml_) return -1;
  if (elems_.empty()) return 0;

  vector<APair> A;

  for (size_t i = 0; i < nsz; i++) {
    assert(elems_[i]);
    const Element& nd = *elems_[i];
    if (nd.FirstChildElement()) continue;  // not leaf
    CStr nm = nd.Name();
    if (!nm) nm = "";
    CStr txt = nd.GetText();
    if (!txt) txt = "";
    lprintf("Leaf-%zu:  name= %s  text= %s  line#%i\n", i, nm, txt, nd.GetLineNum());
    const XMLAttribute* a1 = nd.FirstAttribute();
    if (!a1) {
      lputs("    (no arrributes)");
      continue;
    }
    nm = a1->Name();
    if (!nm) nm = "";
    CStr val = a1->Value();
    if (!val) val = "";
    lprintf("    1st attribute:  %s = %s\n", nm, val);
    A = get_attrs(nd);
    if (A.size()) {
      lprintf("    (arrributes %zu):\n", A.size());
      for (size_t j = 0; j < A.size(); j++) {
        const APair& ap = A[j];
        lprintf("      :%zu:  %s = %s\n", j + 1, ap.first, ap.second);
      }
    }
  }

  return nLeaves;
}

int64_t XML_Reader::countLeaves() const noexcept {
  if (not isValidXml()) return -1;
  if (elems_.empty()) return 0;

  int64_t cnt = 0;
  for (int64_t i = int(elems_.size()) - 1; i >= 0; i--) {
    const Element* nd = elems_[i];
    assert(nd);
    bool is_leaf = not nd->FirstChildElement();
    if (is_leaf) cnt++;
  }

  return cnt;
}

}  // NS fio
