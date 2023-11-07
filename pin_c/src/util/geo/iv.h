// class Iv - Interval
#pragma once

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
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace pinc {

inline constexpr int64_t add64(int a, int b) noexcept {
  return int64_t(a) + int64_t(b);
}

inline constexpr int64_t sub64(int a, int b) noexcept {
  return int64_t(a) - int64_t(b);
}

struct Iv {
  int a_ = INT_MIN, b_ = INT_MIN;

  Iv() noexcept = default;
  Iv(int a, int b) noexcept : a_(a), b_(b) {}

  void set(Iv i) noexcept { *this = i; }
  void set(int a, int b) noexcept {
    a_ = a;
    b_ = b;
  }
  void setZero() noexcept {
    a_ = 0;
    b_ = 0;
  }

  int len() const noexcept { return b_ - a_; }
  bool isLen0() const noexcept { return a_ == b_; }
  bool valid() const noexcept { return b_ != INT_MIN; }
  bool normal() const noexcept { return a_ <= b_; }
  static bool normal(int a, int b) noexcept { return a <= b; }
  void invalidate() noexcept { b_ = INT_MIN; }
  void normalize() noexcept {
    if (a_ > b_) std::swap(a_, b_);
  }
  bool inverted() const noexcept { return not normal(); }
  void invert() noexcept { std::swap(a_, b_); }
  int center() const noexcept { return add64(a_, b_) / 2; }

  static bool inside(int t, int a, int b) noexcept {
    if (a > b) std::swap(a, b);
    return t >= a && t <= b;
  }
  static bool insideNorm(int t, int a, int b) noexcept {
    assert(normal(a, b));
    return t >= a && t <= b;
  }

  static bool insideOpen(int t, int a, int b) noexcept {
    if (a > b) std::swap(a, b);
    return t > a && t < b;
  }
  static bool insideOpenNorm(int t, int a, int b) noexcept {
    assert(normal(a, b));
    return t > a && t < b;
  }

  bool operator<(Iv i) const noexcept {
    if (a_ < i.a_) return true;
    if (a_ > i.a_) return false;
    return b_ < i.b_;
  }
  bool operator==(Iv i) const noexcept { return a_ == i.a_ && b_ == i.b_; }
  bool operator!=(Iv i) const noexcept { return not operator==(i); }
};
inline std::ostream& operator<<(std::ostream& os, Iv iv) {
  os << "(iv " << iv.a_ << ' ' << iv.b_ << ')';
  return os;
}

}  // namespace pinc
