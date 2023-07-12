#include "pin_loc/pin_location.h"
#include "file_readers/pcf_reader.h"
#include "file_readers/rapid_csv_reader.h"
#include "file_readers/Fio.h"

#include "util/cmd_line.h"

#include <random>
#include <set>
#include <map>

namespace pinc {

using namespace std;
using fio::Fio;

#define CERROR std::cerr << "[Error] "
#define OUT_ERROR std::cout << "[Error] "


// use fix to detemined A2F and F2A GBX mode
static constexpr const char* INPUT_MODE_FIX = "_RX";
static constexpr const char* OUTPUT_MODE_FIX = "_TX";

// for mpw1  (no gearbox, a Mode_GPIO is created)
static constexpr const char* GPIO_MODE_FIX = "_GPIO";

// static data (hacky, but temporary)
static vector<string> s_axi_inpQ, s_axi_outQ;


const PinPlacer::Pin*
PinPlacer::find_udes_pin(const vector<Pin>& P, const string& nm) noexcept
{
  for (const Pin& p : P) {
    if (p.user_design_name_ == nm)
      return &p;
  }
  return nullptr;
}

void PinPlacer::print_stats() const
{
  uint16_t tr = ltrace();
  if (!tr) return;
  auto& ls = lout();

  ls << "======== stats:" << endl;

  const vector<string>& inputs = user_design_inputs_;
  const vector<string>& outputs = user_design_outputs_;

  ls << " --> got " << inputs.size() << " inputs and "
     << outputs.size() << " outputs" << endl;
  if (tr >= 4) {
    lprintf("\n ---- inputs(%zu): ---- \n", inputs.size());
    for (size_t i = 0; i < inputs.size(); i++) {
      const string& nm = inputs[i];
      ls << "     in  " << nm;
      const Pin* pp = find_udes_pin(placed_inputs_, nm);
      if (pp) {
        ls << "  placed at " << pp->xyz_
           << "  device: " << pp->device_pin_name_ << "  pt_row: " << pp->pt_row_;
      }
      ls << endl;
    }
    lprintf("\n ---- outputs(%zu): ---- \n", outputs.size());
    for (size_t i = 0; i < outputs.size(); i++) {
      const string& nm = outputs[i];
      ls << "    out  " << nm;
      const Pin* pp = find_udes_pin(placed_outputs_, nm);
      if (pp)
        ls << "  placed at " << pp->xyz_
           << "  device: " << pp->device_pin_name_ << "  pt_row: " << pp->pt_row_;
      ls << endl;
    }
    lputs();
    ls << " <----- pin_c got " << inputs.size() << " inputs and "
       << outputs.size() << " outputs" << endl;
    ls << " <-- pin_c placed " << placed_inputs_.size() << " inputs and "
       << placed_outputs_.size() << " outputs" << endl;
  }

  ls << "======== end stats." << endl;
}

bool PinPlacer::read_pcf(const RapidCsvReader& csvReader)
{
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2) lputs("\nPinPlacer::read_pcf()");

  pcf_pin_cmds_.clear();

  string pcf_name;
  if (temp_os_pcf_name_.size()) {
    // use generated temp pcf for open source flow
    pcf_name = temp_os_pcf_name_;
  } else {
    pcf_name = cl_.get_param("--pcf");
  }

  PcfReader rd_pcf;
  if (!rd_pcf.read_pcf(pcf_name)) {
    CERROR << err_lookup("PIN_CONSTRAINT_PARSE_ERROR") << endl;
    return false;
  }

  if (rd_pcf.commands_.empty())
    return true;

