#pragma once

// LCell-related classes/enums
//
// Enums:    Port_direction_t, Port_direction_t, Pin_t, Timing_arc_t, LCell_t
// Classes:  

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


enum Port_direction_t { INPUT = 0, OUTPUT, INVALID_DIR };

enum Timing_sense_t { POSITIVE = 0, NEGATIVE, INVALID_SENSE };

enum Pin_t { DATA = 0, CLOCK, RESET, SET, ENABLE, INVALID_PIN_TYPE };

enum Timing_arc_t { TRANSITION = 0, SETUP, HOLD };

enum LCell_t { INTERCONNECT = 0, LUT, BRAM, DSP, SEQUENTIAL, BLACKBOX, INVALID_CELL };


class TimingArc;

class LibPin {
private:
  string name_;
  uint bus_width_ = 0;
  Port_direction_t dir_ = INPUT;
  Pin_t pin_type_ = DATA;
  vector<TimingArc> timing_arcs_;

public:
  LibPin() noexcept = default;

  const string& name() const noexcept { return name_; }
  void setName(const string& name) noexcept { name_ = name; }

  uint bus_width() const noexcept { return bus_width_; }
  void setWidth(uint value) noexcept { bus_width_ = value; }

  Port_direction_t direction() const noexcept { return dir_; }
  void setDirection(Port_direction_t value) noexcept { dir_ = value; }

  Pin_t type() const noexcept { return pin_type_; }
  void setType(Pin_t value) noexcept { pin_type_ = value; }

  void add_timing_arc(const TimingArc& arch) noexcept { timing_arcs_.push_back(arch); }
  const vector<TimingArc>& get_timing_arcs() const noexcept { return timing_arcs_; }
};

class TimingArc {
private:
  Timing_sense_t sense_  = POSITIVE;
  Timing_arc_t type_     = TRANSITION;

  LibPin related_pin_;

public:
  TimingArc() = default;

  Timing_sense_t sense() const noexcept { return sense_; }
  void setSense(Timing_sense_t value) noexcept { sense_ = value; }

  Timing_arc_t type() const noexcept { return type_; }
  void setType(Timing_arc_t v) noexcept { type_ = v; }

  const LibPin& related_pin() const noexcept { return related_pin_; }
  void setRelatedPin(LibPin value) noexcept { related_pin_ = value; }
};

class lib_cell {
private:
  string name_;
  LCell_t type_ = INTERCONNECT;
  vector<LibPin> input_pins_;
  vector<LibPin> output_pins_;

public:
  lib_cell() = default;

  const string& name() const noexcept { return name_; }
  void setName(const string& name) noexcept { name_ = name; }

  LCell_t type() const noexcept { return type_; }
  void setType(LCell_t t) noexcept { type_ = t; }

  const vector<LibPin>& get_inputs() const noexcept { return input_pins_; }
  const vector<LibPin>& get_outputs() const noexcept { return output_pins_; }

  void add_input(const LibPin& pin) noexcept { input_pins_.push_back(pin); }
  void add_output(const LibPin& pin) noexcept { output_pins_.push_back(pin); }
};

}

#endif

