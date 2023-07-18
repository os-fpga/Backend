////////////////////////////////////////////////////////////////////////////////
// Important:                                                                 //
//    This is for rs internally - don't make this part public until a written //
//    approval is obtained from rapidsilicon open source review commitee      //
////////////////////////////////////////////////////////////////////////////////

#include "pin_loc/pin_location.h"

#include "file_readers/blif_reader.h"
#include "file_readers/pcf_reader.h"
#include "file_readers/rapid_csv_reader.h"
#include "file_readers/xml_reader.h"
#include "file_readers/Fio.h"
#include "file_readers/nlohmann3_11_2_json.h"

#include "util/cmd_line.h"

#include <set>
#include <map>

namespace pinc {

int pinc_main(const pinc::cmd_line& cmd) {
  using namespace pinc;
  uint16_t tr = ltrace();
  if (tr >= 2) lputs("pinc_main()");

  PinPlacer pl(cmd);

  // pl.get_cmd().print_options();

  if (!pl.reader_and_writer()) {
    return 1;
  }
  return 0;
}

using namespace std;
using fio::Fio;

static string s_err_code;
void PinPlacer::clear_err_code() noexcept { s_err_code.clear(); }
void PinPlacer::set_err_code(const char* cs) noexcept {
  if (!cs || !cs[0]) {
    s_err_code.clear();
    return;
  }
  s_err_code = cs;
}

#define CERROR std::cerr << "[Error] "
#define OUT_ERROR std::cout << "[Error] "

static std::map<string, string> s_err_map = {

  { "MISSING_IN_OUT_FILES",          "Missing input or output file arguments" },
  { "PIN_LOC_XML_PARSE_ERROR",       "Pin location file parse error" },
  { "PIN_MAP_CSV_PARSE_ERROR",       "Pin map file parse error" },
  { "PIN_CONSTRAINT_PARSE_ERROR",    "Pin constraint file parse error" },
  { "PORT_INFO_PARSE_ERROR",         "Port info parse error" },
  { "CONSTRAINED_PORT_NOT_FOUND",    "Constrained port not found in design" },
  { "CONSTRAINED_PIN_NOT_FOUND",     "Constrained pin not found in device" },
  { "RE_CONSTRAINED_PORT",           "Re-constrained port" },
  { "OVERLAP_PIN_IN_CONSTRAINT",     "Overlap pin found in constraint" },
  { "GENERATE_CSV_FILE_FOR_OS_FLOW", "Fail to generate csv file for open source flow" },
  { "OPEN_FILE_FAILURE",             "Open file failure" },
  { "CLOSE_FILE_FAILURE",            "Close file failure" },
  { "PIN_SOURCE_NO_SURFFICENT",      "Design requires more IO pins than device can supply" },
  { "FAIL_TO_CREATE_CLKS_CONSTRAINT_XML",  "Fail to create fpga constraint xml file"  },
  { "FAIL_TO_CREATE_TEMP_PCF",       "Fail to create temp pcf file"  },
  { "INCORRECT_ASSIGN_PIN_METHOD",   "Incorrect assign pin method selection" },
  { "GENERATE_PCF_FILE_FOR_OS_FLOW", "Fail to generate pcf file for open source flow" },

  { "TOO_MANY_INPUTS",   "Too many input pins" },
  { "TOO_MANY_OUTPUTS",  "Too many output pins" }

};

string PinPlacer::err_lookup(const string& key) noexcept {
  assert(!key.empty());
  if (key.empty()) return "Internal error";

  auto fitr = s_err_map.find(key);
  if (fitr == s_err_map.end()) {
    assert(0);
    return "Internal error";
  }
  return fitr->second;
}

//static string USAGE_MSG_0 =
//    "usage options: --xml PINMAP_XML --pcf PCF --blif BLIF --csv CSV_FILE "
//    "[--assign_unconstrained_pins [random | in_define_order]] --output "
//    "OUTPUT";  // for open source; and MPW1

static string USAGE_MSG_1 =
    "usage options: --pcf PCF --port_info JSON --csv CSV_FILE "
    "[--assign_unconstrained_pins [random | in_define_order]] --output "
    "OUTPUT";  // for rs internally, gemini;  user pcf is provided

static string USAGE_MSG_2 =
    "usage options: (--blif BLIF | --port_info JSON) --csv CSV_FILE "
    "[--assign_unconstrained_pins "
    "[random | in_define_order]] --output OUTPUT";  // for rs internall,
                                                    // gemini; no user pcf is provided


PinPlacer::~PinPlacer() {
  if (num_warnings_) {
    lprintf("\n\t pin_c: NOTE ERRORs: %u\n", num_warnings_);
    lputs2();
  }

  // remove temporarily generated file
  if (temp_csv_file_name_.size()) {
    if (remove(temp_csv_file_name_.c_str())) {
      CERROR << err_lookup("CLOSE_FILE_FAILURE") << endl;
    }
  }

  if (temp_pcf_name_.size()) {
    const char* fn = temp_pcf_name_.c_str();
    if (ltrace() >= 4) {
      lprintf("keeping temp_pcf for debugging: %s\n", fn);
    } else if (remove(fn)) {
      CERROR << err_lookup("CLOSE_FILE_FAILURE") << endl;
    }
  }
}

bool PinPlacer::reader_and_writer() {
  num_warnings_ = 0;
  clear_err_code();

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
    lputs("\n  PinPlacer::reader_and_writer()");
    ls << "        xml_name (--xml) : " << xml_name << endl;
    ls << "        csv_name (--csv) : " << csv_name << endl;
    ls << "        pcf_name (--pcf) : " << pcf_name << endl;
    ls << "      blif_name (--blif) : " << blif_name << endl;
    ls << " json_name (--port_info) : " << json_name << endl;
    ls << "    output_name (--output) : " << output_name << endl;
  }

