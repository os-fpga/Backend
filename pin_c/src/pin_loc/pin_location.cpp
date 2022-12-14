////////////////////////////////////////////////////////////////////////////////
// Important:                                                                 //
//    This is for rs internally - don't make this part public until a written //
//    approval is obtained from rapidsilicon open source review commitee      //
////////////////////////////////////////////////////////////////////////////////

#include "pin_location.h"

#include <random>
#include <set>

#include "blif_reader.h"
#include "cmd_line.h"
#include "csv_reader.h"
#include "nlohmann3_11_2/json.hpp"
#include "pcf_reader.h"
#include "rapid_csv_reader.h"
#include "xml_reader.h"

// will use std::filesystem when C++17 works everywhere
// #include <filesystem>
#include <sys/stat.h>
#include <unistd.h>

int pin_constrain_location(const pinc::cmd_line& cmd) {
  using namespace pinc;
  uint16_t tr = ltrace();
  if (tr >= 2) lputs("pin_constrain_location()");

  pin_location pl(cmd);

  // pl.get_cmd().print_options();

  if (!pl.reader_and_writer()) {
    return 1;
  }
  return 0;
}

namespace pinc {

using namespace std;

static bool fs_path_exists(const string& path) noexcept {
  if (path.length() < 1) return false;

  struct stat sb;
  if (::stat(path.c_str(), &sb)) return false;
  if (not(S_IFREG & sb.st_mode)) return false;

  return true;
}

static string USAGE_MSG_0 =
    "usage options: --xml PINMAP_XML --pcf PCF --blif BLIF --csv CSV_FILE "
    "[--assign_unconstrained_pins [random | in_define_order]] --output "
    "OUTPUT";  // for open source; and MPW1

static string USAGE_MSG_1 =
    "usage options: --pcf PCF (--blif BLIF | --port_info JSON) --csv CSV_FILE "
    "[--assign_unconstrained_pins [random | in_define_order]] --output "
    "OUTPUT";  // for rs internally, gemini;  user pcf is provided

static string USAGE_MSG_2 =
    "usage options: (--blif BLIF | --port_info JSON) --csv CSV_FILE "
    "[--assign_unconstrained_pins "
    "[random | in_define_order]] --output OUTPUT";  // for rs internall,
                                                    // gemini; no user pcf is
                                                    // provided

pin_location::pin_location(const cmd_line& cl) : cl_(cl) {
  temp_csv_file_name_ = "";
  temp_pcf_file_name_ = "";
  temp_os_pcf_file_name_ = "";

  // default pin assign method
  pin_assign_method_ = ASSIGN_IN_DEFINE_ORDER;
}

pin_location::~pin_location() {
  // remove temporarily generated file
  if (temp_csv_file_name_.size()) {
    if (remove(temp_csv_file_name_.c_str())) {
      CERROR << error_messages[CLOSE_FILE_FAILURE] << endl;
    }
  }

  if (temp_pcf_file_name_.size()) {
    if (remove(temp_pcf_file_name_.c_str())) {
      CERROR << error_messages[CLOSE_FILE_FAILURE] << endl;
    }
  }
}

bool pin_location::reader_and_writer() {
  const cmd_line& cmd = cl_;
  string xml_name = cmd.get_param("--xml");
  string csv_name = cmd.get_param("--csv");
  string pcf_name = cmd.get_param("--pcf");
  string blif_name = cmd.get_param("--blif");
  string json_name = cmd.get_param("--port_info");
  string output_name = cmd.get_param("--output");

  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2) {
    lputs("\n  pin_location::reader_and_writer()");
    ls << "        xml_name (--xml) : " << xml_name << endl;
    ls << "        csv_name (--csv) : " << csv_name << endl;
    ls << "        pcf_name (--pcf) : " << pcf_name << endl;
    ls << "      blif_name (--blif) : " << blif_name << endl;
    ls << " json_name (--port_info) : " << json_name << endl;
    ls << "    output_name (--output) : " << output_name << endl;
  }

  bool no_blif_no_json = blif_name.empty() && json_name.empty();

