#pragma once
#ifndef _pln_PIN_e9683bb04466931_H_
#define _pln_PIN_e9683bb04466931_H_

#include "util/geo/xyz.h"
#include "util/pln_log.h"
#include <bitset>

namespace pln {

using std::string;
using std::vector;
using StringPair = std::pair<std::string, std::string>;

struct Pin {

  static constexpr uint MAX_PT_COLS = 128;

  string orig_pin_name_;  // never translated
  string udes_pin_name_;  // maybe translated
  string trans_pin_name_; // translated due to netlist edits
  string placed_pin_name_;

  string device_pin_name_;

  XYZ xyz_;

  uint pt_row_   = 0; // row in pin table

  std::bitset<MAX_PT_COLS> rx_modes_;
  std::bitset<MAX_PT_COLS> tx_modes_;
  std::bitset<MAX_PT_COLS> all_modes_; // GPIO mode is non-RX and non-TX

  bool is_usr_input_ = false;
  bool is_dev_input_ = false;

  bool is_a2f_   = false;
  bool is_f2a_   = false;

  Pin() noexcept {
    rx_modes_.reset();
    tx_modes_.reset();
    all_modes_.reset();
  }

  Pin(const string& u, const string& d, const XYZ& xyz, uint r) noexcept
    : orig_pin_name_(u), udes_pin_name_(u),
      device_pin_name_(d),
      xyz_(xyz), pt_row_(r) {
    rx_modes_.reset();
    tx_modes_.reset();
    all_modes_.reset();
  }

  void set_udes_pin_name(const string& pn) noexcept {
    orig_pin_name_ = pn;
    udes_pin_name_ = pn;
  }

  bool is_translated() const noexcept {
    if (trans_pin_name_.empty())
      return false;
    return trans_pin_name_ == udes_pin_name_;
  }

  void reset() noexcept {
    rx_modes_.reset();
    tx_modes_.reset();
    all_modes_.reset();
    orig_pin_name_.clear();
    udes_pin_name_.clear();
    trans_pin_name_.clear();
    device_pin_name_.clear();
    xyz_.inval();
    pt_row_ = 0;
    is_usr_input_ = false;
    is_dev_input_ = false;
    is_a2f_ = false;
    is_f2a_ = false;
  }
};

//
// struct DevPin - new return type for get_available_device_pin()
//
// DevPin replaces StringPair here:
//  // get_available_ methods return pin_and_mode pair, empty strings on error
//  StringPair get_available_device_pin(RapidCsvReader& csv,
//                                      bool is_inp, const string& udesName,
//                                      Pin*& ann_pin);
//
struct DevPin {
  string device_pin_name_;
  string mode_;
  uint pt_row_ = UINT_MAX;

  DevPin() noexcept = default;

  DevPin(const string& dp, const string& m, uint r = UINT_MAX) noexcept
    : device_pin_name_(dp), mode_(m), pt_row_(r)
  {}

  void set(const string& dp, const string& m, uint r = UINT_MAX) noexcept {
    device_pin_name_ = dp;
    mode_ = m;
    pt_row_ = r;
  }
  void set_first(const string& dp) noexcept { device_pin_name_ = dp; }

  const string& first() const noexcept { return device_pin_name_; }

  void reset() noexcept {
    device_pin_name_.clear();
    mode_.clear();
    pt_row_ = UINT_MAX;
  }

  bool empty() const noexcept { return device_pin_name_.empty(); }
  bool valid_pt_row() const noexcept { return pt_row_ != UINT_MAX; }
};

}

#endif

