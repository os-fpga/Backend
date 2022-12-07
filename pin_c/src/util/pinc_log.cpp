#include "pinc_log.h"

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

void lputs(const char* cs) noexcept {
  if (cs && cs[0]) cout << cs;
  cout << endl;
}
void lputs(const std::string& s) noexcept {
  if (s.empty()) {
    cout << endl;
  } else {
    lputs(s.c_str());
  }
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

string strToLower(const string& s) noexcept {
  if (s.empty()) return {};
  const char* cs = s.c_str();
  if (!cs || !cs[0]) return {};

  string result;
  result.reserve(s.length() + 1);
  for (const char* p = cs; *p; p++) result.push_back(std::tolower(*p));
  return result;
}

string strReplicate(char c, uint num) noexcept {
  if (!num || num > 48000) return {};
  char buf[num + 1];
  buf[num] = 0;
  for (uint i = 0; i < num; i++) buf[i] = c;
  return string(buf);
}

}  // namespace pinc
