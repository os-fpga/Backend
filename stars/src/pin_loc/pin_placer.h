#pragma once
#ifndef _pln_PinPlacer_fa337e863b11ab_H_
#define _pln_PinPlacer_fa337e863b11ab_H_

#include <set>
#include <unordered_set>

#include "util/cmd_line.h"
#include "pin_loc/pinc_main.h"
#include "pin_loc/pin.h"

namespace pln {

class RapidCsvReader;

using std::string;
using std::vector;
using StringPair = std::pair<std::string, std::string>;

struct PinPlacer {

  struct EditItem {
    string name_;      // "$iopadmap$flop2flop.dout"
    string module_;    // "O_BUF"
    string js_dir_;    // "OUT", "IN"
    string location_;  // "HR_5_0_0P"
    string mode_;      // "Mode_BP_SDR_A_TX"
    string oldPin_;    // "dout",
    string newPin_;    // newPin_ is always inside fabric, for both ibuf and obuf
    vector<string> Q_bus_;

    int16_t dir_ = 0;

    // use root if there is a chain of buffers
    EditItem* parent_ = nullptr;
    bool isRoot() const noexcept { return ! parent_; }

    EditItem* getRoot() noexcept {
      EditItem* node = this;
      while (not node->isRoot()) {
        node = node->parent_;
      }
      return node;
    }

    EditItem() noexcept = default;

    CStr c_jsdir() const noexcept { return js_dir_.c_str(); }
    CStr cname() const noexcept { return name_.c_str(); }
    CStr c_mod() const noexcept { return module_.c_str(); }
    CStr c_old() const noexcept { return oldPin_.c_str(); }
    CStr c_new() const noexcept { return newPin_.c_str(); }

    bool isInput()  const noexcept { return dir_ > 0; }
    bool isOutput() const noexcept { return dir_ < 0; }
    bool isBus() const noexcept { return Q_bus_.size(); }
    uint busSize() const noexcept { return Q_bus_.size(); }

    bool hasPins() const noexcept { return !oldPin_.empty() and !newPin_.empty(); }
    void swapPins() noexcept { std::swap(oldPin_, newPin_); }

    struct CmpOldPin {
      bool operator()(const EditItem* a, const EditItem* b) const noexcept {
        return a->oldPin_ < b->oldPin_;
      }
    };
    struct CmpNewPin {
      bool operator()(const EditItem* a, const EditItem* b) const noexcept {
        return a->newPin_ < b->newPin_;
      }
    };
  };

private:

  cmd_line cl_;

  string temp_csv_file_name_;
  string temp_pcf_name_;
  string temp_os_pcf_name_;

  vector<string> raw_design_inputs_;
  vector<string> raw_design_outputs_;

  vector<Pin> user_design_inputs_;
  vector<Pin> user_design_outputs_;

  vector<EditItem>  all_edits_;
  vector<EditItem*> ibufs_;
  vector<EditItem*> obufs_;
  vector<EditItem*> ibufs_SortedByOld_;
  vector<EditItem*> obufs_SortedByOld_;

  vector<vector<string>> pcf_pin_cmds_;

  std::set<string> used_bump_pins_;
  std::set<XY>     used_XYs_;
  std::set<XYZ>    used_oxyz_; // used output XYZ
  std::set<XYZ>    used_ixyz_; // used input XYZ
  std::unordered_set<uint> used_tiles_;

  uint otile_overlap_level_ = 1, itile_overlap_level_ = 1;

  vector<Pin>  placed_inputs_, placed_outputs_;  // for debug stats

  uint min_pt_row_ = UINT_MAX, max_pt_row_ = 0;  // for debug stats

  bool pin_assign_def_order_ = true;

  bool auto_pcf_created_ = false;
  string user_pcf_;

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

    PortInfo() noexcept = default;

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
  void resetState() noexcept;

  bool read_and_write();

  bool check_xyz_overlap(const vector<Pin>& inputs,
                         const vector<Pin>& outputs,
                         vector<const Pin*>& inp_ov,
                         vector<const Pin*>& out_ov) const noexcept;

  void print_stats(const RapidCsvReader& csv) const;
  void print_summary(const string& csv_name) const;
  void printTileUsage(const RapidCsvReader& csv) const;

  size_t num_placed_pins() const noexcept {
    return placed_inputs_.size() + placed_outputs_.size();
  }

  const string& user_design_output(uint i) const noexcept {
    assert(i < user_design_outputs_.size());
    return user_design_outputs_[i].udes_pin_name_;
  }
  const string& user_design_input(uint i) const noexcept {
    assert(i < user_design_inputs_.size());
    return user_design_inputs_[i].udes_pin_name_;
  }

  int find_udes_input(const string& pinName) const noexcept {
    if (pinName.empty() or user_design_inputs_.empty())
      return -1;
    for (int i = int(user_design_inputs_.size()) - 1; i >= 0; i--) {
      if (user_design_inputs_[i].udes_pin_name_ == pinName)
        return i;
    }
    return -1;
  }
  int find_udes_output(const string& pinName) const noexcept {
    if (pinName.empty() or user_design_outputs_.empty())
      return -1;
    for (int i = int(user_design_outputs_.size()) - 1; i >= 0; i--) {
      if (user_design_outputs_[i].udes_pin_name_ == pinName)
        return i;
    }
    return -1;
  }

  bool read_csv_file(RapidCsvReader&);
  bool read_design_ports();
  bool read_edits();
  void translate_pcf_cmds();

  bool read_pcf(const RapidCsvReader&);

  bool write_dot_place(const RapidCsvReader&);

  bool create_temp_pcf(RapidCsvReader&);

  void get_pcf_directions(vector<string>& inps,
                          vector<string>& outs,
                          vector<string>& undefs) const noexcept;

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

  bool is_fabric_eblif_ = false;
  bool pin_names_translated_ = false;

  mutable uint num_warnings_ = 0, num_critical_warnings_ = 0;
  void incrCriticalWarnings() const noexcept { num_critical_warnings_++; }

  static bool read_port_info(std::ifstream& json_ifs, vector<string>& inputs,
                             vector<string>& outputs);

  bool read_edit_info(std::ifstream& ifs);
  bool check_edit_info() const;

  void finalize_edits() noexcept;
  void set_edit_dirs(bool initial) noexcept;
  void link_edits() noexcept;
  void dump_edits(const string& memo) noexcept;

  // map logical clocks to physical clocks. status = 0 if NOP, -1 if error
  int map_clocks();
  int write_clocks_logical_to_physical();

  static string err_lookup(const string& key) noexcept;
  static string last_err_lookup() noexcept;

  static void clear_err_code() noexcept;
  static void set_err_code(const char* cs) noexcept;

private:

  static const Pin* find_udes_pin(const vector<Pin>& P, const string& nm) noexcept;

  EditItem* findObufByOldPin(const string& old_pin) const noexcept;
  EditItem* findIbufByOldPin(const string& old_pin) const noexcept;

  string translatePinName(const string& pinName, bool is_input) const noexcept;
  uint translatePinNames(const string& memo) noexcept;
};

}

#endif