  bool no_b_json = false; // json_name.empty();

  // --1. check option selection
  string assign_method = cl_.get_param("--assign_unconstrained_pins");
  if (assign_method.length()) {
    if (tr >= 2) ls << "\t assign_method= " << assign_method << endl;
    if (assign_method == "random") {
      pin_assign_def_order_ = false;
    } else if (assign_method == "in_define_order") {
      pin_assign_def_order_ = true;
    } else {
      CERROR << err_lookup("INCORRECT_ASSIGN_PIN_METHOD") << endl;
      CERROR << err_lookup("MISSING_IN_OUT_FILES") << '\n' << endl
             << USAGE_MSG_1 << ", or" << endl
             << USAGE_MSG_2 << endl;
      return false;
    }
  }

  // it is possible that no pcf is provided by user
  // in such case we need to provide a pcf to prevent vpr using all pins freely

  // usage 0: classical flow form opensource community - device MPW1
  // bool usage_requirement_0 = !(xml_name.empty() || csv_name.empty() ||
  //                             no_b_json || output_name.empty());
  constexpr bool usage_requirement_0 = false;


  // usage 1: rs only - specify csv (which contains coordinate data described in
  // pinmap xml info), pcf, blif, and output files
  bool usage_requirement_1 = !(csv_name.empty() || pcf_name.empty() ||
                               no_b_json || output_name.empty()) && xml_name.empty();

  // usage 2: rs only - user dose not provide a pcf (this is really rare in
  // meaningfull on-board implementation, but could be true in test or
  // evaluation in lab)
  //          in such case, we need to make sure a constraint file is properly
  //          generated to guide VPR use LEGAL pins only.
  bool usage_requirement_2 =
      !(csv_name.empty() || no_b_json || output_name.empty()) &&
      pcf_name.empty();

  if (tr >= 2) {
    ls << "\t usage_requirement_1 : " << boolalpha << usage_requirement_1 << endl;
    ls << "\t usage_requirement_2 : " << boolalpha << usage_requirement_2 << endl;
  }

  if (usage_requirement_0) {
    /*
    // generate new csv file with information from xml
    // and csv for os flow
    if (tr >= 2) {
      ls << "(usage_requirement_0)\n"
         << "generate new csv file with info from xml and csv for os flow"
         << endl;
    }
    if (!generate_csv_file_for_os_flow()) {
      CERROR << err_lookup("GENERATE_CSV_FILE_FOR_OS_FLOW") << endl;
      return false;
    }
    // in os mode, the pcf does not contains any "-mode"
    // we need to generate a temp pcf file with "-mode" option, which selects
    // mode "Mode_GPIO"
    if (pcf_name.size()) {
      if (!convert_pcf_for_os_flow(pcf_name)) {
        CERROR << err_lookup("GENERATE_PCF_FILE_FOR_OS_FLOW") << endl;
        return false;
      }
    }
    */
  } else if (!usage_requirement_1 && !usage_requirement_2) {
    if (tr >= 2)
      lputs("\n[Error] pin_c: !usage_requirement_1 && !usage_requirement_2\n");
    CERROR << err_lookup("MISSING_IN_OUT_FILES") << '\n' << endl
           << USAGE_MSG_1 << ", or" << endl
           << USAGE_MSG_2 << endl;
    return false;
  }

