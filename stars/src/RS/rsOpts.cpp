#include "rsOpts.h"
#include "rsFio.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace rsbe {

using namespace std;
using namespace pinc;

namespace eq {

static const char* _ver_[] = {"V", "v", "ver", "vers", "version", nullptr};

static const char* _dev_ver_[] = {"DV", "VV", "vv", "dev_ver", "dev_version", nullptr};

static const char* _help_[] = {"H", "h", "help", "hel", "hlp", "-he", nullptr};

static const char* _check_[] = {"ch", "che", "chec", "check", nullptr};

static const char* _csv_[] = {"CS", "cs", "csv", nullptr};

static const char* _xml_[] = {"XM", "xm", "xml", "XML", nullptr};

static const char* _pcf_[] = {"PC", "pc", "pcf", nullptr};

static const char* _blif_[] = {"BL", "blif", nullptr};

static const char* _json_[] = {"JS", "js", "jsf", "json", "port_info", "port_i", "PI", nullptr};

static const char* _output_[] = {"O", "o", "ou", "OU", "out", "outp", "output", nullptr};

static const char* _trace_[] = {"TR", "trace", "tr", "tra", nullptr};

static const char* _test_[] = {"TE", "TC", "test", "te", "tc", "tes", "tst", "test_case", "test_c", nullptr};

#ifdef RSBE_UNIT_TEST_ON

const char* _uni1_[] = {"U1", "u1", "Unit1", "unit1", "un1", "uni1", nullptr};
const char* _uni2_[] = {"U2", "u2", "Unit2", "unit2", "un2", "uni2", nullptr};
const char* _uni3_[] = {"U3", "u3", "Unit3", "unit3", "un3", "uni3", nullptr};
const char* _uni4_[] = {"U4", "u4", "Unit4", "unit4", "un4", "uni4", nullptr};
const char* _uni5_[] = {"U5", "u5", "Unit5", "unit5", "un5", "uni5", nullptr};
const char* _uni6_[] = {"U6", "u6", "Unit6", "unit6", "un6", "uni6", nullptr};
const char* _uni7_[] = {"U7", "u7", "Unit7", "unit7", "un7", "uni7", nullptr};

#endif  // RSBE_UNIT_TEST_ON

}

static constexpr size_t UNIX_Path_Max = PATH_MAX - 4;

// non-null string
inline static const char* nns(const char* s) noexcept { return s ? s : "(NULL)"; }

static bool input_dir_exists(const char* pa) noexcept {
  if (!pa) return false;

  struct stat sb;
  if (::stat(pa, &sb)) return false;

  if (not(S_IFDIR & sb.st_mode)) return false;

  int err = ::access(pa, R_OK);
  if (err) return false;

  return true;
}

static bool input_file_exists(const char* fn) noexcept {
  if (!fn) return false;

  struct stat sb;
  if (::stat(fn, &sb)) return false;

  if (not(S_IFREG & sb.st_mode)) return false;

  FILE* f = ::fopen(fn, "r");
  if (!f) return false;

  int64_t sz = 0;
  int err = ::fseek(f, 0, SEEK_END);
  if (not err) sz = ::ftell(f);

  ::fclose(f);
  return sz > 1;  // require non-empty file
}

static bool op_match(const char* op, const char** EQ) noexcept {
  assert(op and EQ);
  if (!op || !EQ) return false;
  assert(op[0] == '-');
  if (op[0] != '-') return false;
  op++;
  if (op[0] == '-') op++;

  for (const char** eq = EQ; *eq; eq++) {
    if (::strcmp(op, *eq) == 0) return true;
  }
  return false;
}

void rsOpts::reset() noexcept {
  p_free(deviceXML_);
  p_free(csvFile_);
  p_free(pcfFile_);
  p_free(blifFile_);
  p_free(jsonFile_);
  p_free(input_);
  p_free(output_);

  CStr keepSV = shortVer_;

  ::memset(this, 0, sizeof(*this));

  shortVer_ = keepSV;
}