  // --1. check option selection
  string assign_method = cl_.get_param("--assign_unconstrained_pins");
  if (assign_method.length()) {
    if (tr >= 2) ls << "\t assign_method= " << assign_method << endl;
    if (assign_method == "random") {
      pin_assign_method_ = ASSIGN_IN_RANDOM;
    } else if (assign_method == "in_define_order") {
      pin_assign_method_ = ASSIGN_IN_DEFINE_ORDER;
    } else {
      CERROR << error_messages[INCORRECT_ASSIGN_PIN_METHOD] << endl;
      CERROR << error_messages[MISSING_IN_OUT_FILES] << endl
             << USAGE_MSG_0 << ", or" << endl
             << USAGE_MSG_1 << ", or" << endl
             << USAGE_MSG_2 << endl;
      return false;
    }
  }

  // it is possible that no pcf is provided by user
  // in such case we need to provide a pcf to prevent vpr using all pins freely

  // usage 0: classical flow form opensource community - device MPW1
  bool usage_requirement_0 = !(xml_name.empty() || csv_name.empty() ||
                               no_blif_no_json || output_name.empty());

  // usage 1: rs only - specify csv (which contains coordinate data described in
  // pinmap xml info), pcf, blif, and output files
  bool usage_requirement_1 = !(csv_name.empty() || pcf_name.empty() ||
                               no_blif_no_json || output_name.empty()) &&
                             xml_name.empty();

  // usage 2: rs only - user dose not provide a pcf (this is really rare in
  // meaningfull on-board implementation, but could be true in test or
  // evaluation in lab)
  //          in such case, we need to make sure a constraint file is properly
  //          generated to guide VPR use LEGAL pins only.
  bool usage_requirement_2 =
      !(csv_name.empty() || no_blif_no_json || output_name.empty()) &&
      pcf_name.empty();

  if (tr >= 2) {
    ls << "\t usage_requirement_0 : " << boolalpha << usage_requirement_0
       << endl;
    ls << "\t usage_requirement_1 : " << boolalpha << usage_requirement_1
       << endl;
    ls << "\t usage_requirement_2 : " << boolalpha << usage_requirement_2
       << endl;
  }

  if (usage_requirement_0) {  // generate new csv file with information from xml
                              // and csv for cassical flow
    if (tr >= 2) {
      ls << "(usage_requirement_0)\n"
         << "generate new csv file with info from xml and csv for cassical flow"
         << endl;
    }
    if (!generate_csv_file_for_os_flow()) {
      CERROR << error_messages[GENERATE_CSV_FILE_FOR_OS_FLOW] << endl;
      return false;
    }
    // in os mode, the pcf does not contains any "-mode"
    // we need to generate a temp pcf file with "-mode" option, which selects
    // mode "Mode_GPIO"
    string pcf_file_name = cl_.get_param("--pcf");
    if (pcf_file_name.size()) {
      if (!convert_pcf_file_for_os_flow(pcf_file_name)) {
        CERROR << error_messages[GENERATE_PCF_FILE_FOR_OS_FLOW] << endl;
        return false;
      }
    }
  } else if ((!usage_requirement_1) && (!usage_requirement_2)) {
    CERROR << error_messages[MISSING_IN_OUT_FILES] << endl
           << USAGE_MSG_0 << ", or" << endl
           << USAGE_MSG_1 << ", or" << endl
           << USAGE_MSG_2 << endl;
    return false;
  }

  // --2. read port info from user design (from .blif or from port_info.json)
  if (!read_user_design()) {
    CERROR << error_messages[INPUT_DESIGN_PARSE_ERROR] << endl;
    return false;
  }

  // --3. read info (pin-table) from csv file in new RS format
  rapidCsvReader rs_csv_rd;
  if (!read_csv_file(rs_csv_rd)) {
    CERROR << error_messages[PIN_MAP_CSV_PARSE_ERROR] << endl;
    return false;
  }

