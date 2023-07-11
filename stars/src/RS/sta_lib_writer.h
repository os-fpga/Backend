#pragma once
#ifndef __rsbe__sta_lib_writer_H
#define __rsbe__sta_lib_writer_H

#include <iostream>
#include <map>
#include <vector>
#include "sta_lib_data.h"

namespace rsbe {

// writer
class sta_lib_writer {
// writer formater macros
#define INDENT1 "\t"
#define INDENT2 "\t\t"
#define INDENT3 "\t\t\t"
#define INDENT4 "\t\t\t\t"
#define INDENT5 "\t\t\t\t\t"
#define SEPARATOR "\t:\t"
#define TRAILER ";\n"

private:
  std::map<std::string, std::string> lib_header;
  std::map<std::string, std::string> lib_info;
  std::map<std::string, std::string> lib_unit;
  std::map<std::string, std::string> lib_cap_load;
  std::map<std::string, std::string> lib_feature;
  std::map<std::string, std::string> lib_data_setting;
  std::map<std::string, std::map<std::string, std::string>> lib_operating_conditions;
  std::map<std::string, std::string> lib_operating_typical;
  std::map<std::string, std::string> lib_bus_type;

public:
  sta_lib_writer(){};
  ~sta_lib_writer(){};
  void init_lib() {
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
  };
  void write_header(std::ostream& os);
  void write_bus_type(std::ostream& os, int from, int to, bool down_to);
  void write_cell(std::ostream& os, const lib_cell& cell);
  void write_cell_pin(std::ostream& os, const lib_cell& cell);
  void write_timing_relation(std::ostream& os, const timing_arch& timing);
  void write_footer(std::ostream& os);
};

}  // NS rsbe

#endif