void rsOpts::print(const char* label) const noexcept {
#ifdef RSBE_UNIT_TEST_ON
  if (label)
    cout << label;
  else
    cout << endl;
  printf("  argc_ = %i\n", argc_);
  if (argv_) {
    for (int i = 1; i < argc_; i++) printf("  argv[%i] = %s", i, argv_[i]);
  } else {
    lputs("\n\t !!! no argv_");
  }
  lputs();

  bool exi = input_file_exists(deviceXML_);
  printf("  deviceXML_: %s  exists:%i\n", nns(deviceXML_), exi);

  exi = input_file_exists(csvFile_);
  printf("  csvFile_: %s  exists:%i\n", nns(csvFile_), exi);

  printf("  pcfFile_: %s\n", nns(pcfFile_));
  printf("  blifFile_: %s\n", nns(blifFile_));
  printf("  jsonFile_: %s\n", nns(jsonFile_));
  printf("  output_: %s\n", nns(output_));
  printf("  input_: %s\n", nns(input_));

  printf("  test_id_ = %i\n", test_id_);
  printf("  trace_ = %i  traceIndex_ = %i\n", trace_, traceIndex_);
  cout << "  trace_specified: " << std::boolalpha << trace_specified() << '\n';
  cout << "   unit_specified: " << std::boolalpha << unit_specified() << endl;

  printf("  help_:%i  check_:%i\n", help_, check_);
  printf("  ver:%i  dev_ver_:%i\n", version_, dev_ver_);

  lputs();
#endif  // RSBE_UNIT_TEST_ON
}

void rsOpts::printHelp() const noexcept {
  cout << "Usage:" << endl;

#ifdef RSBE_UNIT_TEST_ON
  cout << "  pin_c [options] [PinTable.csv]" << endl;

  puts("\nOpts:");

  auto print_line = [](const char* a, const char* b) -> void {
    cout << std::setw(32) << a << "  :  " << b << '\n';
  };

  print_line("--help,-H,-h", "this help");
  print_line("--version,-V,-v", "version");
  print_line("--dev_version,-DV,-vv", "dev version");
  print_line("--trace,-TR <n>", "trace level");

  lputs();
  print_line("--csv,-CS", "csv file (Pin Table)");
  print_line("--pcf,-PC", "pcf file (Pin Constraint File)");
  print_line("--json,-JS,--port_info,-PI", "json file (Port Info File)");

  lputs();
#endif  // RSBE_UNIT_TEST_ON
}

static char* make_file_name(const char* arg) noexcept {
  if (!arg) return nullptr;

  char* fn = nullptr;

  size_t arg_len = ::strlen(arg);
  if (arg_len <= UNIX_Path_Max) {
    fn = ::strdup(arg);
  } else {
    fn = (char*)::calloc(UNIX_Path_Max + 2, sizeof(char));
    ::strncpy(fn, arg, UNIX_Path_Max - 1);
  }

  return fn;
}

