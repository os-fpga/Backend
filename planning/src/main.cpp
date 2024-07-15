static const char* _pln_VERSION_STR = "pln0259";

#include "RS/rsEnv.h"
#include "util/pln_log.h"
#include "util/cmd_line.h"
#include "file_readers/pln_Fio.h"
#include "pin_loc/pinc_main.h"

#ifdef PLN_UNIT_TEST_ON
#include "tes/tes_list.h"
#endif

#include "globals.h"
#include "read_options.h"

#include "RS/rsVPR.h"
#include "RS/rsCheck.h"
#include "RS/sta_file_writer.h"

namespace pln {

using std::cout;
using std::cerr;
using std::endl;
using std::string;

static rsEnv s_env;

static bool deal_pinc(const rsOpts& opts, bool orig_args);

static bool deal_check(const rsOpts& opts) {
  bool status = false;
  uint16_t tr = ltrace();

  CStr checkWhat = "BLIF";
  if (not opts.hasInputFile() and opts.hasCsvFile())
    checkWhat = "CSV";

  if (tr >= 4)
    lprintf("[PLANNER %s-CHECKER] deal_check()\n", checkWhat);

  if (checkWhat[0] == 'B') {
    if (not opts.hasInputFile()) {
      err_puts("\n[PLANNER BLIF-CHECKER] : something wrong with input file\n");
      if (!opts.input_) {
        lputs("[PLANNER BLIF-CHECKER] : input file is not specified");
      } else {
        lprintf("[PLANNER BLIF-CHECKER] : input file '%s'\n", opts.input_);
        lprintf("                  : does not exist or not readable\n");
      }
      flush_out(true);
      return false;
    }
  }

  status = do_check(opts, checkWhat[0] == 'B');

  if (tr >= 3)
    lprintf("(deal_check status) %s\n", status ? "TRUE" : "FALSE");

  flush_out(true);
  return status;
}

static bool deal_stars(const rsOpts& opts, bool orig_args) {
  bool status = false;

  uint16_t tr = ltrace();
  auto& ls = lout();
  int argc;
  const char** argv;
  if (orig_args) {
    argc = opts.argc_;
    argv = opts.argv_;
  } else {
    argc = opts.vprArgc_;
    argv = (const char**)opts.vprArgv_;
  }

  if (argc <= 0 || !argv) {
    lputs("\n [Error] no args");
    return false;
  }

  if (tr >= 10 || getenv("pln_trace_env")) {
    string cmd_fn = str::concat("02_deal_stars.", std::to_string(s_env.pid_), ".stars.sh");
    std::ofstream cmd_os(cmd_fn);
    if (cmd_os.is_open()) {
      s_env.print(cmd_os, "# deal_stars\n#----env----\n");
      cmd_os << "#-----------" << endl;
      ls << "WRITTEN COMMAND FILE: " << cmd_fn << endl;
    }
  }

  // call vpr to build design context
  ls << "\nPLN-stars: Preparing design data ... " << endl;

  // add --analysis if it's missing, and remove --function
  char** vprArgv = (char**)calloc(argc + 4, sizeof(char*));
  int vprArgc = 0;
  bool found_analysis = false;
  string analysis = "--analysis";
  for (int i = 0; i < argc; i++) {
    const char* a = argv[i];
    if (analysis == a) found_analysis = true;
    vprArgv[vprArgc++] = ::strdup(a);
  }
  if (not found_analysis) {
    ls << "PLN: added --analysis to vpr options" << endl;
    vprArgv[argc] = ::strdup("--analysis");
    vprArgc++;
  }

  status = true;
  ls << "PLN-stars: Initializing VPR data ... " << endl;

  int vpr_code = vpr4pln(vprArgc, vprArgv);
  if (tr >= 3)
    lprintf("vpr4pln returned: %i\n", vpr_code);
  if (vpr_code != 0) {
    lputs("\n[Error] PLN: VPR init failed.");
    cerr << "[Error] PLN: VPR init failed." << endl;
    status = false;
  }

  if (status) {
    ls << "PLN-stars: Creating sta files ... " << endl;
    if (!FileWriter::create_files(argc, argv)) {
      lputs("\n[Error] PLN: Creating sta files failed.");
      cerr << "[Error] PLN: Creating sta files failed." << endl;
      status = false;
    } else {
      lputs("PLN-stars: Creating sta files succeeded.");
      status = true;
    }
  }

  return status;
}

static bool validate_partition_opts(const rsOpts& opts) {
  using namespace fio;
  uint16_t tr = ltrace();
  int argc = opts.argc_;
  const char** argv = opts.argv_;

  if (argc <= 0 || !argv) {
    lputs("\n [Error] no args");
    return false;
  }

  if (tr >= 9 || getenv("pln_trace_env")) {
    lputs("-------- traceEnv from validate_partition_opts");
    traceEnv(argc, argv);
    lputs("--------");
  }
  else if (tr >= 3) {
    lputs("-------- Argv");
    lprintf("    argc= %i\n", argc);
    for (int i = 0; i < argc; i++)
      lprintf("    |%i| %s\n", i, argv[i]);
    lputs("--------");
  }

  if (argc < 4) {
    lputs();
    lputs("[Error] too few arguments for function 'partition'");
    cerr << "[Error] too few arguments for function 'partition'" << endl;
    lprintf("    Number of arguments = %i\n", argc - 1);
    lprintf("    Function 'partition' expects at least 3\n\n");
    return false;
  }

  CStr arg3 = argv[3];
  assert(arg3);

  bool ok = Fio::regularFileExists(arg3);
  if (not ok) {
    lprintf("\n[Error] XML arch file  %s  does not exist\n", arg3);
    cerr << "\n[Error] no such file: " << arg3 << endl;
    lputs();
    return false;
  }

  return true;
}

static bool deal_vpr(const rsOpts& opts, bool orig_args) {
  bool status = false;
  uint16_t tr = ltrace();
  auto& ls = lout();

  int argc;
  const char** argv;
  if (orig_args) {
    argc = opts.argc_;
    argv = opts.argv_;
  } else {
    argc = opts.vprArgc_;
    argv = (const char**)opts.vprArgv_;
  }

  if (argc <= 0 || !argv) {
    lputs("\n [Error] no args");
    return false;
  }

  if (tr >= 10 || getenv("pln_trace_env")) {
    string cmd_fn = str::concat("03_deal_vpr.", std::to_string(s_env.pid_), ".vpr.sh");
    std::ofstream cmd_os(cmd_fn);
    if (cmd_os.is_open()) {
      s_env.print(cmd_os, "# deal_vpr\n#----env----\n");
      cmd_os << "#-----------" << endl;
      ls << "WRITTEN COMMAND FILE: " << cmd_fn << endl;
    }
  }
  if (tr >= 8 || getenv("pln_trace_env")) {
    lputs("-------- traceEnv from deal_vpr");
    traceEnv(argc, argv);
    lputs("--------");
  }

  // call vpr to build design context
  ls << "\nPLN-VPR: Preparing design data ... " << endl;

  status = true;
  ls << "PLN-VPR: Initializing VPR data ... " << endl;

  int vpr_code = vpr4pln(argc, (char**)argv);
  if (tr >= 3)
    lprintf("vpr4pln returned: %i\n", vpr_code);
  if (vpr_code != 0) {
    lputs("\n[Error] PLN-VPR: VPR failed.");
    cerr << "[Error] PLN-VPR: VPR failed." << endl;
    status = false;
  }

  return status;
}

static bool deal_cmd(rsOpts& opts) {
  bool status = false;

  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 5)
    lputs("deal_cmd()");

