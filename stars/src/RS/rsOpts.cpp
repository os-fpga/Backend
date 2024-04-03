#include "RS/rsOpts.h"
#include "file_readers/pinc_Fio.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace pln {

using namespace std;
using namespace fio;
using CStr = const char*;

namespace alias {

static CStr _ver_[] = {"V", "v", "ver", "vers", "version", nullptr};

static CStr _det_ver_[] = {"VV", "vv", "VVV", "vvv", "det_ver", "detailed_version", nullptr};

static CStr _help_[] = {"H", "h", "help", "hel", "hlp", "he", nullptr};

static CStr _fun_[] = {"F", "fu", "fun", "func", "funct", "function", nullptr};

static CStr _check_[] = {"ch", "che", "chec", "check", nullptr};

static CStr _csv_[] = {"CS", "cs", "csv", nullptr};

static CStr _xml_[] = {"XM", "xm", "xml", "XML", nullptr};

static CStr _pcf_[] = {"PCF", "pcf", nullptr};

static CStr _blif_[] = {"BL", "blif", nullptr};

static CStr _json_[] = {"JS", "js", "jsf", "json", "port_info", "port_i", "PI", nullptr};

static CStr _output_[] = {"OU", "ou", "out", "outp", "output", nullptr};

static CStr _assOrd_[] = {"ASS", "ass", "assign", "assign_unconstrained", "assign_unconstrained_pins", nullptr};

static CStr _trace_[] = {"TR", "trace", "tr", "tra", nullptr};

static CStr _test_[] = {"TE", "TC", "test", "te", "tc", "tes", "tst", "test_case", "test_c", nullptr};

#ifdef RSBE_UNIT_TEST_ON
#endif  // RSBE_UNIT_TEST_ON

}

static constexpr size_t UNIX_Path_Max = PATH_MAX - 4;

// non-null string
inline static CStr nns(CStr s) noexcept { return s ? s : "(NULL)"; }

