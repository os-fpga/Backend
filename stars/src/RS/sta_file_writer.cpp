
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
#include "sta_lib_data.h"
#include "sta_lib_writer.h"
#include "rsDB.h"
#include "WriterVisitor.h"

namespace rsbe {

using std::cout;
using std::endl;
using std::string;
using std::stringstream;
using std::ostream;
using namespace pinc;

bool sta_file_writer::do_files(int argc, const char** argv) {
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2)
    lprintf("sta_file_writer::do_files( argc=%i )\n", argc);

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
    ls << "    g_vpr_ctx.atom().nlist.netlist_name()  design_name_ = "
       << design_name_ << endl;
  }

  string lib_fn = design_name_ + "_stars.lib";
  string verilog_fn = design_name_ + "_stars.v";
  string sdf_fn = design_name_ + "_stars.sdf";
  string sdc_fn = design_name_ + "_stars.sdc";

  std::ofstream lib_os(lib_fn);
  if (!lib_os.is_open()) {
    ls << "\n[Error] STARS: failed to open for writing: " << lib_fn << endl;
    return false;
  }

  std::ofstream verilog_os(verilog_fn);
  if (!verilog_os.is_open()) {
    ls << "\n[Error] STARS: failed to open for writing: " << verilog_fn << endl;
    return false;
  }

  std::ofstream sdf_os(sdf_fn);
  if (!sdf_os.is_open()) {
    ls << "\n[Error] STARS: failed to open for writing: " << sdf_fn << endl;
    return false;
  }

  std::ofstream sdc_os(sdc_fn);
  if (!sdc_os.is_open()) {
    ls << "\n[Error] STARS: failed to open for writing: " << sdc_fn << endl;
    return false;
  }

  // timing data preparation
  auto& cluster_ctx = g_vpr_ctx.clustering();
  auto net_delay = make_net_pins_matrix<float>((const Netlist<>&)cluster_ctx.clb_nlist);

  load_net_delay_from_routing((const Netlist<>&)g_vpr_ctx.clustering().clb_nlist, net_delay, true);

  auto analysis_delay_calc =
      std::make_shared<AnalysisDelayCalculator>(atom_ctx.nlist, atom_ctx.lookup, net_delay, true);

  // walk netlist to dump info
  StaWriterVisitor visitor(verilog_os, sdf_os, lib_os, analysis_delay_calc, get_vprSetup()->AnalysisOpts);
  NetlistWalker nl_walker(visitor);
  nl_walker.walk();

  // sdc file
  // sdc file is a copy of sdc file which is used by vpr
  // more and complete sdc file should be developed later
  for (int i = 0; i < argc; i++) {
    if (std::strcmp(argv[i], "--sdc_file") == 0) {
      std::ifstream in_file(argv[i + 1]);
      string line;
      if (in_file) {
        while (getline(in_file, line)) {
          sdc_os << line << endl;
        }
      }
      in_file.close();
      break;
    }
  }

  ls << "STARS: Created files <" << lib_fn << ">, <" << verilog_fn << ">, <" << sdf_fn
     << ">, <" << sdc_fn << ">." << endl;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// API entry point
////////////////////////////////////////////////////////////////////////////////
bool sta_file_writer::create_files(int argc, const char** argv) {
  sta_file_writer wr;
  bool wr_ok = wr.do_files(argc, argv);
  if (!wr_ok || ltrace() >= 2) {
    lout() << "wr_ok: " << std::boolalpha << wr_ok << endl;
  }
  return wr_ok;
}

}  // NS rsbe