  // validate modes
  string mode, key;
  for (uint cmd_i = 0; cmd_i < rd_pcf.commands_.size(); cmd_i++) {
    const vector<string>& pcf_cmd = rd_pcf.commands_[cmd_i];
    mode.clear();
    for (uint j = 1; j < pcf_cmd.size() - 1; j++) {
      if (pcf_cmd[j] == "-mode") {
        mode = pcf_cmd[j + 1];
        break;
      }
    }
    if (mode.empty()) continue;
    key = mode;
    RapidCsvReader::prepare_mode_header(key);
    if (csvReader.hasMode(key)) continue;
    ls << endl;
    CERROR << " (ERROR) invalid mode name: " << mode << endl;
    if (tr >= 2) {
      ls << " (ERROR) invalid mode name: " << mode << "  [ " << key << " ]" << endl;
      if (tr >= 3) {
        ls << " valid mode names are:" << endl;
        csvReader.printModeKeys();
        ls << " (ERROR) invalid mode name: " << mode << "  [ " << key << " ]" << endl;
      }
    }
    return false;
  }

  pcf_pin_cmds_ = std::move(rd_pcf.commands_);

  return true;
}

static bool vec_contains(const vector<string>& V, const string& s) noexcept {
  if (V.empty()) return false;
  for (int i = V.size() - 1; i >= 0; i--)
    if (V[i] == s) return true;
  return false;
}

