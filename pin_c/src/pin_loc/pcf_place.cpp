#include "pin_loc/pin_placer.h"
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


// static data (hacky, but temporary)
static vector<string> s_axi_inpQ, s_axi_outQ;


const Pin* PinPlacer::find_udes_pin(const vector<Pin>& P, const string& nm) noexcept {
  for (const Pin& p : P) {
    if (p.udes_pin_name_ == nm)
      return &p;
  }
  return nullptr;
}

void PinPlacer::print_stats(const RapidCsvReader& csv) const {
  uint16_t tr = ltrace();
  auto& ls = lout();

  if (num_warnings_) {
    lprintf("\n\t pin_c: NOTE ERRORs: %u\n", num_warnings_);
    lprintf("\t itile_overlap_level_= %u  otile_overlap_level_= %u\n",
            itile_overlap_level_, otile_overlap_level_);
  }

  if (tr < 3)
    return;

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
           << "  device: " << pp->device_pin_name_ << "  pt_row: " << pp->pt_row_+2;
        const auto& bcd = csv.getBCD(pp->pt_row_);
        ls << "  isInput:" << int(bcd.isInput());
        ls << "  colM_dir: " << bcd.str_colM_dir();
      }
      ls << endl;
    }
    lprintf("\n ---- outputs(%zu): ---- \n", outputs.size());
    for (size_t i = 0; i < outputs.size(); i++) {
      const string& nm = outputs[i];
      ls << "    out  " << nm;
      const Pin* pp = find_udes_pin(placed_outputs_, nm);
      if (pp) {
        ls << "  placed at " << pp->xyz_
           << "  device: " << pp->device_pin_name_ << "  pt_row: " << pp->pt_row_+2;
        const auto& bcd = csv.getBCD(pp->pt_row_);
        ls << "  isInput:" << int(bcd.isInput());
        ls << "  colM_dir: " << bcd.str_colM_dir();
      }
      ls << endl;
    }
    lputs();
    ls << " <----- pin_c got " << inputs.size() << " inputs and "
       << outputs.size() << " outputs" << endl;
    ls << " <-- pin_c placed " << placed_inputs_.size() << " inputs and "
       << placed_outputs_.size() << " outputs" << endl;
  }

  ls << "  min_pt_row= " << min_pt_row_+2 << "  max_pt_row= " << max_pt_row_+2 << '\n';
  ls << "  row0_GBOX_GPIO()= " << csv.row0_GBOX_GPIO()
     << "  row0_CustomerInternal()= " << csv.row0_CustomerInternal() << endl;

  ls << endl;
  csv.print_bcd_stats(ls);

  ls << "======== end stats." << endl;
  if (tr >= 7) {
    uint nr = csv.numRows();
    if (max_pt_row_ > 0 && nr > 10 && max_pt_row_ < nr - 1) {
      uint minRow = std::min(csv.row0_GBOX_GPIO(), csv.row0_CustomerInternal());
      minRow = std::min(minRow, min_pt_row_);
      if (minRow > 0)
        minRow--;
      csv.write_csv("LAST_PINC_PT_reduced.csv", minRow, max_pt_row_ + 1);
    } else if (nr > 2) {
      csv.write_csv("LAST_PINC_PT_full.csv", 0, UINT_MAX);
    }
  }

  if (num_warnings_) {
    lprintf("\n\t pin_c: NOTE ERRORs: %u\n", num_warnings_);
    lprintf("\t itile_overlap_level_= %u  otile_overlap_level_= %u\n",
            itile_overlap_level_, otile_overlap_level_);
  }

  // verify
