// File IO - namespace fio
#include "file_readers/Fio.h"

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace fio {

using namespace pinc;
using namespace std;

static constexpr uint32_t fio_MAX_STACK_USE = 1048576;  // 1 MiB

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

bool Fio::fileAccessible(const char* fn) noexcept {
  if (!fn) return false;
  int status = ::access(fn, F_OK);
  if (status != 0) return false;
  status = ::access(fn, R_OK);
  if (status != 0) return false;
  return true;
}

bool Fio::regularFileExists(const char* fn) noexcept {
  struct stat sb;
  if (::stat(fn, &sb)) return false;

  if (not(S_IFREG & sb.st_mode)) return false;

  return true;
}

void Fio::reset(const char* nm, uint16_t tr) noexcept {
  sz_ = fsz_ = 0;
  num_lines_ = 0;
  fnm_.clear();
  setTrace(tr);
  if (nm) fnm_ = nm;
}


// ======== 1. MMapReader ==============================================

MMapReader::~MMapReader() { reset(nullptr, 0); }

void MMapReader::reset(const char* nm, uint16_t tr) noexcept {
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

// unix command 'wc'
int64_t MMapReader::countWC(int64_t& numWords) const noexcept {
  assert(fsz_ == sz_);
  numWords = 0;
  if (!fsz_ || !sz_ || !buf_) return -1;

  const size_t z = sz_ + 1;
  void* mem = (z <= fio_MAX_STACK_USE ? alloca(z) : malloc(z));
  char* s = (char*)mem;
  ::memcpy(s, buf_, z);
  s[z] = '\0';
  for (size_t i = 0; i < z; i++) {
    if (!s[z]) s[z] = ' ';
  }
  const char* delim = " \t\n";

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

uint64_t MMapReader::hashSum() const noexcept {
  assert(fsz_ == sz_);
  if (!fsz_ || !sz_ || !buf_) return 0;

  uint64_t sum = 0;
  if (sz_ <= 16) {
    for (size_t i = 0; i < sz_; i++) sum = hashCombine(buf_[i], sum);
    return sum;
  }

  constexpr size_t i8 = sizeof(uint64_t);  // 8

  size_t k = sz_ / i8;
  size_t r = sz_ - k * i8;

  for (size_t i = 0; i < k; i += i8) {
    char* p = buf_ + i;
    uint64_t a = *((uint64_t*)p);
    sum = hashCombine(a, sum);
  }

  while (r) {
    sum = hashCombine(buf_[sz_ - r], sum);
    r--;
  }

  return sum;
}

int MMapReader::dprint() const noexcept {
  lprintf(" fname: %s\n", fnm_.c_str());
  lprintf("  fsz_ %zu  sz_= %zu \n", fsz_, sz_);

  return sz_;
}

bool MMapReader::makeLines(bool cutComments) noexcept {
  lines_.clear();

  if (!sz_ || !fsz_) return false;
  if (!buf_) return false;
  if (!num_lines_) return false;

  lines_.reserve(num_lines_ + 4);
  lines_.push_back(nullptr);

  char* curLine = buf_;

  size_t nL = 0;
  do {
    if (!curLine) break;
    lines_.push_back(curLine);
    nL++;
  } while (advanceLine(curLine));

  if (!nL) return false;

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

  if (trace() >= 2) {
    lprintf("MReader::makeLines() OK:  lines_.size()= %zu  num_lines_= %zu\n", lines_.size(), num_lines_);
  }

  return true;
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

LineReader::LineReader(const char* nm, size_t iniCap) noexcept : Fio(nm), cap_(lr_DEFAULT_CAP) {
  if (iniCap) cap_ = std::max(iniCap, lr_MINIMUM_CAP);
}

LineReader::LineReader(const std::string& nm, size_t iniCap) noexcept : Fio(nm), cap_(lr_DEFAULT_CAP) {
  if (iniCap) cap_ = std::max(iniCap, lr_MINIMUM_CAP);
}

void LineReader::reset(const char* nm, uint16_t tr) noexcept {
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

bool LineReader::makeLines(bool cutComments) noexcept {
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
          if (line[i] == '#') line[i] = 0;
        }
      }
    }
    lines_.push_back(nullptr);
    reIterate();

    if (trace() >= 2) {
      lprintf("LReader::makeLines() OK:  lines_.size()= %zu  num_lines_= %zu\n", lines_.size(), num_lines_);
    }
    return true;
  }

  lines_.clear();
  reIterate();

  return false;
}

static constexpr size_t MAX_BUF_sz = 67108860;  // 64 MB

static inline const char* trim_left(const char* p) noexcept {
  if (p && *p)
    while (std::isspace(*p)) p++;
  return p;
}

static void add_to_words(const char* cs, std::vector<std::string>& words, char* buf) noexcept {
  assert(buf);
  if (!cs || !cs[0]) return;

  cs = trim_left(cs);
  size_t len = ::strlen(cs);
  if (!len) return;

  ::strncpy(buf, cs, MAX_BUF_sz);

  for (char* p = buf; *p; p++)
    if (std::isspace(*p)) *p = 0;

  const char* tk = buf;
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

  for (const char* cs : lines_) {
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

  if (mkLines) read_ok = makeLines(cutComments);

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

  uint64_t sum = 0;
  if (fsz_ <= 16) {
    for (size_t i = 0; i < fsz_; i++) sum = hashCombine(buf_[i], sum);
    return sum;
  }

  reIterate2();
  do {
    sum = hashCombine(getHash(curLine2_), sum);
  } while (advanceLine2());
  return sum;
}

int LineReader::dprint() const noexcept {
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
    const char* cs = lines_[i];
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

  char guard[4014] = "__rs_tt_";
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
  nr_ = nc_ = 0;
}

bool CSV_Reader::readCsv(bool cutComments) noexcept {
  bool ok = read();
  if (!ok) return false;

  ok = parse(cutComments);
  return ok;
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
  nr_ = nc_ = 0;
  smat_ = 0;
  nmat_ = 0;

  // 1. replace '\n' by 0 and count lines and commas
  for (size_t i = 0; i < sz_; i++) {
    if (buf_[i] == '\n') {
      buf_[i] = 0;
      num_lines_++;
    }
    if (buf_[i] == ',') num_commas_++;
  }

  if (num_lines_ < 2 || num_commas_ < 2) return false;

  // 2. make lines_
  bool ok = makeLines(cutComments);
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

  // 4. split headLine_, make header_
  ok = split(headLine_, header_);
  if (!ok) return false;

  // 5. header_ size() is the number of columns.
  //    data rows should now have more columns than the header.
  if (header_.size() < 2 || header_.size() > size_t(INT_MAX)) return false;
  nr_ = nel - 1;
  if (nr_ < 2) return false;
  nc_ = header_.size();
  if (nc_ < 2) return false;
  if (trace() >= 3) {
    lprintf("CReader::parse()  nr_= %u  nc_= %u\n", nr_, nc_);
  }
  for (size_t li = 1; li <= num_lines_; li++) {
    char* line = lines_[li];
    if (isEmptyLine(line)) continue;
    uint nCom = countCommas(line);
    if (!nCom || nCom >= nc_) {
      if (trace() >= 2) {
        lprintf("CReader::parse() ERROR: unexpected #commas(%u) at line %zu of %s\n", nCom, li, fnm_.c_str());
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
  uint lcnt = 0;
  for (size_t li = 1; li < lines_.size(); li++) {
    char* line = lines_[li];
    if (isEmptyLine(line)) continue;
    if (line == headLine_) continue;
    V.clear();
    ok = split(line, V);
    if (!ok) {
      return false;
    }
    lcnt++;
  }
  if (trace() >= 4) lprintf("lcnt= %u\n", lcnt);
  assert(lcnt == nr_);

  // 7. populate number-martrix
  V.clear();
  lcnt = 0;
  for (size_t li = 1; li < lines_.size(); li++) {
    char* line = lines_[li];
    if (isEmptyLine(line)) continue;
    if (line == headLine_) continue;
    V.clear();
    ok = split(line, V);
    if (!ok) {
      return false;
    }
    lcnt++;
  }
  if (trace() >= 4) lprintf("lcnt= %u\n", lcnt);
  assert(lcnt == nr_);

  valid_csv_ = true;
  return true;
}

void CSV_Reader::alloc_num_matrix() noexcept {
  assert(!nmat_);
  assert(nr_ > 1 && nc_ > 1);

  nmat_ = new double*[nr_ + 2];
  for (uint r = 0; r < nr_ + 2; r++) {
    nmat_[r] = new double[nc_ + 2];
    ::memset(nmat_[r], 0, (nc_ + 2) * sizeof(double));
  }
}

void CSV_Reader::alloc_str_matrix() noexcept {
  assert(!smat_);
  assert(nr_ > 1 && nc_ > 1);

  smat_ = new string*[nr_ + 2];
  for (uint r = 0; r < nr_ + 2; r++) {
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

  for (uint r = 0; r < nr_ + 2; r++) delete[] nmat_[r];

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

  for (uint r = 0; r < nr_ + 2; r++) delete[] smat_[r];

  delete[] smat_;
  smat_ = nullptr;
}

bool CSV_Reader::isValidCsv() const noexcept {
  if (!sz_ || !fsz_) return false;
  if (!buf_) return false;

  if (num_lines_ < 2 || num_commas_ < 2) return false;

  return valid_csv_;
}

int CSV_Reader::dprint1() const noexcept {
  lprintf("  fname: %s\n", fnm_.c_str());
  lprintf("    fsz_ %zu  sz_= %zu  num_lines_= %zu  num_commas_=%zu  nr_=%u  nc_=%u\n", fsz_, sz_, num_lines_,
          num_commas_, nr_, nc_);

  lprintf("    valid_csv_: %i\n", valid_csv_);
  lprintf("    headLine_: %s\n", headLine_ ? headLine_ : "(NULL)");
  lprintf("    header_.size()= %zu\n", header_.size());
  logVec(header_, "    header_:");
  lprintf("    lines_.size()= %zu\n", lines_.size());
  if (lines_.size() > 3 && lines_[2] && nc_ < 400) lprintf("    lines_[2]  %s\n", lines_[2]);

  return sz_;
}

bool CSV_Reader::split(const char* src, vector<string>& dat) noexcept {
  dat.clear();
  assert(src);
  if (!src || !src[0]) return false;

  size_t len = ::strlen(src);
  if (!len || len > fio_MAX_STACK_USE) return false;

  if (!::strchr(src, ',')) return false;

  char scratch[len + 3];
  ::strcpy(scratch, src);
  scratch[len + 1] = 0;
  scratch[len + 2] = 0;

  for (char* p = scratch; *p; p++)
    if (*p == ',') *p = '\0';

  const char* tk = scratch;
  for (uint i = 0; i <= len; i++) {
    if (scratch[i]) continue;
    dat.emplace_back(tk);
    uint j = i + 1;
    for (; j <= len; j++)
      if (scratch[j]) break;
    if (j == len) break;
    i = j;
    tk = scratch + i;
  }

  return !dat.empty();
}

const char* CSV_Reader::firstSpace(const char* src) noexcept {
  if (!src || !src[0]) return nullptr;
  for (const char* p = src; *p; p++)
    if (std::isspace(*p)) return p;
  return nullptr;
}

const char* CSV_Reader::firstNonSpace(const char* src) noexcept {
  if (!src || !src[0]) return nullptr;
  for (const char* p = src; *p; p++)
    if (!std::isspace(*p)) return p;
  return nullptr;
}

bool CSV_Reader::isEmptyLine(const char* src) noexcept {
  if (!src || !src[0]) return true;
  if (!firstNonSpace(src)) return true;
  size_t len = ::strnlen(src, 4);
  return len < 2;
}

uint CSV_Reader::countCommas(const char* src) noexcept {
  if (!src || !src[0]) return 0;
  uint cnt = 0;
  for (const char* p = src; *p; p++) {
    if (*p == ',') cnt++;
  }
  return cnt;
}

}  // NS fio