bool PinPlacer::write_dot_place(const RapidCsvReader& csv_rd)
{
  placed_inputs_.clear();
  placed_outputs_.clear();
  string out_fn = cl_.get_param("--output");
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2) {
    ls << "\npinc::write_dot_place() __ Creating .place file  get_param(--output) : "
       << out_fn << endl;
  }

  ofstream out_file;
  out_file.open(out_fn);
  if (out_file.fail()) {
    if (tr >= 2) {
        ls << "\n (EE) pinc::write_dot_place() FAILED to write " << out_fn << endl;
    }
    return false;
  }

  out_file << "#Block Name   x   y   z\n";
  out_file << "#------------ --  --  -" << endl;
  out_file << std::flush;

  if (tr >= 2) {
    ls << "#Block Name   x   y   z\n";
    ls << "#------------ --  --  -" << endl;
    ls << std::flush;
  }

  // parse each constraint line in constrain file, generate place location
  // accordingly
  //
  std::set<string> constrained_user_pins,
      constrained_device_pins;  // for sanity check

  string gbox_pin_name, search_name;

  for (uint cmd_i = 0; cmd_i < pcf_pin_cmds_.size(); cmd_i++) {

    const vector<string>& pcf_cmd = pcf_pin_cmds_[cmd_i];

    // only support io and clock for now
    if (pcf_cmd[0] != "set_io" && pcf_cmd[0] != "set_clk") continue;

    gbox_pin_name.clear();
    search_name.clear();

    const string& user_design_pin_name = pcf_cmd[1];
    const string& device_pin_name = pcf_cmd[2];  // bump or ball

    if (tr >= 4) {
      ls << "cmd_i:" << cmd_i
         << "   user_design_pin_name=  " << user_design_pin_name
         << "  -->  device_pin_name=  " << device_pin_name << endl;
      logVec(pcf_cmd, "    cur pcf_cmd: ");
    }

    for (size_t i = 1; i < pcf_cmd.size(); i++) {
      if (pcf_cmd[i] == "-internal_pin") {
        gbox_pin_name = pcf_cmd[++i];
        break;
      }
    }

    bool is_in_pin = vec_contains(user_design_inputs_, user_design_pin_name);

    bool is_out_pin =
        is_in_pin ? false
                  : vec_contains(user_design_outputs_, user_design_pin_name);

    if (!is_in_pin && !is_out_pin) {
      // sanity check
      CERROR << err_lookup("CONSTRAINED_PORT_NOT_FOUND") << ": <"
             << user_design_pin_name << ">" << endl;
      out_file << "\n=== Error happened, .place file is incomplete\n"
               << "=== ERROR:" << err_lookup("CONSTRAINED_PORT_NOT_FOUND")
               << ": <" << user_design_pin_name
               << ">  device_pin_name: " << device_pin_name << "\n\n";
      return false;
    }
    if (!csv_rd.has_io_pin(device_pin_name)) {
      CERROR << err_lookup("CONSTRAINED_PIN_NOT_FOUND") << ": <"
             << device_pin_name << ">" << endl;
      out_file << "\n=== Error happened, .place file is incomplete\n"
               << "=== ERROR:" << err_lookup("CONSTRAINED_PORT_NOT_FOUND")
               << ": <" << user_design_pin_name
               << ">  device_pin_name: " << device_pin_name << "\n\n";
      return false;
    }

    if (!constrained_user_pins.count(user_design_pin_name)) {
      constrained_user_pins.insert(user_design_pin_name);
    } else {
      CERROR << err_lookup("RE_CONSTRAINED_PORT") << ": <"
             << user_design_pin_name << ">" << endl;
      return false;
    }

    // use (device_pin_name + " " + gbox_pin_name) as search name to check
    // overlap pin in constraint
    search_name = device_pin_name;
    search_name.push_back(' ');
    search_name += gbox_pin_name;

    if (!constrained_device_pins.count(search_name)) {
      constrained_device_pins.insert(search_name);
    } else {
      CERROR << err_lookup("OVERLAP_PIN_IN_CONSTRAINT") << ": <"
             << device_pin_name << ">" << endl;
      return false;
    }

    // look for coordinates and write constrain
    if (is_out_pin) {
      out_file << "out:";
      if (tr >= 4) ls << "out:";
    }

    out_file << user_design_pin_name << '\t';
    out_file.flush();

    string mode = pcf_cmd[4];
    RapidCsvReader::prepare_mode_header(mode);

    uint pt_row = 0;
    XYZ xyz = csv_rd.get_pin_xyz_by_name(mode, device_pin_name, gbox_pin_name, pt_row);
    if (!xyz.valid()) {
        CERROR << " PRE-ASSERT: no valid coordinates" << endl;
        lputs("\n [Error] (ERROR) PRE-ASSERT");
        lprintf("   mode %s  device_pin_name %s   gbox_pin_name %s\n",
                mode.c_str(), device_pin_name.c_str(), gbox_pin_name.c_str());
        lputs();
    }
    assert(xyz.valid());
    if (!xyz.valid()) {
        return false;
    }

    out_file << xyz.x_ << '\t' << xyz.y_ << '\t' << xyz.z_;
    if (tr >= 4) {
        out_file << "    #  device: " << device_pin_name;
        out_file << "  pt_row: " << pt_row;
    }
    out_file << endl;
    out_file.flush();

    if (tr >= 4) {
      ls << xyz.x_ << '\t' << xyz.y_ << '\t' << xyz.z_;
      ls << "    #  device: " << device_pin_name;
      ls << "  pt_row: " << pt_row;
      flush_out(true);
    }

    // save for statistics:
    if (is_in_pin)
      placed_inputs_.emplace_back(user_design_pin_name, device_pin_name, xyz, pt_row);
    else
      placed_outputs_.emplace_back(user_design_pin_name, device_pin_name, xyz, pt_row);
  }

  if (tr >= 2) {
    ls << "\nwritten " << num_placed_pins() << " pins to " << out_fn << endl;
  }

  return true;