#ifndef NDEBUG
  if (tr >= 4 && placed_inputs_.size()) {
    // 1. placed_inputs_ XYZ don't intersect
    const Pin* A = placed_inputs_.data();
    uint A_sz = placed_inputs_.size();
    for (uint i = 0; i < A_sz; i++) {
      const Pin& pin_i = A[i];
      for (uint j = i + 1; j < A_sz; j++) {
        const Pin& pin_j = A[j];
        if (pin_i.xyz_ == pin_j.xyz_) {
          ls << '\n' << "placed_inputs_ VERIFICATION FAILED:  pin_i.xyz_ == pin_j.xyz_ : "
             << pin_i.xyz_ << '\n' << endl;
        }
        assert(pin_i.xyz_ != pin_j.xyz_);
      }
    }
    // 2. placed_outputs_ XYZ don't intersect
    A = placed_outputs_.data();
    A_sz = placed_outputs_.size();
    for (uint i = 0; i < A_sz; i++) {
      const Pin& pin_i = A[i];
      for (uint j = i + 1; j < A_sz; j++) {
        const Pin& pin_j = A[j];
        if (pin_i.xyz_ == pin_j.xyz_) {
          ls << '\n' << "placed_outputs_ VERIFICATION FAILED:  pin_i.xyz_ == pin_j.xyz_ : "
             << pin_i.xyz_ << '\n' << endl;
        }
        assert(pin_i.xyz_ != pin_j.xyz_);
      }
    }
  }
#endif // NDEBUG
}

void PinPlacer::printTileUsage(const RapidCsvReader& csv) const {
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 9) lputs("\nPinPlacer::printTileUsage()");

  ls << "  used_bump_pins_.size()= " << used_bump_pins_.size()
     << "  used_XYs_.size()= " << used_XYs_.size()
     << "  used_tiles_.size()= " << used_tiles_.size() << endl;
}

