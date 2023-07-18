#pragma once
#ifndef __rsbe__sta_lib_data_H_h_
#define __rsbe__sta_lib_data_H_h_

#include "pinc_log.h"
#include <map>

#undef INPUT
#undef OUTPUT
#undef POSITIVE
#undef NEGATIVE
#undef DATA
#undef CLOCK
#undef RESET
#undef SET
#undef ENABLE
#undef TRANSITION
#undef SETUP
#undef HOLD
#undef INTERCONNECT
#undef LUT
#undef BRAM
#undef DSP
#undef BLACKBOX

namespace rsbe {

using std::string;
using std::vector;
using namespace pinc;

typedef enum { INPUT = 0, OUTPUT, INVALID_DIR } port_direction_type;
typedef enum { POSITIVE = 0, NEGATIVE, INVALID_SENSE } timing_sense_type;
typedef enum { DATA = 0, CLOCK, RESET, SET, ENABLE, INVALID_PIN_TYPE } pin_type;
typedef enum { TRANSITION = 0, SETUP, HOLD } timing_type_type;

typedef enum { INTERCONNECT = 0, LUT, BRAM, DSP, SEQUENTIAL, BLACKBOX, INVALID_CELL } cell_type;

// cell data container
class TimingArc;

class lib_pin {
private:
  string name_;
  int bus_width_ = 0;
  port_direction_type dir_ = INPUT;
  pin_type pin_type_ = DATA;
  vector<TimingArc> timing_arch_list_;

public:
  lib_pin() = default;

  const string& name() const noexcept { return name_; }
  void setName(const string& name) noexcept { name_ = name; }

  int bus_width() const { return bus_width_; }
  void bus_width(int value) { bus_width_ = value; }
  port_direction_type direction() const { return dir_; }
  void direction(port_direction_type value) { dir_ = value; }

  pin_type type() const { return pin_type_; }
  void type(pin_type value) { pin_type_ = value; }

  void add_timing_arch(TimingArc& arch) {
    timing_arch_list_.push_back(arch);
    return;
  }
  const vector<TimingArc>& get_timing_arch() const { return timing_arch_list_; }
};

class TimingArc {
private:
  timing_sense_type sense_ = POSITIVE;
  timing_type_type type_ = TRANSITION;
  lib_pin related_pin_;

public:
  TimingArc() = default;

  timing_sense_type sense() const noexcept { return sense_; }
  void setSense(timing_sense_type value) noexcept { sense_ = value; }

  timing_type_type type() const noexcept { return type_; }
  void setType(timing_type_type value) noexcept { type_ = value; }

  lib_pin related_pin() const { return related_pin_; }
  void related_pin(lib_pin value) { related_pin_ = value; }
};

class lib_cell {
private:
  string name_;
  cell_type type_ = INTERCONNECT;
  vector<lib_pin> input_pins_;
  vector<lib_pin> output_pins_;
  vector<lib_pin> null_pins;

public:
  lib_cell() = default;

  const string& name() const noexcept { return name_; }
  void setName(const string& name) noexcept { name_ = name; }

  cell_type type() const { return type_; }
  void type(cell_type type) { type_ = type; }

  const vector<lib_pin>& get_pins(port_direction_type dir) const {
    if (dir == INPUT) {
      return input_pins_;
    } else if (dir == OUTPUT) {
      return output_pins_;
    } else {
      std::cerr << "[STARS] Reject returning invalid pin." << std::endl;
    }
    return null_pins;
  }
  void add_pin(lib_pin& pin, port_direction_type dir) {
    if (dir == INPUT) {
      input_pins_.push_back(pin);
    } else if (dir == OUTPUT) {
      output_pins_.push_back(pin);
    } else {
      std::cerr << "[STARS] Reject adding invalid pin." << std::endl;
    }
    return;
  }
  /*
  lib_pin &get_pin_by_name(string name) {
    for (auto pin : input_pins_) {
      if (pin.name() == name) {
        return pin;
      }
    }
    for (auto pin : output_pins_) {
      if (pin.name() == name) {
        return pin;
      }
    }
    null_pin = new lib_pin;
    null_pin.direction(INVALID_DIR);
    return null_pin;
  }
  */
};

}  // end namespace rsbe

#endif
