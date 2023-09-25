//
// logging and debug tracing
//
//   lprintf : log-printf
//     lputs : log-puts
//      lout : replaces cout, will print to both stdout and logfile
//
//  currently, log- functions just print on stdout, real logfile can be added later
//
//  lputs<Number> functions are equivalent to lputs(),
//  there are convenient "anchor points" for setting temporary breakpoints.
//
//  bh<Number> are similar to lputs<Number>, only they can be silent,
//             and can indicate __FILE__,__LINE__
//  "BH" stands for Break Here, inspired by purify_stop_here().
//
#pragma once
#ifndef __rs_PINC_LOG_H_
#define __rs_PINC_LOG_H_

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cfloat>
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
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>
#include <array>

namespace pinc {

// log-trace value (debug print verbosity)
// can be set in main() by calling set_ltrace()
uint16_t ltrace() noexcept;
void set_ltrace(int t) noexcept;

void lprintf(const char* format, ...) __attribute__((format(printf, 1, 2)));
void lputs(const char* cs = 0) noexcept;
void lputs0(const char* cs = 0) noexcept;
void lputs1(const char* cs = 0) noexcept;
void lputs2(const char* cs = 0) noexcept;
void lputs3(const char* cs = 0) noexcept;
void lputs4(const char* cs = 0) noexcept;
void lputs5(const char* cs = 0) noexcept;
void lputs6(const char* cs = 0) noexcept;
void lputs7(const char* cs = 0) noexcept;
void lputs8(const char* cs = 0) noexcept;
void lputs9(const char* cs = 0) noexcept;
void lputsX(const char* cs = 0) noexcept;
void lputs(const std::string& s) noexcept;

void bh1(const char* fn, int l, const char* s = 0) noexcept;
void bh2(const char* fn, int l, const char* s = 0) noexcept;
void bh3(const char* fn, int l, const char* s = 0) noexcept;

void flush_out(bool nl = false) noexcept;

inline std::ostream& lout() noexcept { return std::cout; }

inline char* p_strdup(const char* p) noexcept {
  if (!p) return nullptr;
  return ::strdup(p);
}
inline size_t p_strlen(const char* p) noexcept { return p ? ::strlen(p) : 0; }
inline void p_free(void* p) noexcept {
  if (p) ::free(p);
}

namespace str {

using std::string;

string sToLower(const string& s) noexcept;
string sToUpper(const string& s) noexcept;

inline string sToLower(const char* cs) noexcept {
  string e;
  return (cs && *cs) ? sToLower(string(cs)) : e;
}
inline string sToUpper(const char* cs) noexcept {
  string e;
  return (cs && *cs) ? sToUpper(string(cs)) : e;
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
inline string concat(const string& a, const char* b) noexcept { return b ? a + b : a; }

inline string concat(const char* a, const string& b, const char* c = nullptr) noexcept {
  size_t len = p_strlen(a) + p_strlen(c) + b.length();
  if (!len) return {};
  string z;
  z.reserve(len + 1);
  if (a) z = a;
  z.append(b);
  if (c) z.append(c);
  return z;
}

inline string concat(const string& a, const char* b, const string& c) noexcept {
  size_t len = a.length() + c.length() + p_strlen(b);
  if (!len) return {};
  string z;
  z.reserve(len + 1);
  z = a;
  if (b) z.append(b);
  z.append(c);
  return z;
}

inline string concat(const char* a,
                     const char* b,
                     const char* c = nullptr,
                     const char* d = nullptr) noexcept {
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

inline string concat(const char* a, const char* b, const string& c, const string& d = "") noexcept {
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

inline const char* trimFront(const char* z) noexcept {
  if (z && *z) {
    while (std::isspace(*z)) z++;
  }
  return z;
}

}  // NS str

template <typename T>
inline void logArray(const T* A, size_t n, const char* pref) noexcept {
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
inline void prnArray(std::ostream& os, const T* A, size_t n, const char* pref) noexcept {
  if (pref) os << pref;
  if (!A || !n) {
    os << " (empty)" << std::endl;
    return;
  }
  for (size_t i = 0; i < n; i++) os << ' ' << A[i];
  os << std::endl;
}

inline void logVec(const std::vector<bool>& vec, const char* pref) noexcept {
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
inline void logVec(const std::vector<T>& vec, const char* pref) noexcept {
  const T* A = (vec.empty() ? nullptr : vec.data());
  logArray(A, vec.size(), pref);
}

}  // namespace pinc

#endif