bool PinPlacer::read_pcf(const RapidCsvReader& csv) {
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
    if (csv.hasMode(key)) continue;
    ls << endl;
    CERROR << " (ERROR) invalid mode name: " << mode << endl;
    if (tr >= 2) {
      ls << " (ERROR) invalid mode name: " << mode << "  [ " << key << " ]" << endl;
      if (tr >= 3) {
        ls << " valid mode names are:" << endl;
        csv.printModeNames();
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

bool PinPlacer::write_dot_place(const RapidCsvReader& csv)
{
  placed_inputs_.clear();
  placed_outputs_.clear();
  string out_fn = cl_.get_param("--output");
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2) {
    ls << "\npinc::write_dot_place() __ Creating .place file  get_param(--output) : "
       << out_fn << endl;
    if (tr >= 4) {
      lprintf("  ___ pcf_pin_cmds_ (%zu):\n", pcf_pin_cmds_.size());
      for (const auto& cmd : pcf_pin_cmds_) {
        logVec(cmd, " ");
      }
      lprintf("  ___\n");
    }
  }

  ofstream out_file;
  out_file.open(out_fn);
  if (out_file.fail()) {
    if (tr >= 1) {
      ls << "\n[Error] pinc::write_dot_place() FAILED to write " << out_fn << endl;
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

  string gbox_pin_name;

  string row_str;
  int row_num = -1;

  min_pt_row_ = UINT_MAX; max_pt_row_ = 0;

  for (uint cmd_i = 0; cmd_i < pcf_pin_cmds_.size(); cmd_i++) {

    const vector<string>& pcf_cmd = pcf_pin_cmds_[cmd_i];

    // only support io and clock for now
    if (pcf_cmd[0] != "set_io" && pcf_cmd[0] != "set_clk") continue;

    gbox_pin_name.clear();

    const string& udes_pin_name = pcf_cmd[1];
    const string& device_pin_name = pcf_cmd[2];  // bump or ball

    if (tr >= 4) {
      ls << "cmd_i:" << cmd_i
         << "   udes_pin_name=  " << udes_pin_name
         << "  -->  device_pin_name=  " << device_pin_name << endl;
      logVec(pcf_cmd, "    cur pcf_cmd: ");
    }

    for (size_t i = 1; i < pcf_cmd.size(); i++) {
      if (pcf_cmd[i] == "-internal_pin") {
        gbox_pin_name = pcf_cmd[++i];
        break;
      }
    }

    row_str.clear();
    row_num = -1;
    for (size_t i = 1; i < pcf_cmd.size() - 1; i++) {
      if (pcf_cmd[i] == "-pt_row") {
        row_str = pcf_cmd[i + 1];
        break;
      }
    }
    if (!row_str.empty()) {
      row_num = ::atoi(row_str.c_str());
    }

    bool is_in_pin = vec_contains(user_design_inputs_, udes_pin_name);

    bool is_out_pin =
        is_in_pin ? false
                  : vec_contains(user_design_outputs_, udes_pin_name);

    if (!is_in_pin && !is_out_pin) {
      // sanity check
      CERROR << err_lookup("CONSTRAINED_PORT_NOT_FOUND") << ": <"
             << udes_pin_name << ">" << endl;
      out_file << "\n=== Error happened, .place file is incomplete\n"
               << "=== ERROR:" << err_lookup("CONSTRAINED_PORT_NOT_FOUND")
               << ": <" << udes_pin_name
               << ">  device_pin_name: " << device_pin_name << "\n\n";
      return false;
    }

    if (!csv.has_io_pin(device_pin_name)) {
      CERROR << err_lookup("CONSTRAINED_PIN_NOT_FOUND") << ": <"
             << device_pin_name << ">" << endl;
      out_file << "\n=== Error happened, .place file is incomplete\n"
               << "=== ERROR:" << err_lookup("CONSTRAINED_PORT_NOT_FOUND")
               << ": <" << udes_pin_name
               << ">  device_pin_name: " << device_pin_name << "\n\n";
      return false;
    }

    const char* direction = "INPUT";

    // lookup and write XYZ for 'udes_pin_name'
    if (is_out_pin) {
      direction = "OUTPUT";
      out_file << "out:";
      if (tr >= 4) ls << "out:";
    }

    out_file << udes_pin_name << '\t';
    out_file.flush();

    string mode = pcf_cmd[4];
    RapidCsvReader::prepare_mode_header(mode);

    uint pt_row = 0;
    XYZ xyz;

    if (row_num > 1) {
      // pt_row was explicitely passed in .pcf
      uint row_idx = row_num - 2;
      if (row_idx < csv.numRows()) {
        const RapidCsvReader::BCD& bb = csv.getBCD(row_idx);
        xyz = bb.xyz_;
        pt_row = row_idx;
      }
    }
    else {
      if (is_out_pin) {
        xyz = csv.get_opin_xyz_by_name(mode, device_pin_name, gbox_pin_name,
                                       used_oxyz_, pt_row);
        if (tr >= 6) {
          if (xyz.valid())
            lprintf("    get_opin_xyz annotated pt_row= %u\n", pt_row);
          else
            lputs("\n    get_opin_xyz FAILED");
        }
      } else {
        xyz = csv.get_ipin_xyz_by_name(mode, device_pin_name, gbox_pin_name,
                                       used_ixyz_, pt_row);
        if (tr >= 6) {
          if (xyz.valid())
            lprintf("    get_ipin_xyz annotated pt_row= %u\n", pt_row);
          else
            lputs("\n    get_ipin_xyz FAILED");
        }
      }
    }

    if (!xyz.valid()) {
      lputs();
      CERROR << "\n   no valid coordinates for "
             << direction << " pin: " << udes_pin_name << endl;
      lputs("\n[Error]");
      lprintf("   user_design_pin_name:  %s\n", udes_pin_name.c_str());
      lprintf("   mode %s  device_pin_name %s   gbox_pin_name %s\n\n",
              mode.c_str(), device_pin_name.c_str(), gbox_pin_name.c_str());
      lprintf("   related %s pcf command:\n", auto_pcf_created_ ? "auto" : "user");
      logVec(pcf_cmd, "     ");
      lputs();
      lprintf("   related pin table row: %u\n", pt_row > 0 ? pt_row + 2 : pt_row);
      lputs();
      return false;
    }

    if (is_out_pin)
      used_oxyz_.insert(xyz);
    else
      used_ixyz_.insert(xyz);

    const RapidCsvReader::BCD& bcd = csv.getBCD(pt_row);

    if (pt_row < min_pt_row_)
      min_pt_row_ = pt_row;
    if (pt_row > max_pt_row_)
      max_pt_row_ = pt_row;

    out_file << xyz.x_ << '\t' << xyz.y_ << '\t' << xyz.z_;
    if (tr >= 4) {
      // debug annotation
      out_file << "    #  device: " << device_pin_name;
      out_file << "  pt_row: " << pt_row+2;
      out_file << "  isInput:" << int(bcd.isInput());
      out_file << "  colM_dir: " << bcd.str_colM_dir();
    }
    out_file << endl;
    out_file.flush();

    if (tr >= 4) {
      ls << xyz.x_ << '\t' << xyz.y_ << '\t' << xyz.z_;
      // debug annotation
      ls << "    #  device: " << device_pin_name;
      ls << "  pt_row: " << pt_row+2;
      ls << "  isInput:" << int(bcd.isInput());
      ls << "  colM_dir: " << bcd.str_colM_dir();
      flush_out(true);
    }

    // save for statistics:
    if (is_in_pin)
      placed_inputs_.emplace_back(udes_pin_name, device_pin_name, xyz, pt_row);
    else
      placed_outputs_.emplace_back(udes_pin_name, device_pin_name, xyz, pt_row);
  }

  if (tr >= 2) {
    ls << "\nwritten " << num_placed_pins() << " pins to " << out_fn << endl;
    if (tr >= 3) {
      ls << "  min_pt_row= " << min_pt_row_ << "  max_pt_row= " << max_pt_row_ << endl;
    }
  }

  return true;
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

bool PinPlacer::read_csv_file(RapidCsvReader& csv) {
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
  if (!csv.read_csv(csv_name, check)) {
    CERROR << err_lookup("PIN_MAP_CSV_PARSE_ERROR") << endl;
    return false;
  }

  return true;
}

// 'ann_pin' - annotated pin taken from bcd
DevPin PinPlacer::get_available_device_pin(RapidCsvReader& csv,
                                           bool is_inp,
                                           const string& udesName,
                                           Pin*& ann_pin)
{
  DevPin result;
  ann_pin = nullptr;

  uint16_t tr = ltrace();

  if (is_inp) {
    if (no_more_inp_bumps_) {
      result = get_available_axi_ipin(s_axi_inpQ);
      if (tr >= 4)
        lprintf("  no_more_inp_bumps_ => got axi_ipin: %s\n", result.first().c_str());
      return result;
    }
    result = get_available_bump_ipin(csv, udesName, ann_pin);
    if (result.first().empty() && s_axi_inpQ.size()) {
      no_more_inp_bumps_ = true;
      result = get_available_axi_ipin(s_axi_inpQ);
    }
  } else { // out
    if (no_more_out_bumps_) {
      result = get_available_axi_opin(s_axi_outQ);
      if (tr >= 4)
        lprintf("  no_more_out_bumps_ => got axi_opin: %s\n", result.first().c_str());
      return result;
    }
    result = get_available_bump_opin(csv, udesName, ann_pin);
    if (result.first().empty() && s_axi_outQ.size()) {
      no_more_out_bumps_ = true;
      result = get_available_axi_opin(s_axi_outQ);
    }
  }

  return result;
}

DevPin PinPlacer::get_available_axi_ipin(vector<string>& Q) {
  static uint cnt = 0;
  cnt++;
  if (ltrace() >= 4) {
    lprintf("get_available_axi_ipin()# %u Q.size()= %u\n", cnt, uint(Q.size()));
  }

  DevPin result; // pin_and_mode

  if (Q.empty()) return result;

  result.set_first(Q.back());
  Q.pop_back();

  // need to pick a mode, othewise PcfReader::read_pcf() will not tokenize pcf line properly
  result.mode_ = "MODE_GPIO";

  return result;
}

DevPin PinPlacer::get_available_axi_opin(vector<string>& Q) {
  static uint cnt = 0;
  cnt++;
  if (ltrace() >= 4) {
    lprintf("get_available_axi_opin()# %u Q.size()= %u\n", cnt, uint(Q.size()));
  }

  DevPin result; // pin_and_mode

  if (Q.empty()) return result;

  result.set_first(Q.back());
  Q.pop_back();

  // need to pick a mode, othewise PcfReader::read_pcf() will not tokenize pcf line properly
  result.mode_ = "MODE_GPIO";

  return result;
}

DevPin PinPlacer::get_available_bump_ipin(RapidCsvReader& csv,
                                          const string& udesName,
                                          Pin*& ann_pin) {
  static uint icnt = 0;
  icnt++;
  ann_pin = nullptr;
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 4) {
    lprintf("get_available_bump_ipin()# %u  for udes-pin %s", icnt, udesName.c_str());
    ls << endl;
  }

  bool found = false;
  DevPin result; // pin_and_mode
  RapidCsvReader::BCD* site = nullptr;
  std::bitset<Pin::MAX_PT_COLS> modes;
  uint num_cols = csv.numCols();
  std::unordered_set<uint> except;

  uint iteration = 1;
  for (; iteration <= 100; iteration++) {
    if (tr >= 4)
      lprintf("  start iteration %u\n", iteration);
    RapidCsvReader::Tile* tile = csv.getUnusedTile(true, except, itile_overlap_level_);
    if (!tile) {
      if (tr >= 3) {
        lputs("  no i-tile");
        if (tr >= 5)
          printTileUsage(csv);
      }
      goto ret;
    }
    if (tr >= 4) {
      ls << "  got i-tile " << tile->key2() << endl;
      if (tr >= 6) tile->dump();
    }
    assert(tile->num_used_ < itile_overlap_level_);
    site = tile->bestInputSite();
    if (!site) {
      if (tr >= 3) lputs("  no i-site");
      except.insert(tile->id_);
      continue;
    }
    if (tr >= 13) site->dump();

    modes = site->getRxModes();
    // ==1== non-GPIO RX
    for (uint col = csv.start_MODE_col_; col < num_cols; col++) {
      if (modes[col] && !csv.isTxCol(col)) {
        result.set(site->bump_, csv.col_headers_[col], site->row_);
        site->set_used();
        tile->incr_used();
        ann_pin = site->annotatePin(udesName, site->bump_, true);
        used_bump_pins_.insert(site->bump_);
        used_XYs_.insert(site->xy());
        used_tiles_.insert(tile->id_);
        if (tr >= 6) {
          lprintf("\t\t ==1==RX GABI used_bump_pins_.insert( %s )  row_= %u\n",
              site->bump_.c_str(), site->row_);
        }
        found = true;
        goto ret;
      }
    }
    // ==2== GPIO
    modes = site->getGpioModes();
    for (uint col = csv.start_MODE_col_; col < num_cols; col++) {
      if (modes[col]) {
        result.set(site->bump_, csv.col_headers_[col], site->row_);
        site->set_used();
        tile->incr_used();
        ann_pin = site->annotatePin(udesName, site->bump_, true);
        used_bump_pins_.insert(site->bump_);
        used_XYs_.insert(site->xy());
        used_tiles_.insert(tile->id_);
        if (tr >= 6) {
          lprintf("\t\t ==2==GPIO_rx GABI used_bump_pins_.insert( %s )  row_= %u\n",
              site->bump_.c_str(), site->row_);
        }
        found = true;
        goto ret;
      }
    }
    // not found => try another tile
    if (tr >= 4)
      lprintf("  not found => disabling tile %s\n", tile->key2().c_str());
    except.insert(tile->id_);
  } // iteration

ret:
  if (tr >= 2) {
    lprintf("  did #iterations: %u\n", iteration);
    if (found && tr >= 4) {
      const string& bump_pn = result.first();
      const string& mode_nm = result.mode_;
      lprintf("\t  ret  bump_pin_name= %s  mode_name= %s\n", bump_pn.c_str(),
              mode_nm.c_str());
    } else if (tr >= 7) {
      lprintf("\t (EERR) get_available_bump_ipin()#%u returns NOT_FOUND\n", icnt);
      lputs2();
      lprintf("\t vvv used_bump_pins_.size()= %zu\n", used_bump_pins_.size());
      if (tr >= 8) {
        if (tr >= 9) {
          for (const auto& ubp : used_bump_pins_)
            lprintf("\t    %s\n", ubp.c_str());
        }
        lprintf("\t ^^^ used_bump_pins_.size()= %zu\n", used_bump_pins_.size());
        lprintf("\t (EERR) get_available_bump_ipin()#%u returns NOT_FOUND\n", icnt);
      }
      lputs2();
    }
  }

  if (found) {
    assert(!result.first().empty());
    assert(!result.mode_.empty());
  } else {
    result.reset();
  }

  return result;
}

DevPin PinPlacer::get_available_bump_opin(RapidCsvReader& csv,
                                          const string& udesName,
                                          Pin*& ann_pin) {
  static uint ocnt = 0;
  ocnt++;
  ann_pin = nullptr;
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 4) {
    lprintf("get_available_bump_opin()# %u  for udes-pin %s", ocnt, udesName.c_str());
    ls << endl;
  }

  bool found = false;
  DevPin result; // pin_and_mode
  RapidCsvReader::BCD* site = nullptr;
  std::bitset<Pin::MAX_PT_COLS> modes;
  uint num_cols = csv.numCols();
  std::unordered_set<uint> except;

  uint iteration = 1;
  for (; iteration <= 100; iteration++) {
    if (tr >= 4)
      lprintf("  start iteration %u\n", iteration);
    RapidCsvReader::Tile* tile = csv.getUnusedTile(false, except, otile_overlap_level_);
    if (!tile) {
      if (tr >= 3) {
        lputs("  no o-tile");
        if (tr >= 5)
          printTileUsage(csv);
      }
      goto ret;
    }
    if (tr >= 4) {
      ls << "  got o-tile " << tile->key2() << endl;
      if (tr >= 6) tile->dump();
    }
    assert(tile->num_used_ < otile_overlap_level_);
    site = tile->bestOutputSite();
    if (!site) {
      if (tr >= 3) lputs("  no o-site");
      except.insert(tile->id_);
      continue;
    }
    if (tr >= 13) site->dump();

    modes = site->getTxModes();
    // ==1== non-GPIO TX
    for (uint col = csv.start_MODE_col_; col < num_cols; col++) {
      if (modes[col] && !csv.isRxCol(col)) {
        result.set(site->bump_, csv.col_headers_[col], site->row_);
        site->set_used();
        tile->incr_used();
        ann_pin = site->annotatePin(udesName, site->bump_, false);
        used_bump_pins_.insert(site->bump_);
        used_XYs_.insert(site->xy());
        used_tiles_.insert(tile->id_);
        if (tr >= 6) {
          lprintf("\t\t ==1==TX GABO used_bump_pins_.insert( %s )  row_= %u\n",
              site->bump_.c_str(), site->row_);
        }
        found = true;
        goto ret;
      }
    }
    // ==2== GPIO
    modes = site->getGpioModes();
    for (uint col = csv.start_MODE_col_; col < num_cols; col++) {
      if (modes[col]) {
        result.set(site->bump_, csv.col_headers_[col], site->row_);
        site->set_used();
        tile->incr_used();
        ann_pin = site->annotatePin(udesName, site->bump_, false);
        used_bump_pins_.insert(site->bump_);
        used_XYs_.insert(site->xy());
        used_tiles_.insert(tile->id_);
        if (tr >= 6) {
          lprintf("\t\t ==2==GPIO_tx GABO used_bump_pins_.insert( %s )  row_= %u\n",
              site->bump_.c_str(), site->row_);
        }
        found = true;
        goto ret;
      }
    }
    // not found => try another tile
    if (tr >= 4)
      lprintf("  not found => disabling tile %s\n", tile->key2().c_str());
    except.insert(tile->id_);
  } // iteration

ret:
  if (tr >= 2) {
    if (found && tr >= 4) {
      const string& bump_pn = result.first();
      const string& mode_nm = result.mode_;
      lprintf("\t  ret  bump_pin_name= %s  mode_name= %s\n", bump_pn.c_str(),
              mode_nm.c_str());
    } else if (tr >= 7) {
      lprintf("\t (EERR) get_available_bump_opin()#%u returns NOT_FOUND\n", ocnt);
      lputs2();
      lprintf("\t vvv used_bump_pins_.size()= %zu\n", used_bump_pins_.size());
      if (tr >= 8) {
        if (tr >= 9) {
          for (const auto& ubp : used_bump_pins_)
            lprintf("\t    %s\n", ubp.c_str());
        }
        lprintf("\t ^^^ used_bump_pins_.size()= %zu\n", used_bump_pins_.size());
        lprintf("\t (EERR) get_available_bump_opin()#%u returns NOT_FOUND\n", ocnt);
      }
      lputs2();
    }
  }

  if (found) {
    assert(!result.first().empty());
    assert(!result.mode_.empty());
  } else {
    result.reset();
  }

  return result;
}

// create a temporary pcf file and internally pass it to params
bool PinPlacer::create_temp_pcf(RapidCsvReader& csv) {
  auto_pcf_created_ = false;
  clear_err_code();
  string key = "--pcf";
  temp_pcf_name_ = std::to_string(fio::get_PID()) + ".temp_pcf.pcf";
  cl_.set_param_value(key, temp_pcf_name_);

  // bool continue_on_errors = ::getenv("pinc_continue_on_errors");

  uint16_t tr = ltrace();
  if (tr >= 3) {
    lprintf("\ncreate_temp_pcf() : %s\n", temp_pcf_name_.c_str());
  }

  used_bump_pins_.clear();
  used_XYs_.clear();
  used_tiles_.clear();
  otile_overlap_level_ = 1;
  itile_overlap_level_ = 1;

  // state for get_available_device_pin:
  no_more_inp_bumps_ = false;
  no_more_out_bumps_ = false;
  //
  s_axi_inpQ = csv.get_AXI_inputs();
  s_axi_outQ = csv.get_AXI_outputs();

  if (tr >= 4) {
    lprintf("  s_axi_inpQ.size()= %zu  s_axi_outQ.size()= %zu\n",
               s_axi_inpQ.size(), s_axi_outQ.size());
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

  DevPin dpin;
  ofstream temp_out;
  temp_out.open(temp_pcf_name_, ifstream::out | ifstream::binary);
  if (temp_out.fail()) {
    lprintf(
        "\n  !!! (ERROR) create_temp_pcf(): could not open %s for "
        "writing\n",
        temp_pcf_name_.c_str());
    CERROR << "(EE) could not open " << temp_pcf_name_ << " for writing\n" << endl;
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
  set_io_str.reserve(128);
  Pin* ann_pin = nullptr; // annotated pin

  if (tr >= 2) {
    lprintf("--- writing pcf inputs (%u)\n", input_sz);
  }
  for (uint i = 0; i < input_sz; i++) {
    const string& inpName = user_design_inputs_[i];
    if (tr >= 4) {
      lprintf("assigning user_design_input #%u %s\n", i, inpName.c_str());
      if (csv.hasCustomerInternalName(inpName)) {
        lprintf("\t (CustIntName) hasCustomerInternalName( %s )\n", inpName.c_str());
      }
    }
    assert(!inpName.empty());
    dpin = get_available_device_pin(csv, true /*INPUT*/, inpName, ann_pin);
    if (dpin.first().length()) {
      pinName = csv.bumpName2CustomerName(dpin.first());
      assert(!pinName.empty());

      set_io_str = user_design_inputs_[input_idx[i]];
      set_io_str.push_back(' ');
      set_io_str += pinName;
      set_io_str += " -mode ";
      set_io_str += dpin.mode_;
      if (dpin.valid_pt_row()) {
        set_io_str += "  -pt_row ";
        set_io_str += std::to_string(dpin.pt_row_+2);
      }

      if (tr >= 5 && ann_pin) {
        // debug annotation
        set_io_str += "  #  device: ";
        set_io_str += ann_pin->device_pin_name_;
        set_io_str += "  pt_row: ";
        set_io_str += std::to_string(ann_pin->pt_row_+2);
        set_io_str += "  isInput:";
        set_io_str += std::to_string(int(ann_pin->is_dev_input_));
        set_io_str += "  colM_dir:";
        const auto& bcd = csv.getBCD(ann_pin->pt_row_);
        set_io_str += bcd.str_colM_dir();
      }

      if (tr >= 4) {
        lprintf(" ... writing Input to pcf for  bump_pin= %s  pinName= %s\n",
                dpin.first().c_str(), pinName.c_str());
        lprintf("        set_io %s\n", set_io_str.c_str());
      }
      temp_out << "set_io " << set_io_str << endl;
      if (user_out.is_open()) {
        user_out << "set_io " << set_io_str << endl;
      }
    } else {
      lprintf("\n[Error] pin_c: failed getting device pin for input pin: %s\n", pinName.c_str());
      set_err_code("TOO_MANY_INPUTS");
      num_warnings_++;
      // if (not continue_on_errors)
      //  return false;
      if (i > 0 && itile_overlap_level_ < 5 && s_axi_inpQ.empty()) {
        // increase overlap level and re-try this pin
        itile_overlap_level_++;
        if (tr >= 3) {
          lprintf("NOTE: increased itile_overlap_level_ to %u on i=%u\n",
                  itile_overlap_level_, i);
        }
        i--;
      }
      lputs3();
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
      if (csv.hasCustomerInternalName(outName)) {
        lprintf("\t (CustIntName) hasCustomerInternalName( %s )\n", outName.c_str());
      }
    }
    assert(!outName.empty());
    dpin = get_available_device_pin(csv, false /*OUTPUT*/, outName, ann_pin);
    if (dpin.first().length()) {
      pinName = csv.bumpName2CustomerName(dpin.first());
      assert(!pinName.empty());

      set_io_str = user_design_outputs_[output_idx[i]];
      set_io_str.push_back(' ');
      set_io_str += pinName;
      set_io_str += " -mode ";
      set_io_str += dpin.mode_;
      if (dpin.valid_pt_row()) {
        set_io_str += "  -pt_row ";
        set_io_str += std::to_string(dpin.pt_row_+2);
      }

      if (tr >= 5 && ann_pin) {
        // debug annotation
        set_io_str += "  #  device: ";
        set_io_str += ann_pin->device_pin_name_;
        set_io_str += "  pt_row: ";
        set_io_str += std::to_string(ann_pin->pt_row_+2);
        set_io_str += "  isInput:";
        set_io_str += std::to_string(int(ann_pin->is_dev_input_));
        set_io_str += "  colM_dir:";
        const auto& bcd = csv.getBCD(ann_pin->pt_row_);
        set_io_str += bcd.str_colM_dir();
      }

      temp_out << "set_io " << set_io_str << endl;
      if (user_out.is_open()) {
        user_out << "set_io " << set_io_str << endl;
      }
    } else {
      lprintf("[Error] pin_c: failed getting device pin for output pin: %s\n", pinName.c_str());
      set_err_code("TOO_MANY_OUTPUTS");
      lputs3();
      num_warnings_++;
      // if (not continue_on_errors)
      //  return false;
      continue;
    }
  }

  auto_pcf_created_ = true;
  return true;
}

// static
void PinPlacer::shuffle_candidates(vector<int>& v) {
  static random_device rd;
  static mt19937 g(rd());
  std::shuffle(v.begin(), v.end(), g);
  return;
}

} // namespace pinc

