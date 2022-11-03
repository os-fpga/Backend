////////////////////////////////////////////////////////////////////////////////
// Important:                                                                 //
//    This is for rs internally - don't make this part public until a written //
//    approval is obtained from rapidsilicon open source review commitee      //
////////////////////////////////////////////////////////////////////////////////

#include "pin_location.h"
#include "blif_reader.h"
#include "cmd_line.h"
#include "csv_reader.h"
#include "pcf_reader.h"
#include "rapid_csv_reader.h"
#include "xml_reader.h"
#include <algorithm>
#include <random>
#include <set>

const string USAGE_MSG_0 =
    "usage options: --xml PINMAP_XML --pcf PCF --blif BLIF --csv CSV_FILE "
    "[--assign_unconstrained_pins [random | in_define_order]] --output "
    "OUTPUT"; // for open source; and MPW1
const string USAGE_MSG_1 =
    "usage options: --pcf PCF --blif BLIF --csv CSV_FILE "
    "[--assign_unconstrained_pins [random | in_define_order]] --output "
    "OUTPUT"; // for rs internally, gemini;  user pcf is provided
const string USAGE_MSG_2 =
    "usage options: --blif BLIF --csv CSV_FILE [--assign_unconstrained_pins "
    "[random | in_define_order]] --output OUTPUT"; // for rs internall,
                                                   // gemini; no user pcf
                                                   // is provided
const cmd_line &pin_location::get_cmd() const { return cl_; }
bool pin_location::reader_and_writer() {
  cmd_line cmd = cl_;
  string xml_name = cmd.get_param("--xml");
  string csv_name = cmd.get_param("--csv");
  string pcf_name = cmd.get_param("--pcf");
  string blif_name = cmd.get_param("--blif");
  string output_name = cmd.get_param("--output");

  // check option selection
  string assign_method = cl_.get_param("--assign_unconstrained_pins");
  if (assign_method.length()) {
    if (assign_method == "random") {
      pin_assign_method_ = ASSIGN_IN_RANDOM;
    } else if (assign_method == "in_define_order") {
      pin_assign_method_ = ASSIGN_IN_DEFINE_ORDER;
    } else {
      CERROR << error_messages[INCORRECT_ASSIGN_PIN_METHOD] << endl;
      CERROR << error_messages[MISSING_IN_OUT_FILES] << std::endl
             << USAGE_MSG_0 << ", or" << endl
             << USAGE_MSG_1 << ", or" << endl
             << USAGE_MSG_2 << endl;
      return false;
    }
  }

  // it is possible that no pcf is provided by user
  // in such case we need to provide a pcf to prevent vpr using all pins freely

  // usage 0: classical flow form opensource community - device MPW1
  bool usage_requirement_0 = !((xml_name == "") || (csv_name == "") ||
                               (blif_name == "") || (output_name == ""));
  // usage 1: rs only - specify csv (which contains coordinate data described in
  // pinmap xml info), pcf, blif, and output files
  bool usage_requirement_1 = (!((csv_name == "") || (pcf_name == "") ||
                                (blif_name == "") || (output_name == ""))) &&
                             (xml_name == "");
  // usage 2: rs only - user dose not provide a pcf (this is really rare in
  // meaningfull on-board implementation, but could be true in test or
  // evaluation in lab)
  //          in such case, we need to make sure a constraint file is properly
  //          generated to guide VPR use LEGAL pins only.
  bool usage_requirement_2 =
      (!((csv_name == "") || (blif_name == "") || (output_name == ""))) &&
      (pcf_name == "");
  if (usage_requirement_0) { // generate new csv file with information from xml
                             // and csv for cassical flow
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
    CERROR << error_messages[MISSING_IN_OUT_FILES] << std::endl
           << USAGE_MSG_0 << ", or" << endl
           << USAGE_MSG_1 << ", or" << endl
           << USAGE_MSG_2 << endl;
    return false;
  }

  // read port info from user design
  if (!read_user_design()) {
    CERROR << error_messages[INPUT_DESIGN_PARSE_ERROR] << std::endl;
    return false;
  }

  // read info from csv file in new RS format
  rapidCsvReader rs_csv_rd;
  if (!read_csv_file(rs_csv_rd)) {
    CERROR << error_messages[PIN_MAP_CSV_PARSE_ERROR] << std::endl;
    return false;
  }

  // usage 2: if no user constraint is provided, created a temp one
  if (usage_requirement_2 || (usage_requirement_0 && pcf_name == "")) {
    if (!create_temp_constrain_file(rs_csv_rd)) {
      CERROR << error_messages[FAIL_TO_CREATE_TEMP_PCF] << std::endl;
      return false;
    }
  }

  // read user constraint
  if (!read_user_pin_locatin_constrain_file()) {
    CERROR << error_messages[PIN_CONSTRAINT_PARSE_ERROR] << std::endl;
    return false;
  }

  if (!create_place_file(rs_csv_rd)) {
    // error messages will be issued in callee
    return false;
  }

  // done successfully
  return true;
}