#if 0
  // assign other un-constrained user pins (if any) to legal io tile pin in fpga
  if (constrained_user_pins.size() <
      (user_design_inputs_.size() + user_design_outputs_.size())) {
    vector<int> left_available_device_pin_idx;
    collect_left_available_device_pins(
        constrained_device_pins, left_available_device_pin_idx, csv_rd);
    if (pin_assign_def_order_ == ASSIGN_IN_RANDOM) {
      shuffle_candidates(left_available_device_pin_idx);
    }
    int assign_pin_idx = 0;
    for (auto user_input_pin_name : user_design_inputs_) {
      if (constrained_user_pins.find(user_input_pin_name) ==
          constrained_user_pins.end()) { // input pins not specified in pcf
        out_file << user_input_pin_name << "\t"
                 << std::to_string(csv_rd.get_pin_x_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << "\t"
                 << std::to_string(csv_rd.get_pin_y_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << "\t"
                 << std::to_string(csv_rd.get_pin_z_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << endl;
        assign_pin_idx++;
      }
    }
    for (auto user_output_pin_name : user_design_outputs_) {
      if (constrained_user_pins.find(user_output_pin_name) ==
          constrained_user_pins.end()) { // output pins not specified in pcf
        out_file << "out:" << user_output_pin_name << "\t"
                 << std::to_string(csv_rd.get_pin_x_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << "\t"
                 << std::to_string(csv_rd.get_pin_y_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << "\t"
                 << std::to_string(csv_rd.get_pin_z_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << endl;
        assign_pin_idx++;
      }
    }
  }
#endif  // 0
}

static bool try_open_csv_file(const string& csv_name) {
  if (not Fio::regularFileExists(csv_name)) {
    lprintf("\n[Error] pin_c: csv file %s does not exist\n", csv_name.c_str());
    OUT_ERROR << PinPlacer::err_lookup("OPEN_FILE_FAILURE") << endl;
    CERROR << PinPlacer::err_lookup("OPEN_FILE_FAILURE") << endl;
    return false;
  }

  std::ifstream stream;
  stream.open(csv_name, std::ios::binary);
  if (stream.fail()) {
    OUT_ERROR << PinPlacer::err_lookup("OPEN_FILE_FAILURE") << endl;
    CERROR << PinPlacer::err_lookup("OPEN_FILE_FAILURE") << endl;
    return false;
  }

  return true;
}

bool PinPlacer::read_csv_file(RapidCsvReader& csv_rd) {
  uint16_t tr = ltrace();
  if (tr >= 2) lputs("\nread_csv_file() __ Reading csv");

  string csv_name;
  if (temp_csv_file_name_.size()) {
    // use generated temp csv for open source flow
    csv_name = temp_csv_file_name_;
  } else {
    csv_name = cl_.get_param("--csv");
  }

  if (tr >= 2) lprintf("  cvs_name= %s\n", csv_name.c_str());

  if (!try_open_csv_file(csv_name)) {
    return false;
  }

  string check_csv = cl_.get_param("--check_csv");
  bool check = false;
  if (check_csv == "true") {
    check = true;
    if (tr >= 2) lputs("NOTE: check_csv == True");
  }
  if (!csv_rd.read_csv(csv_name, check)) {
    CERROR << err_lookup("PIN_MAP_CSV_PARSE_ERROR") << endl;
    return false;
  }

  return true;
}

StringPair PinPlacer::get_available_device_pin(const RapidCsvReader& rdr,
                                               bool is_inp, const string& udesName)
{
  StringPair result;

  uint16_t tr = ltrace();

  if (is_inp) {
    if (no_more_inp_bumps_) {
      result = get_available_axi_ipin(s_axi_inpQ);
      if (tr >= 4)
        lprintf("  no_more_inp_bumps_ => got axi_ipin: %s\n", result.first.c_str());
      return result;
    }
    result = get_available_bump_ipin(rdr, udesName);
    if (result.first.empty()) {
      no_more_inp_bumps_ = true;
      result = get_available_axi_ipin(s_axi_inpQ);
    }
  } else { // out
    if (no_more_out_bumps_) {
      result = get_available_axi_opin(s_axi_outQ);
      if (tr >= 4)
        lprintf("  no_more_out_bumps_ => got axi_opin: %s\n", result.first.c_str());
      return result;
    }
    result = get_available_bump_opin(rdr, udesName);
    if (result.first.empty()) {
      no_more_out_bumps_ = true;
      result = get_available_axi_opin(s_axi_outQ);
    }
  }

  return result;
}

StringPair PinPlacer::get_available_axi_ipin(vector<string>& Q) {
  static uint cnt = 0;
  cnt++;
  if (ltrace() >= 4) {
    lprintf("get_available_axi_ipin()# %u Q.size()= %u\n", cnt, uint(Q.size()));
  }

  StringPair result; // pin_and_mode

  if (Q.empty()) return result;

  result.first = Q.back();
  Q.pop_back();

  // need to pick a mode, othewise PcfReader::read_pcf() will not tokenize pcf line properly
  result.second = "MODE_GPIO";

  return result;
}

StringPair PinPlacer::get_available_axi_opin(vector<string>& Q) {
  static uint cnt = 0;
  cnt++;
  if (ltrace() >= 4) {
    lprintf("get_available_axi_opin()# %u Q.size()= %u\n", cnt, uint(Q.size()));
  }

  StringPair result; // pin_and_mode

  if (Q.empty()) return result;

  result.first = Q.back();
  Q.pop_back();

  // need to pick a mode, othewise PcfReader::read_pcf() will not tokenize pcf line properly
  result.second = "MODE_GPIO";

  return result;
}

StringPair PinPlacer::get_available_bump_ipin(const RapidCsvReader& rdr, const string& udesName)
{
  static uint icnt = 0;
  icnt++;
  uint16_t tr = ltrace();
  if (tr >= 4) {
    lprintf("get_available_bump_ipin()# %u  for udes-pin %s\n", icnt, udesName.c_str());
  }

  //constexpr bool is_input_port = true;

  bool found = false;
  StringPair result; // pin_and_mode

  uint num_rows = rdr.numRows();
  for (uint i = rdr.start_GBOX_GPIO_row_; i < num_rows; i++) {
    const auto& bcd = rdr.bcd_[i];
    const string& bump_pin_name = bcd.bump_;

    if (used_bump_pins_.count(bump_pin_name)) {
      if (tr >= 9)
        lprintf("  bump_ipin_name %s is used, skipping..\n", bump_pin_name.c_str());
      continue;
    }

    for (uint j = 0; j < rdr.mode_names_.size(); j++) {
      const string& mode_name = rdr.mode_names_[j];
      const vector<string>* mode_data = rdr.getModeData(mode_name);
      assert(mode_data);
      assert(mode_data->size() == num_rows);

      if (is_input_mode(mode_name)) {
        for (uint k = rdr.start_GBOX_GPIO_row_; k < num_rows; k++) {
          if (mode_data->at(k) == "Y" && bump_pin_name == rdr.bumpPinName(k)) {
            result.first = bump_pin_name;
            result.second = mode_name;
            used_bump_pins_.insert(bump_pin_name);
            if (tr >= 5) {
              lprintf("\t\t  get_available_bump_ipin() used_bump_pins_.insert( %s )  row_i= %u  row_k= %u\n",
                  bump_pin_name.c_str(), i, k);
            }
            found = true;
            goto ret;
          }
        }
      } else {
        continue;
      }
    }
  }

ret:
  if (tr >= 2) {
    if (found && tr >= 4) {
      const string& bump_pn = result.first;
      const string& mode_nm = result.second;
      lprintf("\t  ret  bump_pin_name= %s  mode_name= %s\n", bump_pn.c_str(),
              mode_nm.c_str());
    } else if (tr >= 7) {
      lprintf("\t (EERR) get_available_bump_ipin()#%u returns NOT_FOUND\n", icnt);
      lputs2();
      lprintf("\t vvv used_bump_pins_.size()= %u\n", (uint)used_bump_pins_.size());
      if (tr >= 8) {
        if (tr >= 9) {
          for (const auto& ubp : used_bump_pins_)
            lprintf("\t    %s\n", ubp.c_str());
        }
        lprintf("\t ^^^ used_bump_pins_.size()= %u\n", (uint)used_bump_pins_.size());
        lprintf("\t (EERR) get_available_bump_ipin()#%u returns NOT_FOUND\n", icnt);
      }
      lputs2();
    }
  }

  if (found) {
    assert(!result.first.empty());
    assert(!result.second.empty());
  } else {
    result.first.clear();
    result.second.clear();
  }

  return result;
}

StringPair PinPlacer::get_available_bump_opin(const RapidCsvReader& rdr, const string& udesName)
{
  static uint ocnt = 0;
  ocnt++;
  uint16_t tr = ltrace();
  if (tr >= 4) {
    lprintf("get_available_bump_opin()# %u  for udes-pin %s\n", ocnt, udesName.c_str());
  }

  constexpr bool is_input_port = false;

  bool found = false;
  StringPair result; // pin_and_mode

  uint num_rows = rdr.numRows();
  for (uint i = rdr.start_GBOX_GPIO_row_; i < num_rows; i++) {
    const auto& bcd = rdr.bcd_[i];
    const string& bump_pin_name = bcd.bump_;

    if (used_bump_pins_.count(bump_pin_name)) {
      if (tr >= 9)
        lprintf("  bump_opin_name %s is used, skipping..\n", bump_pin_name.c_str());
      continue;
    }

    for (uint j = 0; j < rdr.mode_names_.size(); j++) {
      const string& mode_name = rdr.mode_names_[j];
      const vector<string>* mode_data = rdr.getModeData(mode_name);
      assert(mode_data);
      assert(mode_data->size() == num_rows);
      if (is_input_port) {
        //if (is_input_mode(mode_name)) {
        //  for (uint k = rdr.start_GBOX_GPIO_row_; k < num_rows; k++) {
        //    if (mode_data->at(k) == "Y" &&
        //        bump_pin_name == rdr.bumpPinName(k)) {
        //      result.first = bump_pin_name;
        //      result.second = mode_name;
        //      used_bump_pins_.insert(bump_pin_name);
        //      if (tr >= 5) {
        //        lprintf("\t\t  get_available_bump_opin() used_bump_pins_.insert( %s )  row_i= %u  row_k= %u\n",
        //            bump_pin_name.c_str(), i, k);
        //      }
        //      found = true;
        //      goto ret;
        //    }
        //  }
        //} else {
        //  continue;
        //}
      } else { // OUTPUT port
        if (is_output_mode(mode_name)) {
          for (uint k = rdr.start_GBOX_GPIO_row_; k < num_rows; k++) {
            if (mode_data->at(k) == "Y" &&
                bump_pin_name == rdr.bumpPinName(k)) {
              result.first = bump_pin_name;
              result.second = mode_name;
              used_bump_pins_.insert(bump_pin_name);
              found = true;
              goto ret;
            }
          }
        }
      }
    }
  }

ret:
  if (tr >= 2) {
    if (found && tr >= 4) {
      const string& bump_pn = result.first;
      const string& mode_nm = result.second;
      lprintf("\t  ret  bump_pin_name= %s  mode_name= %s\n", bump_pn.c_str(),
              mode_nm.c_str());
    } else if (tr >= 7) {
      lprintf("\t (EERR) get_available_bump_opin()#%u returns NOT_FOUND\n", ocnt);
      lputs2();
      lprintf("\t vvv used_bump_pins_.size()= %u\n", (uint)used_bump_pins_.size());
      if (tr >= 8) {
        if (tr >= 9) {
          for (const auto& ubp : used_bump_pins_)
            lprintf("\t    %s\n", ubp.c_str());
        }
        lprintf("\t ^^^ used_bump_pins_.size()= %u\n", (uint)used_bump_pins_.size());
        lprintf("\t (EERR) get_available_bump_opin()#%u returns NOT_FOUND\n", ocnt);
      }
      lputs2();
    }
  }

  if (found) {
    assert(!result.first.empty());
    assert(!result.second.empty());
  } else {
    result.first.clear();
    result.second.clear();
  }

  return result;
}

// create a temporary pcf file and internally pass it to params
bool PinPlacer::create_temp_pcf(const RapidCsvReader& csv_rd)
{
  clear_err_code();
  string key = "--pcf";
  temp_pcf_name_ = std::to_string(fio::get_PID()) + ".temp_pcf.pcf";
  cl_.set_param_value(key, temp_pcf_name_);

  bool continue_on_errors = ::getenv("pinc_continue_on_errors");

  uint16_t tr = ltrace();
  if (tr >= 3)
    lprintf("\ncreate_temp_pcf() : %s\n", temp_pcf_name_.c_str());

  // state for get_available_device_pin:
  no_more_inp_bumps_ = false;
  no_more_out_bumps_ = false;
  //
  s_axi_inpQ = csv_rd.get_AXI_inputs();
  s_axi_outQ = csv_rd.get_AXI_outputs();

  if (tr >= 4) {
    lprintf("  s_axi_inpQ.size()= %u  s_axi_outQ.size()= %u\n",
            (uint)s_axi_inpQ.size(), (uint)s_axi_outQ.size());
  }

  vector<int> input_idx, output_idx;
  uint input_sz = user_design_inputs_.size();
  uint output_sz = user_design_outputs_.size();
  input_idx.reserve(input_sz);
  output_idx.reserve(output_sz);
  for (uint i = 0; i < input_sz; i++) {
    input_idx.push_back(i);
  }
  for (uint i = 0; i < output_sz; i++) {
    output_idx.push_back(i);
  }
  if (pin_assign_def_order_ == false) {
    shuffle_candidates(input_idx);
    shuffle_candidates(output_idx);
    if (tr >= 3)
      lputs("  create_temp_pcf() randomized input_idx, output_idx");
  } else if (tr >= 3) {
    lputs(
        "  input_idx, output_idx are indexing user_design_inputs_, "
        "user_design_outputs_");
    lprintf("  input_idx.size()= %u  output_idx.size()= %u\n",
            uint(input_idx.size()), uint(output_idx.size()));
  }

  StringPair dpin_and_mode; // device pin and mode
  ofstream temp_out;
  temp_out.open(temp_pcf_name_, ifstream::out | ifstream::binary);
  if (temp_out.fail()) {
    lprintf(
        "\n  !!! (ERROR) create_temp_pcf(): could not open %s for "
        "writing\n",
        temp_pcf_name_.c_str());
    CERROR << "(EE) could not open " << temp_pcf_name_ << " for writing\n"
           << endl;
    return false;
  }

  // if user specified "--write_pcf", 'user_out' will be a copy of 'temp_out'
  ofstream user_out;
  {
    string user_pcf_name = cl_.get_param("--write_pcf");
    if (user_pcf_name.size()) {
      user_out.open(user_pcf_name, ifstream::out | ifstream::binary);
      if (user_out.fail()) {
        lprintf(
            "\n  !!! (ERROR) create_temp_pcf(): failed to open user_out "
            "%s\n",
            user_pcf_name.c_str());
        return false;
      }
    }
  }

  string pinName, set_io_str;

  if (tr >= 2) {
    lprintf("--- writing pcf inputs (%u)\n", input_sz);
  }
  for (uint i = 0; i < input_sz; i++) {
    const string& inpName = user_design_inputs_[i];
    if (tr >= 4) {
      lprintf("assigning user_design_input #%u %s\n", i, inpName.c_str());
      if (csv_rd.hasCustomerInternalName(inpName)) {
        lprintf("\t (CustIntName) hasCustomerInternalName( %s )\n", inpName.c_str());
      }
    }
    assert(!inpName.empty());
    dpin_and_mode = get_available_device_pin(csv_rd, true /*INPUT*/, inpName);
    if (dpin_and_mode.first.length()) {
      pinName = csv_rd.bumpName2CustomerName(dpin_and_mode.first);
      assert(!pinName.empty());

      set_io_str = user_design_inputs_[input_idx[i]];
      set_io_str.push_back(' ');
      set_io_str += pinName;
      set_io_str += " -mode ";
      set_io_str += dpin_and_mode.second;

      if (tr >= 4) {
        lprintf(" ... writing Input to pcf for  bump_pin= %s  pinName= %s\n",
                dpin_and_mode.first.c_str(), pinName.c_str());
        lprintf("        set_io %s\n", set_io_str.c_str());
      }
      temp_out << "set_io " << set_io_str << endl;
      if (user_out.is_open()) {
        user_out << "set_io " << set_io_str << endl;
      }
    } else {
      lprintf("\n[Error] pin_c: failed getting device pin for input pin: %s\n", pinName.c_str());
      set_err_code("TOO_MANY_INPUTS");
      lputs3();
      num_warnings_++;
      if (not continue_on_errors)
        return false;
      continue;
    }
  }

  if (tr >= 2) {
    lprintf("--- writing pcf outputs (%u)\n", output_sz);
  }
  for (uint i = 0; i < output_sz; i++) {
    const string& outName = user_design_outputs_[i];
    if (tr >= 4) {
      lprintf("assigning user_design_output #%u %s\n", i, outName.c_str());
      if (csv_rd.hasCustomerInternalName(outName)) {
        lprintf("\t (CustIntName) hasCustomerInternalName( %s )\n", outName.c_str());
      }
    }
    assert(!outName.empty());
    dpin_and_mode = get_available_device_pin(csv_rd, false /*OUTPUT*/, outName);
    if (dpin_and_mode.first.length()) {
      pinName = csv_rd.bumpName2CustomerName(dpin_and_mode.first);
      assert(!pinName.empty());

      set_io_str = user_design_outputs_[output_idx[i]];
      set_io_str.push_back(' ');
      set_io_str += pinName;
      set_io_str += " -mode ";
      set_io_str += dpin_and_mode.second;

      temp_out << "set_io " << set_io_str << endl;
      if (user_out.is_open()) {
        user_out << "set_io " << set_io_str << endl;
      }
    } else {
      lprintf("[Error] pin_c: failed getting device pin for output pin: %s\n", pinName.c_str());
      set_err_code("TOO_MANY_OUTPUTS");
      lputs3();
      num_warnings_++;
      if (not continue_on_errors)
        return false;
      continue;
    }
  }

  return true;
}

// static
void PinPlacer::shuffle_candidates(vector<int>& v) {
  static random_device rd;
  static mt19937 g(rd());
  std::shuffle(v.begin(), v.end(), g);
  return;
}

/*
void PinPlacer::collect_left_available_device_pins(
    set<string> &constrained_device_pins,
    vector<int> &left_available_device_pin_idx, RapidCsvReader &rs_csv_reader) {
  for (uint i = 0; i < rs_csv_reader.bump_pin_name.size(); i++) {
    if (rs_csv_reader.usable[i] == "Y" &&
        (constrained_device_pins.find(rs_csv_reader.bump_pin_name[i]) ==
         constrained_device_pins.end())) { // left-over availalbe pins to be
                                           // assigned to user design
      left_available_device_pin_idx.push_back(i);
    }
  }
  return;
}
*/

bool PinPlacer::is_input_mode(const string& mode_name) const {
  if (mode_name.empty()) return false;

  size_t dash_loc = mode_name.rfind("_");
  if (dash_loc == string::npos) {
    return false;
  }
  string post_fix = mode_name.substr(dash_loc, mode_name.length() - 1);
  if (post_fix == INPUT_MODE_FIX || post_fix == GPIO_MODE_FIX) {
    return true;
  }

  return false;
}

bool PinPlacer::is_output_mode(const string& mode_name) const {
  if (mode_name.empty()) return false;

  size_t dash_loc = mode_name.rfind("_");
  if (dash_loc == string::npos) {
    return false;
  }
  string post_fix = mode_name.substr(dash_loc, mode_name.length() - 1);
  if (post_fix == OUTPUT_MODE_FIX || post_fix == GPIO_MODE_FIX) {
    return true;
  }

  return false;
}

} // namespace pinc