  if (not opts.isCmdInput()) {
    lputs("\n    deal_cmd() : not opts.isCmdInput()\n");
    return false;
  }

  opts.vprArgc_ = 0;
  opts.vprArgv_ = nullptr;
  string arg0;

  if (tr >= 5)
    ls << "\n\t  opts.isCmdInput()" << endl;
  fio::LineReader lr(opts.input_);
  bool rd_ok = lr.read(true, true);
  if (tr >= 6)
    lprintf("\t  rd_ok : %i\n", rd_ok);
  if (rd_ok) {
    if (tr >= 2)
      lprintf("  setting argv from cmd-input: %s\n", opts.input_);
    vector<string> W = lr.getWords();
    if (W.empty()) {
      ls << "\n [Error] no args - W.empty()" << endl;
      err_puts(" [Error] no args - W.empty()");
      return false;
    }
    arg0 = W[0];
    if (tr >= 7) {
      lprintf("  W.size()= %zu  W[0]= %s\n", W.size(), arg0.c_str());
      lputs();
    }
    if (W.size() > 1 && W.size() < 4000) {
      char** cmdArgv = (char**)calloc(W.size() + 4, sizeof(char*));
      int cmdArgc = 0;
      cmdArgv[cmdArgc++] = ::strdup("cmd_pln");
      for (uint j = 1; j < W.size(); j++) {
        cmdArgv[cmdArgc++] = ::strdup(W[j].c_str());
      }
      if (tr >= 4)
        lprintf("  created cmdArgv: cmdArgc= %i\n", cmdArgc);
      opts.vprArgc_ = cmdArgc;
      opts.vprArgv_ = cmdArgv;
    }
  }
  lputs();