void rsOpts::parse(int argc, const char** argv) noexcept {
  using namespace ::rsbe::eq;

  reset();
  argc_ = argc;
  argv_ = argv;

  assert(argc_ > 0 and argv_);

  CStr inp = 0, csv = 0, xml = 0, pcf = 0, blif = 0, jsnf = 0, out = 0;

  for (int i = 1; i < argc_; i++) {
    CStr arg = argv_[i];
    size_t len = ::strlen(arg);
    if (!len) continue;
    if (len == 1) {
      inp = arg;
      continue;
    }
    if (arg[0] != '-') {
      inp = arg;
      continue;
    }
    if (op_match(arg, _ver_)) {
      version_ = true;
      continue;
    }
    if (op_match(arg, _dev_ver_)) {
      dev_ver_ = true;
      continue;
    }
    if (op_match(arg, _help_)) {
      help_ = true;
      continue;
    }
    if (op_match(arg, _check_)) {
      check_ = true;
      continue;
    }

#ifdef RSBE_UNIT_TEST_ON
    if (op_match(arg, _uni1_)) {
      unit1_ = true;
      continue;
    }
    if (op_match(arg, _uni2_)) {
      unit2_ = true;
      continue;
    }
    if (op_match(arg, _uni3_)) {
      unit3_ = true;
      continue;
    }
    if (op_match(arg, _uni4_)) {
      unit4_ = true;
      continue;
    }
    if (op_match(arg, _uni5_)) {
      unit5_ = true;
      continue;
    }
    if (op_match(arg, _uni6_)) {
      unit6_ = true;
      continue;
    }
    if (op_match(arg, _uni7_)) {
      unit7_ = true;
      continue;
    }
#endif  // RSBE_UNIT_TEST_ON

    if (op_match(arg, _csv_)) {
      i++;
      if (i < argc_)
        csv = argv_[i];
      else
        csv = nullptr;
      continue;
    }
    if (op_match(arg, _xml_)) {
      i++;
      if (i < argc_)
        xml = argv_[i];
      else
        xml = nullptr;
      continue;
    }
    if (op_match(arg, _pcf_)) {
      i++;
      if (i < argc_)
        pcf = argv_[i];
      else
        pcf = nullptr;
      continue;
    }
    if (op_match(arg, _blif_)) {
      i++;
      if (i < argc_)
        blif = argv_[i];
      else
        blif = nullptr;
      continue;
    }
    if (op_match(arg, _json_)) {
      i++;
      if (i < argc_)
        jsnf = argv_[i];
      else
        jsnf = nullptr;
      continue;
    }
    if (op_match(arg, _output_)) {
      i++;
      if (i < argc_)
        out = argv_[i];
      else
        out = nullptr;
      continue;
    }
    if (op_match(arg, _test_)) {
      i++;
      if (i < argc_)
        test_id_ = atoi(argv_[i]);
      else
        test_id_ = 0;
      continue;
    }
    if (op_match(arg, _trace_)) {
      traceIndex_ = i;
      i++;
      if (i < argc_) {
        trace_ = atoi(argv_[i]);
      } else {
        trace_ = 1;
        traceIndex_ = 0;
      }
      continue;
    }

    inp = arg;
  }

  p_free(input_);
  input_ = make_file_name(inp);

  if (not test_id_specified()) {
    p_free(output_);
    output_ = make_file_name(out);
  }

  deviceXML_ = p_strdup(xml);
  csvFile_ = p_strdup(csv);
  pcfFile_ = p_strdup(pcf);
  blifFile_ = p_strdup(blif);
  jsonFile_ = p_strdup(jsnf);

  if (trace_ < 0) trace_ = 0;
  if (test_id_ < 0) test_id_ = 0;
}

#ifdef RSBE_UNIT_TEST_ON
static const char* st_TCS_path = "/home/serge/ppc/sta/TCS/PC";
static constexpr size_t st_path_cap = 812;
#endif

bool rsOpts::sprintFiles(const char* subdir, const char* stem) noexcept {
#ifdef RSBE_UNIT_TEST_ON
  assert(subdir && stem);
  assert(csvFile_);
  assert(pcfFile_);
  assert(jsonFile_);

  char buf[UNIX_Path_Max + st_path_cap + 2];
  constexpr size_t buf_sz = sizeof(buf);
  buf[0] = 0;
  buf[1] = 0;
  buf[buf_sz - 2] = 0;
  buf[buf_sz - 1] = 0;

  if (not input_dir_exists(st_TCS_path)) {
    snprintf(buf, buf_sz - 2, "rsOpts-ERROR: directory does not exist: %s", st_TCS_path);
    cerr << endl << buf << endl;
    cout << buf << '\n' << endl;
    return false;
  }

  snprintf(buf, buf_sz - 2, "%s/%s", st_TCS_path, subdir);
  if (not input_dir_exists(buf)) {
    cerr << endl << "rsOpts-ERROR: bad subdir: " << buf << endl;
    cout << "rsOpts-ERROR: bad subdir: " << buf << '\n' << endl;
    return false;
  }

  snprintf(csvFile_, st_path_cap - 1, "%s/%s/Gemini_Pin_Table.csv", st_TCS_path, subdir);
  if (not input_file_exists(csvFile_)) {
    cout << '\n' << "rsOpts-WARNING: csv-file does not exit: " << csvFile_ << '\n' << endl;
    csvFile_[0] = 0;
  }

  snprintf(jsonFile_, st_path_cap - 1, "%s/%s/%s_post_synth_ports.json", st_TCS_path, subdir, stem);
  if (not input_file_exists(jsonFile_)) {
    cout << '\n' << "rsOpts-WARNING: json-file does not exit: " << jsonFile_ << '\n' << endl;
    jsonFile_[0] = 0;
  }

  snprintf(pcfFile_, st_path_cap - 1, "%s/%s/%s_openfpga.pcf", st_TCS_path, subdir, stem);
  if (not input_file_exists(pcfFile_)) pcfFile_[0] = 0;

#endif  // RSBE_UNIT_TEST_ON
  return true;
}

