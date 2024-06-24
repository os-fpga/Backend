//
// RS only - user want to map its design clocks to gemini fabric
// clocks. like gemini has 16 clocks clk[0],clk[1]....,clk[15]. And user clocks
// are clk_a,clk_b and want to map clk_a with clk[15] like it
// in such case, we need to make sure a xml repack constraint file is properly
// generated to guide bitstream generation correctly.
//

#include "pin_loc/pin_placer.h"
#include "file_readers/blif_reader.h"
#include "file_readers/pln_csv_reader.h"
#include "file_readers/xml_reader.h"
#include "file_readers/pln_Fio.h"
#include "util/cmd_line.h"

#include <map>
#include <filesystem>
#include <unistd.h>

namespace pln {

using namespace std;
using fio::Fio;

#define CERROR std::cerr << "[Error] "

int PinPlacer::map_clocks() {
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 4) {
    lputs();
    lputs("PinPlacer::map_clocks()..");
  }

  string fpga_repack = cl_.get_param("--read_repack");
  string user_constrained_repack = cl_.get_param("--write_repack");
  string clk_map_file = cl_.get_param("--clk_map");

  bool constraint_xml_requested = fpga_repack.size() or clk_map_file.size();
  if (not constraint_xml_requested) {
    if (tr >= 4)
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
  string cur_dir = get_CWD();
  if (tr >= 3) {
    lputs("pin_c:  write_clocks_logical_to_physical()..");
    lprintf("pin_c:  current directory= %s\n", cur_dir.c_str());
  }

  bool rd_ok = false, wr_ok = false;
  string clkmap_fn = cl_.get_param("--clk_map");

  if (not Fio::regularFileExists(clkmap_fn)) {
    CERROR << " no such file (--clk_map): " << clkmap_fn << endl;
    ls << " [Error] no such file (--clk_map): " << clkmap_fn << endl;
    return -1;
  }

  if (tr >= 4) {
    ls << "  clkmap_fn : " << clkmap_fn << endl;
    fio::MMapReader mrd(clkmap_fn);
    rd_ok = mrd.read();
    if (rd_ok) {
      int64_t numL = mrd.countLines();
      ls << "  size= " << mrd.fsz_ << "  #lines= " << numL << endl;
      if (numL > 0 && tr >= 4) {
        ls << "=========== lines of " << mrd.fileName() << " ===" << endl;
        mrd.printLines(ls);
        ls << "===========" << endl;
      }
    } else {
      ls << "\n pin_c WARNING: --clk_map file is not readable: " <<  clkmap_fn << endl;
    }
    ls << endl;
  }

  // read clkmap file
  fio::LineReader lr(clkmap_fn);
  rd_ok = lr.read(true, true);
  if (tr >= 4)
    lprintf("\t  rd_ok : %i\n", rd_ok);
  if (not rd_ok) {
    CERROR << " error reading file (--clk_map): " << clkmap_fn << endl;
    ls << " [Error] error reading file (--clk_map): " << clkmap_fn << endl;
    return -1;
  }
  if (not lr.hasLines()) {
    CERROR << " empty file (--clk_map): " << clkmap_fn << endl;
    ls << " [Error] empty file (--clk_map): " << clkmap_fn << endl;
    return -1;
  }

  vector<string> commands;
  lr.copyLines(commands);

  if (tr >= 5) {
    ls << "    commands.size()= " << commands.size() << endl;
  }

  // Tokenize each command
  vector<vector<string>> tokenized_cmds;
  tokenized_cmds.reserve(commands.size());
  for (const string& cmd : commands) {
    const char* cs = cmd.c_str();
    if (Fio::isEmptyLine(cs))
      continue;
    tokenized_cmds.emplace_back();
    vector<string>& words = tokenized_cmds.back();
    Fio::split_spa(cs, words);
  }

  if (tr >= 5) {
    ls << "    tokenized_cmds.size()= " << tokenized_cmds.size() << endl;
  }

  string out_fn = cl_.get_param("--write_repack");
  if (tr >= 4) {
    ls << "  out_fn (--write_repack): " << out_fn << endl;
  }

  string in_fn = cl_.get_param("--read_repack");
  if (tr >= 4) {
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
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(in_fn.c_str());
  if (!result) {
    CERROR << " Error loading repack constraints XML file: "
           << result.description() << '\n';
    return -1;
  }

  vector<string> udes_clocks; // user design clocks
  vector<string> pdev_clocks; // physical device clocks
  udes_clocks.reserve(tokenized_cmds.size());
  pdev_clocks.reserve(tokenized_cmds.size());

  for (const auto& tcmd : tokenized_cmds) {
    assert(tcmd.size() < USHRT_MAX);
    uint sz = tcmd.size();
    if (tr >= 5) {
      logVec(tcmd, "      tcmd: ");
      lprintf("\t  tcmd.size()= %u\n", sz);
    }
    if (sz < 2)
      continue;
    for (uint j = 0; j < sz - 1; j++) {
      const string& tk = tcmd[j];
      if (tk == "-device_clock") {
        pdev_clocks.push_back(tcmd[j + 1]);
        j++;
        continue;
      }
      if (tk == "-design_clock") {
        udes_clocks.push_back(tcmd[j + 1]);
        j++;
        continue;
      }
    }
  }
  if (tr >= 4) {
    lprintf("\n    udes_clocks.size()= %zu  pdev_clocks.size()= %zu\n",
        udes_clocks.size(), pdev_clocks.size());
    logVec(udes_clocks, "  udes_clocks: ");
    logVec(pdev_clocks, "  pdev_clocks: ");
  }
  assert(udes_clocks.size() == pdev_clocks.size());
  if (udes_clocks.empty()) {
    CERROR << " no clocks in file (--clk_map): " << clkmap_fn << endl;
    ls << " [Error] no clocks in file (--clk_map): " << clkmap_fn << endl;
    return -1;
  }

  std::map<string, string> dpin2unet;

  for (size_t i = 0; i < pdev_clocks.size(); i++) {
    string dpin = pdev_clocks[i];
    string unet = udes_clocks[i];
    if (unet.empty())
      unet = "OPEN";
    dpin2unet[dpin] = unet;
  }

  if (tr >= 4) {
    lprintf("\n    dpin2unet.size()= %zu\n", dpin2unet.size());
  }

  // Save the original XML file for diff-ing
  if (tr >= 5) {
    lputs();
    string debug_fn = str::concat("debug_origXML__", out_fn);
    ls << "pin_c-debug:  writing original XML: " << debug_fn << endl;
    try {
      wr_ok = doc.save_file(debug_fn.c_str(), "", pugi::format_no_declaration);
    } catch (...) {
      wr_ok = false;
    }
    if (wr_ok) {
      ls << "pin_c-debug:  written OK: " << debug_fn << endl;
      if (cur_dir.length()) {
        ls << "full path: " << cur_dir << '/' << debug_fn << endl;
        ls << "input was: " << in_fn << endl;
      }
    } else {
      ls << "pin_c-debug:  failed writing debug-XML: " << debug_fn << endl;
    }
    lputs();
  }

  // Update the doc
  for (auto node : doc.child("repack_design_constraints").children("pin_constraint")) {
    auto it = dpin2unet.find(node.attribute("pin").value());
    if (it != dpin2unet.end()) {
      node.attribute("net").set_value(it->second.c_str());
    } else {
      node.attribute("net").set_value("OPEN");
    }
  }

  // Save the updated XML file
  if (tr >= 4) {
    ls << "pin_c:  writing XML: " << out_fn << endl;
    if (tr >= 6)
      lprintf("pin_c:  current directory= %s\n", cur_dir.c_str());
  }
  try {
    wr_ok = doc.save_file(out_fn.c_str(), "", pugi::format_no_declaration);
    if (tr >= 4) ls << "OK: doc.save_file() succeeded" << endl;
  } catch (...) {
    wr_ok = false;
  }
  if (wr_ok) {
    if (tr >= 3) {
      ls << "pin_c:  written OK: " << out_fn << endl;
      if (cur_dir.length()) {
        ls << "full path: " << cur_dir << '/' << out_fn << endl;
        ls << "input was: " << in_fn << endl;
      }
    }
  } else {
    ls << "FAIL:  doc.save_file() failed" << endl;
    ls << "pin_c:  failed writing XML: " << out_fn << endl;
  }

  if (tr >= 4) {
    lprintf("keeping clock-map file for debugging: %s\n", clkmap_fn.c_str());
  } else {
    if (tr >= 3)
      lprintf("pin_c:  removed clock-map file: %s\n", clkmap_fn.c_str());
    std::filesystem::remove(clkmap_fn);
  }

  if (tr >= 4)
    lprintf("pin_c:  current directory= %s\n", cur_dir.c_str());

  return 1;
}

} // namespace pln

