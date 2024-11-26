static const char* _pln_VERSION_STR = "pln0367";

#include "RS/rsEnv.h"
#include "RS/rsDeal.h"
#include "util/cmd_line.h"
#include "file_io/pln_Fio.h"
#include "pin_loc/pinc_main.h"

#include "globals.h"
#include "read_options.h"

#include "RS/rsVPR.h"
#include "RS/rsCheck.h"
#include "RS/sta_file_writer.h"

namespace pln {
  static rsEnv s_env;
  const rsEnv& rsEnv::inst() noexcept { return s_env; }
}

int main(int argc, char** argv) {
  using namespace pln;
  using std::cout;
  using std::endl;
  using std::string;

  rsOpts& opts = s_env.opts_;
  const char* trace = ::getenv("pln_trace");
  if (trace)
    set_ltrace(::atoi(trace));
  else
    set_ltrace(3);

  cout << s_env.traceMarker_ << endl;

  s_env.initVersions(_pln_VERSION_STR);

  s_env.parse(argc, argv);
  if (opts.trace_specified())
    set_ltrace(opts.trace_);

  uint16_t tr = ltrace();
  if (opts.trace_ >= 8 or tr >= 9 or ::getenv("pln_trace_env")) {
    s_env.dump("\n----env----\n");
    lout() << "-----------" << endl;
  }

  if (opts.ver_or_help()) {
    if (tr >= 2) {
      printf("\t %s\n", _pln_VERSION_STR);
      printf("\t compiled:    %s\n", s_env.compTimeCS());
    }
    deal_help(opts);
    pln::flush_out();
    std::quick_exit(0);
  }

  if (tr >= 2) {
    lprintf("\t %s\n", _pln_VERSION_STR);
    lprintf("\t compiled:  %s\n", s_env.compTimeCS());
  }
  if (tr >= 4) {
    lprintf("ltrace= %u  cmd.argc= %i\n", tr, argc);
  }

  if (opts.unit_specified()) {
    deal_units(opts);
    ::exit(0);
  }

  if (opts.check_ or opts.is_implicit_check()) {
    bool check_ok = deal_check(opts);
    if (check_ok)
      ::exit(0);
    else
      ::exit(1);
  }
  if (opts.cleanup_) {
    bool cleanup_ok = deal_cleanup(opts);
    if (cleanup_ok)
      ::exit(0);
    else
      ::exit(1);
  }

  int status = 1;
  bool ok = false;

  if (opts.is_fun_cmd()) {
    ok = deal_cmd(opts);
    if (ok) {
      if (tr >= 2) lputs("deal_cmd() succeeded.");
      status = 0;
    } else {
      if (tr >= 2) lputs("deal_cmd() failed.");
    }
    goto ret;
  }

  if (opts.is_fun_pinc() or opts.is_arg0_pinc() or opts.is_implicit_pinc()) {
    ok = deal_pinc(opts, true);
    if (ok) {
      if (tr >= 2) lputs("deal_pinc() succeeded.");
      status = 0;
    } else {
      if (tr >= 2) lputs("deal_pinc() failed.");
    }
    goto ret;
  }

  if (opts.is_fun_partition()) {
    lputs("======== PARTITION MODE specified ========");
    ok = validate_partition_opts(opts);
    if (!ok) {
      if (tr >= 4) lputs("validate_partition_opts() failed.");
      status = 2;
      goto ret;
    }
  }

  if (opts.is_fun_partition() or opts.is_fun_pack()) {
    ok = deal_vpr(opts, true);
    if (ok) {
      if (tr >= 2) lputs("deal_vpr() succeeded.");
      status = 0;
    } else {
      if (tr >= 2) lputs("deal_vpr() failed.");
    }
    goto ret;
  }

  if (opts.is_fun_shell()) {
    lputs("======== SHELL MODE specified ========");
    ok = true; // validate_shell_opts(opts);
    if (!ok) {
      if (tr >= 4) lputs("validate_shell_opts() failed.");
      status = 2;
      goto ret;
    }
    ok = deal_shell(opts);
    if (ok) {
      if (tr >= 2) lputs("deal_shell() succeeded.");
      status = 0;
    } else {
      if (tr >= 2) lputs("deal_shell() failed.");
    }
    goto ret;
  }

  // default function is stars
  ok = deal_stars(opts, true);
  if (ok) {
    if (tr >= 2) lputs("deal_stars() succeeded.");
    status = 0;
  } else {
    if (tr >= 2) lputs("deal_stars() failed.");
  }

ret:
  if (tr >= 4)
    lprintf("(planner main status) %i\n", status);
  pln::flush_out(true);
  return status;
}

const char* pln_get_version() {
  const char* v = pln::s_env.shortVerCS();
  return v ? v : "";
}