bool rsOpts::set_PINC_test() noexcept {
  cout << " rsOpts::set_PINC_test()  test-ID: " << test_id_ << endl;

#ifdef RSBE_UNIT_TEST_ON
  assert(test_id_specified());
  p_free(csvFile_);
  p_free(pcfFile_);
  p_free(jsonFile_);
  p_free(output_);

  if (test_id_ <= 0) {
    csvFile_ = pcfFile_ = jsonFile_ = nullptr;
    output_ = nullptr;
    return false;
  }

  auto tc2subdir = [](int tcId) -> const char* {
    static const char* tc_tab[] = {nullptr,
                                   "and2_Gem_001",          // 1
                                   "vex_no_carry_gem_002",  // 2
                                   "and2_VG50_003",         // 3
                                   "AES_DECRYPT_gem_004",   // 4
                                   "AES_DECRYPT_VG50_005",  // 5
                                   "VEX_100K_1GE100_006",   // 6
                                   "vex_no_carry_gem_007",  // 7
                                   nullptr,
                                   nullptr};
    if (tcId < 1 || tcId > 7) return nullptr;
    return tc_tab[tcId];
  };

  const char* subdir = tc2subdir(test_id_);
  if (!subdir) {
    csvFile_ = pcfFile_ = jsonFile_ = nullptr;
    output_ = nullptr;
    goto ret;
  }
  cout << "       setupTest()  test-ID: " << test_id_ << "  subdir: " << subdir << endl;

  csvFile_ = (char*)::calloc(st_path_cap, sizeof(char));
  pcfFile_ = (char*)::calloc(st_path_cap, sizeof(char));
  jsonFile_ = (char*)::calloc(st_path_cap, sizeof(char));
  output_ = (char*)::calloc(st_path_cap, sizeof(char));
  sprintf(output_, "PIN_C__tc%i_.place", test_id_);

  if (test_id_ == 1) {
    return sprintFiles(subdir, "and2_gemini");
  }
  if (test_id_ == 2) {
    return sprintFiles(subdir, "vex_soc_no_carry");
  }
  if (test_id_ == 3) {
    return sprintFiles(subdir, "and2_1VG50");
  }
  if (test_id_ == 4) {
    return sprintFiles(subdir, "AES_DECRYPT_gem");
  }
  if (test_id_ == 5) {
    return sprintFiles(subdir, "AES_DECRYPT_VG50");
  }
  if (test_id_ == 6) {
    return sprintFiles(subdir, "VEX_100K_1GE100");
  }
  if (test_id_ == 7) {
    return sprintFiles(subdir, "vex_soc_no_carry");
  }

ret:
  cout << "\t  !!! invalid test-ID: " << test_id_ << endl;

#endif  // RSBE_UNIT_TEST_ON
  return false;
}

// and2_gemini
bool rsOpts::set_VPR_TC1() noexcept {
  lputs(" O-set_VPR_TC1: and2_gemini");
  assert(argc_ > 0 && argv_);
  bool ok = false;

#ifdef RSBE_UNIT_TEST_ON
  static const char* raw_TC1 = R"(
   /home/serge/raps/5jul/Raptor/build/share/raptor/etc/devices/gemini_compact_104x68/gemini_vpr.xml
   /home/serge/raps/5jul/Raptor/and2_gemini/run_1/synth_1_1/synthesis/and2_gemini_post_synth.v
   --sdc_file /home/serge/raps/5jul/Raptor/and2_gemini/run_1/impl_1_1/packing/and2_gemini_openfpga.sdc
   --route_chan_width 160 --suppress_warnings check_rr_node_warnings.log,check_rr_node
   --clock_modeling ideal --absorb_buffer_luts off --skip_sync_clustering_and_routing_results off
   --constant_net_method route --post_place_timing_report and2_gemini_post_place_timing.rpt --device castor104x68_heterogeneous
   --allow_unrelated_clustering on --allow_dangling_combinational_nodes on
   --place_delta_delay_matrix_calculation_method dijkstra
   --gen_post_synthesis_netlist on --post_synth_netlist_unconn_inputs gnd
   --inner_loop_recompute_divider 1 --max_router_iterations 1500 --timing_report_detail detailed
   --timing_report_npaths 100 --top and2
   --net_file /home/serge/raps/5jul/Raptor/and2_gemini/run_1/impl_1_1/packing/and2_gemini_post_synth.net
   --place_file /home/serge/raps/5jul/Raptor/and2_gemini/run_1/impl_1_1/placement/and2_gemini_post_synth.place
   --route_file /home/serge/raps/5jul/Raptor/and2_gemini/run_1/impl_1_1/routing/and2_gemini_post_synth.route
   --place
  )";
  ok = set_VPR_TC_args(raw_TC1);