// for open source flow
// process pinmap xml and csv file and gerate the csv file like gemini one (with
// only partial information for pin constraint)
bool pin_location::generate_csv_file_for_os_flow() {
#ifdef DEBUG_PIN_C
  cout << "Generateing new csv" << endl;
#endif
  XmlReader rd_xml;
  if (!rd_xml.read_xml(cl_.get_param("--xml"))) {
    CERROR << error_messages[PIN_LOC_XML_PARSE_ERROR] << std::endl;
    return false;
  }
  std::map<std::string, PinMappingData> xml_port_map = rd_xml.get_port_map();
  CsvReader rd_csv;
  if (!rd_csv.read_csv(cl_.get_param("--csv"))) {
    CERROR << error_messages[PIN_MAP_CSV_PARSE_ERROR] << std::endl;
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
  itr++; // skip the first pair "mapped_pin" = ""port_name"
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
    CERROR << error_messages[PIN_CONSTRAINT_PARSE_ERROR] << std::endl;
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
  for (unsigned int i = 0; i < pcf_pin_cstr.size(); i++) {
    unsigned int j = 0;
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
#ifdef DEBUG_PIN_C
  cout << "Reading user design" << endl;
#endif
  BlifReader rd_blif;
  if (!rd_blif.read_blif(cl_.get_param("--blif"))) {
    CERROR << error_messages[INPUT_DESIGN_PARSE_ERROR] << std::endl;
    return false;
  }
  user_design_inputs_ = rd_blif.get_inputs();
  user_design_outputs_ = rd_blif.get_outputs();
  return true;
}

bool pin_location::read_user_pin_locatin_constrain_file() {
#ifdef DEBUG_PIN_C
  cout << "Reading user place constrain" << endl;
#endif

  string pcf_file_name;
  if (temp_os_pcf_file_name_
          .size()) { // use generated temp pcf for open source flow
    pcf_file_name = temp_os_pcf_file_name_;
  } else {
    pcf_file_name = cl_.get_param("--pcf");
  }
  PcfReader rd_pcf;
  if (!rd_pcf.read_pcf(pcf_file_name)) {
    CERROR << error_messages[PIN_CONSTRAINT_PARSE_ERROR] << std::endl;
    return false;
  }
  pcf_pin_cstr_ = rd_pcf.get_commands();
  return true;
}

bool pin_location::create_place_file(rapidCsvReader &rs_csv_reader) {
#ifdef DEBUG_PIN_C
  cout << "Creating place file" << endl;
#endif
  std::ofstream out_file;
  out_file.open(cl_.get_param("--output"));
  if (out_file.fail()) {
    // no error message here, caller is to show failure message
    return false;
  }
  out_file << "#Block Name   x   y   z\n";
  out_file << "#------------ --  --  -\n";

  // parser each constraint line in constrain file, generate place location
  // accordingly
  std::set<std::string> constrained_user_pins,
      constrained_device_pins; // for sanity check
  for (auto pin_cstr_v : pcf_pin_cstr_) {
    // only support io and clock for now
    if ((pin_cstr_v[0] != "set_io") && (pin_cstr_v[0] != "set_clk")) {
      continue;
    }

    string user_design_pin_name = pin_cstr_v[1];
    string device_pin_name = pin_cstr_v[2];
    string gbox_pin_name = std::string("");
    for (int i = 0; i < pin_cstr_v.size(); i++) {
      if (pin_cstr_v[i] == "-internal_pin") {
        gbox_pin_name = pin_cstr_v[++i];
        break;
      }
    }
    bool is_in_pin =
        std::find(user_design_inputs_.begin(), user_design_inputs_.end(),
                  user_design_pin_name) != user_design_inputs_.end();
    bool is_out_pin = false;
    if (!is_in_pin) {
      is_out_pin =
          std::find(user_design_outputs_.begin(), user_design_outputs_.end(),
                    user_design_pin_name) != user_design_outputs_.end();
    }

    // sanity check
    if (!(is_in_pin || is_out_pin)) {
      CERROR << error_messages[CONSTRAINED_PORT_NOT_FOUND] << ": <"
             << user_design_pin_name << ">" << endl;
      out_file.close();
      return false;
    }
    if (!rs_csv_reader.find_io_pin(device_pin_name)) {
      CERROR << error_messages[CONSTRAINED_PIN_NOT_FOUND] << ": <"
             << device_pin_name << ">" << std::endl;
      out_file.close();
      return false;
    }

    if (constrained_user_pins.find(user_design_pin_name) ==
        constrained_user_pins.end()) {
      constrained_user_pins.insert(user_design_pin_name);
    } else {
      CERROR << error_messages[RE_CONSTRAINED_PORT] << ": <"
             << user_design_pin_name << ">" << std::endl;
      out_file.close();
      return false;
    }
    // use device_pin_name + " " + gbox_pin_name as search name to check overlap
    // pin in constraint
    std::string search_name =
        device_pin_name + std::string(" ") + gbox_pin_name;
    if (constrained_device_pins.find(search_name) ==
        constrained_device_pins.end()) {
      constrained_device_pins.insert(search_name);
    } else {
      CERROR << error_messages[OVERLAP_PIN_IN_CONSTRAINT] << ": <"
             << device_pin_name << ">" << std::endl;
      out_file.close();
      return false;
    }

    // look for coordinates and write constrain
    if (is_out_pin) {
      out_file << "out:";
    }
    string mode = pin_cstr_v[4];
    out_file << user_design_pin_name << "\t"
             << std::to_string(rs_csv_reader.get_pin_x_by_bump_name(
                    mode, device_pin_name, gbox_pin_name))
             << "\t"
             << std::to_string(rs_csv_reader.get_pin_y_by_bump_name(
                    mode, device_pin_name, gbox_pin_name))
             << "\t"
             << std::to_string(rs_csv_reader.get_pin_z_by_bump_name(
                    mode, device_pin_name, gbox_pin_name))
             << endl;
  }

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
#endif

  out_file.close();
  return true;
}

bool pin_location::read_csv_file(rapidCsvReader &rs_csv_rd) {
#ifdef DEBUG_PIN_C
  cout << "Reading new csv" << endl;
#endif
  string csv_name;
  if (temp_csv_file_name_
          .size()) { // use generated temp csv for open source flow
    csv_name = temp_csv_file_name_;
  } else {
    csv_name = cl_.get_param("--csv");
  }
  std::ifstream stream;
  stream.open(csv_name, std::ios::binary);
  if (stream.fail()) {
    CERROR << error_messages[OPEN_FILE_FAILURE] << std::endl;
    return false;
  }
  stream.close();
  string check_csv = cl_.get_param("--check_csv");
  bool check = false;
  if (check_csv == "true") {
    check = true;
  }
  if (!rs_csv_rd.read_csv(csv_name, check)) {
    CERROR << error_messages[PIN_MAP_CSV_PARSE_ERROR] << std::endl;
    return false;
  }

  return true;
}

bool pin_location::get_available_bump_pin(
    rapidCsvReader &rs_csv_rd,
    std::pair<std::string, std::string> &bump_pin_and_mode,
    PortDirection port_direction) {
  for (unsigned int i = rs_csv_rd.start_position;
       i < rs_csv_rd.bump_pin_name.size(); i++) {
    std::string bump_pin_name = rs_csv_rd.bump_pin_name[i];
    if (used_bump_pins_.find(bump_pin_name) == used_bump_pins_.end()) {
      for (unsigned int j = 0; j < rs_csv_rd.mode_names.size(); j++) {
        std::string mode_name = rs_csv_rd.mode_names[j];
        std::vector<std::string> mode_data = rs_csv_rd.modes_map[mode_name];
        if (port_direction == INPUT) {
          if (is_input_mode(mode_name)) {
            for (unsigned int k = rs_csv_rd.start_position;
                 k < mode_data.size(); k++) {
              if ((mode_data[k] == "Y") &&
                  (bump_pin_name == rs_csv_rd.bump_pin_name[k])) {
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
            for (unsigned int k = rs_csv_rd.start_position;
                 k < mode_data.size(); k++) {
              if ((mode_data[k] == "Y") &&
                  (bump_pin_name == rs_csv_rd.bump_pin_name[k])) {
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

bool pin_location::create_temp_constrain_file(rapidCsvReader &rs_csv_rd) {

  // create a temp constraint file and internally specified that to down-flow
  // functions
  string key = "--pcf";
  temp_pcf_file_name_ = ".temp_pcf";
  cl_.set_param_value(key, temp_pcf_file_name_);

  std::vector<int> input_idx;
  std::vector<int> output_idx;
  for (unsigned int i = 0; i < user_design_inputs_.size(); i++) {
    input_idx.push_back(i);
  }
  for (unsigned int i = 0; i < user_design_outputs_.size(); i++) {
    output_idx.push_back(i);
  }
  if (pin_assign_method_ == ASSIGN_IN_RANDOM) {
    shuffle_candidates(input_idx);
    shuffle_candidates(output_idx);
  }

  std::pair<std::string, std::string> bump_pin_and_mode;
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

  for (unsigned int i = 0; i < input_idx.size(); i++) {
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
      CERROR << error_messages[PIN_SOURCE_NO_SURFFICENT] << std::endl;
      outfile.close();
      if (write_out_pcf) {
        out_pcf_file.close();
      }
      return false;
    }
  }
  for (unsigned int i = 0; i < user_design_outputs_.size(); i++) {
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
      CERROR << error_messages[PIN_SOURCE_NO_SURFFICENT] << std::endl;
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

void pin_location::shuffle_candidates(vector<int> &v) {
  random_device rd;
  mt19937 g(rd());
  shuffle(v.begin(), v.end(), g);
  return;
}

#if 0
void pin_location::collect_left_available_device_pins(
    set<string> &constrained_device_pins,
    vector<int> &left_available_device_pin_idx, rapidCsvReader &rs_csv_reader) {
  for (unsigned int i = 0; i < rs_csv_reader.bump_pin_name.size(); i++) {
    if (rs_csv_reader.usable[i] == "Y" &&
        (constrained_device_pins.find(rs_csv_reader.bump_pin_name[i]) ==
         constrained_device_pins.end())) { // left-over availalbe pins to be
                                           // assigned to user design
      left_available_device_pin_idx.push_back(i);
    }
  }
  return;
}
#endif
