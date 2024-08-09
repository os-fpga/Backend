#include "util/pln_log.h"

#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace pln {

using namespace std;

// log-trace value (debug print verbosity)
// can be set in main() by calling set_ltrace()
static uint16_t s_logLevel = 0;

static constexpr size_t LOut_buf_cap = 32756;
static std::ofstream __attribute__((init_priority(103)))  _plnLF; // log file
static LOut __attribute__((init_priority(104)))           _plnLS; // log splitter

uint16_t ltrace() noexcept { return s_logLevel; }

void set_ltrace(int t) noexcept {
  cerr.tie(&cout);
  _plnLS.doFlush(false);
  if (t <= 0) {
    s_logLevel = 0;
    return;
  }
  if (t >= USHRT_MAX) {
    s_logLevel = USHRT_MAX;
    return;
  }
  s_logLevel = t;
}

//
// LOut is an ostream that splits the output,
// i.e. it prints to a file (logfile) and to stdout.
/////////////

LOut& getLOut() noexcept { return _plnLS; }
std::ostream& lout() noexcept { return _plnLS; }

bool open_pln_logfile(CStr fn) noexcept {
  assert(not _plnLF.is_open());
  assert(fn and fn[0]);
  if (_plnLF.is_open())
    return false;
  if (!fn or !fn[0])
    return false;

  _plnLF.open(fn);
  if (not _plnLF.is_open()) {
    cout << endl;
    cerr << endl;
    cerr << "pln: could not open logfile: " << fn << endl;
    cout << endl;
    return false;
  }
  return true;
}

LOut::LOut() noexcept : std::ostream(this), buf_sz_(0) {
  static char s_buf[LOut_buf_cap + 4];
  buf_ = s_buf;
  buf_[0] = 0;
  buf_[1] = 0;
}

LOut::~LOut() {
  if (buf_sz_) {
    cout << buf_ << endl;
    if (_plnLF.is_open()) {
      _plnLF << buf_ << endl;
      _plnLF.flush();
    }
  }
  fflush(stdout);
  fflush(stderr);
  cout.flush();
  cerr.flush();
}

void LOut::flushBuf() noexcept {
  if (!buf_[0]) {
    cout.flush();
    fflush(stdout);
    if (_plnLF.is_open())
      _plnLF.flush();
    return;
  }
  cout << buf_;
  cout.flush();
  fflush(stdout);
  if (_plnLF.is_open()) {
    _plnLF << buf_;
    _plnLF.flush();
  }
  buf_sz_ = 0;
  buf_[0] = 0;
  buf_[1] = 0;
}

void LOut::doFlush(bool nl) noexcept {
  flushBuf();
  if (nl) {
    cout << endl;
    if (_plnLF.is_open())
      _plnLF << endl;
  }
  fflush(stdout);
  fflush(stderr);
  cout.flush();
  cerr.flush();
}

void LOut::putS(CStr z, bool nl) noexcept {
  if (buf_[0])
    doFlush(false);
  if (z and z[0]) {
    cout << z;
    if (nl) cout << endl;
    cout.flush();
    fflush(stdout);
    if (_plnLF.is_open()) {
      _plnLF << z;
      if (nl) _plnLF << endl;
      _plnLF.flush();
    }
    return;
  }
  if (nl) {
    cout << endl;
    if (_plnLF.is_open())
      _plnLF << endl;
  }
}

int LOut::overflow(int c) {
  CStr cs = buf_;
  if (c == '\n') {
    putS(cs, false);
    doFlush(true);
    buf_sz_ = 0;
    buf_[0] = 0;
    buf_[1] = 0;
    return 0;
  }
  if (buf_sz_ < LOut_buf_cap - 2) {
    buf_[buf_sz_++] = c;
    buf_[buf_sz_] = 0;
    return 0;
  }
  // reached buffer capacity.
  // flush, but without endl:
  putS(cs, false);
  doFlush(false);
  buf_sz_ = 0;
  buf_[0] = 0;
  buf_[1] = 0;
  return 0;
}

/////////////

void lputs(CStr cs) noexcept {
  if (s_logLevel >= 4) {
    _plnLS.doFlush(false);
  }
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}
void err_puts(CStr cs) noexcept {
  _plnLS.doFlush(false);
  if (cs && cs[0]) cerr << cs;
  cerr << endl; fflush(stdout);
}

void lputs0(CStr cs) noexcept {
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}
void lputs1(CStr cs) noexcept {
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}
void lputs2(CStr cs) noexcept {
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}
void lputs3(CStr cs) noexcept {
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}
void lputs4(CStr cs) noexcept {
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}
void lputs5(CStr cs) noexcept {
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}
void lputs6(CStr cs) noexcept {
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}
void lputs7(CStr cs) noexcept {
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}
void lputs8(CStr cs) noexcept {
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}
void lputs9(CStr cs) noexcept {
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}
void lputsX(CStr cs) noexcept {
  _plnLS.putS(cs, false);
  _plnLS.doFlush(true);
}

void lputs(const string& s) noexcept {
  if (s_logLevel >= 4) _plnLS.doFlush(false);
  if (s.empty())
    _plnLS.doFlush(true);
  else
    lputs(s.c_str());
}
void err_puts(const string& s) noexcept {
  _plnLS.doFlush(false);
  if (s.empty())
    cerr << endl;
  else
    err_puts(s.c_str());
}