#endif  // RSBE_UNIT_TEST_ON

  flush_out(true);
  return ok;
}

bool rsOpts::set_STA_TC2() noexcept {
  lputs(" O-set_STA_TC2: arch 1GE100-ES1");
  assert(argc_ > 0 && argv_);
  bool ok = false;

#ifdef RSBE_UNIT_TEST_ON
  static const char* raw_TC2 = R"(
    /home/serge/raps/5jul/Raptor/build/share/raptor/etc/devices/1GE100-ES1/gemini_vpr.xml
    /home/serge/raps/5jul/Raptor/EDA-1704/stars_TC/synth_1_1/synthesis/flop2flop_post_synth.v
    --sdc_file /home/serge/raps/5jul/Raptor/EDA-1704/stars_TC/impl_1_1/packing/flop2flop_openfpga.sdc
    --route_chan_width 160 --suppress_warnings check_rr_node_warnings.log,check_rr_node
    --clock_modeling ideal --absorb_buffer_luts off --skip_sync_clustering_and_routing_results off
    --constant_net_method route --post_place_timing_report flop2flop_post_place_timing.rpt
    --device castor104x68_heterogeneous --allow_unrelated_clustering on
    --allow_dangling_combinational_nodes on --place_delta_delay_matrix_calculation_method dijkstra
    --gen_post_synthesis_netlist on
    --post_synth_netlist_unconn_inputs gnd
    --inner_loop_recompute_divider 1 --max_router_iterations 1500
    --timing_report_detail detailed --timing_report_npaths 100
    --net_file /home/serge/raps/5jul/Raptor/EDA-1704/stars_TC/impl_1_1/packing/flop2flop_post_synth.net
    --place_file /home/serge/raps/5jul/Raptor/EDA-1704/stars_TC/impl_1_1/placement/flop2flop_post_synth.place
    --route_file /home/serge/raps/5jul/Raptor/EDA-1704/stars_TC/impl_1_1/routing/flop2flop_post_synth.route
    --place
  )";
  ok = set_VPR_TC_args(raw_TC2);
#endif  // RSBE_UNIT_TEST_ON

  flush_out(true);
  return ok;
}

bool rsOpts::set_STA_TC3() noexcept {
  lputs(" O-set_STA_TC3: flop2flop, arch GEMINI");
  assert(argc_ > 0 && argv_);
  bool ok = false;

#ifdef RSBE_UNIT_TEST_ON
  static const char* raw_TC3 = R"(
    /home/serge/raps/10jul/Raptor/build/share/raptor/etc/devices/1GE100-ES1/gemini_vpr.xml
    /home/serge/raps/10jul/Raptor/EDA-1704/stars_TC3/synth_1_1/synthesis/flop2flop_post_synth.v
    --sdc_file /home/serge/raps/10jul/Raptor/EDA-1704/stars_TC3/impl_1_1/packing/flop2flop_openfpga.sdc
    --route_chan_width 160 --suppress_warnings check_rr_node_warnings.log,check_rr_node
    --clock_modeling ideal --absorb_buffer_luts off --skip_sync_clustering_and_routing_results off
    --constant_net_method route --post_place_timing_report flop2flop_post_place_timing.rpt
    --device castor104x68_heterogeneous --allow_unrelated_clustering on
    --allow_dangling_combinational_nodes on --place_delta_delay_matrix_calculation_method dijkstra
    --gen_post_synthesis_netlist on
    --post_synth_netlist_unconn_inputs gnd
    --inner_loop_recompute_divider 1 --max_router_iterations 1500
    --timing_report_detail detailed --timing_report_npaths 100
    --net_file /home/serge/raps/10jul/Raptor/EDA-1704/stars_TC3/impl_1_1/packing/flop2flop_post_synth.net
    --place_file /home/serge/raps/10jul/Raptor/EDA-1704/stars_TC3/impl_1_1/placement/flop2flop_post_synth.place
    --route_file /home/serge/raps/10jul/Raptor/EDA-1704/stars_TC3/impl_1_1/routing/flop2flop_post_synth.route
    --place
  )";
  ok = set_VPR_TC_args(raw_TC3);
