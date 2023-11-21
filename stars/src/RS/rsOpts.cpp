#include "rsOpts.h"
#include "pinc_Fio.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace rsbe {

using namespace std;
using namespace pinc;

namespace alias {

static const char* _ver_[] = {"V", "v", "ver", "vers", "version", nullptr};

static const char* _det_ver_[] = {"VV", "vv", "VVV", "vvv", "det_ver", nullptr};

static const char* _help_[] = {"H", "h", "help", "hel", "hlp", "-he", nullptr};

static const char* _check_[] = {"ch", "che", "chec", "check", nullptr};

static const char* _csv_[] = {"CS", "cs", "csv", nullptr};

static const char* _xml_[] = {"XM", "xm", "xml", "XML", nullptr};

static const char* _pcf_[] = {"PC", "pc", "pcf", nullptr};

static const char* _blif_[] = {"BL", "blif", nullptr};

static const char* _json_[] = {"JS", "js", "jsf", "json", "port_info", "port_i", "PI", nullptr};

static const char* _output_[] = {"O", "o", "ou", "OU", "out", "outp", "output", nullptr};

static const char* _trace_[] = {"TR", "trace", "tr", "tra", nullptr};

static const char* _test_[] = {"TE", "TC", "test", "te", "tc", "tes", "tst",
                               "test_case", "test_c", nullptr};

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

static bool op_match(const char* op, const char** aliases) noexcept {
  assert(op and aliases);
  if (!op || !aliases) return false;
  assert(op[0] == '-');
  if (op[0] != '-') return false;
  op++;
  if (op[0] == '-') op++;

  for (const char** al = aliases; *al; al++) {
    if (::strcmp(op, *al) == 0) return true;
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
  printf("  ver:%i  det_ver_:%i\n", version_, det_ver_);

  lputs();
#endif  // RSBE_UNIT_TEST_ON
}

void rsOpts::printHelp() const noexcept {
  cout << "Usage:" << endl;

#ifdef RSBE_UNIT_TEST_ON
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
  using namespace ::rsbe::alias;

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

bool rsOpts::set_STA_testCase(int TC_id) noexcept {
  return false;
}

bool rsOpts::set_VPR_TC_args(CStr raw_tc) noexcept {
  assert(raw_tc);
  cout << '\n' << ::strlen(raw_tc) << endl;
  assert(::strlen(raw_tc) > 3);

  vector<string> W;
  fio::Fio::split_spa(raw_tc, W);

  return createVprArgv(W);
}

static inline bool starts_with_HOME(const char* z) noexcept {
  if (!z or !z[0])
    return false;
  constexpr size_t LEN = 6; // len("$HOME/")
  if (::strlen(z) < LEN)
    return false;
  return z[0] == '$' && z[1] == 'H' && z[2] == 'O' && z[3] == 'M' && z[4] == 'E' && z[5] == '/';
}

bool rsOpts::createVprArgv(vector<string>& W) noexcept {
  size_t sz = W.size();
  lout() << "W.size()= " << sz << endl;
  if (sz < 3) return false;

  // -- expand $HOME:
  const char* home = ::getenv("HOME");
  constexpr size_t H_LEN = 5; // len("$HOME")
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

static inline bool ends_with_dot_cmd(const char* z, size_t len) noexcept {
  assert(z);
  if (len < 5) return false;
  return z[len - 1] == 'd' and z[len - 2] == 'm' and z[len - 3] == 'c' and
         z[len - 4] == '.';
  // .cmd
  // dmc.
}

bool rsOpts::isCmdInput() const noexcept {

  if (!input_ || !input_[0])
    return false;
  size_t len = ::strlen(input_);
  if (len < 5 || len > UNIX_Path_Max)
    return false;

  if (!ends_with_dot_cmd(input_, len))
    return false;

  return input_file_exists(input_);
}

}  // NS