  // usage 2: if no user constraint is provided, created a temp one
  if (usage_requirement_2 || (usage_requirement_0 && pcf_name == "")) {
    if (!create_temp_constrain_file(rs_csv_rd)) {
      CERROR << error_messages[FAIL_TO_CREATE_TEMP_PCF] << endl;
      return false;
    }
  }

  // --4. read user constraint
  if (!read_user_pinloc_constrain_file()) {
    CERROR << error_messages[PIN_CONSTRAINT_PARSE_ERROR] << endl;
    return false;
  }

  // --5. create .place file
  if (!create_place_file(rs_csv_rd)) {
    // error messages will be issued in callee
    if (tr) {
      ls << " (EE) !create_place_file(rs_csv_rd)" << endl;
      cerr << "ERROR: create_place_file() failed" << endl;
    }
    return false;
  }

  // -- done successfully
  if (tr >= 2)
    ls << "done: pin_location::reader_and_writer() done successfully" << endl;
  return true;
}

// for open source flow
// process pinmap xml and csv file and gerate the csv file like gemini one (with
// only partial information for pin constraint)
bool pin_location::generate_csv_file_for_os_flow() {
  uint16_t tr = ltrace();
  if (tr >= 2) {
    if (tr >= 3) lputs("pin_location::generate_csv_file_for_os_flow()");
    cout << "Generateing new csv" << endl;
  }

  XmlReader rd_xml;
  if (!rd_xml.read_xml(cl_.get_param("--xml"))) {
    CERROR << error_messages[PIN_LOC_XML_PARSE_ERROR] << endl;
    return false;
  }
  std::map<string, PinMappingData> xml_port_map = rd_xml.get_port_map();
  CsvReader rd_csv;
  if (!rd_csv.read_csv(cl_.get_param("--csv"))) {
    CERROR << error_messages[PIN_MAP_CSV_PARSE_ERROR] << endl;
    return false;
  }
  map<string, string> csv_port_map = rd_csv.get_port_map();

  temp_csv_file_name_ = ".temp_file_csv";
  std::ofstream outfile;
  outfile.open(temp_csv_file_name_, std::ifstream::out | std::ifstream::binary);
  if (outfile.fail()) {
    // no error message here, caller is to show GENERATE_CSV_FILE_FOR_OS_FLOW
    // message
    return false;
  }
  // we add "Bump/Pin Name" in the generated temp file to make the downstream
  // rapid csv reader work the value in "Bump/Pin Name" column is same as
  // "IO_tile_pin" column
  outfile << "Group,Bump/Pin "
             "Name,IO_tile_pin,IO_tile_pin_x,IO_tile_pin_y,IO_tile_pin_z,Mode_"
             "GPIO"
          << endl;

  std::map<string, string>::iterator itr = csv_port_map.begin();
  itr++;  // skip the first pair "mapped_pin" = ""port_name"
  for (; itr != csv_port_map.end(); itr++) {
    PinMappingData pinMapData = xml_port_map.at(itr->second);
    outfile << "GBOX GPIO," /* fake group name */ << itr->first << ","
            << itr->second << "," << std::to_string(pinMapData.get_x()) << ","
            << pinMapData.get_y() << "," << pinMapData.get_z() << ",Y" << endl;
  }
  outfile.close();

  return true;
}

bool pin_location::convert_pcf_file_for_os_flow(string pcf_file_name) {
  PcfReader rd_pcf;
  if (!rd_pcf.read_os_pcf(pcf_file_name)) {
    CERROR << error_messages[PIN_CONSTRAINT_PARSE_ERROR] << endl;
    return false;
  }
  vector<vector<string>> pcf_pin_cstr = rd_pcf.get_commands();
  temp_os_pcf_file_name_ = ".temp_os_file_pcf";
  std::ofstream outfile;
  outfile.open(temp_os_pcf_file_name_,
               std::ifstream::out | std::ifstream::binary);
  if (outfile.fail()) {
    // no error message here, caller is to show GENERATE_CSV_FILE_FOR_OS_FLOW
    // message
    return false;
  }
  for (uint i = 0; i < pcf_pin_cstr.size(); i++) {
    uint j = 0;
    string command = pcf_pin_cstr[i][j];
    for (j++; j < pcf_pin_cstr[i].size(); j++) {
      command += string(" ") + pcf_pin_cstr[i][j];
    }
    command += string(" -mode Mode_GPIO");
    outfile << command << endl;
  }
  outfile.close();

  return true;
}