#endif  // RSBE_UNIT_TEST_ON

  flush_out(true);
  return ok;
}

bool rsOpts::set_STA_TC4() noexcept {
  lputs(" O-set_STA_TC4: vex_soc_no_carry, arch GEMINI");
  assert(argc_ > 0 && argv_);
  bool ok = false;

#ifdef RSBE_UNIT_TEST_ON
  static const char* raw_TC4 = R"(
  /home/serge/raps/01STA_vex/Raptor/build/share/raptor/etc/devices/gemini/gemini_vpr.xml
  /home/serge/raps/01STA_vex/Raptor/vex_soc_no_carry/run_1/synth_1_1/synthesis/vex_soc_no_carry_post_synth.v
  --sdc_file /home/serge/raps/01STA_vex/Raptor/vex_soc_no_carry/run_1/synth_1_1/impl_1_1/packing/vex_soc_no_carry_openfpga.sdc
  --route_chan_width 192
  --suppress_warnings check_rr_node_warnings.log,check_rr_node
  --clock_modeling ideal --absorb_buffer_luts off
  --skip_sync_clustering_and_routing_results on
  --constant_net_method route
  --post_place_timing_report vex_soc_no_carry_post_place_timing.rpt
  --device castor82x68_heterogeneous --allow_unrelated_clustering on
  --gen_post_synthesis_netlist on
  --allow_dangling_combinational_nodes on
  --post_synth_netlist_unconn_inputs gnd
  --inner_loop_recompute_divider 1
  --max_router_iterations 1500
  --timing_report_detail detailed
  --timing_report_npaths 100
  --top vex_soc
  --net_file /home/serge/raps/01STA_vex/Raptor/vex_soc_no_carry/run_1/synth_1_1/impl_1_1/packing/vex_soc_no_carry_post_synth.net
  --place_file /home/serge/raps/01STA_vex/Raptor/vex_soc_no_carry/run_1/synth_1_1/impl_1_1/placement/vex_soc_no_carry_post_synth.place
  --route_file /home/serge/raps/01STA_vex/Raptor/vex_soc_no_carry/run_1/synth_1_1/impl_1_1/routing/vex_soc_no_carry_post_synth.route
  )";
  ok = set_VPR_TC_args(raw_TC4);
#endif  // RSBE_UNIT_TEST_ON

  flush_out(true);
  return ok;
}

bool rsOpts::set_VPR_TC_args(CStr raw_tc) noexcept {
  assert(raw_tc);
  cout << '\n' << ::strlen(raw_tc) << endl;
  assert(::strlen(raw_tc) > 3);

  vector<string> W;
  fio::Fio::split_spa(raw_tc, W);

  return createVprArgv(W);
}

bool rsOpts::createVprArgv(const vector<string>& W) noexcept {
  size_t sz = W.size();
  lout() << "W.size()= " << sz << endl;
  if (sz < 3) return false;

  lout() << "created ARGV for VPR:" << endl;
  for (size_t i = 0; i < sz; i++) {
    lprintf("\t |%zu|  %s\n", i, W[i].c_str());
  }

  vprArgv_ = (char**)::calloc(sz + 4, sizeof(char*));
  uint cnt = 0;
  vprArgv_[cnt++] = ::strdup(argv_[0]);
  for (size_t i = 0; i < sz; i++) {
    const string& a = W[i];
    vprArgv_[cnt++] = ::strdup(a.c_str());
  }
  vprArgc_ = cnt;

  return true;
}

}  // NS
