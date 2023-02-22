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

int pin_constrain_location(const pinc::cmd_line &cmd)
{
  using namespace pinc;
  uint16_t tr = ltrace();
  if (tr >= 2)
    lputs("pin_constrain_location()");

  pin_location pl(cmd);

  // pl.get_cmd().print_options();

  if (!pl.reader_and_writer())
  {
    return 1;
  }
  return 0;
}

namespace pinc
{

  using namespace std;

#define CERROR std::cerr << "[Error] "

  // use fix to detemined A2F and F2A GBX mode
  static constexpr const char *INPUT_MODE_FIX = "_RX";
  static constexpr const char *OUTPUT_MODE_FIX = "_TX";

  // for mpw1  (no gearbox, a Mode_GPIO is created)
  static constexpr const char *GPIO_MODE_FIX = "_GPIO";

  static bool fs_path_exists(const string &path) noexcept
  {
    if (path.length() < 1)
      return false;

    struct stat sb;
    if (::stat(path.c_str(), &sb))
      return false;
    if (not(S_IFREG & sb.st_mode))
      return false;

    return true;
  }

  static string USAGE_MSG_0 =
      "usage options: --xml PINMAP_XML --pcf PCF --blif BLIF --csv CSV_FILE "
      "[--assign_unconstrained_pins [random | in_define_order]] --output "
      "OUTPUT"; // for open source; and MPW1

  static string USAGE_MSG_1 =
      "usage options: --pcf PCF (--blif BLIF | --port_info JSON) --csv CSV_FILE "
      "[--assign_unconstrained_pins [random | in_define_order]] --output "
      "OUTPUT"; // for rs internally, gemini;  user pcf is provided

  static string USAGE_MSG_2 =
      "usage options: (--blif BLIF | --port_info JSON) --csv CSV_FILE "
      "[--assign_unconstrained_pins "
      "[random | in_define_order]] --output OUTPUT"; // for rs internall,
                                                     // gemini; no user pcf is
                                                     // provided

  pin_location::pin_location(const cmd_line &cl) : cl_(cl)
  {
    temp_csv_file_name_ = "";
    temp_pcf_file_name_ = "";
    temp_os_pcf_file_name_ = "";

    // default pin assign method
    pin_assign_method_ = ASSIGN_IN_DEFINE_ORDER;
  }

  pin_location::~pin_location()
  {
    if (num_warnings_)
    {
      lprintf("\n\t NOTE warnings: %u\n", num_warnings_);
      lputs2();
    }

    // remove temporarily generated file
    if (temp_csv_file_name_.size())
    {
      if (remove(temp_csv_file_name_.c_str()))
      {
        CERROR << error_messages_[CLOSE_FILE_FAILURE] << endl;
      }
    }

    if (temp_pcf_file_name_.size())
    {
      const char *fn = temp_pcf_file_name_.c_str();
      if (ltrace() >= 4)
      {
        lprintf("keeping temp_pcf_file for debugging: %s\n", fn);
      }
      else if (remove(fn))
      {
        CERROR << error_messages_[CLOSE_FILE_FAILURE] << endl;
      }
    }
  }

  // static data (hacky, but temporary)
  static vector<string> s_axi_inpQ, s_axi_outQ;

