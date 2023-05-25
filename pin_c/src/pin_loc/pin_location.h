#pragma once
////////////////////////////////////////////////////////////////////////////////
// Important:                                                                 //
//    This is for rs internally - don't make this part public until a written //
//    approval is obtained from rapidsilicon open source review commitee      //
////////////////////////////////////////////////////////////////////////////////

#ifndef __rs_PIN_LOCATION_H
#define __rs_PIN_LOCATION_H

#include <set>

#include "util/cmd_line.h"
#include "util/geo/iv.h"
#include "util/pinc_log.h"
#include "pin_loc/pinc_main.h"

namespace pinc {

class RapidCsvReader;

using std::string;
using std::vector;

class pin_location {

  enum { ASSIGN_IN_RANDOM = 0, ASSIGN_IN_DEFINE_ORDER = 1 } pin_assign_method_ = ASSIGN_IN_DEFINE_ORDER;

  cmd_line cl_;

  string temp_csv_file_name_;
  string temp_pcf_name_;
  string temp_os_pcf_name_;

  vector<string> user_design_inputs_;
  vector<string> user_design_outputs_;

  vector<vector<string>> pcf_pin_cmds_;
  std::set<string> used_bump_pins_;

public:
  enum class PortDir : uint8_t {
    Undefined = 0,
    Input = 1,
    Output = 2,
    Bidi = 3
  };

  enum class PortType : uint8_t { Undefined = 0, Wire = 1, Reg = 2 };

  enum class Order : uint8_t { Undefined = 0, LSB_to_MSB = 1, MSB_to_LSB = 2 };

  struct PortInfo {
    string name_;
    PortDir dir_ = PortDir::Undefined;
    PortType type_ = PortType::Undefined;
    Order order_ = Order::Undefined;
    Iv range_;

    PortInfo() = default;

    bool isInput() const noexcept { return dir_ == PortDir::Input; }
    bool isOutput() const noexcept { return dir_ == PortDir::Output; }
    bool hasRange() const noexcept { return range_.valid(); }
    bool hasOrder() const noexcept { return order_ != Order::Undefined; }

    // define_order: "LSB_TO_MSB"
    bool is_LSB_to_MSB() const noexcept {
      return hasRange() && order_ == Order::LSB_to_MSB;
    }
    bool is_MSB_to_LSB() const noexcept {
      return hasRange() && order_ == Order::MSB_to_LSB;
    }
  };

  pin_location(const cmd_line& cl)
   : cl_(cl) {
    pin_assign_method_ = ASSIGN_IN_DEFINE_ORDER;
  }
  ~pin_location();

  const cmd_line& get_cmd() const noexcept { return cl_; }

  bool reader_and_writer();
  void print_stats() const;

  bool generate_csv_file_for_os_flow();
  bool read_csv_file(RapidCsvReader&);
  bool read_design_ports();

  bool read_pcf(const RapidCsvReader& rdr);

  bool write_dot_place(const RapidCsvReader&);

  bool create_temp_pcf(const RapidCsvReader& rdr);

  static void shuffle_candidates(vector<int>& v);

  bool convert_pcf_for_os_flow(const string& pcf_name);

  // get_available_ methods return pin_and_mode pair, empty strings on error
  //
  std::pair<string, string> get_available_device_pin(const RapidCsvReader& rdr, bool is_inp);
  //
  std::pair<string, string> get_available_bump_ipin(const RapidCsvReader& rdr);
  std::pair<string, string> get_available_bump_opin(const RapidCsvReader& rdr);
  std::pair<string, string> get_available_axi_ipin(vector<string>& Q);
  std::pair<string, string> get_available_axi_opin(vector<string>& Q);
  //
  bool no_more_inp_bumps_ = false, no_more_out_bumps_ = false; // state for get_available_device_pin()
  uint num_warnings_ = 0;
  //

  bool is_input_mode(const string& mode_name) const;
  bool is_output_mode(const string& mode_name) const;

  static bool read_port_info(std::ifstream& json_ifs, vector<string>& inputs,
                             vector<string>& outputs);

  bool write_logical_clocks_to_physical_clks();
};

}  // namespace pinc

#endif

