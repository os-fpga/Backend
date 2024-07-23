// class Iv - Interval
#pragma once
#ifndef _PLN_util_geo_IV_h_b6e76e5fa705cfb_
#define _PLN_util_geo_IV_h_b6e76e5fa705cfb_

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
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace pln {

inline constexpr uint64_t hashComb(uint64_t a, uint64_t b) noexcept {
  constexpr uint64_t m = 0xc6a4a7935bd1e995;
  return (a ^ (b * m + (a << 6) + (a >> 2))) + 0xe6546b64;
}

inline constexpr uint64_t hashComb(uint64_t a, uint64_t b, uint64_t c) noexcept {
  return hashComb(a, hashComb(b, c));
}

inline constexpr int protectedAdd(int a, int b) noexcept {
  int64_t c = int64_t(a) + int64_t(b);
  if (c > INT_MAX) return INT_MAX;
  if (c < INT_MIN) return INT_MIN;
  return c;
}

inline constexpr int protectedSub(int a, int b) noexcept {
  int64_t c = int64_t(a) - int64_t(b);
  if (c > INT_MAX) return INT_MAX;
  if (c < INT_MIN) return INT_MIN;
  return c;
}

inline constexpr int64_t add64(int a, int b) noexcept {
  return int64_t(a) + int64_t(b);
}

inline constexpr int64_t sub64(int a, int b) noexcept {
  return int64_t(a) - int64_t(b);
}

inline constexpr int protectedRound(double x) noexcept {
  if (x >= 0) {
    if (x >= double(INT_MAX) - 0.5) return INT_MAX;
    return x + 0.5;
  }
  if (x <= double(INT_MIN) + 0.5) return INT_MIN;
  return x - 0.5;
}

inline constexpr int inlineRound(double x) noexcept {
  return x >= 0 ? x + 0.5 : x - 0.5;
}

inline constexpr int64_t inlineRound64(double x) noexcept {
  return x >= 0 ? int64_t(x + 0.5) : int64_t(x - 0.5);
}

inline constexpr int64_t protectedRound64(double x) noexcept {
  if (x >= 0) {
    if (x >= double(LLONG_MAX) - 0.5) return LLONG_MAX;
    return int64_t(x + 0.5);
  }
  if (x <= double(LLONG_MIN) + 0.5) return LLONG_MIN;
  return int64_t(x - 0.5);
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
  void toZero() noexcept {
    a_ = 0;
    b_ = 0;
  }

  uint64_t hashCode() const noexcept { return hashComb(a_, b_); }

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

  void bloat(int lo, int hi) noexcept {
    assert(valid() and normal());
    assert(lo >= 0 and hi >= 0);
    a_ -= lo;
    b_ += hi;
    assert(valid() and normal());
  }

  void bloat(int delta) noexcept {
    assert(delta >= 0);
    bloat(delta, delta);
  }

  void moveTo(int t) noexcept {
    assert(valid() and normal());
    int l = len();
    a_ = t;
    b_ = t + l;
  }
  void moveRel(int dt) noexcept {
    assert(valid() and normal());
    a_ += dt;
    b_ += dt;
  }

  inline void unite(Iv i) noexcept;
  inline void unite(int t) noexcept;

  bool intersects(Iv iv) const noexcept {
    return intersects(a_, b_, iv.a_, iv.b_);
  }
  inline static bool intersects(int a, int b, int c, int d) noexcept;

  bool overlaps(Iv iv) const noexcept { return overlaps(a_, b_, iv.a_, iv.b_); }
  inline static bool overlaps(int a, int b, int c, int d) noexcept;

  inline static int ovLen(int a, int b, int c, int d, int* p1 = nullptr,
                          int* p2 = nullptr) noexcept;
  int ovLen(Iv iv, int* p1 = nullptr, int* p2 = nullptr) const noexcept {
    return ovLen(a_, b_, iv.a_, iv.b_, p1, p2);
  }

  static bool inside(int t, int a, int b) noexcept {
    if (a > b) std::swap(a, b);
    return t >= a and t <= b;
  }
  static bool insideNorm(int t, int a, int b) noexcept {
    assert(normal(a, b));
    return t >= a and t <= b;
  }

  static bool insideOpen(int t, int a, int b) noexcept {
    if (a > b) std::swap(a, b);
    return t > a and t < b;
  }
  static bool insideOpenNorm(int t, int a, int b) noexcept {
    assert(normal(a, b));
    return t > a and t < b;
  }

  bool covers(Iv iv) const noexcept {
    assert(valid() and normal());
    assert(iv.valid());
    return insideNorm(iv.a_, a_, b_) and insideNorm(iv.b_, a_, b_);
  }
  bool strictlyCovers(Iv iv) const noexcept {
    assert(valid() and normal());
    assert(iv.valid());
    return insideOpenNorm(iv.a_, a_, b_) and insideOpenNorm(iv.b_, a_, b_);
  }

  bool operator<(Iv i) const noexcept {
    if (a_ < i.a_) return true;
    if (a_ > i.a_) return false;
    return b_ < i.b_;
  }
  bool operator==(Iv i) const noexcept { return a_ == i.a_ and b_ == i.b_; }
  bool operator!=(Iv i) const noexcept { return not operator==(i); }
};
inline std::ostream& operator<<(std::ostream& os, Iv iv) {
  os << "(iv " << iv.a_ << ' ' << iv.b_ << ')';
  return os;
}

inline bool Iv::intersects(int a, int b, int c, int d) noexcept {
  assert(a < b and c < d);
  if (c < a) {
    std::swap(a, c);
    std::swap(b, d);
  }
  if (d < b) return true;
  if (c > b) return false;
  return true;
}

inline bool Iv::overlaps(int a, int b, int c, int d) noexcept {
  assert(a < b and c < d);
  if (c < a) {
    std::swap(a, c);
    std::swap(b, d);
  }
  if (d <= b) return true;
  if (c >= b) return false;
  return true;
}

inline int Iv::ovLen(int a, int b, int c, int d, int* p1, int* p2) noexcept {
  assert(a < b and c < d);
  if (c < a) {
    std::swap(a, c);
    std::swap(b, d);
  }
  if (d <= b) {
    if (p1) *p1 = c;
    if (p2) *p2 = d;
    return d - c;
  }
  if (c >= b) return 0;

  if (p1) *p1 = c;
  if (p2) *p2 = b;
  return b - c;
}

inline void Iv::unite(Iv i) noexcept {
  assert(i.valid());
  i.normalize();
  if (valid()) {
    assert(normal());
    if (i.a_ < a_) a_ = i.a_;
    if (i.b_ > b_) b_ = i.b_;
    return;
  }
  set(i);
}

inline void Iv::unite(int t) noexcept {
  if (valid()) {
    assert(normal());
    if (t < a_) a_ = t;
    if (t > b_) b_ = t;
    return;
  }
  set(t, t);
}

}

#endif