bool pin_location::read_user_design() {
  uint16_t tr = ltrace();
  if (tr >= 2)
    lprintf(
        "pin_location::read_user_design() __ getting port info from "
        "blif/json\n");

  string port_info_fn = cl_.get_param("--port_info");
  // filesystem::path path;
  string path;
  ifstream json_ifs;

  if (port_info_fn.length() > 0) {
    path = port_info_fn;
    if (fs_path_exists(path)) {
      json_ifs.open(port_info_fn);
      if (!json_ifs.is_open())
        lprintf("WARNING: could not open port info file %s => using blif\n",
                port_info_fn.c_str());
    } else {
      lprintf("WARNING: port info file %s does not exist => using blif\n",
              port_info_fn.c_str());
    }
  } else {
    if (tr >= 1) lprintf("port_info cmd option not specified => using blif\n");
  }

  if (json_ifs.is_open()) {
    if (tr >= 2) lprintf("... reading %s\n", port_info_fn.c_str());
    if (!read_port_info(json_ifs, user_design_inputs_, user_design_outputs_)) {
      CERROR << " failed reading " << port_info_fn << endl;
      return false;
    }
    json_ifs.close();
  } else {
    string blif_fn = cl_.get_param("--blif");
    if (tr >= 2) lprintf("... reading %s\n", blif_fn.c_str());
    path = blif_fn;
    if (not fs_path_exists(path)) {
      lprintf("WARNING: blif file %s does not exist\n", blif_fn.c_str());
    }
    BlifReader rd_blif;
    if (!rd_blif.read_blif(blif_fn)) {
      CERROR << error_messages[INPUT_DESIGN_PARSE_ERROR] << endl;
      return false;
    }
    user_design_inputs_ = rd_blif.get_inputs();
    user_design_outputs_ = rd_blif.get_outputs();
  }

  if (tr >= 2) {
    lprintf(
        "DONE read_user_design()  user_design_inputs_.size()= %u  "
        "user_design_outputs_.size()= %u\n",
        uint(user_design_inputs_.size()), uint(user_design_outputs_.size()));
  }

  return true;
}

