#include "pin_loc/pin_placer.h"

#include "file_readers/blif_reader.h"
#include "file_readers/pcf_reader.h"
#include "file_readers/pinc_csv_reader.h"
#include "file_readers/pinc_Fio.h"
#include "util/cmd_line.h"

#include <set>
#include <map>
#include <filesystem>

namespace pinc {

int pinc_main(const pinc::cmd_line& cmd) {
  using namespace pinc;
  uint16_t tr = ltrace();
  if (tr >= 4) lputs("pinc_main()");

  PinPlacer pl(cmd);

  // pl.get_cmd().print_options();

  if (!pl.read_and_write()) {
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

PinPlacer::PinPlacer(const cmd_line& cl)
 : cl_(cl) {
  pin_assign_def_order_ = true;
  auto_pcf_created_ = false;
  min_pt_row_ = UINT_MAX;
  max_pt_row_ = 0;
}

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

bool PinPlacer::read_and_write() {
  num_warnings_ = 0;
  clear_err_code();

  string xml_name = cl_.get_param("--xml");
  string csv_name = cl_.get_param("--csv");
  string pcf_name = cl_.get_param("--pcf");
  string blif_name = cl_.get_param("--blif");
  string json_name = cl_.get_param("--port_info");
  string output_name = cl_.get_param("--output");
  string edits_file = cl_.get_param("--edits");

  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2) {
    lputs("\n  === pin_c options ===");
    ls << "        xml_name (--xml) : " << xml_name << endl;
    ls << "        csv_name (--csv) : " << csv_name << endl;
    ls << "        pcf_name (--pcf) : " << pcf_name << endl;
    ls << "      blif_name (--blif) : " << blif_name << endl;
    ls << " json_name (--port_info) : " << json_name << endl;
    ls << "    edits_file (--edits) : " << edits_file << endl;
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

  // --2.5 optionally, read netlist edits (--edits option)
  bool has_edits = read_edits();
  if (tr >= 3)
    lprintf("\t  has_edits : %i\n", has_edits);

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

  // --6. optionally, map logical clocks to physical clocks
  //      status = 0 if NOP, -1 if error
  int map_clk_status = map_clocks();
  if (map_clk_status < 0) {
    if (tr) {
      ls << "[Error] pin_c: map_clocks() failed" << endl;
      CERROR << "map_clocks() failed" << endl;
    }
    CERROR << err_lookup("FAIL_TO_CREATE_CLKS_CONSTRAINT_XML") << endl;
    return false;
  }

  // -- done successfully
  if (tr >= 2) {
    ls << endl;
    lprintf("pin_c done:  read_and_write() succeeded.  map_clk_status= %i\n",
            map_clk_status);
    print_stats(csv_rd);
  }
  return true;
}

} // namespace pinc

