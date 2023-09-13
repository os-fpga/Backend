#pragma once
#ifndef __rs_PIN_e9683bb04466931_H_
#define __rs_PIN_e9683bb04466931_H_

#include "util/geo/xyz.h"
#include "util/pinc_log.h"
#include <bitset>

namespace pinc {

using std::string;
using std::vector;
using StringPair = std::pair<std::string, std::string>;

struct Pin {

  static constexpr uint MAX_PT_COLS = 128;

  string udes_pin_name_;
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
    : udes_pin_name_(u), device_pin_name_(d),
      xyz_(xyz), pt_row_(r) {
    rx_modes_.reset();
    tx_modes_.reset();
    all_modes_.reset();
  }

  void reset() noexcept {
    rx_modes_.reset();
    tx_modes_.reset();
    all_modes_.reset();
    udes_pin_name_.clear();
    device_pin_name_.clear();
    xyz_.invalidate();
    pt_row_ = 0;
    is_usr_input_ = false;
    is_dev_input_ = false;
    is_a2f_ = false;
    is_f2a_ = false;
  }
};

}

#endif