  // --2. read PT from csv file
  RapidCsvReader csv_rd;
  if (!read_csv_file(csv_rd)) {
    if (tr >= 2)
      lputs("\n[Error] pin_c: !read_csv_file()\n");
    CERROR << err_lookup("PIN_MAP_CSV_PARSE_ERROR") << endl;
    return false;
  }

  // --3. read port info from user design (from port_info.json)
  if (!read_design_ports()) {
    if (tr >= 2)
      lputs("\n[Error] pin_c: !read_design_ports()\n");
    CERROR << err_lookup("PORT_INFO_PARSE_ERROR") << endl;
    return false;
  }

  // usage 2: if no user pcf is provided, created a temp one
  if (usage_requirement_2 || (usage_requirement_0 && pcf_name == "")) {
    if (!create_temp_pcf(csv_rd)) {
      string ec = s_err_code.empty() ? "FAIL_TO_CREATE_TEMP_PCF" : s_err_code;
      CERROR << err_lookup(ec) << endl;
      ls << "[Error] " << err_lookup(ec) << endl;
      ls << "  design has " << user_design_inputs_.size() << " input pins" << endl;
      ls << "  design has " << user_design_outputs_.size() << " output pins" << endl;
      return false;
    }
    if (num_warnings_) {
      if (tr >= 2)
        lprintf("\nafter create_temp_pcf() #errors: %u\n", num_warnings_);
      lprintf("\t pin_c: NOTE ERRORs: %u\n", num_warnings_);
      lputs2();
    }
  }

  // --4. read user pcf (or our temprary pcf).
  if (!read_pcf(csv_rd)) {
    CERROR << err_lookup("PIN_CONSTRAINT_PARSE_ERROR") << endl;
    return false;
  }

  // --5. create .place file
  if (!write_dot_place(csv_rd)) {
    // error messages will be issued in callee
    if (tr) {
      ls << "[Error] pin_c: !write_dot_place(csv_rd)" << endl;
      CERROR << "write_dot_place() failed" << endl;
    }
    return false;
  }

  //  rs only - user want to map its design clocks to gemini fabric
  // clocks. like gemini has 16 clocks clk[0],clk[1]....,clk[15].And user clocks
  // are clk_a,clk_b and want to map clk_a with clk[15] like it
  // in such case, we need to make sure a xml repack constraint file is properly
  // generated to guide bitstream generation correctly.
  string fpga_repack = cmd.get_param("--read_repack");
  string user_constrained_repack = cmd.get_param("--write_repack");
  string clk_map_file = cmd.get_param("--clk_map");
  bool create_constraint_xml_requirement =
      !(fpga_repack.empty() && clk_map_file.empty());

  if (create_constraint_xml_requirement) {
    if (!write_logical_clocks_to_physical_clks()) {
      CERROR << err_lookup("FAIL_TO_CREATE_CLKS_CONSTRAINT_XML") << endl;
      return false;
    }
  }

  // -- done successfully
  if (tr >= 2) {
    ls << endl;
    ls << "pin_c done: reader_and_writer() done successfully" << endl;
    if (tr >= 3) {
      print_stats(csv_rd);
    }
  }
  return true;
}

