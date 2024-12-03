// class XY  - 2D Point
// class XYZ - 3D Point
#pragma once
#ifndef _pln_util_geo_XYZ_H_1c192a5bf5646b833_
#define _pln_util_geo_XYZ_H_1c192a5bf5646b833_

#include "util/geo/iv.h"

#undef XY
#undef XYZ

namespace pln {

struct XY {
  int x_ = INT_MIN, y_ = INT_MIN;

  XY() noexcept = default;
  XY(int a, int b) noexcept : x_(a), y_(b) {}
  explicit XY(ipair p) noexcept : x_(p.first), y_(p.second) {}

  bool valid() const noexcept { return x_ != INT_MIN; }
  bool nonNeg() const noexcept { return x_ >= 0; }
  void inval() noexcept { x_ = INT_MIN; }

  void setPoint(int a, int b) noexcept {
    x_ = a;
    y_ = b;
  }
  void setPoint(XY a) noexcept { *this = a; }
  void setZero() noexcept {
    x_ = 0;
    y_ = 0;
  }

  XY translated(int dx, int dy) const noexcept {
    XY ret(*this);
    ret.x_ += dx;
    ret.y_ += dy;
    return ret;
  }
  void translate(int dx, int dy) noexcept {
    x_ += dx;
    y_ += dy;
  }
  void translate(XY d) noexcept { translate(d.x_, d.y_); }

  void negate() noexcept {
    x_ = -x_;
    y_ = -y_;
  }
  XY negated() const noexcept { return XY(-x_, -y_); }

  int64_t dot(XY b) const noexcept {
    return int64_t(x_) * int64_t(b.x_) + int64_t(y_) * int64_t(b.y_);
  }
  int64_t det(XY b) const noexcept {
    return int64_t(x_) * int64_t(b.y_) - int64_t(y_) * int64_t(b.x_);
  }
  XY perp() const noexcept { return XY(-y_, x_); }
  XY conj() const noexcept { return XY(x_, -y_); }

  static int64_t det3(XY a, XY b, XY c) noexcept {
    b -= a;
    c -= a;
    return b.det(c);
  }

  bool equals2(int a, int b) const noexcept { return x_ == a && y_ == b; }
  bool equals2(XY p) const noexcept { return x_ == p.x_ && y_ == p.y_; }
  bool less(const XY& p) const noexcept {
    if (x_ < p.x_) return true;
    if (x_ > p.x_) return false;
    return y_ < p.y_;
  }
  bool operator==(XY p) const noexcept { return x_ == p.x_ && y_ == p.y_; }
  bool operator!=(XY p) const noexcept { return not operator==(p); }
  bool operator<(XY p) const noexcept { return less(p); }
  bool operator<=(XY p) const noexcept { return not p.less(*this); }
  bool operator>(XY p) const noexcept { return p.less(*this); }
  bool operator>=(XY p) const noexcept { return not less(p); }
  void operator+=(XY p) noexcept {
    x_ += p.x_;
    y_ += p.y_;
  }
  void operator-=(XY p) noexcept {
    x_ -= p.x_;
    y_ -= p.y_;
  }

  void operator = (ipair p) noexcept {
    x_ = p.first;
    y_ = p.second;
  }

  static constexpr int p_round(double x) noexcept {
    if (x >= 0) {
      if (x >= double(INT_MAX) - 0.5) return INT_MAX;
      return x + 0.5;
    }
    if (x <= double(INT_MIN) + 0.5) return INT_MIN;
    return x - 0.5;
  }

  void multiply(double t) noexcept {
    x_ = p_round(t * x_);
    y_ = p_round(t * y_);
  }
  XY multipliedBy(double t) const noexcept {
    XY c(*this);
    c.multiply(t);
    return c;
  }
  XY& operator*=(double t) noexcept {
    multiply(t);
    return *this;
  }

  int distL1(XY p) const noexcept {
    return std::abs(x_ - p.x_) + std::abs(y_ - p.y_);
  }
  int dist(XY p) const noexcept {
    return std::abs(x_ - p.x_) + std::abs(y_ - p.y_);
  }
  int dist(int a, int b) const noexcept {
    return std::abs(x_ - a) + std::abs(y_ - b);
  }
  int distL1(int a, int b) const noexcept {
    return std::abs(x_ - a) + std::abs(y_ - b);
  }
  int distL2(XY p) const noexcept {
    return p_round(::hypot(p.x_ - x_, p.y_ - y_));
  }
  int distL2(int a, int b) const noexcept {
    return p_round(::hypot(a - x_, b - y_));
  }
  double sqDist(XY p) const noexcept {
    return double(p.x_ - x_) * (p.x_ - x_) + double(p.y_ - y_) * (p.y_ - y_);
  }