static bool input_file_exists(CStr fn) noexcept {
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

static bool op_match(CStr op, CStr* aliases) noexcept {
  assert(op and aliases);
  if (!op || !aliases) return false;
  assert(op[0] == '-');
  if (op[0] != '-') return false;
  op++;
  if (op[0] == '-') op++;

  for (CStr* al = aliases; *al; al++) {
    if (::strcmp(op, *al) == 0) return true;
  }
  return false;
}

bool rsOpts::isFunctionArg(CStr arg) noexcept {
  if (!arg or !arg[0]) return false;
  return op_match(arg, alias::_fun_);
}

void rsOpts::reset() noexcept {
  p_free(deviceXML_);
  p_free(csvFile_);
  p_free(pcfFile_);
  p_free(blifFile_);
  p_free(jsonFile_);
  p_free(input_);
  p_free(output_);
  p_free(assignOrder_);

  CStr keepSV = shortVer_;

  ::memset(this, 0, sizeof(*this));

  shortVer_ = keepSV;
}

void rsOpts::print(CStr label) const noexcept {
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
  printf("  assignOrder_: %s\n", nns(assignOrder_));

  printf("  input_: %s\n", nns(input_));

  printf("  test_id_ = %i\n", test_id_);
  printf("  trace_ = %i  traceIndex_ = %i\n", trace_, traceIndex_);
  cout << "  trace_specified: " << std::boolalpha << trace_specified() << '\n';
  cout << "   unit_specified: " << std::boolalpha << unit_specified() << endl;

  printf("  help_:%i  check_:%i\n", help_, check_);
  printf("  ver:%i  det_ver_:%i\n", version_, det_ver_);

  lputs();
#endif  // RSBE_UNIT_TEST_ON
}

void rsOpts::printHelp() const noexcept {
  cout << "Usage:" << endl;

#ifdef RSBE_UNIT_TEST_ON
#endif  // RSBE_UNIT_TEST_ON
}

static char* make_file_name(CStr arg) noexcept {
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

void rsOpts::setFunction(CStr fun) noexcept {
  function_ = nullptr;
  if (!fun) return;
  static CStr s_funList[] = {"cmd",        // 0
                             "pinc",       // 1
                             "stars",      // 2
                             "partition",  // 3
                             "pack",       // 4
                             "route",      // 5
                             nullptr};
  string f = str::s2lower(fun);
  if (f.empty()) return;
  if (f == "cmd") {
    function_ = s_funList[0];
    assert(is_fun_cmd());
    return;
  }
  if (f == "pin" or f == "pinc" or f == "pin_c") {
    function_ = s_funList[1];
    assert(is_fun_pinc());
    return;
  }
  if (f == "sta" or f == "star" or f == "stars") {
    function_ = s_funList[2];
    assert(is_fun_stars());
    return;
  }
  if (f == "par" or f == "part" or f == "partition") {
    function_ = s_funList[3];
    assert(is_fun_partition());
    return;
  }
  if (f == "pac" or f == "pack" or f == "packing") {
    function_ = s_funList[4];
    assert(is_fun_pack());
    return;
  }
  if (f == "rt" or f == "route" or f == "routing") {
    function_ = s_funList[5];
    assert(is_fun_route());
    return;
  }
}

bool rsOpts::is_arg0_pinc() const noexcept {
  assert(argv_);
  assert(argv_[0]);
  assert(argc_ > 0);
  if (!argv_ or !argv_[0])
    return false;
  if (! ::strcmp(argv_[0], "pin_c"))  
    return true;
  return ends_with_pin_c(argv_[0]);
}

bool rsOpts::is_implicit_pinc() const noexcept {
  assert(argv_);
  assert(argv_[0]);
  assert(argc_ > 0);
  if (!argv_ or !argv_[0])
    return false;

  if (!csvFile_ or !csvFile_[0])
    return false;

  if (!output_ or !output_[0])
    return false;

  if (!assignOrder_ or !assignOrder_[0])
    return false;

  return true;
}

void rsOpts::parse(int argc, const char** argv) noexcept {
  using namespace ::pln::alias;

  reset();
  argc_ = argc;
  argv_ = argv;

  assert(argc_ > 0 and argv_);

  CStr inp = 0, out = 0, csv = 0, xml = 0, pcf = 0, blif = 0, jsnf = 0,
       fun = 0, assignOrd = 0;

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
    if (op_match(arg, _det_ver_)) {
      det_ver_ = true;
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
    if (op_match(arg, _fun_)) {
      i++;
      if (i < argc_)
        fun = argv_[i];
      else
        fun = nullptr;
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
    if (op_match(arg, _assOrd_)) {
      i++;
      if (i < argc_)
        assignOrd = argv_[i];
      else
        assignOrd = nullptr;
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

  assignOrder_ = p_strdup(assignOrd);

  deviceXML_ = p_strdup(xml);
  csvFile_ = p_strdup(csv);
  pcfFile_ = p_strdup(pcf);
  blifFile_ = p_strdup(blif);
  jsonFile_ = p_strdup(jsnf);

  if (fun)
    setFunction(fun);
  else if (isCmdInput())
    setFunction("cmd");

  if (trace_ < 0) trace_ = 0;
  if (test_id_ < 0) test_id_ = 0;
}

// and2_gemini
bool rsOpts::set_VPR_TC1() noexcept {
  lputs(" O-set_VPR_TC1: and2_gemini");
  assert(argc_ > 0 && argv_);
  bool ok = false;

#ifdef RSBE_UNIT_TEST_ON
#endif  // RSBE_UNIT_TEST_ON

  flush_out(true);
  return ok;
}

bool rsOpts::set_STA_testCase(int TC_id) noexcept { return false; }

bool rsOpts::set_VPR_TC_args(CStr raw_tc) noexcept {
  assert(raw_tc);
  cout << '\n' << ::strlen(raw_tc) << endl;
  assert(::strlen(raw_tc) > 3);

  vector<string> W;
  Fio::split_spa(raw_tc, W);

  return createVprArgv(W);
}

static inline bool starts_with_HOME(CStr z) noexcept {
  if (!z or !z[0]) return false;
  constexpr size_t LEN = 6;  // len("$HOME/")
  if (::strlen(z) < LEN) return false;
  return z[0] == '$' && z[1] == 'H' && z[2] == 'O' && z[3] == 'M' && z[4] == 'E' && z[5] == '/';
}

bool rsOpts::createVprArgv(vector<string>& W) noexcept {
  size_t sz = W.size();
  lout() << "W.size()= " << sz << endl;
  if (sz < 3) return false;

  // -- expand $HOME:
  CStr home = ::getenv("HOME");
  constexpr size_t H_LEN = 5;  // len("$HOME")
  if (home) {
    size_t home_len = ::strlen(home);
    if (home_len && home_len < UNIX_Path_Max) {
      string buf;
      for (size_t i = 0; i < sz; i++) {
        string& a = W[i];
        CStr cs = a.c_str();
        if (starts_with_HOME(cs)) {
          buf.clear();
          buf.reserve(a.length() + home_len + 1);
          buf = home;
          buf += cs + H_LEN;
          a.swap(buf);
        }
      }
    }
  }

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

static inline bool ends_with_dot_cmd(CStr z, size_t len) noexcept {
  assert(z);
  if (len < 5) return false;
  return z[len - 1] == 'd' and z[len - 2] == 'm' and z[len - 3] == 'c' and z[len - 4] == '.';
  // .cmd
  // dmc.
}

bool rsOpts::isCmdInput() const noexcept {
  if (!input_ || !input_[0]) return false;
  size_t len = ::strlen(input_);
  if (len < 5 || len > UNIX_Path_Max) return false;
  if (!ends_with_dot_cmd(input_, len)) return false;

  return input_file_exists(input_);
}

bool rsOpts::ends_with_pin_c(CStr z) noexcept {
  if (!z or !z[0])
    return false;
  size_t len = ::strlen(z);
  if (len < 5)
    return false;
  return z[len - 1] == 'c' and z[len - 2] == '_' and z[len - 3] == 'n' and
         z[len - 4] == 'i' and z[len - 5] == 'p';
  // pin_c
  // c_nip
}

// static 
bool rsOpts::validate_pinc_args(int argc, const char** argv) noexcept {
  using std::cerr;
  if (argc < 1 or !argv)
    return false;
  if (!argv[0])
    return false;

  if (ltrace() >= 5) {
    lprintf("=== pin_c args (%i):\n", argc - 1);
    for (int i = 1; i < argc; i++)
      lprintf("    |%i|  %s\n", i, argv[i]);
  }

  rsOpts tmpO(argc, argv);

  CStr csv = tmpO.csvFile_;
  if (!csv or !csv[0]) {
    lputs("\n[Error] required CSV file is not specified");
    cerr << "[Error] required CSV file is not specified" << endl;
    return false;
  }

  if (not Fio::regularFileExists(csv)) {
    lprintf("\n[Error] specified CSV file %s does not exist\n", csv);
    cerr << "[Error] specified CSV file does not exist: " << csv << endl;
    return false;
  }
  if (not Fio::nonEmptyFileExists(csv)) {
    lprintf("\n[Error] specified CSV file %s is empty\n", csv);
    cerr << "[Error] specified CSV file is empty: " << csv << endl;
    return false;
  }

  CStr pcf = tmpO.pcfFile_;
  if (pcf) {
    if (not Fio::regularFileExists(pcf)) {
      lprintf("\n[Error] specified PCF file %s does not exist\n", pcf);
      cerr << "[Error] specified PCF file does not exist" << endl; 
      return false;
    }
  }

  return true;
}

}  // NS

