#include "util/pinc_log.h"

#include <stdarg.h>

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

}  // namespace pinc
