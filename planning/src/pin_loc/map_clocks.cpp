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

#undef h
#undef H

namespace pln {

using namespace std;
using fio::Fio;

#define CERROR std::cerr << "[Error] "

const PinPlacer::EditItem* PinPlacer::findTerminalLink(const string& pinName) const noexcept {
  if (pinName.empty() or all_edits_.empty())
    return nullptr;
  size_t h = str::hashf(pinName.c_str());

  int found_old = -1, cnt_old = 0;
  int found_new = -1, cnt_new = 0;

  const EditItem* A = all_edits_.data();
  for (int i = int(all_edits_.size()) - 1; i >= 0; i--) {
    const EditItem& itm = A[i];
    //if ( strstr(itm.c_old(), "design_clk_9") or strstr(itm.c_new(), "design_clk_9"))
    //  lputs9();
    if (itm.oldPinHash_ == h and itm.oldPin_ == pinName) {
      found_old = i;
      cnt_old++;
      continue;
    }
    if (itm.newPinHash_ == h and itm.newPin_ == pinName) {
      found_new = i;
      cnt_new++;
    }
  }

  // if found > 1 times, then the link is not terminal
  if (cnt_old == 1) {
    assert(found_old >= 0);
    return A + found_old;
  }
  if (cnt_new == 1) {
    assert(found_new >= 0);
    return A + found_new;
  }

  return nullptr;
}

int PinPlacer::map_clocks() {
  clk_map_file_.clear();
  uint16_t tr = ltrace();
  if (tr >= 4) {
    flush_out(true);
    lputs("PinPlacer::map_clocks()..");
  }

  string fpga_repack = cl_.get_param("--read_repack");
  string user_constrained_repack = cl_.get_param("--write_repack");
  clk_map_file_ = cl_.get_param("--clk_map");

  bool constraint_xml_requested = fpga_repack.size() or clk_map_file_.size();
  if (not constraint_xml_requested) {
    if (tr >= 4)
      lputs("PinPlacer::map_clocks() returns NOP");
    flush_out(false);
    return 0;
  }

  if (write_clocks_logical_to_physical() < 0) {
    CERROR << err_lookup("FAIL_TO_CREATE_CLKS_CONSTRAINT_XML") << endl;
    return -1;
  }

  if (tr >= 3)
    lputs("PinPlacer::map_clocks() returns OK");
  flush_out(false);
  return 1;
}

static bool is_post_edit_clock(CStr cs, size_t len) noexcept {
  if (!cs or !cs[0])
    return false;
  if (len < 18)
    return false;
  if (cs[0] != '$')
    return false;
  if (cs[1] != 'a')
    return false;

  // len("$auto$clkbufmap") == 15
  if (::strncmp(cs, "$auto$clkbufmap", 15) == 0)
    return true;
  return false;
}

int PinPlacer::write_clocks_logical_to_physical() {
  flush_out(false);
  uint16_t tr = ltrace();
  auto& ls = lout();
  string cur_dir = get_CWD();
  if (tr >= 3) {
    lputs("pin_c:  write_clocks_logical_to_physical()..");
    lprintf("pin_c:  current directory= %s\n", cur_dir.c_str());
  }

  bool rd_ok = false, wr_ok = false;

  clk_map_file_ = cl_.get_param("--clk_map");
  const string& clkmap_fn = clk_map_file_;

  if (not Fio::regularFileExists(clkmap_fn)) {
    flush_out(true);
    err_puts();
    lprintf2("[Error] no such file (--clk_map): %s\n", clkmap_fn.c_str());
    err_puts();
    flush_out(true);
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
    flush_out(true);
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

  string uclk, postEdit_uclk;
  for (const auto& tcmd : tokenized_cmds) {
    uclk.clear();
    postEdit_uclk.clear();
    assert(tcmd.size() < USHRT_MAX);
    uint sz = tcmd.size();
    if (tr >= 7) {
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
        uclk = udes_clocks.back();
        postEdit_uclk.clear();
        CStr cs = uclk.c_str();
        bool is_post_edit = is_post_edit_clock(cs, uclk.length());
        if (tr >= 6)
          lprintf("\t  uclk: %s  is_post_edit:%i\n", cs, is_post_edit);
        if (not is_post_edit) {
          bool is_input = true;
          const EditItem* terminal_link = findTerminalLink(uclk);
          if (terminal_link) {
            const EditItem& tl = *terminal_link;
            if (tl.dir_ < 0)
              is_input = false;
            if (tr >= 6) {
              lprintf(
                "\t (tl)  %zu   mod: %s  js_dir:%s  dir:%i    old: %s  new: %s  is_inp:%i\n",
                tl.nameHash(), tl.c_mod(), tl.c_jsdir(), tl.dir_, tl.c_old(), tl.c_new(), is_input);
            }
          }
          postEdit_uclk = translatePinName(uclk, is_input);
          if (tr >= 6)
            lprintf("\t\t  ..... postEdit_uclk: %s\n", postEdit_uclk.c_str());
          if (postEdit_uclk != uclk) {
            udes_clocks.back() = postEdit_uclk;
            if (tr >= 5) {
              lprintf("clock mapping: translated design_clock value from '%s' to '%s'\n",
                      cs, postEdit_uclk.c_str());
            }
          }
        }
        j++;
        continue;
      }
    }
  }

  if (tr >= 3) {
    flush_out(true);
    lprintf("clock mapping:  # user-design clocks = %zu  # device clocks = %zu",
            udes_clocks.size(), pdev_clocks.size());
  }
  if (tr >= 4) {
    flush_out(true);
    lprintf("    udes_clocks.size()= %zu  pdev_clocks.size()= %zu\n",
        udes_clocks.size(), pdev_clocks.size());
    flush_out(true);
    logVec(udes_clocks, "  udes_clocks: ");
    flush_out(true);
    logVec(pdev_clocks, "  pdev_clocks: ");
    flush_out(true);
  }

  if (udes_clocks.empty()) {
    flush_out(true); err_puts();
    lprintf2("[Error] no clocks in file (--clk_map): %s\n", clkmap_fn.c_str());
    err_puts(); flush_out(true);
    return -1;
  }
  if (udes_clocks.size() != pdev_clocks.size()) {
    flush_out(true); err_puts();
    lprintf2("[Error] reading --clk_map file: %s\n", clkmap_fn.c_str());
    flush_out(true); err_puts();
    lprintf2("[Error] number of user-design clocks (%zu) does not match number of device clocks (%zu)\n",
        udes_clocks.size(), pdev_clocks.size());
    err_puts(); flush_out(true);
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

  flush_out(false);
  return 1;
}

} // namespace pln

