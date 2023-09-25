#pragma once
////////////////////////////////////////////////////////////////////////////////
// Important:                                                                 //
//    This is for rs internally - don't make this part public until a written //
//    approval is obtained from rapidsilicon open source review commitee      //
////////////////////////////////////////////////////////////////////////////////

#ifndef __rs_PinPlacer_fa337e863b11ab_H_
#define __rs_PinPlacer_fa337e863b11ab_H_

#include <set>
#include <unordered_set>

#include "util/cmd_line.h"
#include "pin_loc/pinc_main.h"
#include "pin_loc/pin.h"

namespace pinc {

class RapidCsvReader;

using std::string;
using std::vector;
using StringPair = std::pair<std::string, std::string>;

class PinPlacer {

  cmd_line cl_;

  string temp_csv_file_name_;
  string temp_pcf_name_;
  string temp_os_pcf_name_;

  vector<string> user_design_inputs_;
  vector<string> user_design_outputs_;

  vector<vector<string>> pcf_pin_cmds_;

  std::set<string> used_bump_pins_;
  std::set<XY>     used_XYs_;
  std::set<XYZ>    used_oxyz_; // used output XYZ
  std::set<XYZ>    used_ixyz_; // used input XYZ
  std::unordered_set<uint> used_tiles_;

  vector<Pin>  placed_inputs_, placed_outputs_;  // for debug stats

  uint min_pt_row_ = UINT_MAX, max_pt_row_ = 0;  // for debug stats

  bool pin_assign_def_order_ = true;

  bool auto_pcf_created_ = false;

  // bool uniq_by_xy_ = true; // new mode of device pin uniqueness check
  //                         // for old mode: getenv("pinc_old_uniqueness_by_bump_name")

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

  PinPlacer(const cmd_line& cl);
  ~PinPlacer();

  // const cmd_line& get_cmd() const noexcept { return cl_; }

  bool reader_and_writer();

  void print_stats(const RapidCsvReader& csv) const;
  void printTileUsage(const RapidCsvReader& csv) const;

  size_t num_placed_pins() const noexcept {
    return placed_inputs_.size() + placed_outputs_.size();
  }

  bool read_csv_file(RapidCsvReader&);
  bool read_design_ports();

  bool read_pcf(const RapidCsvReader&);

  bool write_dot_place(const RapidCsvReader&);

  bool create_temp_pcf(RapidCsvReader&);

  static void shuffle_candidates(vector<int>& v);

  DevPin get_available_device_pin(RapidCsvReader& csv,
                                  bool is_inp, const string& udesName,
                                  Pin*& ann_pin);
  //
  DevPin get_available_bump_ipin(RapidCsvReader& csv,
                                 const string& udesName, Pin*& ann_pin);
  DevPin get_available_bump_opin(RapidCsvReader& csv,
                                 const string& udesName, Pin*& ann_pin);
  //
  DevPin get_available_axi_ipin(vector<string>& Q);
  DevPin get_available_axi_opin(vector<string>& Q);
  //
  bool no_more_inp_bumps_ = false; // state for get_available_device_pin()
  bool no_more_out_bumps_ = false; //
  uint num_warnings_ = 0;

  static bool read_port_info(std::ifstream& json_ifs, vector<string>& inputs,
                             vector<string>& outputs);

  bool write_logical_clocks_to_physical_clks();

  static string err_lookup(const string& key) noexcept; // err_map lookup
  static void clear_err_code() noexcept;
  static void set_err_code(const char* cs) noexcept;

private:

  static const Pin* find_udes_pin(const vector<Pin>& P, const string& nm) noexcept;
};

}

#endif

