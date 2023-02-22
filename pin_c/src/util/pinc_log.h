// logging and debug tracing
#pragma once
#ifndef __rs_PINC_LOG_H_
#define __rs_PINC_LOG_H_

#include <inttypes.h>
#include <strings.h>

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
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <sstream>
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

// log-printf, log-puts, lout
// currently, log- functions just print on cout, real logfile can be added later
void lprintf(const char* format, ...) __attribute__((format(printf, 1, 2)));
void lputs(const char* cs = nullptr) noexcept;
void lputs2(const char* cs = nullptr) noexcept;
void lputs(const std::string& s) noexcept;
inline std::ostream& lout() noexcept { return std::cout; }

std::string strToLower(const std::string& s) noexcept;

std::string strReplicate(char c, uint num) noexcept;
inline std::string strSpace(uint num) noexcept {
  return strReplicate(' ', num);
}

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
