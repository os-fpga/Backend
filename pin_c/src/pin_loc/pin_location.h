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
#include "util/geo/xyz.h"
#include "util/pinc_log.h"
#include "pin_loc/pinc_main.h"

namespace pinc {

class RapidCsvReader;

using std::string;
using std::vector;

class PinPlacer {

  struct Pin {
    string user_design_name_;
    string device_pin_name_;
    XYZ xyz_;
    uint pt_row_ = 0; // row in pin table

    Pin() noexcept = default;

    Pin(const string& u, const string& d, const XYZ& xyz, uint r) noexcept
      : user_design_name_(u), device_pin_name_(d),
        xyz_(xyz), pt_row_(r)
    {}
  };

  cmd_line cl_;

  string temp_csv_file_name_;
  string temp_pcf_name_;
  string temp_os_pcf_name_;

  vector<string> user_design_inputs_;
  vector<string> user_design_outputs_;

  vector<vector<string>> pcf_pin_cmds_;
  std::set<string> used_bump_pins_;

  vector<Pin> placed_inputs_, placed_outputs_;

  bool pin_assign_def_order_ = true;

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

  PinPlacer(const cmd_line& cl)
   : cl_(cl) {
    pin_assign_def_order_ = true;
  }
  ~PinPlacer();

  const cmd_line& get_cmd() const noexcept { return cl_; }

  bool reader_and_writer();
  void print_stats() const;
  size_t num_placed_pins() const noexcept {
    return placed_inputs_.size() + placed_outputs_.size();
  }

  bool read_csv_file(RapidCsvReader&);
  bool read_design_ports();

  bool read_pcf(const RapidCsvReader& rdr);

  bool write_dot_place(const RapidCsvReader&);

  bool create_temp_pcf(const RapidCsvReader& rdr);

  static void shuffle_candidates(vector<int>& v);

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

  static string err_lookup(const string& key) noexcept; // err_map lookup
  static void clear_err_code() noexcept;
  static void set_err_code(const char* cs) noexcept;

private:

  static const Pin* find_udes_pin(const vector<Pin>& P, const string& nm) noexcept;
};

}  // namespace pinc

#endif

