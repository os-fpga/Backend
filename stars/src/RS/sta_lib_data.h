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

enum Port_direction_type { INPUT = 0, OUTPUT, INVALID_DIR };

enum Timing_sense_type { POSITIVE = 0, NEGATIVE, INVALID_SENSE };

enum Pin_type { DATA = 0, CLOCK, RESET, SET, ENABLE, INVALID_PIN_TYPE };

enum Timing_arc_type { TRANSITION = 0, SETUP, HOLD };

enum Cell_type { INTERCONNECT = 0, LUT, BRAM, DSP, SEQUENTIAL, BLACKBOX, INVALID_CELL };

//const char* str_Pin_type(Pin_type t) noexcept;

class TimingArc;

class lib_pin {
private:
  string name_;
  int bus_width_ = 0;
  Port_direction_type dir_ = INPUT;
  Pin_type pin_type_ = DATA;
  vector<TimingArc> timing_arcs_;

public:
  lib_pin() noexcept = default;

  const string& name() const noexcept { return name_; }
  void setName(const string& name) noexcept { name_ = name; }

  int bus_width() const noexcept { return bus_width_; }
  void bus_width(int value) noexcept { bus_width_ = value; }

  Port_direction_type direction() const noexcept { return dir_; }
  void setDirection(Port_direction_type value) noexcept { dir_ = value; }

  Pin_type type() const noexcept { return pin_type_; }
  void setType(Pin_type value) noexcept { pin_type_ = value; }

  void add_timing_arc(const TimingArc& arch) noexcept { timing_arcs_.push_back(arch); }
  const vector<TimingArc>& get_timing_arcs() const noexcept { return timing_arcs_; }
};

class TimingArc {
private:
  Timing_sense_type sense_ = POSITIVE;
  Timing_arc_type type_ = TRANSITION;
  lib_pin related_pin_;

public:
  TimingArc() = default;

  Timing_sense_type sense() const noexcept { return sense_; }
  void setSense(Timing_sense_type value) noexcept { sense_ = value; }

  Timing_arc_type type() const noexcept { return type_; }
  void setType(Timing_arc_type value) noexcept { type_ = value; }

  lib_pin related_pin() const noexcept { return related_pin_; }
  void setRelatedPin(lib_pin value) noexcept { related_pin_ = value; }
};

class lib_cell {
private:
  string name_;
  Cell_type type_ = INTERCONNECT;
  vector<lib_pin> input_pins_;
  vector<lib_pin> output_pins_;

public:
  lib_cell() = default;

  const string& name() const noexcept { return name_; }
  void setName(const string& name) noexcept { name_ = name; }

  Cell_type type() const noexcept { return type_; }
  void setType(Cell_type type) noexcept { type_ = type; }

  const vector<lib_pin>& get_inputs() const noexcept { return input_pins_; }
  const vector<lib_pin>& get_outputs() const noexcept { return output_pins_; }

  void add_input(const lib_pin& pin) noexcept { input_pins_.push_back(pin); }
  void add_output(const lib_pin& pin) noexcept { output_pins_.push_back(pin); }

/*
  void add_pin(const lib_pin& pin, Port_direction_type dir) noexcept {
    if (dir == INPUT) {
      input_pins_.push_back(pin);
    } else if (dir == OUTPUT) {
      output_pins_.push_back(pin);
    } else {
      std::cout << "[Error] [STARS] Reject adding invalid pin." << std::endl;
      std::cerr << "[Error] [STARS] Reject adding invalid pin." << std::endl;
      assert(0);
    }
  }
*/
};

}  // end namespace rsbe

#endif
