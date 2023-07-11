
#include "sta_lib_writer.h"

#include <fstream>
#include <map>
#include <set>

namespace rsbe {

void sta_lib_writer::write_header(std::ostream& os) {
  // initialize lib data
  init_lib();

  // header
  os << "library (" << lib_header["name"] << ") {\n";

  // lib info
  for (auto const& field : lib_info) {
    os << INDENT1 << field.first << SEPARATOR << field.second << TRAILER;
  }

  // lib unit
  for (auto const& field : lib_unit) {
    os << INDENT1 << field.first << SEPARATOR << field.second << TRAILER;
  }

  // lib cap load
  for (auto const& field : lib_cap_load) {
    os << INDENT1 << field.first << INDENT1 << field.second << TRAILER;
  }

  // lib feature
  for (auto const& field : lib_feature) {
    os << INDENT1 << field.first << INDENT1 << field.second << TRAILER;
  }

  // lib data settings
  for (auto const& field : lib_data_setting) {
    os << INDENT1 << field.first << SEPARATOR << field.second << TRAILER;
  }

  // lib operating conditions
  for (auto const& condition : lib_operating_conditions) {
    os << INDENT1 << "operation_conditions(" << condition.first << ")" << INDENT1 << "{\n";
    for (auto const& field : condition.second) {
      os << INDENT2 << field.first << SEPARATOR << field.second << TRAILER;
    }
    os << INDENT1 << "}\n";
  }

  // bus type
  for (auto const& field : lib_bus_type) {
    os << INDENT1 << field.first << SEPARATOR << field.second << TRAILER;
  }

  return;
};

void sta_lib_writer::write_bus_type(std::ostream& os, int from, int to, bool down_to) {
  int width = 0;
  if (down_to) {
    width = from - to + 1;
  } else {
    width = to - from + 1;
  }
  os << INDENT1 << "type(bus" << width << ") {\n";
  os << INDENT2 << "base_type" << SEPARATOR << "array" << TRAILER;
  os << INDENT2 << "data_type" << SEPARATOR << "bit" << TRAILER;
  os << INDENT2 << "bit_width" << SEPARATOR << width << TRAILER;
  os << INDENT2 << "bit_from" << SEPARATOR << from << TRAILER;
  os << INDENT2 << "bit_to" << SEPARATOR << to << TRAILER;
  os << INDENT2 << "down_to" << SEPARATOR << (down_to ? "true" : "false") << TRAILER;
  os << INDENT1 << "}" << TRAILER;
  return;
};

void sta_lib_writer::write_cell(std::ostream& os, const lib_cell& cell) {
  std::cout << "STARS: Writing cell <" << cell.name() << ">..." << std::endl;

  // 1. cell name
  os << INDENT1 << "cell (" << cell.name() << ") {\n";
  // ff needs pre-fix
  if (cell.type() == SEQUENTIAL) {
    std::string q_name, clk_name, reset_name, set_name, enable_name;
    for (auto const& out_pin : cell.get_pins(OUTPUT)) {
      q_name = out_pin.name();
      break;
    }
    for (auto const& in_pin : cell.get_pins(INPUT)) {
      switch (in_pin.type()) {
        case CLOCK:
          clk_name = in_pin.name();
          break;
        case RESET:
          reset_name = in_pin.name();
          break;
        case SET:
          set_name = in_pin.name();
          break;
        case ENABLE:
          enable_name = in_pin.name();
          break;
        case INVALID_PIN_TYPE:
          assert(0);
          break;
      }
    }

    os << INDENT2 << "ff (\"" << q_name << "\") {\n";
    os << INDENT3 << "clocked_on: " << clk_name << ";\n";
    if (!reset_name.empty()) os << INDENT3 << "clear: " << reset_name << ";\n";
    if (!set_name.empty()) os << INDENT3 << "set: " << set_name << ";\n";
    if (!enable_name.empty()) os << INDENT3 << "enable: " << enable_name << ";\n";
    os << INDENT2 << "}\n";
  }

  // 2. pin
  write_cell_pin(os, cell);

  // 3. tailer
  os << INDENT1 << "}\n";

  return;
};

void sta_lib_writer::write_cell_pin(std::ostream& os, const lib_cell& cell) {
  // 1. output
  for (auto const& out_pin : cell.get_pins(OUTPUT)) {
    if (cell.type() == SEQUENTIAL) {
      os << INDENT2 << "pin(" << out_pin.name() << ") {\n";
      os << INDENT3 << "direction: output;\n";
      for (auto const& timing_arch : out_pin.get_timing_arch()) {
        write_timing_relation(os, timing_arch);
      }
    } else {
      if (out_pin.bus_width() > 1) {
        os << INDENT2 << "bus(" << out_pin.name() << ") {\n";
        os << INDENT3 << "bus_type: bus" << out_pin.bus_width() << ";\n";
      } else {
        os << INDENT2 << "pin(" << out_pin.name() << ") {\n";
      }
      os << INDENT3 << "direction: output;\n";
      for (auto const& timing_arch : out_pin.get_timing_arch()) {
        write_timing_relation(os, timing_arch);
      }
    }
    os << INDENT2 << "}\n";
  }
  // 2. input
  for (auto const& in_pin : cell.get_pins(INPUT)) {
    if (cell.type() == SEQUENTIAL) {
      os << INDENT2 << "pin(" << in_pin.name() << ") {\n";
      if (in_pin.type() == CLOCK) {
        os << INDENT3 << "min_pulse_width_low: 0.600;\n";
        os << INDENT3 << "min_pulse_width_high: 0.600;\n";
        os << INDENT3 << "min_period: 0.500;\n";
        os << INDENT3 << "direction: input;\n";
        os << INDENT3 << "clock: true;\n";
      } else {
        os << INDENT3 << "direction: input;\n";
        for (auto const& timing_arch : in_pin.get_timing_arch()) {
          write_timing_relation(os, timing_arch);
        }
      }
      os << INDENT2 << "}\n";
    } else {
      if (in_pin.bus_width() > 1) {
        os << INDENT2 << "bus(" << in_pin.name() << ") {\n";
        os << INDENT3 << "bus_type: bus" << in_pin.bus_width() << ";\n";
      } else {
        os << INDENT2 << "pin(" << in_pin.name() << ") {\n";
      }
      os << INDENT3 << "direction: input;\n";
      for (auto const& timing_arch : in_pin.get_timing_arch()) {
        write_timing_relation(os, timing_arch);
      }
      os << INDENT2 << "}\n";
    }
  }

  return;
};

void sta_lib_writer::write_timing_relation(std::ostream& os, const timing_arch& timing) {
  lib_pin related_pin = timing.related_pin();
  os << INDENT3 << "timing() {\n";
  os << INDENT4 << "related_pin: \"" << related_pin.name() << "\";\n";
  if (related_pin.type() == RESET) {
    os << INDENT4 << "timing_type: clear;\n";
    os << INDENT4 << "timing_sense: " << ((timing.sense() == POSITIVE) ? "positive_unate" : "negative_unate")
       << ";\n";
    os << INDENT4 << "cell_rise(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "cell_fall(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "rise_transition(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "fall_transition(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
  } else if (related_pin.type() == SET) {
    os << INDENT4 << "timing_type: clear;\n";
    os << INDENT4 << "timing_sense: " << ((timing.sense() == POSITIVE) ? "positive_unate" : "negative_unate")
       << ";\n";
    os << INDENT4 << "cell_rise(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "cell_fall(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "rise_transition(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "fall_transition(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
  } else if (related_pin.type() == CLOCK) {
    os << INDENT4 << "timing_type: " << ((timing.sense() == POSITIVE) ? "rising_edge" : "falling_edge")
       << ";\n";
    os << INDENT4 << "cell_rise(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "cell_fall(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "rise_transition(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "fall_transition(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
  } else {
    os << INDENT4 << "timing_sense: " << ((timing.sense() == POSITIVE) ? "positive_unate" : "negative_unate")
       << ";\n";
    os << INDENT4 << "cell_rise(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "cell_fall(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "rise_transition(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
    os << INDENT4 << "fall_transition(scalar) {\n";
    os << INDENT5 << "values(\"0\");\n";
    os << INDENT4 << "}\n";
  }
  os << INDENT3 << "}\n";

  return;
};

void sta_lib_writer::write_footer(std::ostream& os) {
  os << "}\n";
  return;
};

}  // NS rsbe
