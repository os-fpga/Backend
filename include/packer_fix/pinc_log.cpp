#include "pinc_log.h"

#include <stdarg.h>
#include <alloca.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace pinc {

using namespace std;

// log-trace value (debug print verbosity)
// can be set in main() by calling set_ltrace()
static uint16_t s_logLevel = 0;
uint16_t ltrace() noexcept { return s_logLevel; }
void set_ltrace(int t) noexcept {
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

#define LPUT if (cs && cs[0]) cout << cs;
#define LEND cout << endl; fflush(stdout);

void lputs(const char* cs) noexcept {
  LPUT
  LEND
}
void err_puts(const char* cs) noexcept {
  if (cs && cs[0]) cerr << cs;
  cerr << endl; fflush(stdout);
}

void lputs0(const char* cs) noexcept {
  LPUT
  LEND
}
void lputs1(const char* cs) noexcept {
  LPUT
  LEND
}
void lputs2(const char* cs) noexcept {
  LPUT
  LEND
}
void lputs3(const char* cs) noexcept {
  LPUT
  LEND
}
void lputs4(const char* cs) noexcept {
  LPUT
  LEND
}
void lputs5(const char* cs) noexcept {
  LPUT
  LEND
}
void lputs6(const char* cs) noexcept {
  LPUT
  LEND
}
void lputs7(const char* cs) noexcept {
  LPUT
  LEND
}
void lputs8(const char* cs) noexcept {
  LPUT
  LEND
}
void lputs9(const char* cs) noexcept {
  LPUT
  LEND
}
void lputsX(const char* cs) noexcept {
  LPUT
  LEND
}

void lputs(const string& s) noexcept {
  if (s.empty())
    cout << endl;
  else
    lputs(s.c_str());
}
void err_puts(const string& s) noexcept {
  if (s.empty())
    cerr << endl;
  else
    err_puts(s.c_str());
}

static constexpr char q = '\'';
void bh1(const char* fn, int l, const char* s) noexcept {
  if (!s) return;
  cout << "\n bh1: " << fn << ':' << l << q << s << q << endl;
}
void bh2(const char* fn, int l, const char* s) noexcept {
  if (!s) return;
  cout << "\n bh2: " << fn << ':' << l << q << s << q << endl;
}
void bh3(const char* fn, int l, const char* s) noexcept {
  if (!s) return;
  cout << "\n bh3: " << fn << ':' << l << q << s << q << endl;
}

void flush_out(bool nl) noexcept {
  if (nl)
    cout << endl;
  cout.flush();
  fflush(stdout);
}

void lprintf(const char* format, ...) {
  char buf[32768];
  va_list args;
  va_start(args, format);
  vsnprintf(buf, 32766, format, args);
  buf[32767] = 0;
  va_end(args);

  size_t len = strlen(buf);
  if (!len) return;

  cout << buf;
  if ((len > 2 && buf[len - 1] == '\n') || len > 128) {
    cout.flush();
    fflush(stdout);
  }
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

  const char* cs = path.c_str();
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

void traceEnv(int argc, const char** argv) noexcept {
  int pid  = ::getpid();
  int ppid = ::getppid();
  string selfPath, parentPath, exe_link;

  exe_link = str::concat("/proc/", std::to_string(pid), "/exe");
  s_readLink(exe_link, selfPath);

  exe_link = str::concat("/proc/", std::to_string(ppid), "/exe");
  s_readLink(exe_link, parentPath);

  lprintf("    PID: %i   parentPID: %i\n", pid, ppid);
  lout() << "      self-path: " << selfPath << endl;
  lout() << "    parent-path: " << parentPath << endl;
  lout() << "    CWD: " << get_CWD() << endl;

  if (argc > 0 && argv) {
    lprintf("    argc= %i\n", argc);
    for (int i = 0; i < argc; i++)
      lprintf("    |%i| %s\n", i, argv[i]);
  }
  lputs();
}

namespace str {

  string sToLower(const string& s) noexcept {
    if (s.empty()) return {};
    const char* cs = s.c_str();
    if (!cs || !cs[0]) return {};

    string result;
    result.reserve(s.length() + 1);
    for (const char* p = cs; *p; p++) result.push_back(std::tolower(*p));
    return result;
  }

  string sToUpper(const string& s) noexcept {
    if (s.empty()) return {};
    const char* cs = s.c_str();
    if (!cs || !cs[0]) return {};

    string result;
    result.reserve(s.length() + 1);
    for (const char* p = cs; *p; p++) result.push_back(std::toupper(*p));
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

}  // NS pinc

