// logging and debug tracing
#pragma once
#ifndef __rs_PINC_LOG_H_
#define __rs_PINC_LOG_H_

#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cctype>
#include <climits>
#include <cassert>
#include <inttypes.h>
#include <type_traits>
#include <algorithm>
#include <utility>
#include <numeric>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <memory>
#include <limits>

namespace pinc {

// log-trace value (debug print verbosity)
// can be set in main() by calling set_ltrace()
uint16_t ltrace() noexcept;
void set_ltrace(int t) noexcept;

// log-printf, log-puts, lout
// currently, log- functions just print on cout, real logfile can be added later
void lprintf(const char* format, ...) __attribute__ ((format (printf, 1, 2)));
void lputs(const char* cs = nullptr) noexcept;
void lputs(const std::string& s) noexcept;
inline std::ostream& lout() noexcept { return std::cout; }

std::string strToLower(const std::string& s) noexcept;

std::string strReplicate(char c, uint num) noexcept;
inline std::string strSpace(uint num) noexcept { return strReplicate(' ', num); }

inline bool hasSpaces(const char* cs) noexcept {
    if (!cs || !cs[0]) return false;
    for (; *cs; cs++)
        if (std::isspace(*cs))
            return true;
    return false;
}
inline bool hasSpaces(const std::string& s) noexcept { return s.empty() ? false : hasSpaces(s.c_str()); }
inline bool hasNonSpaces(const char* cs) noexcept {
    if (!cs || !cs[0]) return true;
    for (; *cs; cs++)
        if (not std::isspace(*cs))
            return true;
    return false;
}
inline bool hasNonSpaces(const std::string& s) noexcept { return s.empty() ? true : hasNonSpaces(s.c_str()); }

std::string strUnspace(const char* cs) noexcept;
inline std::string strUnspace(const std::string& s) noexcept { return s.empty() ? s : strUnspace(s.c_str()); }

template <typename T>
inline void logArray(const T* A, size_t n, const char* prefix) noexcept {
    auto& os = lout();
    if (prefix) os << prefix;
    if (!A || !n) {
        os << " (empty)" << std::endl;
        return;
    }
    for (size_t i = 0; i < n; i++)
        os << ' ' << A[i];
    os << std::endl;
}

inline void logVec(const std::vector<bool>& vec, const char* prefix) noexcept {
    auto& os = lout();
    if (prefix) os << prefix;
    if (vec.empty()) {
        os << " (empty)" << std::endl;
        return;
    }
    for (size_t i = 0; i < vec.size(); i++)
        os << ' ' << vec[i];
    os << std::endl;
}

template <typename T>
inline void logVec(const std::vector<T>& vec, const char* prefix) noexcept {
    const T* A = (vec.empty() ? nullptr : vec.data());
    logArray(A, vec.size(), prefix);
}

#ifndef NDEBUG
void  stop_here(const char* fn, int lnum, const char* mess=nullptr) noexcept;
void stop_here1(const char* fn, int lnum, const char* mess=nullptr) noexcept;
void stop_here2(const char* fn, int lnum, const char* mess=nullptr) noexcept;
void stop_here3(const char* fn, int lnum, const char* mess=nullptr) noexcept;
#endif

}

#endif

