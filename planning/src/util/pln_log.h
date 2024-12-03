//
// logging and debug tracing
//
//   lprintf  : log-printf
//   lprintfl : log-printf with file:line
//   lprintf2 : log-printf with CC to stderr
//
//     lputs  : log-puts
//      LOut  : replaces cout, prints to both stdout and logfile
//
//  lputs<Number> functions are equivalent to lputs(),
//  there are convenient "anchor points" for setting temporary breakpoints.
//
//  bh<Number> are similar to lputs<Number>, only they can be silent,
//             and can indicate __FILE__,__LINE__
//  "BH" stands for Break Here, inspired by purify_stop_here().
//
#pragma once
#ifndef _pln_UTIL_LOG_H__9acc8907c045af_
#define _pln_UTIL_LOG_H__9acc8907c045af_

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cfloat>
#include <ctime>
#include <cmath>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <inttypes.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>
#include <array>

namespace pln {

using CStr = const char*;
using ipair = std::pair<int, int>;
using upair = std::pair<uint, uint>;
using uspair = std::pair<uint, std::string>;

struct LOut;

// log-trace value (debug print verbosity)
// can be set in main() by calling set_ltrace()
uint16_t ltrace() noexcept;
void set_ltrace(int t) noexcept;

LOut& getLOut() noexcept;      // global log-splitter
std::ostream& lout() noexcept; // global log-splitter as ostream

// log-printf - output goes to stdout and to logfile
void lprintf(CStr format, ...) __attribute__((format(printf, 1, 2)));

// lprintf2 : log-printf with CC to stderr
void lprintf2(CStr format, ...) __attribute__((format(printf, 1, 2)));

// lprintfl : log-printf with file:line
void lprintfl(CStr fn, uint l, CStr format, ...) __attribute__((format(printf, 3, 4)));

// printf to output-stream 'os'
void os_printf(std::ostream& os, CStr format, ...) __attribute__((format(printf, 2, 3)));

void lputs(CStr cs = 0) noexcept;
void lputs(const std::string& s) noexcept;
void err_puts(CStr cs = 0) noexcept;

void lputs0(CStr cs = 0) noexcept;
void lputs1(CStr cs = 0) noexcept;
void lputs2(CStr cs = 0) noexcept;
void lputs3(CStr cs = 0) noexcept;
void lputs4(CStr cs = 0) noexcept;
void lputs5(CStr cs = 0) noexcept;
void lputs6(CStr cs = 0) noexcept;
void lputs7(CStr cs = 0) noexcept;
void lputs8(CStr cs = 0) noexcept;
void lputs9(CStr cs = 0) noexcept;
void lputsX(CStr cs = 0) noexcept;

void err_puts(const std::string& s) noexcept;

void bh1(CStr fn, int l, CStr s = 0) noexcept;
void bh2(CStr fn, int l, CStr s = 0) noexcept;
void bh3(CStr fn, int l, CStr s = 0) noexcept;

void flush_out(bool nl = false) noexcept;

inline char* p_strdup(CStr p) noexcept {
  if (!p) return nullptr;
  return ::strdup(p);
}
inline size_t p_strlen(CStr p) noexcept { return p ? ::strlen(p) : 0; }

inline void p_free(void* p) noexcept {
  if (p) ::free(p);
}

int get_PID() noexcept;
inline std::string str_PID() noexcept { return std::to_string(get_PID()); }

std::string get_CWD() noexcept;
void traceEnv(int argc = 0, CStr* argv = nullptr) noexcept;

namespace str {

using std::string;

inline constexpr bool c_is_digit(char c) noexcept {
  return uint32_t(c) - '0' < 10u;
}
inline constexpr bool c_is_dot_digit(char c) noexcept {
  return uint32_t(c) - '0' < 10u or c == '.';
}
inline constexpr bool c_is_space(char c) noexcept {
  return c == ' ' or uint32_t(c) - 9u < 5u;
}
inline constexpr bool c_is_7bit(char c) noexcept {
  return uint32_t(c) < 128u;
}
inline constexpr bool i_is_7bit(int i) noexcept {
  return uint32_t(i) < 128u;
}
inline constexpr bool c_is_text(char c) noexcept {
  // for text/binary file classification
  if (not c_is_7bit(c))
    return false;
  return uint32_t(c) > 31u or c_is_space(c);
}

string s2lower(const string& s) noexcept;
string s2upper(const string& s) noexcept;

inline string s2lower(CStr cs) noexcept {
  string e;
  return (cs && *cs) ? s2lower(string(cs)) : e;
}
inline string s2upper(CStr cs) noexcept {
  string e;
  return (cs && *cs) ? s2upper(string(cs)) : e;
}

string sReplicate(char c, uint num) noexcept;
inline string mkSpace(uint num) noexcept { return sReplicate(' ', num); }

inline string concat(const string& a, const string& b) noexcept { return a + b; }

inline string concat(const string& a, const string& b, const string& c) noexcept {
  string z;
  z.reserve(a.length() + b.length() + c.length() + 1);
  z = a + b + c;
  return z;
}
inline string concat(const string& a, const string& b, const string& c, const string& d) noexcept {
  string z;
  z.reserve(a.length() + b.length() + c.length() + d.length() + 1);
  z = a;
  z += b;
  z += c;
  z += d;
  return z;
}
inline string concat(CStr a, const string& b, CStr c, CStr d) noexcept {
  string z;
  z.reserve(p_strlen(a) + b.length() + p_strlen(c) + p_strlen(d) + 1);
  if (a)
    z = a;
  z += b;
  if (c) z += c;
  if (d) z += d;
  return z;
}
inline string concat(CStr a, const string& b, CStr c, const string& d) noexcept {
  string z;
  z.reserve(p_strlen(a) + b.length() + p_strlen(c) + d.length() + 1);
  if (a)
    z = a;
  z += b;
  if (c) z += c;
  z += d;
  return z;
}
inline string concat(const string& a, CStr b) noexcept { return b ? a + b : a; }

inline string concat(CStr a, const string& b, CStr c = nullptr) noexcept {
  size_t len = p_strlen(a) + p_strlen(c) + b.length();
  if (!len) return {};
  string z;
  z.reserve(len + 1);
  if (a) z = a;
  z.append(b);
  if (c) z.append(c);
  return z;
}

inline string concat(const string& a, CStr b, const string& c) noexcept {
  size_t len = a.length() + c.length() + p_strlen(b);
  if (!len) return {};
  string z;
  z.reserve(len + 1);
  z = a;
  if (b) z.append(b);
  z.append(c);
  return z;
}

inline string concat(CStr a,
                     CStr b,
                     CStr c = nullptr,
                     CStr d = nullptr) noexcept {
  size_t len = p_strlen(a) + p_strlen(b) + p_strlen(c) + p_strlen(d);
  if (!len) return {};
  string z;
  z.reserve(len + 1);
  if (a) z.append(a);
  if (b) z.append(b);
  if (c) z.append(c);
  if (d) z.append(d);
  return z;
}

inline string concat(CStr a, CStr b, const string& c, const string& d = "") noexcept {
  size_t len = c.length() + d.length() + p_strlen(b) + p_strlen(b);
  if (!len) return {};
  string z;
  z.reserve(len + 1);
  if (a) z.append(a);
  if (b) z.append(b);
  z.append(c);
  z.append(d);
  return z;
}

inline string concat(const string& a,
                     CStr d, CStr e, CStr f, CStr g = nullptr) noexcept {
  size_t len = a.length();
  len += p_strlen(d) + p_strlen(e) + p_strlen(f) + p_strlen(g);
  if (!len) return {};
  string z;
  z.reserve(len + 1);
  z.append(a);
  if (d) z.append(d);
  if (e) z.append(e);
  if (f) z.append(f);
  if (g) z.append(g);
  return z;
}

inline string concat(const string& a, const string& b,
                     CStr d, CStr e, CStr f, CStr g = nullptr) noexcept {
  size_t len = a.length() + b.length();
  len += p_strlen(d) + p_strlen(e) + p_strlen(f) + p_strlen(g);
  if (!len) return {};
  string z;
  z.reserve(len + 1);
  z.append(a); z.append(b);
  if (d) z.append(d);
  if (e) z.append(e);
  if (f) z.append(f);
  if (g) z.append(g);
  return z;
}

inline string concat(const string& a, const string& b, const string& c,
                     CStr d, CStr e, CStr f, CStr g = nullptr) noexcept {
  size_t len = a.length() + b.length() + c.length();
  len += p_strlen(d) + p_strlen(e) + p_strlen(f) + p_strlen(g);
  if (!len) return {};
  string z;
  z.reserve(len + 1);
  z.append(a); z.append(b); z.append(c);
  if (d) z.append(d);
  if (e) z.append(e);
  if (f) z.append(f);
  if (g) z.append(g);
  return z;
}

inline string concat(CStr a) noexcept {
  // workaround bug in C++ standard.
  // string construction from null should produce 'empty', not 'throw'
  if (!a or !a[0])
    return {};
  return a;
}

inline CStr trimFront(CStr z) noexcept {
  if (z and z[0]) {
    while (c_is_space(*z)) z++;
  }
  return z;
}

// remove '\n' at the end
inline void chomp(char* z) noexcept {
  if (z and z[0]) {
    size_t len = ::strlen(z);
    if (z[len-1] == '\n')
      z[len-1] = 0;
  }
}

inline size_t hashf(CStr z) noexcept {
  if (!z) return 0;
  if (!z[0]) return 1;
  std::hash<std::string_view> h;
  return h(std::string_view{z});
}
inline size_t hashf(const std::string& s) noexcept {
  if (s.empty()) return 1;
  std::hash<std::string> h;
  return h(s);
}

}  // NS str

template <typename T>
inline void logArray(const T* A, size_t n, CStr pref) noexcept {
  auto& os = lout();
  if (pref) os << pref;
  if (!A || !n) {
    os << " (empty)" << std::endl;
    return;
  }
  for (size_t i = 0; i < n; i++) os << ' ' << A[i];
  os << std::endl;
}

template <typename T>
inline void prnArray(std::ostream& os, const T* A, size_t n,
                     CStr pref, CStr suf = nullptr) noexcept {
  if (pref) os << pref;
  if (!A || !n) {
    os << " (empty)" << std::endl;
    return;
  }
  for (size_t i = 0; i < n; i++) os << ' ' << A[i];
  if (suf) os << suf;
  os << std::endl;
}

template <typename T>
inline void hexArray(std::ostream& os, const T* A, size_t n,
                     CStr pref, CStr suf = nullptr) noexcept {
  if (pref) os << pref;
  if (!A || !n) {
    os << " (empty)" << std::endl;
    return;
  }
  std::ios_base::fmtflags savedF = os.flags();
  os.setf(std::ios::showbase);
  for (size_t i = 0; i < n; i++) {
    os << ' ' << std::hex << A[i] << ',';
  }
  os.setf(savedF);
  if (suf) os << suf;
  os << std::endl;
}

inline void logVec(const std::vector<bool>& vec, CStr pref) noexcept {
  auto& os = lout();
  if (pref) os << pref;
  if (vec.empty()) {
    os << " (empty)" << std::endl;
    return;
  }
  for (size_t i = 0; i < vec.size(); i++) os << ' ' << vec[i];
  os << std::endl;
}

template <typename T>
inline void logVec(const std::vector<T>& vec, CStr pref) noexcept {
  const T* A = (vec.empty() ? nullptr : vec.data());
  logArray(A, vec.size(), pref);
}

template <typename T>
inline T* unconst(const T* p) noexcept { return const_cast<T*>(p); }

//
// LOut is an ostream that splits the output,
// i.e. it prints to a file (logfile) and to stdout.
//
struct LOut : public std::ostream, public std::streambuf {
  char* buf_ = nullptr;
  uint buf_sz_ = 0;

  LOut() noexcept;
  virtual ~LOut();

  virtual int overflow(int c) override;

  void putS(CStr z, bool nl) noexcept;
  void putS(const std::string& s, bool nl) noexcept { putS(s.c_str(), nl); }

  void doFlush(bool nl) noexcept;

  bool bufEmpty() const noexcept { return !buf_[0]; }
  void flushBuf() noexcept;

  LOut(const LOut&) = delete;
  LOut& operator = (const LOut&) = delete;
};

bool open_pln_logfile(CStr fn) noexcept;

}  // namespace pln

const char* pln_get_version();

#endif
