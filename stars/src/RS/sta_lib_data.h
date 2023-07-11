#pragma once
#ifndef __rsbe__sta_lib_data_H_h_
#define __rsbe__sta_lib_data_H_h_

#include "pinc_log.h"
#include <map>

namespace rsbe {

typedef enum { INPUT = 0, OUTPUT, INVALID_DIR } port_direction_type;
typedef enum { POSITIVE = 0, NEGATIVE, INVALID_SENSE } timing_sense_type;
typedef enum { DATA = 0, CLOCK, RESET, SET, ENABLE, INVALID_PIN_TYPE } pin_type;
typedef enum { TRANSITION = 0, SETUP, HOLD } timing_type_type;

typedef enum {
  INTERCONNECT = 0,
  LUT,
  BRAM,
  DSP,
  SEQUENTIAL,
  BLACKBOX,
  INVALID_CELL
} cell_type;

// cell data container
class timing_arch;
class lib_pin {
private:
  std::string name_;
  int bus_width_;
  port_direction_type dir_;
  pin_type pin_type_;
  std::vector<timing_arch> timing_arch_list_;

public:
  lib_pin(){};
  ~lib_pin(){};
  std::string name() const { return name_; }
  void name(std::string name) { name_ = name; }
  int bus_width() const { return bus_width_; }
  void bus_width(int value) { bus_width_ = value; }
  port_direction_type direction() const { return dir_; }
  void direction(port_direction_type value) { dir_ = value; }
  pin_type type() const { return pin_type_; }
  void type(pin_type value) { pin_type_ = value; }
  void add_timing_arch(timing_arch &arch) {
    timing_arch_list_.push_back(arch);
    return;
  }
  const std::vector<timing_arch> &get_timing_arch() const {
    return timing_arch_list_;
  }
};

class timing_arch {
private:
  timing_sense_type sense_;
  timing_type_type type_;
  lib_pin related_pin_;

public:
  timing_arch(){};
  ~timing_arch(){};
  timing_sense_type sense() const { return sense_; }
  void sense(timing_sense_type value) { sense_ = value; }
  timing_type_type type() const { return type_; }
  void type(timing_type_type value) { type_ = value; }
  lib_pin related_pin() const { return related_pin_; }
  void related_pin(lib_pin value) { related_pin_ = value; }
};

class lib_cell {
private:
  std::string name_;
  cell_type type_;
  std::vector<lib_pin> input_pins_;
  std::vector<lib_pin> output_pins_;
  std::vector<lib_pin> null_pins;

public:
  lib_cell(){};
  ~lib_cell(){};
  std::string name() const { return name_; }
  void name(std::string name) { name_ = name; }
  cell_type type() const { return type_; }
  void type(cell_type type) { type_ = type; }
  const std::vector<lib_pin> &get_pins(port_direction_type dir) const {
    if (dir == INPUT) {
      return input_pins_;
    } else if (dir == OUTPUT) {
      return output_pins_;
    } else {
      std::cerr << "[STARS] Reject returning invalid pin." << std::endl;
    }
    return null_pins;
  }
  void add_pin(lib_pin &pin, port_direction_type dir) {
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
  lib_pin &get_pin_by_name(std::string name) {
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

} // end namespace rsbe

#endif

