
#include "pinc_log.h"

// vpr/src/base
#include "atom_netlist.h"
#include "atom_netlist_utils.h"
#include "globals.h"
#include "logic_vec.h"
#include "netlist_walker.h"
#include "read_blif.h"
#include "vpr_types.h"
#include "vtr_version.h"

// vpr/src/timing
#include "AnalysisDelayCalculator.h"
#include "net_delay.h"

#include "rsGlobal.h"
#include "rsVPR.h"
#include "sta_file_writer.h"
#include "LCell.h"
#include "sta_lib_writer.h"
#include "rsDB.h"
#include "WriterVisitor.h"

#include "rsFio.h"
#include <filesystem>

namespace rsbe {

using std::cout;
using std::endl;
using std::string;
using std::stringstream;
using std::ostream;
using namespace pinc;

bool FileWriter::do_files(int argc, const char** argv) {
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2)
    lprintf("FileWriter::do_files( argc=%i )\n", argc);

  // vpr context
  auto& atom_ctx = g_vpr_ctx.atom();
  //auto& timing_ctx = g_vpr_ctx.timing();
  design_name_ = atom_ctx.nlist.netlist_name();
  if (design_name_.empty()) {
    ls << "\n[Error] STARS: design_name is empty\n" << endl;
    return false;
  }

  // io initialization
  ls << "STARS: Input design <" << design_name_ << ">" << endl;
  if (tr >= 3) {
    ls << "  g_vpr_ctx.atom().nlist.netlist_name()  design_name_ = "
       << design_name_ << endl;
  }

  lib_fn_ = design_name_ + "_stars.lib";
  verilog_fn_ = design_name_ + "_stars.v";
  sdf_fn_ = design_name_ + "_stars.sdf";
  sdc_fn_ = design_name_ + "_stars.sdc";

  std::ofstream lib_os(lib_fn_);
  if (!lib_os.is_open()) {
    ls << "\n[Error] STARS: failed to open for writing: " << lib_fn_ << endl;
    return false;
  }

  std::ofstream verilog_os(verilog_fn_);
  if (!verilog_os.is_open()) {
    ls << "\n[Error] STARS: failed to open for writing: " << verilog_fn_ << endl;
    return false;
  }

  std::ofstream sdf_os(sdf_fn_);
  if (!sdf_os.is_open()) {
    ls << "\n[Error] STARS: failed to open for writing: " << sdf_fn_ << endl;
    return false;
  }

  // timing data preparation
  auto& cluster_ctx = g_vpr_ctx.clustering();
  auto net_delay = make_net_pins_matrix<float>((const Netlist<>&)cluster_ctx.clb_nlist);

  load_net_delay_from_routing((const Netlist<>&)g_vpr_ctx.clustering().clb_nlist, net_delay);

  AnalysisDelayCalculator* an_del_calc = new AnalysisDelayCalculator(atom_ctx.nlist, atom_ctx.lookup, net_delay, true);

  // walk netlist to dump info
  WriterVisitor visitor(verilog_os, sdf_os, lib_os, an_del_calc, get_vprSetup()->AnalysisOpts);
  NetlistWalker nl_walker(visitor);
  nl_walker.walk();

  // sdc file
  // sdc file is a copy of sdc file which is used by vpr
  // more and complete sdc file should be developed later
  for (int i = 0; i < argc - 1; i++) {
    if (std::strcmp(argv[i], "--sdc_file"))
      continue;

    const char* sdc_s = argv[i + 1];
    assert(sdc_s);
    if (!sdc_s)
      break;
    fio::Info inf{sdc_s};
    if (not inf.exists_) {
      ls << "\nSTARS [Warning] input SDC file does not exist: " << sdc_s << '\n' << endl;
      break;
    }
    if (not inf.accessible_) {
      ls << "\nSTARS [Warning] input SDC file is not accessible: " << sdc_s << '\n' << endl;
      break;
    }

    std::ofstream sdc_os(sdc_fn_);
    if (!sdc_os.is_open()) {
      ls << "\n[Error] STARS: failed to open SDC for writing: " << sdc_fn_ << '\n' << endl;
      break;
    }

    if (tr >= 3) {
      ls << "STARS: Copying SDC file...\n"
         << " from: " << sdc_s << '\n'
         << "   to: " << sdc_fn_ << endl;
    }

    std::ifstream in_file(sdc_s);
    string line;
    if (in_file) {
      while (getline(in_file, line)) {
        sdc_os << line << endl;
      }
    }
    in_file.close();
    break;

  }

  return true;
}

void FileWriter::printStats() const {
  uint16_t tr = ltrace();
  auto& ls = lout();
  ls << "STARS: Created files <"
     << lib_fn_ << ">, <"
     << verilog_fn_ << ">, <"
     << sdf_fn_ << ">, <"
     << sdc_fn_ << ">." << endl;

  if (tr < 2)
    return;

  bool exist = false;

  exist = fio::regular_file_exists(lib_fn_);
  if (exist) {
    fio::Info inf{lib_fn_};
    ls << "(stars) .lib: " << lib_fn_
       << "  size= " << inf.size_
       << "  abs: " << inf.absName_ << endl;
  } else {
    ls << "(stars) .lib ABSENT: " << lib_fn_ << " ABSENT" << endl;
  }

  exist = fio::regular_file_exists(verilog_fn_);
  if (exist) {
    fio::Info inf{verilog_fn_};
    ls << "(stars) verilog: " << verilog_fn_
       << "  size= " << inf.size_
       << "  abs: " << inf.absName_ << endl;
  } else {
    ls << "(stars) verilog ABSENT: " << verilog_fn_ << " ABSENT" << endl;
  }

  exist = fio::regular_file_exists(sdf_fn_);
  if (exist) {
    fio::Info inf{sdf_fn_};
    ls << "(stars) .sdf: " << sdf_fn_
       << "  size= " << inf.size_
       << "  abs: " << inf.absName_ << endl;
  } else {
    ls << "(stars) .sdf ABSENT: " << sdf_fn_ << " ABSENT" << endl;
  }

  exist = fio::regular_file_exists(sdc_fn_);
  if (exist) {
    fio::Info inf{sdc_fn_};
    ls << "(stars) .sdc: " << sdc_fn_
       << "  size= " << inf.size_
       << "  abs: " << inf.absName_ << endl;
  } else {
    ls << "(stars) .sdc ABSENT: " << sdc_fn_ << " ABSENT" << endl;
  }
}

////////////////////////////////////////////////////////////////////////////////
// API entry point
////////////////////////////////////////////////////////////////////////////////
bool FileWriter::create_files(int argc, const char** argv) {
  FileWriter wr;
  bool wr_ok = wr.do_files(argc, argv);
  if (!wr_ok || ltrace() >= 2) {
    lout() << "wr_ok: " << std::boolalpha << wr_ok << endl;
  }
  if (wr_ok) {
    wr.printStats();
  } else {
    lout()    << "[Error] stars failed." << endl;
    std::cerr << "[Error] stars failed." << endl;
  }
  return wr_ok;
}

}  // NS rsbe
