#include "pinc_log.h"
#include "sta_lib_writer.h"

#include <fstream>
#include <map>
#include <set>

static const char* INDENT1    = "\t";
static const char* INDENT2    = "\t\t";
static const char* INDENT3    = "\t\t\t";
static const char* INDENT4    = "\t\t\t\t";
static const char* INDENT5    = "\t\t\t\t\t";
static const char* SEPARATOR  = "\t:\t";
static const char* TRAILER    = ";\n";

namespace rsbe {

using std::endl;
using std::string;
using namespace pinc;

void LibWriter::init_lib() {
  // todo: develop an utility for smart configuration and query
  // lib header
  lib_header["name"] = "fpga";

  // lib info
  lib_info["comment"] = "\"All rights reserved by Rapid Silicon.\"";
  lib_info["delay_model"] = "table_lookup";
  lib_info["simulation"] = "false";

  // lib unit
  lib_unit["leak_power_unit"] = "1pW";
  lib_unit["current_unit"] = "1A";
  lib_unit["pulling_resistance_unit"] = "1kohm";
  lib_unit["time_unit"] = "1ns";
  lib_unit["voltage_unit"] = "1V";

  // lib cap load
  lib_cap_load["capacitive_load_unit"] = "(1,pf)";

  // lib feature
  lib_feature["library_features"] = "(report_delay_calculation)";

  // lib data settings
  lib_data_setting["input_threshold_pct_rise"] = "50";
  lib_data_setting["input_threshold_pct_fall"] = "50";
  lib_data_setting["output_threshold_pct_rise"] = "50";
  lib_data_setting["output_threshold_pct_fall"] = "50";
  lib_data_setting["slew_derate_from_library"] = "1.0";
  lib_data_setting["slew_lower_threshold_pct_fall"] = "20";
  lib_data_setting["slew_lower_threshold_pct_rise"] = "20";
  lib_data_setting["slew_upper_threshold_pct_fall"] = "80";
  lib_data_setting["slew_upper_threshold_pct_rise"] = "80";
  lib_data_setting["default_max_fanout"] = "40";
  lib_data_setting["default_max_transition"] = "2.00";
  lib_data_setting["default_cell_leakage_power"] = "100";
  lib_data_setting["default_fanout_load"] = "1.0";
  lib_data_setting["default_inout_pin_cap"] = "0.0";
  lib_data_setting["default_input_pin_cap"] = "0.0";
  lib_data_setting["default_output_pin_cap"] = "0.0";
  lib_data_setting["nom_process"] = "1.0";
  lib_data_setting["nom_temperature"] = "125.00";
  lib_data_setting["nom_voltage"] = "1.62";

  // lib operating settting
  // typical case
  lib_operating_typical["process"] = "0.819";
  lib_operating_typical["temperature"] = "25.00";
  lib_operating_typical["voltage"] = "1.8";
  lib_operating_typical["tree_type"] = "balanced_tree";
  lib_operating_conditions["typical_case"] = lib_operating_typical;

  // bus naming style
  lib_bus_type["bus_naming_style"] = "%s[%d]";
}

void LibWriter::write_header(std::ostream& os) {
  uint16_t tr = ltrace();
  if (tr >= 2) lputs("LibWriter::write_header()");

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
}

void LibWriter::write_bus_type(std::ostream& os, int from, int to, bool down_to) {
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
}

void LibWriter::write_lcell(std::ostream& os, const lib_cell& lc) const {
  lout() << "STARS: Writing lcell <" << lc.name() << ">..." << endl;

  // 1. cell name
  os << INDENT1 << "cell (" << lc.name() << ") {\n";
  // ff needs pre-fix
  if (lc.type() == SEQUENTIAL) {
    string q_name, clk_name, reset_name, set_name, enable_name;
    for (auto const& out_pin : lc.get_outputs()) {
      q_name = out_pin.name();
      break;
    }
    for (auto const& in_pin : lc.get_inputs()) {
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

  // 2. pins
  write_lcell_pins(os, lc);

  // 3. tailer
  os << INDENT1 << "}\n";
}

void LibWriter::write_lcell_pins(std::ostream& os, const lib_cell& lc) const {
  uint16_t tr = ltrace();
  const string& lc_name = lc.name();
  if (tr >= 4) {
    lprintf("  LW::write_cell_pins( lc= %s )\n", lc_name.c_str());
  }

  // 1. output
  const vector<lib_pin>& outputs = lc.get_outputs();
  for (const lib_pin& out_pin : outputs) {
    if (lc.type() == SEQUENTIAL) {
      os << INDENT2 << "pin(" << out_pin.name() << ") {\n";
      os << INDENT3 << "direction: output;\n";
      for (auto const& timing_arch : out_pin.get_timing_arcs()) {
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
      for (auto const& timing_arch : out_pin.get_timing_arcs()) {
        write_timing_relation(os, timing_arch);
      }
    }
    os << INDENT2 << "}\n";
  }

  // 2. input
  const vector<lib_pin>& inputs = lc.get_inputs();
  for (const lib_pin& in_pin : inputs) {
    if (lc.type() == SEQUENTIAL) {
      os << INDENT2 << "pin(" << in_pin.name() << ") {\n";
      if (in_pin.type() == CLOCK) {
        os << INDENT3 << "min_pulse_width_low: 0.600;\n";
        os << INDENT3 << "min_pulse_width_high: 0.600;\n";
        os << INDENT3 << "min_period: 0.500;\n";
        os << INDENT3 << "direction: input;\n";
        os << INDENT3 << "clock: true;\n";
      } else {
        os << INDENT3 << "direction: input;\n";
        for (auto const& timing_arch : in_pin.get_timing_arcs()) {
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
      for (auto const& timing_arch : in_pin.get_timing_arcs()) {
        write_timing_relation(os, timing_arch);
      }
      os << INDENT2 << "}\n";
    }
  }
}

void LibWriter::write_timing_relation(std::ostream& os, const TimingArc& arc) const {
  lib_pin related_pin = arc.related_pin();
  string related_pin_nm = related_pin.name();
  bool positive = (arc.sense() == POSITIVE);

  uint16_t tr = ltrace();
  if (tr >= 4) {
    lprintf("  LW::write_timing_relation()  related_pin= %s  positive:%i\n",
               related_pin_nm.c_str(), positive);
  }

  os << INDENT3 << "timing() {\n";
  os << INDENT4 << "related_pin: \"" << related_pin_nm << "\";\n";

  if (related_pin.type() == RESET) {
    os << INDENT4 << "timing_type: clear;\n";
    os << INDENT4 << "timing_sense: " << (positive ? "positive_unate" : "negative_unate")
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
    os << INDENT4 << "timing_sense: " << (positive ? "positive_unate" : "negative_unate")
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

    os << INDENT4 << "timing_type: " << (positive ? "rising_edge" : "falling_edge") << ";\n";
    //os << INDENT4 << "timing_type: " << (positive ? "setup_rising" : "falling_edge") << ";\n";

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
    os << INDENT4 << "timing_sense: " << (positive ? "positive_unate" : "negative_unate")
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
}

void LibWriter::write_footer(std::ostream& os) const {
  os << "}\n";
  return;
}

}  // NS rsbe