  if (opts.vprArgc_ <= 0 || !opts.vprArgv_) {
    ls << "\n [Error] no args" << endl;
    err_puts(" [Error] no args");
    return false;
  }

  if (arg0 == "pin_c" or rsOpts::ends_with_pin_c(arg0.c_str())) {

    status = deal_pinc(opts, false);
    if (status) {
      if (tr >= 3)
        lputs("  (from deal_cmd)  deal_pinc() succeeded.");
    } else {
      if (tr >= 3)
        lputs("  (from deal_cmd)  deal_pinc() failed.");
    }

    return status;
  }

  status = deal_vpr(opts, false);
  if (status) {
    if (tr >= 3)
      lputs("  (from deal_cmd)  deal_vpr() succeeded.");
  } else {
    if (tr >= 3)
      lputs("  (from deal_cmd)  deal_vpr() failed.");
  }

  return status;
}

static bool deal_pinc(const rsOpts& opts, bool orig_args) {
  bool status = false;

  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 5)
    lputs("deal_pinc()");
  int argc;
  const char** argv;
  if (orig_args) {
    argc = opts.argc_;
    argv = opts.argv_;
  } else {
    argc = opts.vprArgc_;
    argv = (const char**)opts.vprArgv_;
  }

  if (argc <= 0 || !argv) {
    ls << "\n [Error] no args" << endl;
    err_puts(" [Error] no args");
    return false;
  }

  if (tr >= 11 || getenv("pln_trace_env")) {
    string cmd_fn = str::concat("01_deal_pinc.", std::to_string(s_env.pid_), ".pinc.sh");
    std::ofstream cmd_os(cmd_fn);
    if (cmd_os.is_open()) {
      s_env.print(cmd_os, "# deal_pinc\n#----env----\n");
      cmd_os << "#-----------" << endl;
      ls << "WRITTEN COMMAND FILE: " << cmd_fn << endl;
    }
  }

  if (opts.isCmdInput()) {
    ls << "\n\t  opts.isCmdInput()" << endl;
    fio::LineReader lr(opts.input_);
    bool rd_ok = lr.read(true, true);
    lprintf("\t  rd_ok : %i\n", rd_ok);
    if (rd_ok) {
      lprintf("  setting argv from cmd-input: %s\n", opts.input_);
      vector<string> W = lr.getWords();
      lprintf("  W.size()= %zu\n", W.size());
      lputs();
      if (W.size() > 1 && W.size() < 4000) {
        char** pcArgv = (char**)calloc(W.size() + 4, sizeof(char*));
        int pcArgc = 0;
        pcArgv[pcArgc++] = ::strdup("cmd_pin_c");
        for (uint j = 1; j < W.size(); j++) {
          pcArgv[pcArgc++] = ::strdup(W[j].c_str());
        }
        lprintf("  created pin_loc.cmd argv: pcArgc= %i\n", pcArgc);
        argc = pcArgc;
        argv = (const char**) pcArgv;
      }
    }
    lputs();
  }

  // ======= pin_c/src/main.cpp: =====
  const char* trace = getenv("pinc_trace");
  if (trace) {
    int t = atoi(trace);
    if (t > 0 and t > int(tr))
      set_ltrace(t);
  }

  if (! rsOpts::validate_pinc_args(argc, argv)) {
    ls << "\n [Error] bad pin_c args" << endl;
    err_puts(" [Error] bad pin_c args\n");
    return false;
  }

  cmd_line cmd(argc, argv);

  if (ltrace() >= 3) {
    lputs("\n    pin_c");
    if (ltrace() >= 7)
      traceEnv(argc, argv);
    cmd.print_options();
  }

  int pc_main_ret = pinc_main(cmd);
  // ======= pin_c/src/main.cpp^ =====

  status = (pc_main_ret == 0);

  return status;
}

