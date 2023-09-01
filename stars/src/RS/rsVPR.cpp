/**
 * This is a wrapper file for VPR API. Mirrored from the main.cpp of vpr
 */

/**
 * VPR program
 * Generate FPGA architecture given architecture description
 * Pack, place, and route circuit into FPGA architecture
 * Electrical timing analysis on results
 *
 * Overall steps
 * 1.  Initialization
 * 2.  Pack
 * 3.  Place-and-route and timing analysis
 * 4.  Clean up
 */

#include "pinc_log.h"

#include "vtr_error.h"
#include "vtr_log.h"
#include "vtr_memory.h"
#include "vtr_time.h"

#include "tatum/error.hpp"

#include "vpr_api.h"
#include "vpr_error.h"
#include "vpr_exit_codes.h"
#include "vpr_signal_handler.h"
#include "vpr_tatum_error.h"
#include "vpr_types.h"

// VPR header: vpr/src/base/globals.h
// #include "base/globals.h"
#include "globals.h"

#include "rsGlobal.h"
#include "rsVPR.h"

namespace rsbe {

static t_vpr_setup s_vpr_setup;

t_vpr_setup* get_vprSetup() noexcept { return &s_vpr_setup; }

int vpr4stars(int argc, char** argv) {
  assert(argc);
  assert(argv);

  vtr::ScopedFinishTimer tmonitor("The entire flow of VPR");

  t_options opa;

  t_arch* ar = new t_arch;

  try {
    vpr_init(argc, (const char**)argv, &opa, rsbe::get_vprSetup(), ar);

    if (opa.show_version) {
      return 0;
    }

    bool flow_succeeded = vpr_flow(rsbe::s_vpr_setup, *ar);
    if (!flow_succeeded) {
      VTR_LOG("VPR failed to implement circuit\n");
      return UNIMPLEMENTABLE_EXIT_CODE;
    }

    auto& timing_ctx = g_vpr_ctx.timing();
    print_timing_stats("Flow", timing_ctx.stats);

    VTR_LOG("VPR suceeded\n");

  } catch (const tatum::Error& tatum_error) {
    VTR_LOG_ERROR("%s\n", format_tatum_error(tatum_error).c_str());

    return ERROR_EXIT_CODE;

  } catch (const VprError& vpr_error) {
    vpr_print_error(vpr_error);

    if (vpr_error.type() == VPR_ERROR_INTERRUPTED) {
      return INTERRUPTED_EXIT_CODE;
    } else {
      return ERROR_EXIT_CODE;
    }

  } catch (const vtr::VtrError& vtr_error) {
    VTR_LOG_ERROR("%s:%d %s\n", vtr_error.filename_c_str(), vtr_error.line(), vtr_error.what());

    return ERROR_EXIT_CODE;
  }

  return 0;
}

int do_vpr(const rsOpts& opts) {
  int argc = opts.vprArgc_;
  const char** argv = (const char**)opts.vprArgv_;
  assert(argc);
  assert(argv);
  if (argc <= 0 || !argv) {
    std::cout << std::endl;
    std::cerr << " [Error] do_vpr() ASSERT" << std::endl;
    std::cout << " [Error] do_vpr() ASSERT\n" << std::endl;
    return 1;
  }

  vtr::ScopedFinishTimer t("The entire flow of VPR");

  t_options Options = t_options();
  t_arch Arch = t_arch();
  t_vpr_setup vpr_setup = t_vpr_setup();

  try {
    vpr_install_signal_handler();

    /* Read options, architecture, and circuit netlist */
    vpr_init(argc, argv, &Options, &vpr_setup, &Arch);
    const Netlist<>& net_list = vpr_setup.RouterOpts.flat_routing
                                    ? (const Netlist<>&)g_vpr_ctx.atom().nlist
                                    : (const Netlist<>&)g_vpr_ctx.clustering().clb_nlist;
    if (Options.show_version) {
      //vpr_free_all(net_list, Arch, vpr_setup); // TODO
      return SUCCESS_EXIT_CODE;
    }

    bool flow_succeeded = vpr_flow(vpr_setup, Arch);
    if (!flow_succeeded) {
      VTR_LOG("VPR failed to implement circuit\n");
      vpr_free_all(net_list, Arch, vpr_setup);
      return UNIMPLEMENTABLE_EXIT_CODE;
    }

    auto& timing_ctx = g_vpr_ctx.timing();
    print_timing_stats("Flow", timing_ctx.stats);

    /* free data structures */
    vpr_free_all(net_list, Arch, vpr_setup);

    VTR_LOG("VPR succeeded\n");

  } catch (const tatum::Error& tatum_error) {
    VTR_LOG_ERROR("%s\n", format_tatum_error(tatum_error).c_str());
    auto net_list = vpr_setup.RouterOpts.flat_routing ? (const Netlist<>&)g_vpr_ctx.atom().nlist
                                                      : (const Netlist<>&)g_vpr_ctx.clustering().clb_nlist;
    vpr_free_all(net_list, Arch, vpr_setup);

    return ERROR_EXIT_CODE;

  } catch (const VprError& vpr_error) {
    vpr_print_error(vpr_error);
    auto net_list = vpr_setup.RouterOpts.flat_routing ? (const Netlist<>&)g_vpr_ctx.atom().nlist
                                                      : (const Netlist<>&)g_vpr_ctx.clustering().clb_nlist;
    if (vpr_error.type() == VPR_ERROR_INTERRUPTED) {
      vpr_free_all(net_list, Arch, vpr_setup);
      return INTERRUPTED_EXIT_CODE;
    } else {
      vpr_free_all(net_list, Arch, vpr_setup);
      return ERROR_EXIT_CODE;
    }

  } catch (const vtr::VtrError& vtr_error) {
    VTR_LOG_ERROR("%s:%d %s\n", vtr_error.filename_c_str(), vtr_error.line(), vtr_error.what());
    auto net_list = vpr_setup.RouterOpts.flat_routing ? (const Netlist<>&)g_vpr_ctx.atom().nlist
                                                      : (const Netlist<>&)g_vpr_ctx.clustering().clb_nlist;
    vpr_free_all(net_list, Arch, vpr_setup);

    return ERROR_EXIT_CODE;
  }

  /* Signal success to scripts */
  return SUCCESS_EXIT_CODE;
}

}  // NS rsbe