  bool pin_location::reader_and_writer()
  {
    num_warnings_ = 0;
    const cmd_line &cmd = cl_;
    string xml_name = cmd.get_param("--xml");
    string csv_name = cmd.get_param("--csv");
    string pcf_name = cmd.get_param("--pcf");
    string blif_name = cmd.get_param("--blif");
    string json_name = cmd.get_param("--port_info");
    string output_name = cmd.get_param("--output");

    uint16_t tr = ltrace();
    auto &ls = lout();
    if (tr >= 2)
    {
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
    if (assign_method.length())
    {
      if (tr >= 2)
        ls << "\t assign_method= " << assign_method << endl;
      if (assign_method == "random")
      {
        pin_assign_method_ = ASSIGN_IN_RANDOM;
      }
      else if (assign_method == "in_define_order")
      {
        pin_assign_method_ = ASSIGN_IN_DEFINE_ORDER;
      }
      else
      {
        CERROR << error_messages_[INCORRECT_ASSIGN_PIN_METHOD] << endl;
        CERROR << error_messages_[MISSING_IN_OUT_FILES] << endl
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

    if (tr >= 2)
    {
      ls << "\t usage_requirement_0 : " << boolalpha << usage_requirement_0
         << endl;
      ls << "\t usage_requirement_1 : " << boolalpha << usage_requirement_1
         << endl;
      ls << "\t usage_requirement_2 : " << boolalpha << usage_requirement_2
         << endl;
    }

    if (usage_requirement_0)
    { // generate new csv file with information from xml
      // and csv for cassical flow
      if (tr >= 2)
      {
        ls << "(usage_requirement_0)\n"
           << "generate new csv file with info from xml and csv for cassical flow"
           << endl;
      }
      if (!generate_csv_file_for_os_flow())
      {
        CERROR << error_messages_[GENERATE_CSV_FILE_FOR_OS_FLOW] << endl;
        return false;
      }
      // in os mode, the pcf does not contains any "-mode"
      // we need to generate a temp pcf file with "-mode" option, which selects
      // mode "Mode_GPIO"
      string pcf_file_name = cl_.get_param("--pcf");
      if (pcf_file_name.size())
      {
        if (!convert_pcf_file_for_os_flow(pcf_file_name))
        {
          CERROR << error_messages_[GENERATE_PCF_FILE_FOR_OS_FLOW] << endl;
          return false;
        }
      }
    }
    else if ((!usage_requirement_1) && (!usage_requirement_2))
    {
      CERROR << error_messages_[MISSING_IN_OUT_FILES] << endl
             << USAGE_MSG_0 << ", or" << endl
             << USAGE_MSG_1 << ", or" << endl
             << USAGE_MSG_2 << endl;
      return false;
    }

    // --2. read port info from user design (from .blif or from port_info.json)
    if (!read_user_design())
    {
      CERROR << error_messages_[INPUT_DESIGN_PARSE_ERROR] << endl;
      return false;
    }

    // --3. read info (pin-table) from csv file in new RS format
    RapidCsvReader csv_rd;
    if (!read_csv_file(csv_rd))
    {
      CERROR << error_messages_[PIN_MAP_CSV_PARSE_ERROR] << endl;
      return false;
    }

    // usage 2: if no user constraint is provided, created a temp one
    if (usage_requirement_2 || (usage_requirement_0 && pcf_name == ""))
    {
      if (!create_temp_pcf_file(csv_rd))
      {
        CERROR << error_messages_[FAIL_TO_CREATE_TEMP_PCF] << endl;
        return false;
      }
      if (tr >= 3 || num_warnings_)
      {
        lprintf("\nafter create_temp_pcf_file() num_warnings_= %u\n",
                num_warnings_);
        if (num_warnings_)
        {
          lprintf("\t NOTE warnings: %u\n", num_warnings_);
          lputs2();
        }
      }
    }

    // --4. read user constraint
    if (!read_pcf_file())
    {
      CERROR << error_messages_[PIN_CONSTRAINT_PARSE_ERROR] << endl;
      return false;
    }

    // --5. create .place file
    if (!create_place_file(csv_rd))
    {
      // error messages will be issued in callee
      if (tr)
      {
        ls << " (EE) !create_place_file(csv_rd)" << endl;
        cerr << "ERROR: create_place_file() failed" << endl;
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
    bool create_constraint_xml_requirement =
        !(pcf_name.empty() || fpga_repack.empty());

    if (create_constraint_xml_requirement)
    {
      if (!write_logical_clocks_to_physical_clks())
      {
        CERROR << error_messages_[FAIL_TO_CREATE_CLKS_CONSTRAINT_XML] << endl;
        return false;
      }
    }

    // -- done successfully
    if (tr >= 2)
      ls << "done: pin_location::reader_and_writer() done successfully" << endl;
    return true;
  }

  // for open source flow
  // process pinmap xml and csv file and gerate the csv file like gemini one (with
  // only partial information for pin constraint)
  bool pin_location::generate_csv_file_for_os_flow()
  {
    uint16_t tr = ltrace();
    if (tr >= 2)
    {
      if (tr >= 3)
        lputs("pin_location::generate_csv_file_for_os_flow()");
      cout << "Generateing new csv" << endl;
    }

    XmlReader rd_xml;
    if (!rd_xml.read_xml(cl_.get_param("--xml")))
    {
      CERROR << error_messages_[PIN_LOC_XML_PARSE_ERROR] << endl;
      return false;
    }
    std::map<string, PinMappingData> xml_port_map = rd_xml.get_port_map();
    CsvReader rd_csv;
    if (!rd_csv.read_csv(cl_.get_param("--csv")))
    {
      CERROR << error_messages_[PIN_MAP_CSV_PARSE_ERROR] << endl;
      return false;
    }
    map<string, string> csv_port_map = rd_csv.get_port_map();

    temp_csv_file_name_ = ".temp_file_csv";
    std::ofstream outfile;
    outfile.open(temp_csv_file_name_, std::ifstream::out | std::ifstream::binary);
    if (outfile.fail())
    {
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
    for (; itr != csv_port_map.end(); itr++)
    {
      PinMappingData pinMapData = xml_port_map.at(itr->second);
      outfile << "GBOX GPIO," /* fake group name */ << itr->first << ","
              << itr->second << "," << std::to_string(pinMapData.get_x()) << ","
              << pinMapData.get_y() << "," << pinMapData.get_z() << ",Y" << endl;
    }
    outfile.close();

    return true;
  }

  bool pin_location::convert_pcf_file_for_os_flow(string pcf_file_name)
  {
    PcfReader rd_pcf;
    if (!rd_pcf.read_os_pcf(pcf_file_name))
    {
      CERROR << error_messages_[PIN_CONSTRAINT_PARSE_ERROR] << endl;
      return false;
    }
    vector<vector<string>> pcf_pin_cstr = rd_pcf.get_commands();
    temp_os_pcf_file_name_ = ".temp_os_file_pcf";
    std::ofstream outfile;
    outfile.open(temp_os_pcf_file_name_,
                 std::ifstream::out | std::ifstream::binary);
    if (outfile.fail())
    {
      // no error message here, caller is to show GENERATE_CSV_FILE_FOR_OS_FLOW
      // message
      return false;
    }
    for (uint i = 0; i < pcf_pin_cstr.size(); i++)
    {
      uint j = 0;
      string command = pcf_pin_cstr[i][j];
      for (j++; j < pcf_pin_cstr[i].size(); j++)
      {
        command += string(" ") + pcf_pin_cstr[i][j];
      }
      command += string(" -mode Mode_GPIO");
      outfile << command << endl;
    }
    outfile.close();

    return true;
  }

  bool pin_location::read_user_design()
  {
    uint16_t tr = ltrace();
    if (tr >= 2)
    {
      lputs("\nread_user_design() __ getting port info .json");
    }

    string port_info_fn = cl_.get_param("--port_info");
    // filesystem::path path;
    string path;
    ifstream json_ifs;

    if (port_info_fn.length() > 0)
    {
      path = port_info_fn;
      if (fs_path_exists(path))
      {
        json_ifs.open(port_info_fn);
        if (!json_ifs.is_open())
          lprintf("WARNING: could not open port info file %s => using blif\n",
                  port_info_fn.c_str());
      }
      else
      {
        lprintf("WARNING: port info file %s does not exist => using blif\n",
                port_info_fn.c_str());
      }
    }
    else
    {
      if (tr >= 1)
        lprintf("port_info cmd option not specified => using blif\n");
    }

    if (json_ifs.is_open())
    {
      if (tr >= 2)
        lprintf("... reading %s\n", port_info_fn.c_str());
      if (!read_port_info(json_ifs, user_design_inputs_, user_design_outputs_))
      {
        CERROR << " failed reading " << port_info_fn << endl;
        return false;
      }
      json_ifs.close();
    }
    else
    {
      string blif_fn = cl_.get_param("--blif");
      if (tr >= 2)
        lprintf("... reading %s\n", blif_fn.c_str());
      path = blif_fn;
      if (not fs_path_exists(path))
      {
        lprintf("WARNING: blif file %s does not exist\n", blif_fn.c_str());
      }
      BlifReader rd_blif;
      if (!rd_blif.read_blif(blif_fn))
      {
        CERROR << error_messages_[INPUT_DESIGN_PARSE_ERROR] << endl;
        return false;
      }
      user_design_inputs_ = rd_blif.get_inputs();
      user_design_outputs_ = rd_blif.get_outputs();
    }

    if (tr >= 2)
    {
      lprintf(
          "DONE read_user_design()  user_design_inputs_.size()= %u  "
          "user_design_outputs_.size()= %u\n",
          uint(user_design_inputs_.size()), uint(user_design_outputs_.size()));
    }

    return true;
  }

  bool pin_location::read_pcf_file()
  {
    uint16_t tr = ltrace();
    if (tr >= 2)
      lputs("\npin_location::read_pcf_file()");

    string pcf_file_name;
    if (temp_os_pcf_file_name_.size())
    {
      // use generated temp pcf for open source flow
      pcf_file_name = temp_os_pcf_file_name_;
    }
    else
    {
      pcf_file_name = cl_.get_param("--pcf");
    }

    PcfReader rd_pcf;
    if (!rd_pcf.read_pcf(pcf_file_name))
    {
      CERROR << error_messages_[PIN_CONSTRAINT_PARSE_ERROR] << endl;
      return false;
    }

    pcf_pin_cmds_ = std::move(rd_pcf.commands_);

    return true;
  }

  static bool vec_contains(const vector<string> &V, const string &s) noexcept
  {
    if (V.empty())
      return false;
    for (int i = V.size() - 1; i >= 0; i--)
      if (V[i] == s)
        return true;
    return false;
  }

  bool pin_location::create_place_file(const RapidCsvReader &csv_rd)
  {
    string out_fn = cl_.get_param("--output");
    uint16_t tr = ltrace();
    auto &ls = lout();
    if (tr >= 2)
    {
      ls << "\npin_location::create_place_file() __ Creating place file\n"
            "  cl_.get_param(--output) : "
         << out_fn << endl;
    }

    std::ofstream out_file;
    out_file.open(out_fn);
    if (out_file.fail())
    {
      // no error message here, caller is to show failure message
      return false;
    }

    out_file << "#Block Name   x   y   z\n";
    out_file << "#------------ --  --  -" << endl;
    out_file << std::flush;

    if (tr >= 2)
    {
      ls << "#Block Name   x   y   z\n";
      ls << "#------------ --  --  -" << endl;
      ls << std::flush;
    }

    // parse each constraint line in constrain file, generate place location
    // accordingly
    //
    std::set<string> constrained_user_pins,
        constrained_device_pins; // for sanity check

    string gbox_pin_name, search_name;

    for (const vector<string> &pcf_cmd : pcf_pin_cmds_)
    {
      // only support io and clock for now
      if (pcf_cmd[0] != "set_io" && pcf_cmd[0] != "set_clk")
        continue;
      gbox_pin_name.clear();
      search_name.clear();

      const string &user_design_pin_name = pcf_cmd[1];
      const string &device_pin_name = pcf_cmd[2]; // bump or ball

      if (tr >= 4)
      {
        logVec(pcf_cmd, "    cur pcf_cmd:");
        ls << "\t user_design_pin_name=  " << user_design_pin_name
           << "  -->  device_pin_name=  " << device_pin_name << endl;
      }

      for (size_t i = 1; i < pcf_cmd.size(); i++)
      {
        if (pcf_cmd[i] == "-internal_pin")
        {
          gbox_pin_name = pcf_cmd[++i];
          break;
        }
      }

      bool is_in_pin = vec_contains(user_design_inputs_, user_design_pin_name);

      bool is_out_pin =
          is_in_pin ? false
                    : vec_contains(user_design_outputs_, user_design_pin_name);

      if (!is_in_pin && !is_out_pin)
      {
        // sanity check
        CERROR << error_messages_[CONSTRAINED_PORT_NOT_FOUND] << ": <"
               << user_design_pin_name << ">" << endl;
        out_file << "\n=== Error happened, .place file is incomplete\n"
                 << "=== ERROR:" << error_messages_[CONSTRAINED_PORT_NOT_FOUND]
                 << ": <" << user_design_pin_name
                 << ">  device_pin_name: " << device_pin_name << "\n\n";
        out_file.close();
        return false;
      }
      if (!csv_rd.has_io_pin(device_pin_name))
      {
        CERROR << error_messages_[CONSTRAINED_PIN_NOT_FOUND] << ": <"
               << device_pin_name << ">" << endl;
        out_file << "\n=== Error happened, .place file is incomplete\n"
                 << "=== ERROR:" << error_messages_[CONSTRAINED_PORT_NOT_FOUND]
                 << ": <" << user_design_pin_name
                 << ">  device_pin_name: " << device_pin_name << "\n\n";
        out_file.close();
        return false;
      }

      if (!constrained_user_pins.count(user_design_pin_name))
      {
        constrained_user_pins.insert(user_design_pin_name);
      }
      else
      {
        CERROR << error_messages_[RE_CONSTRAINED_PORT] << ": <"
               << user_design_pin_name << ">" << endl;
        out_file.close();
        return false;
      }

      // use (device_pin_name + " " + gbox_pin_name) as search name to check
      // overlap pin in constraint
      search_name = device_pin_name;
      search_name.push_back(' ');
      search_name += gbox_pin_name;

      if (!constrained_device_pins.count(search_name))
      {
        constrained_device_pins.insert(search_name);
      }
      else
      {
        CERROR << error_messages_[OVERLAP_PIN_IN_CONSTRAINT] << ": <"
               << device_pin_name << ">" << endl;
        out_file.close();
        return false;
      }

      // look for coordinates and write constrain
      if (is_out_pin)
      {
        out_file << "out:";
        if (tr >= 4)
          ls << "out:";
      }

      string mode = pcf_cmd[4];
      out_file << user_design_pin_name << '\t';
      out_file.flush();

      XYZ xyz = csv_rd.get_pin_xyz_by_name(mode, device_pin_name, gbox_pin_name);
      assert(xyz.valid());

      out_file << xyz.x_ << '\t' << xyz.y_ << '\t' << xyz.z_ << endl;
      out_file.flush();

      if (tr >= 4)
      {
        ls << xyz.x_ << '\t' << xyz.y_ << '\t' << xyz.z_ << endl;
        ls.flush();
      }
    }

    if (tr >= 2)
      ls << "\nwritten " << out_fn << endl;

    return true;

#if 0
  // assign other un-constrained user pins (if any) to legal io tile pin in fpga
  if (constrained_user_pins.size() <
      (user_design_inputs_.size() + user_design_outputs_.size())) {
    vector<int> left_available_device_pin_idx;
    collect_left_available_device_pins(
        constrained_device_pins, left_available_device_pin_idx, csv_rd);
    if (pin_assign_method_ == ASSIGN_IN_RANDOM) {
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
#endif // 0
  }

  bool pin_location::read_csv_file(RapidCsvReader &csv_rd)
  {
    uint16_t tr = ltrace();
    if (tr >= 2)
      lputs("\nread_csv_file() __ Reading new csv");

    string csv_name;
    if (temp_csv_file_name_.size())
    {
      // use generated temp csv for open source flow
      csv_name = temp_csv_file_name_;
    }
    else
    {
      csv_name = cl_.get_param("--csv");
    }

    if (tr >= 2)
      lprintf("  cvs_name= %s\n", csv_name.c_str());

    {
      std::ifstream stream;
      stream.open(csv_name, std::ios::binary);
      if (stream.fail())
      {
        CERROR << error_messages_[OPEN_FILE_FAILURE] << endl;
        return false;
      }
      stream.close();
    }

    string check_csv = cl_.get_param("--check_csv");
    bool check = false;
    if (check_csv == "true")
    {
      check = true;
      if (tr >= 2)
        lputs("NOTE: check_csv == True");
    }
    if (!csv_rd.read_csv(csv_name, check))
    {
      CERROR << error_messages_[PIN_MAP_CSV_PARSE_ERROR] << endl;
      return false;
    }

    return true;
  }

  pair<string, string> pin_location::get_available_device_pin(
      const RapidCsvReader &rdr, bool is_inp)
  {
    pair<string, string> result;

    uint16_t tr = ltrace();

    if (is_inp)
    {
      if (no_more_inp_bumps_)
      {
        result = get_available_axi_ipin(s_axi_inpQ);
        if (tr >= 4)
          lprintf("  no_more_inp_bumps_ => got axi_ipin: %s\n",
                  result.first.c_str());
        return result;
      }
      result = get_available_bump_ipin(rdr);
      if (result.first.empty())
      {
        no_more_inp_bumps_ = true;
        result = get_available_axi_ipin(s_axi_inpQ);
      }
    }
    else
    { // out
      if (no_more_out_bumps_)
      {
        result = get_available_axi_opin(s_axi_outQ);
        if (tr >= 4)
          lprintf("  no_more_out_bumps_ => got axi_opin: %s\n",
                  result.first.c_str());
        return result;
      }
      result = get_available_bump_opin(rdr);
      if (result.first.empty())
      {
        no_more_out_bumps_ = true;
        result = get_available_axi_opin(s_axi_outQ);
      }
    }

    return result;
  }

  pair<string, string> pin_location::get_available_axi_ipin(vector<string> &Q)
  {
    static uint cnt = 0;
    cnt++;
    if (ltrace() >= 4)
    {
      lprintf("get_available_axi_ipin()# %u Q.size()= %u\n", cnt, uint(Q.size()));
    }

    pair<string, string> result; // pin_and_mode

    if (Q.empty())
      return result;

    result.first = Q.back();
    Q.pop_back();

    // need to pick a mode, othewise PcfReader::read_pcf() will not tokenize pcf
    // line properly
    result.second = "MODE_GPIO";

    return result;
  }

  pair<string, string> pin_location::get_available_axi_opin(vector<string> &Q)
  {
    static uint cnt = 0;
    cnt++;
    if (ltrace() >= 4)
    {
      lprintf("get_available_axi_opin()# %u Q.size()= %u\n", cnt, uint(Q.size()));
    }

    pair<string, string> result; // pin_and_mode

    if (Q.empty())
      return result;

    result.first = Q.back();
    Q.pop_back();

    // need to pick a mode, othewise PcfReader::read_pcf() will not tokenize pcf
    // line properly
    result.second = "MODE_GPIO";

    return result;
  }

  pair<string, string> pin_location::get_available_bump_ipin(
      const RapidCsvReader &rdr)
  {
    static uint icnt = 0;
    icnt++;
    uint16_t tr = ltrace();
    if (tr >= 4)
    {
      lprintf("get_available_bump_ipin()# %u\n", icnt);
    }

    // constexpr bool is_input_port = true;

    bool found = false;
    pair<string, string> result; // pin_and_mode

    uint num_rows = rdr.numRows();
    for (uint i = rdr.start_GBOX_GPIO_row_; i < num_rows; i++)
    {
      const auto &bcd = rdr.bcd_[i];
      const string &bump_pin_name = bcd.bump_;

      if (used_bump_pins_.count(bump_pin_name))
        continue;

      for (uint j = 0; j < rdr.mode_names_.size(); j++)
      {
        const string &mode_name = rdr.mode_names_[j];
        const vector<string> *mode_data = rdr.getModeData(mode_name);
        assert(mode_data);
        assert(mode_data->size() == num_rows);

        if (is_input_mode(mode_name))
        {
          for (uint k = rdr.start_GBOX_GPIO_row_; k < num_rows; k++)
          {
            if (mode_data->at(k) == "Y" && bump_pin_name == rdr.bumpPinName(k))
            {
              result.first = bump_pin_name;
              result.second = mode_name;
              used_bump_pins_.insert(bump_pin_name);
              if (tr >= 5)
              {
                lprintf(
                    "\t\t  get_available_bump_ipin() used_bump_pins_.insert( %s "
                    ")  row_i= %u  row_k= %u\n",
                    bump_pin_name.c_str(), i, k);
              }
              found = true;
              goto ret;
            }
          }
        }
        else
        {
          continue;
        }
      }
    }

  ret:
    if (tr >= 2)
    {
      if (found && tr >= 4)
      {
        const string &bump_pn = result.first;
        const string &mode_nm = result.second;
        lprintf("\t  ret  bump_pin_name= %s  mode_name= %s\n", bump_pn.c_str(),
                mode_nm.c_str());
      }
      else if (tr >= 7)
      {
        lprintf("\t (EERR) get_available_bump_ipin()#%u returns NOT_FOUND\n",
                icnt);
        lputs2();
        lprintf("\t vvv used_bump_pins_.size()= %u\n",
                (uint)used_bump_pins_.size());
        if (tr >= 8)
        {
          if (tr >= 9)
          {
            for (const auto &ubp : used_bump_pins_)
              lprintf("\t    %s\n", ubp.c_str());
          }
          lprintf("\t ^^^ used_bump_pins_.size()= %u\n",
                  (uint)used_bump_pins_.size());
          lprintf("\t (EERR) get_available_bump_ipin()#%u returns NOT_FOUND\n",
                  icnt);
        }
        lputs2();
      }
    }

    if (found)
    {
      assert(!result.first.empty());
      assert(!result.second.empty());
    }
    else
    {
      result.first.clear();
      result.second.clear();
    }

    return result;
  }

  pair<string, string> pin_location::get_available_bump_opin(
      const RapidCsvReader &rdr)
  {
    static uint ocnt = 0;
    ocnt++;
    uint16_t tr = ltrace();
    if (tr >= 4)
    {
      lprintf("get_available_bump_opin()# %u\n", ocnt);
    }

    constexpr bool is_input_port = false;

    bool found = false;
    pair<string, string> result; // pin_and_mode

    uint num_rows = rdr.numRows();
    for (uint i = rdr.start_GBOX_GPIO_row_; i < num_rows; i++)
    {
      const auto &bcd = rdr.bcd_[i];
      const string &bump_pin_name = bcd.bump_;

      if (used_bump_pins_.count(bump_pin_name))
        continue;

      for (uint j = 0; j < rdr.mode_names_.size(); j++)
      {
        const string &mode_name = rdr.mode_names_[j];
        const vector<string> *mode_data = rdr.getModeData(mode_name);
        assert(mode_data);
        assert(mode_data->size() == num_rows);
        if (is_input_port)
        {
          // if (is_input_mode(mode_name)) {
          //  for (uint k = rdr.start_GBOX_GPIO_row_; k < num_rows; k++) {
          //    if (mode_data->at(k) == "Y" &&
          //        bump_pin_name == rdr.bumpPinName(k)) {
          //      result.first = bump_pin_name;
          //      result.second = mode_name;
          //      used_bump_pins_.insert(bump_pin_name);
          //      if (tr >= 5) {
          //        lprintf("\t\t  get_available_bump_opin()
          //        used_bump_pins_.insert( %s )  row_i= %u  row_k= %u\n",
          //            bump_pin_name.c_str(), i, k);
          //      }
          //      found = true;
          //      goto ret;
          //    }
          //  }
          //} else {
          //  continue;
          //}
        }
        else
        { // OUTPUT port
          if (is_output_mode(mode_name))
          {
            for (uint k = rdr.start_GBOX_GPIO_row_; k < num_rows; k++)
            {
              if (mode_data->at(k) == "Y" &&
                  bump_pin_name == rdr.bumpPinName(k))
              {
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
    if (tr >= 2)
    {
      if (found && tr >= 4)
      {
        const string &bump_pn = result.first;
        const string &mode_nm = result.second;
        lprintf("\t  ret  bump_pin_name= %s  mode_name= %s\n", bump_pn.c_str(),
                mode_nm.c_str());
      }
      else if (tr >= 7)
      {
        lprintf("\t (EERR) get_available_bump_opin()#%u returns NOT_FOUND\n",
                ocnt);
        lputs2();
        lprintf("\t vvv used_bump_pins_.size()= %u\n",
                (uint)used_bump_pins_.size());
        if (tr >= 8)
        {
          if (tr >= 9)
          {
            for (const auto &ubp : used_bump_pins_)
              lprintf("\t    %s\n", ubp.c_str());
          }
          lprintf("\t ^^^ used_bump_pins_.size()= %u\n",
                  (uint)used_bump_pins_.size());
          lprintf("\t (EERR) get_available_bump_opin()#%u returns NOT_FOUND\n",
                  ocnt);
        }
        lputs2();
      }
    }

    if (found)
    {
      assert(!result.first.empty());
      assert(!result.second.empty());
    }
    else
    {
      result.first.clear();
      result.second.clear();
    }

    return result;
  }

  // create a temporary pcf file and internally pass it to params
  bool pin_location::create_temp_pcf_file(const RapidCsvReader &csv_rd)
  {
    string key = "--pcf";
    temp_pcf_file_name_ = std::to_string(getpid()) + ".temp_pcf.pcf";
    cl_.set_param_value(key, temp_pcf_file_name_);

    uint16_t tr = ltrace();
    if (tr >= 3)
      lprintf("\ncreate_temp_pcf_file() : %s\n", temp_pcf_file_name_.c_str());

    // state for get_available_device_pin:
    no_more_inp_bumps_ = false;
    no_more_out_bumps_ = false;
    //
    s_axi_inpQ = csv_rd.get_AXI_inputs();
    s_axi_outQ = csv_rd.get_AXI_outputs();

    if (tr >= 4)
    {
      lprintf("  s_axi_inpQ.size()= %u  s_axi_outQ.size()= %u\n",
              (uint)s_axi_inpQ.size(), (uint)s_axi_outQ.size());
    }

    vector<int> input_idx, output_idx;
    uint input_sz = user_design_inputs_.size();
    uint output_sz = user_design_outputs_.size();
    input_idx.reserve(input_sz);
    output_idx.reserve(output_sz);
    for (uint i = 0; i < input_sz; i++)
    {
      input_idx.push_back(i);
    }
    for (uint i = 0; i < output_sz; i++)
    {
      output_idx.push_back(i);
    }
    if (pin_assign_method_ == ASSIGN_IN_RANDOM)
    {
      shuffle_candidates(input_idx);
      shuffle_candidates(output_idx);
      if (tr >= 3)
        lputs("  create_temp_pcf_file() randomized input_idx, output_idx");
    }
    else if (tr >= 3)
    {
      lputs(
          "  input_idx, output_idx are indexing user_design_inputs_, "
          "user_design_outputs_");
      lprintf("  input_idx.size()= %u  output_idx.size()= %u\n",
              uint(input_idx.size()), uint(output_idx.size()));
    }

    pair<string, string> device_pin_n_mode;
    ofstream temp_out;
    temp_out.open(temp_pcf_file_name_, ifstream::out | ifstream::binary);
    if (temp_out.fail())
    {
      lprintf(
          "\n  !!! (ERROR) create_temp_pcf_file(): could not open %s for "
          "writing\n",
          temp_pcf_file_name_.c_str());
      CERROR << "(EE) could not open " << temp_pcf_file_name_ << " for writing\n"
             << endl;
      return false;
    }

    // if user specified "--write_pcf", 'user_out' will be a copy of 'temp_out'
    ofstream user_out;
    {
      string user_pcf_name = cl_.get_param("--write_pcf");
      if (user_pcf_name.size())
      {
        user_out.open(user_pcf_name, ifstream::out | ifstream::binary);
        if (user_out.fail())
        {
          lprintf(
              "\n  !!! (ERROR) create_temp_pcf_file(): failed to open user_out "
              "%s\n",
              user_pcf_name.c_str());
          return false;
        }
      }
    }

    string pinName, set_io_str;

    if (tr >= 2)
    {
      lprintf("--- writing pcf inputs (%u)\n", input_sz);
    }
    for (uint i = 0; i < input_sz; i++)
    {
      const string &inpName = user_design_inputs_[i];
      if (tr >= 4)
      {
        lprintf("assigning user_design_input #%u %s\n", i, inpName.c_str());
        if (csv_rd.hasCustomerInternalName(inpName))
        {
          lprintf("\t (CustIntName) hasCustomerInternalName( %s )\n",
                  inpName.c_str());
        }
      }
      assert(!inpName.empty());
      device_pin_n_mode = get_available_device_pin(csv_rd, true /*INPUT*/);
      if (device_pin_n_mode.first.length())
      {
        pinName = csv_rd.bumpName2CustomerName(device_pin_n_mode.first);
        assert(!pinName.empty());

        set_io_str = user_design_inputs_[input_idx[i]];
        set_io_str.push_back(' ');
        set_io_str += pinName;
        set_io_str += " -mode ";
        set_io_str += device_pin_n_mode.second;

        if (tr >= 4)
        {
          lprintf(" ... writing Input to pcf for  bump_pin= %s  pinName= %s\n",
                  device_pin_n_mode.first.c_str(), pinName.c_str());
          lprintf("        set_io %s\n", set_io_str.c_str());
        }
        temp_out << "set_io " << set_io_str << endl;
        if (user_out.is_open())
        {
          user_out << "set_io " << set_io_str << endl;
        }
      }
      else
      {
        if (tr >= 1)
          lputs("WARNING: (inp) get_available_device_pin() FAILED\n");
        num_warnings_++;
        continue;
      }
    }

    if (tr >= 2)
    {
      lprintf("--- writing pcf outputs (%u)\n", output_sz);
    }
    for (uint i = 0; i < output_sz; i++)
    {
      const string &outName = user_design_outputs_[i];
      if (tr >= 4)
      {
        lprintf("assigning user_design_output #%u %s\n", i, outName.c_str());
        if (csv_rd.hasCustomerInternalName(outName))
        {
          lprintf("\t (CustIntName) hasCustomerInternalName( %s )\n",
                  outName.c_str());
        }
      }
      assert(!outName.empty());
      device_pin_n_mode = get_available_device_pin(csv_rd, false /*OUTPUT*/);
      if (device_pin_n_mode.first.length())
      {
        pinName = csv_rd.bumpName2CustomerName(device_pin_n_mode.first);
        assert(!pinName.empty());

        set_io_str = user_design_outputs_[output_idx[i]];
        set_io_str.push_back(' ');
        set_io_str += pinName;
        set_io_str += " -mode ";
        set_io_str += device_pin_n_mode.second;

        temp_out << "set_io " << set_io_str << endl;
        if (user_out.is_open())
        {
          user_out << "set_io " << set_io_str << endl;
        }
      }
      else
      {
        if (tr >= 1)
          lputs("WARNING: (out) get_available_device_pin() FAILED\n");
        num_warnings_++;
        continue;
      }
    }

    return true;
  }

  // static
  void pin_location::shuffle_candidates(vector<int> &v)
  {
    static random_device rd;
    static mt19937 g(rd());
    std::shuffle(v.begin(), v.end(), g);
    return;
  }

  /*
  void pin_location::collect_left_available_device_pins(
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

  bool pin_location::is_input_mode(const string &mode_name) const
  {
    if (mode_name.empty())
      return false;

    size_t dash_loc = mode_name.rfind("_");
    if (dash_loc == string::npos)
    {
      return false;
    }
    string post_fix = mode_name.substr(dash_loc, mode_name.length() - 1);
    if (post_fix == INPUT_MODE_FIX || post_fix == GPIO_MODE_FIX)
    {
      return true;
    }

    return false;
  }

  bool pin_location::is_output_mode(const string &mode_name) const
  {
    if (mode_name.empty())
      return false;

    size_t dash_loc = mode_name.rfind("_");
    if (dash_loc == string::npos)
    {
      return false;
    }
    string post_fix = mode_name.substr(dash_loc, mode_name.length() - 1);
    if (post_fix == OUTPUT_MODE_FIX || post_fix == GPIO_MODE_FIX)
    {
      return true;
    }

    return false;
  }

  static bool read_json_ports(const nlohmann::ordered_json &from,
                              vector<pin_location::PortInfo> &ports)
  {
    ports.clear();

    nlohmann::ordered_json portsObj = from["ports"];
    if (!portsObj.is_array())
      return false;

    uint16_t tr = ltrace();
    auto &ls = lout();

    if (tr >= 3)
      ls << " .... portsObj.size() = " << portsObj.size() << endl;

    string s;
    for (auto I = portsObj.cbegin(); I != portsObj.cend(); ++I)
    {
      const nlohmann::ordered_json &obj = *I;
      if (obj.is_array())
      {
        ls << " (WW) unexpected json array..." << endl;
        continue;
      }

      // 1. check size and presence of 'name'
      if (tr >= 4)
        ls << " ...... obj.size() " << obj.size() << endl;
      if (obj.size() == 0)
      {
        ls << " (WW) unexpected json object size..." << endl;
        continue;
      }
      if (not obj.contains("name"))
      {
        ls << " (WW) expected obj.name string..." << endl;
        continue;
      }
      if (not obj["name"].is_string())
      {
        ls << " (WW) expected obj.name string..." << endl;
        continue;
      }

      ports.emplace_back();

      // 2. read 'name'
      pin_location::PortInfo &last = ports.back();
      last.name_ = obj["name"];
      if (tr >= 4)
        ls << " ........ last.name_ " << last.name_ << endl;

      // 3. read 'direction'
      if (obj.contains("direction"))
      {
        s = strToLower(obj["direction"]);
        if (s == "input")
          last.dir_ = pin_location::PortDir::Input;
        else if (s == "output")
          last.dir_ = pin_location::PortDir::Output;
      }

      // 4. read 'type'
      if (obj.contains("type"))
      {
        s = strToLower(obj["type"]);
        if (s == "wire")
          last.type_ = pin_location::PortType::Wire;
        else if (s == "reg")
          last.type_ = pin_location::PortType::Reg;
      }

      // 5. read 'range'
      if (obj.contains("range"))
      {
        const auto &rangeObj = obj["range"];
        if (rangeObj.size() == 2 && rangeObj.contains("lsb") &&
            rangeObj.contains("msb"))
        {
          if (tr >= 5)
            ls << " ........ has range..." << endl;
          const auto &lsb = rangeObj["lsb"];
          const auto &msb = rangeObj["msb"];
          if (lsb.is_number_integer() && msb.is_number_integer())
          {
            int l = lsb.get<int>();
            int m = msb.get<int>();
            if (l >= 0 && m >= 0)
            {
              if (tr >= 4)
                ls << " ........ has valid range  l= " << l << "  m= " << m
                   << endl;
              last.range_.set(l, m);
            }
          }
        }
      }

      // 6. read 'define_order'
      if (obj.contains("define_order"))
      {
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
  bool pin_location::read_port_info(std::ifstream &ifs, vector<string> &inputs,
                                    vector<string> &outputs)
  {
    inputs.clear();
    outputs.clear();
    if (!ifs.is_open())
      return false;

    uint16_t tr = ltrace();
    auto &ls = lout();

    nlohmann::ordered_json rootObj;

    try
    {
      ifs >> rootObj;
    }
    catch (...)
    {
      CERROR << "(EE) Failed json input stream reading" << endl;
      return false;
    }

    size_t root_sz = rootObj.size();
    if (tr >= 2)
    {
      ls << " rootObj.size() = " << root_sz
         << "  rootObj.is_array() : " << std::boolalpha << rootObj.is_array()
         << endl;
    }

    if (root_sz == 0)
    {
      CERROR << "(EE) root_sz == 0" << endl;
      return false;
    }

    std::vector<PortInfo> ports;
    if (rootObj.is_array())
      read_json_ports(rootObj[0], ports);
    else
      read_json_ports(rootObj, ports);

    if (tr >= 2)
      ls << " got ports.size()= " << ports.size() << endl;

    if (ports.empty())
      return false;

    uint num_inp = 0, num_out = 0;
    for (const PortInfo &p : ports)
    {
      if (p.isInput())
        num_inp++;
      else if (p.isOutput())
        num_out++;
    }
    inputs.reserve(num_inp);
    outputs.reserve(num_out);

    for (const PortInfo &p : ports)
    {
      if (p.isInput())
      {
        if (p.name_.length())
          inputs.push_back(p.name_);
        else
          ls << "WARNING: got empty name in port_info" << endl;
      }
      else if (p.isOutput())
      {
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

  bool pin_location::write_logical_clocks_to_physical_clks()
  {
    std::vector<std::string> set_clks;
    string clkmap_file_name = cl_.get_param("--clk_map");
    std::ifstream file(clkmap_file_name);
    std::string command;
    std::vector<std::string> commands;
    if (file.is_open())
    {
      while (getline(file, command))
      {
        commands.push_back(command);
      }
      file.close();
    }
    else
    {
      return false;
    }

    // Tokenize each command
    std::vector<std::vector<std::string>> tokens;
    for (const auto &cmd : commands)
    {
      std::stringstream ss(cmd);
      std::vector<std::string> cmd_tokens;
      std::string token;
      while (ss >> token)
      {
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
    if (!infile.is_open())
    {
      std::cerr << "ERROR: cound not open the file " << in_fn << std::endl;
      return false;
    }
    // Load the XML file
    pugi::xml_parse_result result = doc.load_file(in_fn.c_str());
    if (!result)
    {
      std::cerr << "Error loading repack constraints XML file: " << result.description() << '\n';
      return 1;
    }

    for (const auto &cmd_tokens : tokens)
    {
      for (const auto &token : cmd_tokens)
      {
        if (token == "-device_clock")
        {
          d_c = true;
          p_c = false;
          continue;
        }
        else if (token == "-design_clock")
        {
          d_c = false;
          p_c = true;
          continue;
        }
        if (d_c)
        {
          device_clk.push_back(token);
        }

        if (p_c)
        {
          design_clk.push_back(token);
        }
      }
    }

    std::map<std::string, std::string> userPins;

    for (size_t k = 0; k < device_clk.size(); k++)
    {
      userPin = device_clk[k];
      userNet = design_clk[k];

      if (userNet.empty())
      {
        userNet = "OPEN";
      }
      userPins[userPin] = userNet;
    }

    // If the user does not provide a net name, set it to "open"
    if (userNet.empty())
    {
      userNet = "OPEN";
    }

    // Update the XML file
    for (pugi::xml_node node : doc.child("repack_design_constraints").children("pin_constraint"))
    {
      auto it = userPins.find(node.attribute("pin").value());
      if (it != userPins.end())
      {
        node.attribute("net").set_value(it->second.c_str());
      }
      else
      {
        node.attribute("net").set_value("OPEN");
      }
    }

    // Save the updated XML file
    doc.save_file(out_fn.c_str(), "", pugi::format_no_declaration);
    // removing clkmap_file which was used to read set_clock_pin command
    remove(clkmap_file_name.c_str());
    return true;
  }

} // namespace pinc