bool pin_location::read_user_pinloc_constrain_file() {
  uint16_t tr = ltrace();
  if (tr >= 2)
    lprintf(
        "pin_location::read_user_pinloc_constrain_file() __ Reading user place "
        "constrain\n");

  string pcf_file_name;
  if (temp_os_pcf_file_name_.size()) {
    // use generated temp pcf for open source flow
    pcf_file_name = temp_os_pcf_file_name_;
  } else {
    pcf_file_name = cl_.get_param("--pcf");
  }

  PcfReader rd_pcf;
  if (!rd_pcf.read_pcf(pcf_file_name)) {
    CERROR << error_messages[PIN_CONSTRAINT_PARSE_ERROR] << endl;
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

bool pin_location::create_place_file(rapidCsvReader& rs_csv_reader) {
  string out_fn = cl_.get_param("--output");
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2) {
    ls << "pin_location::create_place_file() __ Creating place file\n"
          "  cl_.get_param(--output) : "
       << out_fn << endl;
  }

  std::ofstream out_file;
  out_file.open(out_fn);
  if (out_file.fail()) {
    // no error message here, caller is to show failure message
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

  for (const vector<string>& pcf_cmd : pcf_pin_cmds_) {
    // only support io and clock for now
    if (pcf_cmd[0] != "set_io" && pcf_cmd[0] != "set_clk") continue;
    gbox_pin_name.clear();
    search_name.clear();

    const string& user_design_pin_name = pcf_cmd[1];
    const string& device_pin_name = pcf_cmd[2];

    if (tr >= 4) {
      logVec(pcf_cmd, "    cur pcf_cmd:");
      ls << "\t user_design_pin_name=  " << user_design_pin_name
         << "  -->  device_pin_name=  " << device_pin_name << endl;
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
      CERROR << error_messages[CONSTRAINED_PORT_NOT_FOUND] << ": <"
             << user_design_pin_name << ">" << endl;
      out_file << "\n=== Error happened, .place file is incomplete\n"
               << "=== ERROR:" << error_messages[CONSTRAINED_PORT_NOT_FOUND]
               << ": <" << user_design_pin_name
               << ">  device_pin_name: " << device_pin_name << "\n\n";
      out_file.close();
      return false;
    }
    if (!rs_csv_reader.has_io_pin(device_pin_name)) {
      CERROR << error_messages[CONSTRAINED_PIN_NOT_FOUND] << ": <"
             << device_pin_name << ">" << endl;
      out_file << "\n=== Error happened, .place file is incomplete\n"
               << "=== ERROR:" << error_messages[CONSTRAINED_PORT_NOT_FOUND]
               << ": <" << user_design_pin_name
               << ">  device_pin_name: " << device_pin_name << "\n\n";
      out_file.close();
      return false;
    }

    if (!constrained_user_pins.count(user_design_pin_name)) {
      constrained_user_pins.insert(user_design_pin_name);
    } else {
      CERROR << error_messages[RE_CONSTRAINED_PORT] << ": <"
             << user_design_pin_name << ">" << endl;
      out_file.close();
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
      CERROR << error_messages[OVERLAP_PIN_IN_CONSTRAINT] << ": <"
             << device_pin_name << ">" << endl;
      out_file.close();
      return false;
    }

    // look for coordinates and write constrain
    if (is_out_pin) {
      out_file << "out:";
      if (tr >= 4) ls << "out:";
    }

    string mode = pcf_cmd[4];
    out_file << user_design_pin_name << '\t';
    out_file.flush();

    XYZ xyz = rs_csv_reader.get_pin_xyz_by_bump_name(mode, device_pin_name,
                                                     gbox_pin_name);
    assert(xyz.valid());

    out_file << xyz.x_ << '\t' << xyz.y_ << '\t' << xyz.z_ << endl;
    out_file.flush();

    if (tr >= 4) {
      ls << xyz.x_ << '\t' << xyz.y_ << '\t' << xyz.z_ << endl;
      ls.flush();
    }
  }

  if (tr >= 2) ls << "\nwritten " << out_fn << endl;

  return true;

#if 0
  // assign other un-constrained user pins (if any) to legal io tile pin in fpga
  if (constrained_user_pins.size() <
      (user_design_inputs_.size() + user_design_outputs_.size())) {
    vector<int> left_available_device_pin_idx;
    collect_left_available_device_pins(
        constrained_device_pins, left_available_device_pin_idx, rs_csv_reader);
    if (pin_assign_method_ == ASSIGN_IN_RANDOM) {
      shuffle_candidates(left_available_device_pin_idx);
    }
    int assign_pin_idx = 0;
    for (auto user_input_pin_name : user_design_inputs_) {
      if (constrained_user_pins.find(user_input_pin_name) ==
          constrained_user_pins.end()) { // input pins not specified in pcf
        out_file << user_input_pin_name << "\t"
                 << std::to_string(rs_csv_reader.get_pin_x_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << "\t"
                 << std::to_string(rs_csv_reader.get_pin_y_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << "\t"
                 << std::to_string(rs_csv_reader.get_pin_z_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << endl;
        assign_pin_idx++;
      }
    }
    for (auto user_output_pin_name : user_design_outputs_) {
      if (constrained_user_pins.find(user_output_pin_name) ==
          constrained_user_pins.end()) { // output pins not specified in pcf
        out_file << "out:" << user_output_pin_name << "\t"
                 << std::to_string(rs_csv_reader.get_pin_x_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << "\t"
                 << std::to_string(rs_csv_reader.get_pin_y_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << "\t"
                 << std::to_string(rs_csv_reader.get_pin_z_by_pin_idx(
                        left_available_device_pin_idx[assign_pin_idx]))
                 << endl;
        assign_pin_idx++;
      }
    }
  }
#endif  // 0
}

bool pin_location::read_csv_file(rapidCsvReader& rs_csv_rd) {
  uint16_t tr = ltrace();
  if (tr >= 2) lputs("pin_location::read_csv_file() __ Reading new csv");

  string csv_name;
  if (temp_csv_file_name_.size()) {
    // use generated temp csv for open source flow
    csv_name = temp_csv_file_name_;
  } else {
    csv_name = cl_.get_param("--csv");
  }

  if (tr >= 2) lprintf("  cvs_name= %s\n", csv_name.c_str());

  {
    std::ifstream stream;
    stream.open(csv_name, std::ios::binary);
    if (stream.fail()) {
      CERROR << error_messages[OPEN_FILE_FAILURE] << endl;
      return false;
    }
    stream.close();
  }

  string check_csv = cl_.get_param("--check_csv");
  bool check = false;
  if (check_csv == "true") {
    check = true;
    if (tr >= 2) lputs("NOTE: check_csv == True");
  }
  if (!rs_csv_rd.read_csv(csv_name, check)) {
    CERROR << error_messages[PIN_MAP_CSV_PARSE_ERROR] << endl;
    return false;
  }

  return true;
}

bool pin_location::get_available_bump_pin(
    rapidCsvReader& rs_csv_rd, std::pair<string, string>& bump_pin_and_mode,
    PortDirection port_direction) {
  string bump_pin_name;

  for (uint i = rs_csv_rd.start_position_; i < rs_csv_rd.bump_pin_name_.size();
       i++) {
    bump_pin_name = rs_csv_rd.bump_pin_name_[i];
    if (used_bump_pins_.find(bump_pin_name) == used_bump_pins_.end()) {
      for (uint j = 0; j < rs_csv_rd.mode_names_.size(); j++) {
        string mode_name = rs_csv_rd.mode_names_[j];
        vector<string> mode_data = rs_csv_rd.modes_map_[mode_name];
        if (port_direction == INPUT) {
          if (is_input_mode(mode_name)) {
            for (uint k = rs_csv_rd.start_position_; k < mode_data.size();
                 k++) {
              if ((mode_data[k] == "Y") &&
                  (bump_pin_name == rs_csv_rd.bump_pin_name_[k])) {
                bump_pin_and_mode.first = bump_pin_name;
                bump_pin_and_mode.second = mode_name;
                used_bump_pins_.insert(bump_pin_name);
                return true;
              }
            }
          } else {
            continue;
          }
        } else if (port_direction == OUTPUT) {
          if (is_output_mode(mode_name)) {
            for (uint k = rs_csv_rd.start_position_; k < mode_data.size();
                 k++) {
              if ((mode_data[k] == "Y") &&
                  (bump_pin_name == rs_csv_rd.bump_pin_name_[k])) {
                bump_pin_and_mode.first = bump_pin_name;
                bump_pin_and_mode.second = mode_name;
                used_bump_pins_.insert(bump_pin_name);
                return true;
              }
            }
          } else {
            continue;
          }
        }
      }
    }
  }

  return false;
}

bool pin_location::create_temp_constrain_file(rapidCsvReader& rs_csv_rd) {
  // create a temp constraint file and internally specified that to down-flow
  // functions
  string key = "--pcf";
  temp_pcf_file_name_ = ".temp_pcf";
  cl_.set_param_value(key, temp_pcf_file_name_);

  if (ltrace() >= 2)
    lprintf("create_temp_constrain_file() : %s\n", temp_pcf_file_name_.c_str());

  vector<int> input_idx;
  vector<int> output_idx;
  for (uint i = 0; i < user_design_inputs_.size(); i++) {
    input_idx.push_back(i);
  }
  for (uint i = 0; i < user_design_outputs_.size(); i++) {
    output_idx.push_back(i);
  }
  if (pin_assign_method_ == ASSIGN_IN_RANDOM) {
    shuffle_candidates(input_idx);
    shuffle_candidates(output_idx);
  }

  std::pair<string, string> bump_pin_and_mode;
  std::ofstream outfile;
  outfile.open(temp_pcf_file_name_, std::ifstream::out | std::ifstream::binary);
  if (outfile.fail()) {
    return false;
  }

  // if user specified "--write_pcf", this file will be generated there with
  // same contents
  string out_pcf_name = cl_.get_param("--write_pcf");
  std::ofstream out_pcf_file;
  bool write_out_pcf = false;
  if (out_pcf_name.size()) {
    out_pcf_file.open(out_pcf_name, std::ifstream::out | std::ifstream::binary);
    if (out_pcf_file.fail()) {
      // error message is printed at caller side
      return false;
    }
    write_out_pcf = true;
  }

  for (uint i = 0; i < input_idx.size(); i++) {
    if (get_available_bump_pin(rs_csv_rd, bump_pin_and_mode, INPUT)) {
      outfile << "set_io " << user_design_inputs_[input_idx[i]] << " "
              << bump_pin_and_mode.first << " -mode "
              << bump_pin_and_mode.second << endl;
      if (write_out_pcf) {
        out_pcf_file << "set_io " << user_design_inputs_[input_idx[i]] << " "
                     << bump_pin_and_mode.first << " -mode "
                     << bump_pin_and_mode.second << endl;
      }
    } else {
      CERROR << error_messages[PIN_SOURCE_NO_SURFFICENT] << endl;
      outfile.close();
      if (write_out_pcf) {
        out_pcf_file.close();
      }
      return false;
    }
  }
  for (uint i = 0; i < user_design_outputs_.size(); i++) {
    if (get_available_bump_pin(rs_csv_rd, bump_pin_and_mode, OUTPUT)) {
      outfile << "set_io " << user_design_outputs_[output_idx[i]] << " "
              << bump_pin_and_mode.first << " -mode "
              << bump_pin_and_mode.second << endl;
      if (write_out_pcf) {
        out_pcf_file << "set_io " << user_design_outputs_[output_idx[i]] << " "
                     << bump_pin_and_mode.first << " -mode "
                     << bump_pin_and_mode.second << endl;
      }
    } else {
      CERROR << error_messages[PIN_SOURCE_NO_SURFFICENT] << endl;
      outfile.close();
      if (write_out_pcf) {
        out_pcf_file.close();
      }
      return false;
    }
  }
  outfile.close();
  if (write_out_pcf) {
    out_pcf_file.close();
  }
  return true;
}

// static
void pin_location::shuffle_candidates(vector<int>& v) {
  static random_device rd;
  static mt19937 g(rd());
  std::shuffle(v.begin(), v.end(), g);
  return;
}

/*
void pin_location::collect_left_available_device_pins(
    set<string> &constrained_device_pins,
    vector<int> &left_available_device_pin_idx, rapidCsvReader &rs_csv_reader) {
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

bool pin_location::is_input_mode(const string& mode_name) const {
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

bool pin_location::is_output_mode(const string& mode_name) const {
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

static bool read_json_ports(const nlohmann::ordered_json& from,
                            vector<pin_location::PortInfo>& ports) {
  ports.clear();

  nlohmann::ordered_json portsObj = from["ports"];
  if (!portsObj.is_array()) return false;

  uint16_t tr = ltrace();
  auto& ls = lout();

  if (tr >= 3) ls << " .... portsObj.size() = " << portsObj.size() << endl;

  string s;
  for (auto I = portsObj.cbegin(); I != portsObj.cend(); ++I) {
    const nlohmann::ordered_json& obj = *I;
    if (obj.is_array()) {
      ls << " (WW) unexpected json array..." << endl;
      continue;
    }

    // 1. check size and presence of 'name'
    if (tr >= 4) ls << " ...... obj.size() " << obj.size() << endl;
    if (obj.size() == 0) {
      ls << " (WW) unexpected json object size..." << endl;
      continue;
    }
    if (not obj.contains("name")) {
      ls << " (WW) expected obj.name string..." << endl;
      continue;
    }
    if (not obj["name"].is_string()) {
      ls << " (WW) expected obj.name string..." << endl;
      continue;
    }

    ports.emplace_back();

    // 2. read 'name'
    pin_location::PortInfo& last = ports.back();
    last.name_ = obj["name"];
    if (tr >= 4) ls << " ........ last.name_ " << last.name_ << endl;

    // 3. read 'direction'
    if (obj.contains("direction")) {
      s = strToLower(obj["direction"]);
      if (s == "input")
        last.dir_ = pin_location::PortDir::Input;
      else if (s == "output")
        last.dir_ = pin_location::PortDir::Output;
    }

    // 4. read 'type'
    if (obj.contains("type")) {
      s = strToLower(obj["type"]);
      if (s == "wire")
        last.type_ = pin_location::PortType::Wire;
      else if (s == "reg")
        last.type_ = pin_location::PortType::Reg;
    }

    // 5. read 'range'
    if (obj.contains("range")) {
      const auto& rangeObj = obj["range"];
      if (rangeObj.size() == 2 && rangeObj.contains("lsb") &&
          rangeObj.contains("msb")) {
        if (tr >= 5) ls << " ........ has range..." << endl;
        const auto& lsb = rangeObj["lsb"];
        const auto& msb = rangeObj["msb"];
        if (lsb.is_number_integer() && msb.is_number_integer()) {
          int l = lsb.get<int>();
          int m = msb.get<int>();
          if (l >= 0 && m >= 0) {
            if (tr >= 4)
              ls << " ........ has valid range  l= " << l << "  m= " << m
                 << endl;
            last.range_.set(l, m);
          }
        }
      }
    }

    // 6. read 'define_order'
    if (obj.contains("define_order")) {
      s = strToLower(obj["define_order"]);
      if (s == "lsb_to_msb")
        last.order_ = pin_location::Order::LSB_to_MSB;
      else if (s == "msb_to_lsb")
        last.order_ = pin_location::Order::MSB_to_LSB;
      if (tr >= 4 && last.hasOrder())
        ls << " ........ has define_order: " << s << endl;
    }
  }

  return true;
}

// static
bool pin_location::read_port_info(std::ifstream& ifs, vector<string>& inputs,
                                  vector<string>& outputs) {
  inputs.clear();
  outputs.clear();
  if (!ifs.is_open()) return false;

  uint16_t tr = ltrace();
  auto& ls = lout();

  nlohmann::ordered_json rootObj;

  try {
    ifs >> rootObj;
  } catch (...) {
    CERROR << "(EE) Failed json input stream reading" << endl;
    return false;
  }

  size_t root_sz = rootObj.size();
  if (tr >= 2) {
    ls << " rootObj.size() = " << root_sz
       << "  rootObj.is_array() : " << std::boolalpha << rootObj.is_array()
       << endl;
  }

  if (root_sz == 0) {
    CERROR << "(EE) root_sz == 0" << endl;
    return false;
  }

  std::vector<PortInfo> ports;
  if (rootObj.is_array())
    read_json_ports(rootObj[0], ports);
  else
    read_json_ports(rootObj, ports);

  if (tr >= 2) ls << " got ports.size()= " << ports.size() << endl;

  if (ports.empty()) return false;

  uint num_inp = 0, num_out = 0;
  for (const PortInfo& p : ports) {
    if (p.isInput())
      num_inp++;
    else if (p.isOutput())
      num_out++;
  }
  inputs.reserve(num_inp);
  outputs.reserve(num_out);

  for (const PortInfo& p : ports) {
    if (p.isInput()) {
      if (p.name_.length())
        inputs.push_back(p.name_);
      else
        ls << "WARNING: got empty name in port_info" << endl;
    } else if (p.isOutput()) {
      if (p.name_.length())
        outputs.push_back(p.name_);
      else
        ls << "WARNING: got empty name in port_info" << endl;
    }
  }

  if (tr >= 2)
    ls << " got " << inputs.size() << " inputs and " << outputs.size()
       << " outputs" << endl;

  return true;
}

}  // namespace pinc