bool PinPlacer::read_design_ports() {
  uint16_t tr = ltrace();
  if (tr >= 2) {
    lputs("\nread_design_ports() __ getting port info .json");
  }

  string port_info_fn = cl_.get_param("--port_info");
  string path;
  ifstream json_ifs;

  if (port_info_fn.length() > 0) {
    path = port_info_fn;
    if (Fio::regularFileExists(path)) {
      json_ifs.open(port_info_fn);
      if (!json_ifs.is_open())
        lprintf("\nWARNING: could not open port info file %s => using blif\n",
                port_info_fn.c_str());
    } else {
      lprintf("\nWARNING: port info file %s does not exist => using blif\n",
              port_info_fn.c_str());
    }
  } else {
    if (tr >= 1) lprintf("port_info cmd option not specified => using blif\n");
  }

  if (json_ifs.is_open()) {
    if (tr >= 2) lprintf("... reading %s\n", port_info_fn.c_str());
    if (!read_port_info(json_ifs, user_design_inputs_, user_design_outputs_)) {
      CERROR    << " failed reading " << port_info_fn << endl;
      OUT_ERROR << " failed reading " << port_info_fn << endl;
      return false;
    }
    json_ifs.close();
  } else {
    string blif_fn = cl_.get_param("--blif");
    if (tr >= 2) lprintf("... reading %s\n", blif_fn.c_str());
    path = blif_fn;
    if (not Fio::regularFileExists(path)) {
      lprintf("\nWARNING: blif file %s does not exist\n", blif_fn.c_str());
    }
    BlifReader rd_blif;
    if (!rd_blif.read_blif(blif_fn)) {
      CERROR << err_lookup("PORT_INFO_PARSE_ERROR") << endl;
      return false;
    }
    user_design_inputs_ = rd_blif.get_inputs();
    user_design_outputs_ = rd_blif.get_outputs();
  }

  if (tr >= 2) {
    lprintf(
        "DONE read_design_ports()  user_design_inputs_.size()= %zu  "
        "user_design_outputs_.size()= %zu\n",
        user_design_inputs_.size(), user_design_outputs_.size());
  }

  return true;
}

static bool read_json_ports(const nlohmann::ordered_json& from,
                            vector<PinPlacer::PortInfo>& ports) {
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
      ls << " [Warning] unexpected json array..." << endl;
      continue;
    }

    // 1. check size and presence of 'name'
    if (tr >= 7) ls << " ...... obj.size() " << obj.size() << endl;
    if (obj.size() == 0) {
      ls << " [Warning] unexpected json object size..." << endl;
      continue;
    }
    if (not obj.contains("name")) {
      ls << " [Warning] expected obj.name string..." << endl;
      continue;
    }
    if (not obj["name"].is_string()) {
      ls << " [Warning] expected obj.name string..." << endl;
      continue;
    }

    ports.emplace_back();

    // 2. read 'name'
    PinPlacer::PortInfo& last = ports.back();
    last.name_ = obj["name"];
    if (tr >= 7) ls << " ........ last.name_ " << last.name_ << endl;

    // 3. read 'direction'
    if (obj.contains("direction")) {
      s = str::sToLower(obj["direction"]);
      if (s == "input")
        last.dir_ = PinPlacer::PortDir::Input;
      else if (s == "output")
        last.dir_ = PinPlacer::PortDir::Output;
    }

    // 4. read 'type'
    if (obj.contains("type")) {
      s = str::sToLower(obj["type"]);
      if (s == "wire")
        last.type_ = PinPlacer::PortType::Wire;
      else if (s == "reg")
        last.type_ = PinPlacer::PortType::Reg;
    }

    // 5. read 'range'
    if (obj.contains("range")) {
      const auto& rangeObj = obj["range"];
      if (rangeObj.size() == 2 && rangeObj.contains("lsb") &&
          rangeObj.contains("msb")) {
        if (tr >= 8) ls << " ........ has range..." << endl;
        const auto& lsb = rangeObj["lsb"];
        const auto& msb = rangeObj["msb"];
        if (lsb.is_number_integer() && msb.is_number_integer()) {
          int l = lsb.get<int>();
          int m = msb.get<int>();
          if (l >= 0 && m >= 0) {
            if (tr >= 7)
              ls << " ........ has valid range  l= " << l << "  m= " << m
                 << endl;
            last.range_.set(l, m);
          }
        }
      }
    }

    // 6. read 'define_order'
    if (obj.contains("define_order")) {
      s = str::sToLower(obj["define_order"]);
      if (s == "lsb_to_msb")
        last.order_ = PinPlacer::Order::LSB_to_MSB;
      else if (s == "msb_to_lsb")
        last.order_ = PinPlacer::Order::MSB_to_LSB;
      if (tr >= 7 && last.hasOrder())
        ls << " ........ has define_order: " << s << endl;
    }
  }

  return true;
}