static void deal_help(const rsOpts& opts) {
  printf("PLANNER ver. %s\n", _pln_VERSION_STR);

  if (opts.det_ver_) {
    s_env.listDevEnv();
    return;
  }
  if (opts.version_) {
    lputs();
    return;
  }
  if (opts.help_) {
    opts.printHelp();
  }
}

static void deal_units(const rsOpts& opts) {
#ifdef PLN_UNIT_TEST_ON
  using namespace tes;
  if (opts.unit1_) tes::run_U1();
  if (opts.unit2_) tes::run_U2();
  if (opts.unit3_) tes::run_U3();
  if (opts.unit4_) tes::run_U4();
  if (opts.unit5_) tes::run_U5();
  if (opts.unit6_) tes::run_U6();
  if (opts.unit7_) tes::run_U7();
#endif
}

}  // NS pln

int main(int argc, char** argv) {
  using namespace pln;
  using std::cout;
  using std::endl;
  using std::string;

  rsOpts& opts = s_env.opts_;
  const char* trace = getenv("pln_trace");
  if (trace)
    set_ltrace(atoi(trace));
  else
    set_ltrace(opts.trace_);

  cout << s_env.traceMarker_ << endl;

  s_env.initVersions(_pln_VERSION_STR);

  if (ltrace() >= 3) {
    lprintf("\t %s\n", _pln_VERSION_STR);
    lprintf("\t compiled:  %s\n", s_env.compTimeCS());
  }

  s_env.parse(argc, argv);

  if (opts.trace_ >= 8 || ltrace() >= 9 || getenv("pln_trace_env")) {
    s_env.dump("\n----env----\n");
    lout() << "-----------" << endl;
  }

  if (opts.ver_or_help()) {
    deal_help(opts);
    pln::flush_out();
    std::quick_exit(0);
  }

  if (ltrace() >= 4) {
    lprintf("ltrace()= %u  cmd.argc= %i\n", ltrace(), argc);
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

  int status = 1;
  bool ok = false;

  if (opts.is_fun_cmd()) {
    ok = deal_cmd(opts);
    if (ok) {
      if (ltrace() >= 2) lputs("deal_cmd() succeeded.");
      status = 0;
    } else {
      if (ltrace() >= 2) lputs("deal_cmd() failed.");
    }
    goto ret;
  }

  if (opts.is_fun_pinc() or opts.is_arg0_pinc() or opts.is_implicit_pinc()) {
    ok = deal_pinc(opts, true);
    if (ok) {
      if (ltrace() >= 2) lputs("deal_pinc() succeeded.");
      status = 0;
    } else {
      if (ltrace() >= 2) lputs("deal_pinc() failed.");
    }
    goto ret;
  }

  if (opts.is_fun_partition()) {
    lputs("======== PARTITION MODE specified ========");
    ok = validate_partition_opts(opts);
    if (!ok) {
      if (ltrace() >= 4) lputs("validate_partition_opts() failed.");
      status = 2;
      goto ret;
    }
  }

  if (opts.is_fun_partition() or opts.is_fun_pack()) {
    ok = deal_vpr(opts, true);
    if (ok) {
      if (ltrace() >= 2) lputs("deal_vpr() succeeded.");
      status = 0;
    } else {
      if (ltrace() >= 2) lputs("deal_vpr() failed.");
    }
    goto ret;
  }

  // default function is stars
  ok = deal_stars(opts, true);
  if (ok) {
    if (ltrace() >= 2) lputs("deal_stars() succeeded.");
    status = 0;
  } else {
    if (ltrace() >= 2) lputs("deal_stars() failed.");
  }

ret:
  if (ltrace() >= 4)
    lprintf("(planner main status) %i\n", status);
  pln::flush_out(true);
  return status;
}

const char* pln_get_version() {
  const char* v = pln::s_env.shortVerCS();
  return v ? v : "";
}

