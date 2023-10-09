
#include "pin_loc/pin_placer.h"

#include "file_readers/blif_reader.h"
#include "file_readers/pcf_reader.h"
#include "file_readers/rapid_csv_reader.h"
#include "file_readers/xml_reader.h"
#include "file_readers/Fio.h"
#include "util/cmd_line.h"

#include <set>
#include <map>
#include <filesystem>
#include <unistd.h>

namespace pinc {

using namespace std;
using fio::Fio;

#define CERROR std::cerr << "[Error] "

int PinPlacer::map_clocks() {
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 3)
    lputs("PinPlacer::map_clocks()..");

  // RS only - user want to map its design clocks to gemini fabric
  // clocks. like gemini has 16 clocks clk[0],clk[1]....,clk[15].And user clocks
  // are clk_a,clk_b and want to map clk_a with clk[15] like it
  // in such case, we need to make sure a xml repack constraint file is properly
  // generated to guide bitstream generation correctly.

  string fpga_repack = cl_.get_param("--read_repack");
  string user_constrained_repack = cl_.get_param("--write_repack");
  string clk_map_file = cl_.get_param("--clk_map");

  bool constraint_xml_requested = fpga_repack.size() or clk_map_file.size();
  if (not constraint_xml_requested) {
    if (tr >= 3)
      lputs("PinPlacer::map_clocks() returns NOP");
    return 0;
  }

  if (write_clocks_logical_to_physical() < 0) {
    CERROR << err_lookup("FAIL_TO_CREATE_CLKS_CONSTRAINT_XML") << endl;
    return -1;
  }

  if (tr >= 3)
    ls << "PinPlacer::map_clocks() returns OK" << endl;
  return 1;
}

int PinPlacer::write_clocks_logical_to_physical() {
  uint16_t tr = ltrace();
  auto& ls = lout();
  string cur_dir;
  if (tr >= 3) {
    lputs("pin_c:  write_clocks_logical_to_physical()..");
    char buf[5000] = {}; // unix max path is 4096
    if (::getcwd(buf, 4098)) {
      cur_dir = buf;
      lprintf("pin_c:  current directory= %s\n", buf);
    }
  }

  bool rd_ok = false;
  vector<string> set_clks;
  string clkmap_file_name = cl_.get_param("--clk_map");

  if (tr >= 4) {
    bool exi = Fio::regularFileExists(clkmap_file_name);
    ls << "    clkmap_file_name : " << clkmap_file_name
       << "  exists:" << int(exi);
    if (exi) {
      fio::MMapReader mrd(clkmap_file_name);
      rd_ok = mrd.read();
      if (rd_ok) {
        int64_t numL = mrd.countLines();
        ls << "  size= " << mrd.fsz_ << "  #lines= " << numL << endl;
        if (numL > 0) {
          ls << "=========== lines of " << mrd.fileName() << " ===" << endl;
          mrd.printLines(ls);
          ls << "===========" << endl;
        }
      }
    }
    ls << endl;
  }

  std::ifstream file(clkmap_file_name);
  string command;
  vector<string> commands;
  if (file.is_open()) {
    while (getline(file, command)) {
      commands.push_back(command);
    }
    file.close();
  } else {
    return -1;
  }

  if (tr >= 4) {
    ls << "    commands.size()= " << commands.size() << endl;
  }

  // Tokenize each command
  vector<vector<string>> tokens;
  for (const string& cmd : commands) {
    stringstream ss(cmd);
    vector<string> cmd_tokens;
    string token;
    while (ss >> token) {
      cmd_tokens.push_back(token);
    }
    tokens.push_back(cmd_tokens);
  }

  pugi::xml_document doc;
  vector<string> design_clk;
  vector<string> device_clk;
  string userPin;
  string userNet;
  bool d_c = false;
  bool p_c = false;
  string out_fn = cl_.get_param("--write_repack");
  if (tr >= 3) {
    ls << "  out_fn (--write_repack): " << out_fn << endl;
  }

  string in_fn = cl_.get_param("--read_repack");
  if (tr >= 3) {
    ls << "   in_fn (--read_repack): " << in_fn << endl;
    lprintf("pin_c:  current directory= %s\n", cur_dir.c_str());
  }
  if (not Fio::regularFileExists(in_fn)) {
    CERROR << " no such file (--read_repack): " << in_fn << endl;
    ls << " no such file (--read_repack): " << in_fn << endl;
    return -1;
  }
  fio::MMapReader reReader(in_fn);
  rd_ok = reReader.read();
  if (not rd_ok) {
    CERROR << " can't read file (--read_repack): " << in_fn << endl;
    ls << " can't read file (--read_repack): " << in_fn << endl;
    return -1;
  }
  if (tr >= 5) {
    int64_t numL = reReader.countLines();
    ls << "\n  size= " << reReader.fsz_ << "  #lines= " << numL << endl;
    if (numL > 0) {
      ls << "=========== lines of " << reReader.fileName() << " ===" << endl;
      reReader.printLines(ls);
      ls << "===========\n" << endl;
    }
    lprintf("pin_c:  current directory= %s\n", cur_dir.c_str());
  }

  // Load the XML file
  pugi::xml_parse_result result = doc.load_file(in_fn.c_str());
  if (!result) {
    CERROR << " Error loading repack constraints XML file: "
           << result.description() << '\n';
    return -1;
  }

  for (const auto& cmd_tokens : tokens) {
    for (const string& token : cmd_tokens) {
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

  std::map<string, string> userPins;

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
  bool wr_ok = false;
  if (tr >= 3) {
    ls << "pin_c:  writing XML: " << out_fn << endl;
    lprintf("pin_c:  current directory= %s\n", cur_dir.c_str());
  }
  try {
    doc.save_file(out_fn.c_str(), "", pugi::format_no_declaration);
    wr_ok = true;
    if (tr >= 4) ls << "OK: doc.save_file() succeeded" << endl;
  } catch (...) {
    ls << "FAIL: doc.save_file() failed" << endl;
    ls << "pin_c:  failed writing XML: " << out_fn << endl;
  }
  if (wr_ok && tr >= 3) {
    ls << "pin_c:  written OK: " << out_fn << endl;
    if (cur_dir.length()) {
      ls << "full path: " << cur_dir << '/' << out_fn << endl;
      ls << "input was: " << in_fn << endl;
    }
  }

  if (tr >= 4) {
    lprintf("keeping clock-map file for debugging: %s\n",
            clkmap_file_name.c_str());
  } else {
    remove(clkmap_file_name.c_str());
  }

  if (tr >= 3)
    lprintf("pin_c:  current directory= %s\n", cur_dir.c_str());

  return 1;
}

} // namespace pinc