void bh1(CStr fn, int l, CStr s) noexcept {
  if (!s) return;
  char msg[512] = {};
  ::snprintf(msg, 510, "\n bh1: %s:%i '%s'", fn, l, s);
  _plnLS.putS(msg, false);
  _plnLS.doFlush(true);
}
void bh2(CStr fn, int l, CStr s) noexcept {
  if (!s) return;
  char msg[512] = {};
  ::snprintf(msg, 510, "\n bh2: %s:%i '%s'", fn, l, s);
  _plnLS.putS(msg, false);
  _plnLS.doFlush(true);
}
void bh3(CStr fn, int l, CStr s) noexcept {
  if (!s) return;
  char msg[512] = {};
  ::snprintf(msg, 510, "\n bh3: %s:%i '%s'", fn, l, s);
  _plnLS.putS(msg, false);
  _plnLS.doFlush(true);
}

void flush_out(bool nl) noexcept {
  _plnLS.doFlush(nl);
}

void lprintf(CStr format, ...) {
  if (s_logLevel >= 4)
    _plnLS.doFlush(false);
  char buf[32768];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, 32766, format, args);
  buf[32767] = 0;
  va_end(args);

  size_t len = ::strlen(buf);
  if (!len) return;

  _plnLS.putS(buf, false);

  if ((len > 2 && buf[len - 1] == '\n') || len > 128) {
    _plnLS.doFlush(false);
  }
}

// lprintf2 : log-printf with CC to stderr
void lprintf2(CStr format, ...) {
  _plnLS.doFlush(false);
  char buf[32768];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, 32766, format, args);
  buf[32767] = 0;
  va_end(args);

  size_t len = strlen(buf);
  if (!len) return;

  _plnLS.putS(buf, false);
  _plnLS.doFlush(false);

  cerr << buf;
  cerr.flush();
  fflush(stderr);
}

// lprintfl : log-printf with file:line
void lprintfl(CStr fn, uint l, CStr format, ...) {
  _plnLS.doFlush(false);
  char buf[32768];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, 32766, format, args);
  buf[32767] = 0;
  va_end(args);

  size_t len = strlen(buf);
  if (!len) return;

  if (!fn)
    fn = "";
  _plnLS << ' ' << fn << " : " << l << "  " << buf;
  _plnLS.doFlush(true);
}

// printf to output-stream 'os'
void os_printf(std::ostream& os, CStr format, ...) {
  char buf[32768];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, 32766, format, args);
  buf[32767] = 0;
  va_end(args);

  size_t len = strlen(buf);
  if (!len) return;

  os << buf;
  os.flush();
}

int get_PID() noexcept { return ::getpid(); }

string get_CWD() noexcept {
  char buf[4100] = {}; // unix max path is 4096
  if (::getcwd(buf, 4098))
    return buf;
  return {};
}

static bool s_readLink(const string& path, string& out) noexcept {
  out.clear();
  if (path.empty()) return false;

  CStr cs = path.c_str();
  struct stat sb;

  if (::stat(cs, &sb))
    return false;
  if (not(S_IFREG & sb.st_mode))
    return false;

  if (::lstat(cs, &sb))
    return false;

  if (not(S_ISLNK(sb.st_mode))) {
    out = "(* NOT A LINK : !S_ISLNK *)";
    return false;
  }

  char buf[8192];
  buf[0] = 0;
  buf[1] = 0;
  buf[8190] = 0;
  buf[8191] = 0;

  int64_t numBytes = ::readlink(cs, buf, 8190);
  if (numBytes == -1) {
      perror("readlink()");
      return false;
  }
  buf[numBytes] = 0;
  out = buf;

  return true;
}

void traceEnv(int argc, CStr* argv) noexcept {
  _plnLS.doFlush(false);
  int pid  = ::getpid();
  int ppid = ::getppid();
  string selfPath, parentPath, exe_link;

  exe_link = str::concat("/proc/", std::to_string(pid), "/exe");
  s_readLink(exe_link, selfPath);

  exe_link = str::concat("/proc/", std::to_string(ppid), "/exe");
  s_readLink(exe_link, parentPath);

  string work_dir = get_CWD();

  lprintf("    PID: %i   parentPID: %i\n", pid, ppid);
  lprintf("      self-path: %s\n", selfPath.c_str());
  lprintf("    parent-path: %s\n", parentPath.c_str());
  lprintf("    CWD: %s\n", work_dir.c_str());

  if (argc > 0 && argv) {
    lprintf("    argc= %i\n", argc);
    for (int i = 0; i < argc; i++)
      lprintf("    |%i| %s\n", i, argv[i]);
  }
  lputs();
}

namespace str {

  string s2lower(const string& s) noexcept {
    if (s.empty()) return {};
    CStr cs = s.c_str();
    if (!cs || !cs[0]) return {};

    string result;
    result.reserve(s.length() + 1);
    for (CStr p = cs; *p; p++) result.push_back(std::tolower(*p));
    return result;
  }

  string s2upper(const string& s) noexcept {
    if (s.empty()) return {};
    CStr cs = s.c_str();
    if (!cs || !cs[0]) return {};

    string result;
    result.reserve(s.length() + 1);
    for (CStr p = cs; *p; p++) result.push_back(std::toupper(*p));
    return result;
  }

  string sReplicate(char c, uint num) noexcept {
    constexpr uint num_lim = INT_MAX;
    assert(num < num_lim);
    if (!num || num > num_lim) return {};

    string result;
    result.reserve(num + 1);
    result.insert(result.end(), num, c);

    return result;
  }

}  // NS str

}  // NS pln

