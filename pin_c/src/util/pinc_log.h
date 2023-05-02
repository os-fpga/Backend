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

#include <inttypes.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cfloat>
#include <climits>
#include <cmath>
#include <cstdint>
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
#include <type_traits>
#include <functional>
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

void bh1(const char* fn, int l, const char* s=0) noexcept;
void bh2(const char* fn, int l, const char* s=0) noexcept;
void bh3(const char* fn, int l, const char* s=0) noexcept;

void flush_out(bool nl = false) noexcept;

inline std::ostream& lout() noexcept { return std::cout; }

std::string sToLower(const std::string& s) noexcept;
std::string sToUpper(const std::string& s) noexcept;

inline std::string sToLower(const char* cs) noexcept {
    std::string e;
    return (cs && *cs) ? sToLower(std::string(cs)) : e;
}
inline std::string sToUpper(const char* cs) noexcept {
    std::string e;
    return (cs && *cs) ? sToUpper(std::string(cs)) : e;
}

std::string sReplicate(char c, uint num) noexcept;
inline std::string sSpace(uint num) noexcept { return sReplicate(' ', num); }

template <typename T>
inline void logArray(const T* A, size_t n, const char* prefix) noexcept {
  auto& os = lout();
  if (prefix) os << prefix;
  if (!A || !n) {
    os << " (empty)" << std::endl;
    return;
  }
  for (size_t i = 0; i < n; i++) os << ' ' << A[i];
  os << std::endl;
}

inline void logVec(const std::vector<bool>& vec, const char* prefix) noexcept {
  auto& os = lout();
  if (prefix) os << prefix;
  if (vec.empty()) {
    os << " (empty)" << std::endl;
    return;
  }
  for (size_t i = 0; i < vec.size(); i++) os << ' ' << vec[i];
  os << std::endl;
}

template <typename T>
inline void logVec(const std::vector<T>& vec, const char* prefix) noexcept {
  const T* A = (vec.empty() ? nullptr : vec.data());
  logArray(A, vec.size(), prefix);
}

}  // namespace pinc

#endif
