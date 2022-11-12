#ifndef sta_lib_data_H
#define sta_lib_data_H

#include <iostream>
#include <map>
#include <vector>

namespace stars {

typedef enum { INPUT = 0, OUTPUT, INVALID_DIR } port_direction_type;
typedef enum { POSITIVE = 0, NEGATIVE, INVALID_SENSE } timing_sense_type;
typedef enum { DATA = 0, CLOCK, RESET, SET, ENABLE, INVALID_PIN_TYPE } pin_type;

typedef enum {
  INTERCONNECT = 0,
  LUT,
  BRAM,
  DSP,
  FLIPFLOP,
  BLACKBOX,
  INVALID_CELL
} cell_type;

// cell data container
class lib_pin {
private:
  std::string name_;
  int bus_width_;
  port_direction_type dir_;
  std::vector<lib_pin> related_pins_;
  timing_sense_type timing_sense_;
  pin_type pin_type_;

public:
  lib_pin(){};
  ~lib_pin(){};
  std::string name() const { return name_; }
  void name(std::string name) { name_ = name; }
  int bus_width() const { return bus_width_; }
  void bus_width(int value) { bus_width_ = value; }
  port_direction_type direction() const { return dir_; }
  void direction(port_direction_type value) { dir_ = value; }
  timing_sense_type timing_sense() const { return timing_sense_; }
  void timing_sense(timing_sense_type value) { timing_sense_ = value; }
  pin_type type() const { return pin_type_; }
  void type(pin_type value) { pin_type_ = value; }
  void add_related_pin(lib_pin &pin) {
    related_pins_.push_back(pin);
    return;
  }
  const std::vector<lib_pin> &get_related_pins() const { return related_pins_; }
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
};

} // end namespace stars
#endif
