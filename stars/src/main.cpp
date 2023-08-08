static const char* _rsbe_VERSION_STR = "rsbe0093";

#include "RS/rsEnv.h"
#include "util/pinc_log.h"

#ifdef RSBE_UNIT_TEST_ON
#include "tes/tes_list.h"
#endif

#include "globals.h"
#include "read_options.h"

#include "RS/rsVPR.h"
#include "RS/sta_file_writer.h"

namespace rsbe {

using namespace pinc;
using std::cout;
using std::cerr;
using std::endl;
using std::string;

static rsEnv s_env;

static bool do_stars(const rsOpts& opts, bool orig_args) {
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

  if (ltrace() >= 8 || getenv("rsbe_trace_env")) {
    string cmd_fn = str::concat("00_do_stars.", std::to_string(s_env.pid_), ".stars.sh");
    std::ofstream cmd_os(cmd_fn);
    if (cmd_os.is_open()) {
      s_env.print(cmd_os, "# stars\n#----env----\n");
      cmd_os << "#-----------" << endl;
      lout() << "WRITTEN COMMAND FILE: " << cmd_fn << endl;
    }
  }

  // call vpr to build design context
  ls << "\nSTARS: Preparing design data ... " << endl;

  // add --analysis if it's missing:
  char** vprArgv = (char**)calloc(argc + 4, sizeof(char*));
  int vprArgc = argc;
  bool found_analysis = false;
  string analysis = "--analysis";
  for (int i = 0; i < argc; i++) {
    const char* a = argv[i];
    if (analysis == a) found_analysis = true;
    vprArgv[i] = strdup(a);
  }
  if (not found_analysis) {
    ls << "STARS: added --analysis to vpr options" << endl;
    vprArgv[argc] = strdup("--analysis");
    vprArgc++;
  }

  bool status = true;
  ls << "STARS: Initializing VPR data ... " << endl;

  int vpr_code = vpr4stars(vprArgc, vprArgv);
  if (tr >= 2)
    lprintf("vpr4stars returned: %i\n", vpr_code);
  if (vpr_code != 0) {
      lputs("\n[Error] STARS: VPR init failed.");
      cerr << "[Error] STARS: VPR init failed." << endl;
      status = false;
  }

  if (status) {
    ls << "STARS: Creating sta files ... " << endl;
    if (!FileWriter::create_files(argc, argv)) {
      lputs("\n[Error] STARS: Creating sta files failed.");
      cerr << "[Error] STARS: Creating sta files failed." << endl;
      status = false;
    } else {
      lputs("STARS: Creating sta files succeeded.");
      status = true;
    }
  }

  return status;
}

static void do_help(const rsOpts& opts) {
  printf("RSBE ver. %s\n", _rsbe_VERSION_STR);

  if (opts.dev_ver_) {
    s_env.listDevEnv();
  }
  if (opts.dev_ver_ || opts.version_) {
    return;
  }
  if (opts.help_) {
    opts.printHelp();
  }
}

static void do_units(const rsOpts& opts) {
#ifdef RSBE_UNIT_TEST_ON
  using namespace tes;
  if (opts.unit1_) tes::run_U1();
  if (opts.unit2_) tes::run_U2();
  if (opts.unit3_) tes::run_U3();
  if (opts.unit4_) tes::run_U4();
#endif
}

}  // NS rsbe

int main(int argc, char** argv) {
  using namespace pinc;
  using namespace rsbe;
  using std::cout;
  using std::endl;
  using std::string;

  rsOpts& opts = s_env.opts_;
  const char* trace = getenv("rsbe_trace");
  if (trace)
    set_ltrace(atoi(trace));
  else
    set_ltrace(opts.trace_);

  cout << s_env.traceMarker_ << endl;

  s_env.initVersions(_rsbe_VERSION_STR);

  if (ltrace() >= 2) {
    s_env.printPids(_rsbe_VERSION_STR);
    printf("\t compiled:  %s\n", s_env.compTimeCS());
  }

  s_env.parse(argc, argv);

  if (opts.trace_ >= 4 || ltrace() >= 8 || getenv("rsbe_trace_env")) {
    s_env.dump("\n----env----\n");
    lout() << "-----------" << endl;
  }

  if (opts.ver_or_help()) {
    do_help(opts);
    pinc::flush_out();
    std::quick_exit(0);
  }

  if (opts.unit_specified()) {
    do_units(opts);
    ::exit(0);
  }

  if (ltrace() >= 2) {
    lprintf("ltrace()= %u  cmd.argc= %i\n", ltrace(), argc);
  }

  int status = 1;
  bool ok = false;

#ifdef RSBE_UNIT_TEST_ON
  const char* str_STA_TC = getenv("rsbe_builtin_STA_TC");
  int num_STA_TC = 0;
  if (str_STA_TC)
    num_STA_TC = ::atoi(str_STA_TC);
  constexpr bool rsbe_builtin_VPR_TC = false; //getenv("rsbe_builtin_VPR_TC");
  if (num_STA_TC > 0) {
    lprintf("\n### num_STA_TC= %i\n", num_STA_TC);
    ok = opts.set_STA_testCase(num_STA_TC);
    if (ok) {
      ok = do_stars(opts, false);
      if (ok) {
        lputs("do_stars() succeeded.");
        status = 0;
      }
    }
  } else if (rsbe_builtin_VPR_TC) {
    lputs("\n(rsbe_builtin_VPR_TC)\n");
    ok = opts.set_VPR_TC1();
    if (ok) {
      status = do_vpr(opts);
      lprintf("DID vpr. status= %i\n", status);
    } else {
      lputs(" [Error] set_VPR_TC FAILED");
    }
  }
  goto ret;
#endif  // RSBE_UNIT_TEST_ON

  ok = do_stars(opts, true);
  if (ok) {
    if (ltrace() >= 2) lputs("do_stars() succeeded.");
    status = 0;
  } else {
    if (ltrace() >= 2) lputs("do_stars() failed.");
  }

ret:
  pinc::flush_out(true);
  return status;
}