// static
bool PinPlacer::read_port_info(std::ifstream& ifs,
                               vector<string>& inputs,
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
    ls << "[Error] pin_c: Failed json input stream reading" << endl;
    CERROR << "[Error] pin_c: Failed json input stream reading" << endl;
    return false;
  }

  size_t root_sz = rootObj.size();
  if (tr >= 2) {
    ls << "json: rootObj.size() = " << root_sz
       << "  rootObj.is_array() : " << std::boolalpha << rootObj.is_array()
       << endl;
  }

  if (root_sz == 0) {
    ls << "[Error] pin_c: json: rootObj.size() == 0" << endl;
    CERROR << "[Error] pin_c: json: rootObj.size() == 0" << endl;
    return false;
  }

  vector<PortInfo> ports;
  if (rootObj.is_array())
    read_json_ports(rootObj[0], ports);
  else
    read_json_ports(rootObj, ports);

  if (tr >= 2) ls << "json: got ports.size()= " << ports.size() << endl;

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
        ls << "\nWARNING: got empty name in port_info" << endl;
    } else if (p.isOutput()) {
      if (p.name_.length())
        outputs.push_back(p.name_);
      else
        ls << "\nWARNING: got empty name in port_info" << endl;
    }
  }

  if (tr >= 2) {
    ls << " got " << inputs.size() << " inputs and " << outputs.size()
       << " outputs" << endl;
    if (tr >= 4) {
      lprintf("\n ---- inputs(%zu): ---- \n", inputs.size());
      for (size_t i = 0; i < inputs.size(); i++)
        lprintf("     in  %s\n", inputs[i].c_str());
      lprintf("\n ---- outputs(%zu): ---- \n", outputs.size());
      for (size_t i = 0; i < outputs.size(); i++)
        lprintf("    out  %s\n", outputs[i].c_str());
      lputs();
    }
  }

  return true;
}

bool PinPlacer::write_logical_clocks_to_physical_clks() {
  std::vector<std::string> set_clks;
  string clkmap_file_name = cl_.get_param("--clk_map");
  std::ifstream file(clkmap_file_name);
  std::string command;
  std::vector<std::string> commands;
  if (file.is_open()) {
    while (getline(file, command)) {
      commands.push_back(command);
    }
    file.close();
  } else {
    return false;
  }

  // Tokenize each command
  std::vector<std::vector<std::string>> tokens;
  for (const auto &cmd : commands) {
    std::stringstream ss(cmd);
    std::vector<std::string> cmd_tokens;
    std::string token;
    while (ss >> token) {
      cmd_tokens.push_back(token);
    }
    tokens.push_back(cmd_tokens);
  }

  pugi::xml_document doc;
  std::vector<std::string> design_clk;
  std::vector<std::string> device_clk;
  std::string userPin;
  std::string userNet;
  bool d_c = false;
  bool p_c = false;
  string out_fn = cl_.get_param("--write_repack");
  string in_fn = cl_.get_param("--read_repack");
  std::ifstream infile(in_fn);
  if (!infile.is_open()) {
    std::cerr << "ERROR: cound not open the file " << in_fn << std::endl;
    return false;
  }
  // Load the XML file
  pugi::xml_parse_result result = doc.load_file(in_fn.c_str());
  if (!result) {
    std::cerr << "Error loading repack constraints XML file: "
              << result.description() << '\n';
    return 1;
  }

  for (const auto &cmd_tokens : tokens) {
    for (const auto &token : cmd_tokens) {
      if (token == "-device_clock") {
        d_c = true;
        p_c = false;
        continue;
      } else if (token == "-design_clock") {
        d_c = false;
        p_c = true;
        continue;
      }
      if (d_c) {
        device_clk.push_back(token);
      }

      if (p_c) {
        design_clk.push_back(token);
        p_c = false;
      }
    }
  }

  std::map<std::string, std::string> userPins;

  for (size_t k = 0; k < device_clk.size(); k++) {
    userPin = device_clk[k];
    userNet = design_clk[k];
    if (userNet.empty()) {
      userNet = "OPEN";
    }
    userPins[userPin] = userNet;
  }

  // If the user does not provide a net name, set it to "open"
  if (userNet.empty()) {
    userNet = "OPEN";
  }

  // Update the XML file
  for (pugi::xml_node node :
       doc.child("repack_design_constraints").children("pin_constraint")) {
    auto it = userPins.find(node.attribute("pin").value());
    if (it != userPins.end()) {
      node.attribute("net").set_value(it->second.c_str());
    } else {
      node.attribute("net").set_value("OPEN");
    }
  }

  // Save the updated XML file
  doc.save_file(out_fn.c_str(), "", pugi::format_no_declaration);
  remove(clkmap_file_name.c_str());
  return true;
}

} // namespace pinc

