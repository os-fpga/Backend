#include "pin_loc/pin_placer.h"

#include "file_readers/blif_reader.h"
#include "file_readers/pcf_reader.h"
#include "file_readers/pln_csv_reader.h"
#include "file_readers/pln_Fio.h"
#include "util/cmd_line.h"

#include <set>
#include <map>
#include <filesystem>

namespace pln {

int pinc_main(const pln::cmd_line& cmd) {
  uint16_t tr = ltrace();
  CStr separator = "********************************";
  if (tr >= 3) {
    flush_out(true);
    err_puts();
    lputs(separator);
    flush_out(true);
  }
  if (tr >= 4) {
    lputs("pinc_main()");
  }

  PinPlacer pl(cmd);

  if (tr >= 3) {
    flush_out(true);
    err_puts();
    lputs(separator);
    flush_out(true);
  }

  // pl.get_cmd().print_options();

  if (!pl.read_and_write()) {
    flush_out(true);
    err_puts();
    string last_err = PinPlacer::last_err_lookup();
    if (last_err.empty())
      lprintf2("pinc_main: pin_c PinPlacer failed\n");
    else
      lprintf2("pinc_main: pin_c PinPlacer failed, last error: %s\n",
               last_err.c_str());
    err_puts();
    flush_out(true);
    return 1;
  }

  flush_out(false);
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
  { "PIN_LOC_XML_PARSE_ERROR",       "Pin location XML file parse error" },
  { "PIN_MAP_CSV_PARSE_ERROR",       "Pin Table CSV file parse error\n" },
  { "PIN_CONSTRAINT_PARSE_ERROR",    "Pin constraint file (.pcf) parse error\n" },
  { "PORT_INFO_PARSE_ERROR",         "Port info parse error (.eblif or .json)\n" },
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

static string s_last_err_string;
string PinPlacer::err_lookup(const string& key) noexcept {
  assert(!key.empty());
  if (key.empty()) return "Internal error";

  auto fitr = s_err_map.find(key);
  if (fitr == s_err_map.end()) {
    assert(0);
    return "Internal error";
  }
  s_last_err_string = fitr->second;
  return s_last_err_string;
}
string PinPlacer::last_err_lookup() noexcept { return s_last_err_string; }

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
  resetState();
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

void PinPlacer::resetState() noexcept {
  auto_pcf_created_ = false;
  min_pt_row_ = UINT_MAX;
  max_pt_row_ = 0;
  no_more_inp_bumps_ = false;
  no_more_out_bumps_ = false;
  is_fabric_eblif_ = false;
  pin_names_translated_ = false;
  num_warnings_ = 0;
  num_critical_warnings_ = 0;
  clear_err_code();
}

bool PinPlacer::read_and_write() {
  flush_out(false);
  resetState();

  string xml_name = cl_.get_param("--xml");
  string csv_name = cl_.get_param("--csv");
  user_pcf_ = cl_.get_param("--pcf");
  string blif_name = cl_.get_param("--blif");
  string json_name = cl_.get_param("--port_info");
  string output_name = cl_.get_param("--output");
  string edits_file = cl_.get_param("--edits");

  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2) {
    flush_out(true);
    lputs("  === pin_c options ===");
    ls << "        xml_name (--xml) : " << xml_name << endl;
    ls << "        csv_name (--csv) : " << csv_name << endl;
    ls << "       user_pcf_ (--pcf) : " << user_pcf_ << endl;
    ls << "      blif_name (--blif) : " << blif_name << endl;
    ls << " json_name (--port_info) : " << json_name << endl;
    ls << "    edits_file (--edits) : " << edits_file << endl;
    ls << "    output_name (--output) : " << output_name << endl;
  }
  flush_out(false);

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
  bool usage_requirement_1 = !(csv_name.empty() || user_pcf_.empty() ||
                               no_b_json || output_name.empty()) && xml_name.empty();

  // usage 2: rs only - user dose not provide a pcf (this is really rare in
  // meaningfull on-board implementation, but could be true in test or
  // evaluation in lab)
  //          in such case, we need to make sure a constraint file is properly
  //          generated to guide VPR use LEGAL pins only.
  bool usage_requirement_2 =
      !(csv_name.empty() || no_b_json || output_name.empty()) &&
      user_pcf_.empty();

  if (tr >= 4) {
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
    if (user_pcf_.size()) {
      if (!convert_pcf_for_os_flow(user_pcf_)) {
        CERROR << err_lookup("GENERATE_PCF_FILE_FOR_OS_FLOW") << endl;
        return false;
      }
    }
    */
  } else if (!usage_requirement_1 && !usage_requirement_2) {
    flush_out(true);
    if (tr >= 2)
      lputs("[Error] pin_c: !usage_requirement_1 && !usage_requirement_2\n");
    CERROR << err_lookup("MISSING_IN_OUT_FILES") << '\n' << endl
           << USAGE_MSG_1 << ", or" << endl
           << USAGE_MSG_2 << endl;
    flush_out(true);
    return false;
  }

  // --2. read port info from user design (from port_info.json)
  if (!read_design_ports()) {
    flush_out(true);
    if (tr >= 2)
      lputs("[Error] pin_c: !read_design_ports()\n");
    err_puts();
    CERROR << err_lookup("PORT_INFO_PARSE_ERROR") << endl;
    flush_out(true);
    return false;
  }

  assert(user_design_inputs_.size() == raw_design_inputs_.size());
  assert(user_design_outputs_.size() == raw_design_outputs_.size());

  // --3. read PT from csv file
  RapidCsvReader csv_rd;
  if (!read_csv_file(csv_rd)) {
    flush_out(true);
    if (tr >= 2) {
      err_puts();
      lprintf2("[Error] pin_c: !read_csv_file()");
      flush_out(true);
    }
    CERROR << err_lookup("PIN_MAP_CSV_PARSE_ERROR") << endl;
    flush_out(true);
    return false;
  }

  // --4. read netlist edits (--edits option)
  bool has_edits = read_edits();
  flush_out(false);
  if (tr >= 3)
    lprintf("\t  has_edits : %i\n", has_edits);


  // if no user pcf is provided, created a temp one
  if (usage_requirement_2 || (usage_requirement_0 && user_pcf_ == "")) {
    flush_out(true);
    if (has_edits) {
      // if auto-PCF and has_edits, translate and de-duplicate
      // user-design ports now, since edits.json could remove some design ports.
      if (not pin_names_translated_)
        translatePinNames("(auto-PCF)");
      //
      // de-duplicate inputs
      vector<string> dups;
      if (user_design_inputs_.size() > 1) {
        bool done = false;
        while (not done) {
          done = true;
          for (int i = int(user_design_inputs_.size()) - 1; i > 0; i--) {
            for (int j = i - 1; j >= 0; j--) {
              if (user_design_inputs_[i].udes_pin_name_ == user_design_inputs_[j].udes_pin_name_) {
                dups.push_back(user_design_outputs_[i].udes_pin_name_);
                user_design_inputs_.erase(user_design_inputs_.begin() + i);
                done = false;
                break; // next i
              }
            }
          }
        }
        if (tr >= 2 and !dups.empty()) {
          flush_out(true);
          lprintf("NOTE: pin_c removed duplicate inputs (%zu):\n", dups.size());
          for (const string& s : dups)
            lprintf("    removed duplicate input: %s\n", s.c_str());
          flush_out(true);
          if (tr >= 4) {
            lprintf("after de-dup:  user_design_inputs_.size()= %zu  raw_design_inputs_.size()= %zu\n",
                    user_design_inputs_.size(), raw_design_inputs_.size());
          }
        }
      }
      // de-duplicate outputs
      dups.clear();
      if (user_design_outputs_.size() > 1) {
        bool done = false;
        while (not done) {
          done = true;
          for (int i = int(user_design_outputs_.size()) - 1; i > 0; i--) {
            for (int j = i - 1; j >= 0; j--) {
              if (user_design_outputs_[i].udes_pin_name_ == user_design_outputs_[j].udes_pin_name_) {
                dups.push_back(user_design_outputs_[i].udes_pin_name_);
                user_design_outputs_.erase(user_design_outputs_.begin() + i);
                done = false;
                break; // next i
              }
            }
          }
        }
        if (tr >= 2 and !dups.empty()) {
          flush_out(true);
          lprintf("NOTE: pin_c removed duplicate outputs (%zu):\n", dups.size());
          for (const string& s : dups)
            lprintf("    removed duplicate output: %s\n", s.c_str());
          flush_out(true);
          if (tr >= 4) {
            lprintf("after de-dup:  user_design_outputs_.size()= %zu  raw_design_outputs_.size()= %zu\n",
                    user_design_outputs_.size(), raw_design_outputs_.size());
          }
        }
      }

      if (tr >= 3) {
        flush_out(true);
        uint sz = user_design_inputs_.size();
        lprintf(" ---- dumping user_design_inputs_ after translation (%u) --\n", sz);
        for (uint i = 0; i < sz; i++)
          lprintf("  inp-%u  %s\n", i, user_design_input(i).c_str());
        lprintf(" ----\n");
        sz = user_design_outputs_.size();
        lprintf(" ---- dumping user_design_outputs_ after translation (%u) --\n", sz);
        for (uint i = 0; i < sz; i++)
          lprintf("  out-%u  %s\n", i, user_design_output(i).c_str());
        lprintf(" ----\n");
      }

      // check overlap of inputs with outputs:
      uint numInp = user_design_inputs_.size();
      uint numOut = user_design_outputs_.size();
      for (uint i = 0; i < numInp; i++) {
        const string& inp = user_design_input(i);
        assert(!inp.empty());
        for (uint j = 0; j < numOut; j++) {
          const string& out = user_design_output(j);
          assert(!out.empty());
          if (inp == out) {
            flush_out(true);
            err_puts();
            lprintf2("  [CRITICAL_WARNING]  pin '%s' is both input and output (after NL-edits).\n",
                     inp.c_str());
            incrCriticalWarnings();
            flush_out(true);
          }
        }
      }

    } // has_edits

    if (!create_temp_pcf(csv_rd)) {
      flush_out(true);
      string ec = s_err_code.empty() ? "FAIL_TO_CREATE_TEMP_PCF" : s_err_code;
      err_puts();
      CERROR << err_lookup(ec) << endl;
      ls << "[Error] " << err_lookup(ec) << endl;
      ls << "  design has " << user_design_inputs_.size() << " input pins" << endl;
      ls << "  design has " << user_design_outputs_.size() << " output pins" << endl;
      flush_out(true);
      return false;
    }
    if (num_warnings_) {
      flush_out(true);
      if (tr >= 2)
        lprintf("after create_temp_pcf() #errors: %u\n", num_warnings_);
      lprintf("\t pin_c: NOTE ERRORs: %u\n", num_warnings_);
      flush_out(true);
    }
  }

  // --5. read user pcf (or our temprary pcf).
  if (!read_pcf(csv_rd)) {
    flush_out(true);
    err_puts();
    CERROR << err_lookup("PIN_CONSTRAINT_PARSE_ERROR") << endl;
    OUT_ERROR << err_lookup("PIN_CONSTRAINT_PARSE_ERROR") << endl;
    flush_out(true);
    return false;
  }

  // --6. adjust edits based on PCF pin-lists
  finalize_edits();

  // --7. create .place file
  if (!write_dot_place(csv_rd)) {
    // error messages will be issued in callee
    if (tr) {
      flush_out(true);
      ls << "[Error] pin_c: !write_dot_place(csv_rd)" << endl;
      err_puts();
      CERROR << "write_dot_place() failed" << endl;
      flush_out(true);
    }
    return false;
  }

  // --8. optionally, map logical clocks to physical clocks
  //      status = 0 if NOP, -1 if error
  int map_clk_status = map_clocks();
  if (map_clk_status < 0) {
    flush_out(true);
    if (tr) {
      ls << "[Error] pin_c: map_clocks() failed" << endl;
      CERROR << "map_clocks() failed" << endl;
    }
    CERROR << err_lookup("FAIL_TO_CREATE_CLKS_CONSTRAINT_XML") << endl;
    flush_out(true);
    return false;
  }

  // -- done successfully
  if (tr >= 2) {
    flush_out(true);
    lprintf("pin_c done:  read_and_write() succeeded.  map_clk_status= %i\n",
            map_clk_status);
    print_stats(csv_rd);
    print_summary(csv_name);
  }
  return true;
}

uint PinPlacer::translatePinNames(const string& memo) noexcept {
  flush_out(false);
  uint16_t tr = ltrace();
  uint cnt = 0;
  CStr mem = memo.c_str();

  for (Pin& pin : user_design_inputs_) {
    pin.trans_pin_name_ = translatePinName(pin.udes_pin_name_, true);
    if (pin.udes_pin_name_ == pin.trans_pin_name_)
      continue;
    if (tr >= 3) {
      lprintf("design input pin TRANSLATED %s: %s --> %s\n", mem,
               pin.udes_pin_name_.c_str(), pin.trans_pin_name_.c_str());
    }
    pin.udes_pin_name_ = pin.trans_pin_name_;
    cnt++;
  }

  flush_out(true);
  for (Pin& pin : user_design_outputs_) {
    pin.trans_pin_name_ = translatePinName(pin.udes_pin_name_, false);
    if (pin.udes_pin_name_ == pin.trans_pin_name_)
      continue;
    if (tr >= 3) {
      lprintf("design output pin TRANSLATED %s: %s --> %s\n", mem,
               pin.udes_pin_name_.c_str(), pin.trans_pin_name_.c_str());
    }
    pin.udes_pin_name_ = pin.trans_pin_name_;
    cnt++;
  }

  pin_names_translated_ = true;
  return cnt;
}

} // namespace pln

