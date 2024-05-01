//
// logging and debug tracing
//
//   lprintf  : log-printf
//   lprintfl : log-printf with file:line
//   lprintf2 : log-printf with CC to stderr
//
//     lputs  : log-puts
//      lout  : replaces cout, will print to both stdout and logfile
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
#ifndef __pln_PINC_LOG_H__5b74600299d7_
#define __pln_PINC_LOG_H__5b74600299d7_

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
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>
#include <array>

#define RSBE_PLANNER_MODE 1

namespace pln {

using CStr = const char*;

// log-trace value (debug print verbosity)
// can be set in main() by calling set_ltrace()
uint16_t ltrace() noexcept;
void set_ltrace(int t) noexcept;

void lprintf(CStr format, ...) __attribute__((format(printf, 1, 2)));

// lprintf2 : log-printf with CC to stderr
void lprintf2(CStr format, ...) __attribute__((format(printf, 1, 2)));

// lprintfl : log-printf with file:line
void lprintfl(CStr fn, uint l, CStr format, ...);

// printf to output-stream 'os'
void os_printf(std::ostream& os, CStr format, ...);

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
void bh4(CStr fn, int l, CStr s = 0) noexcept;
void bh5(CStr fn, int l, CStr s = 0) noexcept;
void bh6(CStr fn, int l, CStr s = 0) noexcept;
void bh7(CStr fn, int l, CStr s = 0) noexcept;

void flush_out(bool nl = false) noexcept;

inline std::ostream& lout() noexcept { return std::cout; }

inline char* p_strdup(CStr p) noexcept {
  if (!p) return nullptr;
  return ::strdup(p);
}
inline size_t p_strlen(CStr p) noexcept { return p ? ::strlen(p) : 0; }
inline void p_free(void* p) noexcept {
  if (p) ::free(p);
}

int get_PID() noexcept;
std::string get_CWD() noexcept;
void traceEnv(int argc = 0, CStr* argv = nullptr) noexcept;

namespace str {

using std::string;

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

inline CStr trimFront(CStr z) noexcept {
  if (z && *z) {
    while (std::isspace(*z)) z++;
  }
  return z;
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
inline void prnArray(std::ostream& os, const T* A, size_t n, CStr pref) noexcept {
  if (pref) os << pref;
  if (!A || !n) {
    os << " (empty)" << std::endl;
    return;
  }
  for (size_t i = 0; i < n; i++) os << ' ' << A[i];
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

}  // namespace pln

const char* pln_get_version();

#endif