  uint64_t cantorHash() const noexcept {
    uint64_t a = x_;
    uint64_t b = y_;
    return ((a + b) >> 1) * (a + b + 1) + b;
  }
  uint64_t fnvHash() const noexcept {
    uint64_t a = x_;
    uint64_t b = y_;
    constexpr uint64_t m = 0xc6a4a7935bd1e995;
    return (a ^ (b * m + (a << 6) + (a >> 2))) + 0xe6546b64;
  }

  static inline std::string i2s(int i) noexcept { return std::to_string(i); }

  static inline std::string conc(const std::string& a, char ch, const std::string& b) noexcept {
    std::string z;
    z.reserve(a.length() + b.length() + 2);
    z = a;
    z.push_back(ch);
    z += b;
    return z;
  }

  static inline std::string conc(char a, const std::string& s, char b) noexcept {
    std::string z;
    z.reserve(s.length() + 3);
    z.push_back(a);
    z += s;
    z.push_back(b);
    return z;
  }

  std::string x_spc_y() const noexcept { return conc( i2s(x_), ' ', i2s(y_) ); }
  std::string x_com_y() const noexcept { return conc( i2s(x_), ',', i2s(y_) ); }
  std::string toString() const noexcept { return conc( '(', x_spc_y(), ')' ); }
};

struct XYZ : public XY {
  int z_ = INT_MIN;

  XYZ() noexcept = default;
  XYZ(int a, int b, int c = INT_MIN) noexcept : XY(a, b), z_(c) {}
  XYZ(XY p, int c) noexcept : XY(p), z_(c) {}

  const XY& cast2xy() const noexcept { return *this; }

  void set3(int a, int b, int c) noexcept {
    x_ = a;
    y_ = b;
    z_ = c;
  }
  void set3(XY p, int c) noexcept {
    x_ = p.x_;
    y_ = p.y_;
    z_ = c;
  }
  void set3(XYZ u) noexcept { *this = u; }
  void setZ(int zz) noexcept { z_ = zz; }

  bool operator==(XYZ u) const noexcept {
    return x_ == u.x_ && y_ == u.y_ && z_ == u.z_;
  }
  bool operator!=(XYZ u) const noexcept { return not operator==(u); }
  bool operator<(XYZ u) const noexcept {
    if (this->less(u)) return true;
    if (u.less(*this)) return false;
    return z_ < u.z_;
  }
  bool equals(int a, int b, int c) const noexcept {
    return x_ == a && y_ == b && z_ == c;
  }

  int dist3D(XYZ u) const noexcept { return dist(u) + std::abs(z_ - u.z_); }

  std::string toString() const noexcept {
    std::string s{"("};
    s += std::to_string(x_);
    s.push_back(' ');
    s += std::to_string(y_);
    s.push_back(' ');
    s += std::to_string(z_);
    s.push_back(')');
    return s;
  }
};

inline std::ostream& operator<<(std::ostream& os, XY p) {
  os << '(' << p.x_ << ',' << p.y_ << ')';
  return os;
}

inline std::ostream& operator<<(std::ostream& os, const XYZ& u) {
  os << '(' << u.x_ << ' ' << u.y_ << " _" << u.z_ << ')';
  return os;
}

inline int64_t dot(XY a, XY b) noexcept { return a.dot(b); }
inline int64_t det(XY a, XY b) noexcept { return a.det(b); }

inline XY operator-(XY a, XY b) noexcept {
  return XY(a.x_ - b.x_, a.y_ - b.y_);
}
inline XY operator+(XY a, XY b) noexcept {
  return XY(a.x_ + b.x_, a.y_ + b.y_);
}

inline int abc_ori(XY a, XY b, XY c) noexcept {
  int64_t det = XY::det3(a, b, c);
  if (det > 0) return 1;
  if (det < 0) return -1;
  return 0;
}

}

#endif

