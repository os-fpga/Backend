#include "RS/rsEnv.h"
#include "RS/rsGlobal.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

namespace pln {

using std::cout;
using std::endl;

rsEnv::rsEnv() noexcept {
  CStr ts = ::getenv("pln_trace_marker");
  if (ts) traceMarker_ = ts;

  ts = ::getenv("pln_no_logfile");
  if (not ts) {
    open_pln_logfile("rs_planner.log");
  }

  init();
}

void rsEnv::init() noexcept {
  pid_ = getpid();
  ppid_ = getppid();

  rusage u;
  int status = getrusage(RUSAGE_SELF, &u);
  if (status == 0) rss0_ = u.ru_maxrss;

  std::string exe_link = "/proc/";
  exe_link += std::to_string(pid_);
  exe_link += "/exe";

  readLink(exe_link, selfPath_);

  exe_link = "/proc/";
  exe_link += std::to_string(ppid_);
  exe_link += "/exe";

  readLink(exe_link, parentPath_);
}

void rsEnv::initVersions(CStr vs) noexcept {
  assert(vs);
  shortVer_ = vs ? vs : "(NULL)";

  longVer_.reserve(128);
  compTimeStr_.reserve(48);

#ifdef NDEBUG
  longVer_ = "NDEBUG";
#else
  longVer_ = "ASSER";
#endif

#ifdef __OPTIMIZE__
  longVer_ += " OPT ";
#else
  longVer_ += " O0 ";
#endif

  if (vs) longVer_ += vs;

  longVer_ += " main";

  compTimeStr_ = __DATE__;
  compTimeStr_ += "  ";
  compTimeStr_ += __TIME__;
};

void rsEnv::reset() noexcept {
  arg0_.clear();
  abs_arg0_.clear();

  orig_argV_.clear();
  filtered_argV_.clear();

  init();

  opts_.reset();
}

void rsEnv::parse(int argc, char** argv) noexcept {
  assert(argc > 0 && argv);
  assert(argc < 1000000);
  assert(argv[0]);

  reset();

  pid_ = getpid();
  ppid_ = getppid();

  arg0_ = argv[0];
  orig_argV_.reserve(argc + 1);
  filtered_argV_.reserve(argc + 1);
  int i;
  for (i = 0; i < argc; i++) {
    assert(argv[i]);
    orig_argV_.emplace_back(argv[i]);
  }

  char buf[8192];
  buf[0] = 0;
  buf[1] = 0;
  buf[8190] = 0;
  buf[8191] = 0;

  if (!realpath(argv[0], buf)) {
    perror("realpath()");
    abs_arg0_ = arg0_;
  } else {
    abs_arg0_ = buf;
  }

  opts_.parse(argc, (const char**)argv);

  if (opts_.traceIndex_ > 0) {
    filtered_argV_.emplace_back(arg0_);
    for (i = 1; i < argc; i++) {
      if (i == opts_.traceIndex_) {
        i++;
        continue;
      }
      filtered_argV_.emplace_back(orig_argV_[i]);
    }
  } else {
    filtered_argV_ = orig_argV_;
  }
}

void rsEnv::dump(CStr prefix) const noexcept {
  if (prefix) cout << prefix;

  cout.flush();
  fflush(stdout);
  printPids("\n ");

  cout << "      longVer_  " << longVer_ << endl;
  cout << "  compTimeStr_  " << compTimeStr_ << endl;

  cout << "  argCount()  = " << argCount() << '\n';
  cout << "      arg0_   = " << arg0_ << endl;
  cout << "  abs_arg0_   = " << abs_arg0_ << endl;

  cout << "     self-path: " << selfPath_ << endl;
  cout << "   parent-path: " << parentPath_ << endl;
  cout << "  initial RSS = " << megaRss0() << " MB\n" << endl;

  cout << "filtered_argV_.size() = " << filtered_argV_.size() << endl;

  size_t sz = orig_argV_.size();
  cout << "\t    [arguments]  orig_argV_.size()= " << sz << endl;
  for (size_t i = 1; i < sz; i++) {
    cout << "\t    " << i << ": " << orig_argV_[i] << endl;
  }
  cout << "\t    [arguments]  orig_argV_.size()= " << sz << endl;

  cout << endl;
}

void rsEnv::print(std::ostream& os, CStr prefix) const noexcept {
  if (prefix) os << prefix;

  os << "#      longVer_  " << longVer_ << endl;
  os << "#  compTimeStr_  " << compTimeStr_ << endl;
  os << "#  pid_: " << pid_ << "  ppid_:" << ppid_ << endl;

  os << "#  argCount()  = " << argCount() << '\n';
  os << "#      arg0_   = " << arg0_ << endl;
  os << "#  abs_arg0_   = " << abs_arg0_ << endl;

  os << "#     self-path: " << selfPath_ << endl;
  os << "#   parent-path: " << parentPath_ << endl;
  os << "#  initial RSS = " << megaRss0() << " MB\n" << endl;

  size_t sz = orig_argV_.size();
  os << "#\t    [arguments]  orig_argV_.size()= " << sz << endl;
  for (size_t i = 1; i < sz; i++) {
    os << "#\t    " << i << ": " << orig_argV_[i] << '\n';
  }
  os << "#\t    [arguments]  orig_argV_.size()= " << sz << '\n' << endl;

  os << "# COMMAND LINE:\n" << endl;
  os << ' ' << abs_arg0_;
  for (size_t i = 1; i < sz; i++) os << ' ' << orig_argV_[i];

  os << '\n' << endl;
}

void rsEnv::printPids(CStr prefix) const noexcept {
  if (!prefix) prefix = " ";
  printf("%s pid_: %i  ppid_: %i\n", prefix, pid_, ppid_);
}

bool rsEnv::readLink(const std::string& path, std::string& out) noexcept {
  out.clear();
  if (path.empty()) return false;

  CStr cs = path.c_str();

  struct stat sb;
  memset(&sb, 0, sizeof(sb));

  if (::stat(cs, &sb)) return false;
  if (not(S_IFREG & sb.st_mode)) return false;

  memset(&sb, 0, sizeof(sb));
  if (::lstat(cs, &sb)) return false;

  if (not(S_ISLNK(sb.st_mode))) {
    out = "(* NOT A LINK : !S_ISLNK *)";
    return false;
  }

  char buf[8192];
  buf[0] = 0;
  buf[1] = 0;
  buf[8190] = 0;
  buf[8191] = 0;

  int64_t numBytes = ::readlink(cs, buf, 8190);
  if (numBytes == -1) {
    perror("readlink()");
    return false;
  }
  buf[numBytes] = 0;
  out = buf;

  return true;
}

bool rsEnv::getPidExePath(int pid, std::string& out) noexcept { return true; }

#define LIST_DEC(ITzz)  { cout << std::setw(32) << #ITzz << " : " << (ITzz) << endl; }

#define LIST_HEX(ITzz) { std::ios_base::fmtflags origF = cout.flags(); \
    cout.setf(std::ios::showbase); cout << std::setw(32) << #ITzz << " : " << std::hex << (ITzz) << endl; \
    cout.setf(origF); }

void rsEnv::listDevEnv() const noexcept {
  printf("\n\t   PLN ver.  %s\n", shortVer_.c_str());
  printf("\t compiled:  %s\n", compTimeCS());

  size_t cxxstd = __cplusplus;
  printf("\n(compiler) -- c++std= %zu\n", cxxstd);
#ifdef __GNUC__
  printf("\t  __GNUC__ \t\t %i\n", __GNUC__);
#endif
#ifdef __GNUC_MINOR__
  printf("\t  __GNUC_MINOR__ \t %i\n", __GNUC_MINOR__);
#endif
#ifdef __VERSION__
  printf("\t  __VERSION__ \t\t %s\n", __VERSION__);
#endif
#ifdef __OPTIMIZE__
  printf("\t  __OPTIMIZE__ \t\t %i\n", __OPTIMIZE__);
#else
  printf("\t  __OPTIMIZE__  :\t NOT DEFINED  => deb binary\n");
#endif
#ifdef __NO_INLINE__
  printf("\t  __NO_INLINE__ :\t (defined)\n");
#endif
#ifdef NDEBUG
  printf("\t  NDEBUG :  (defined)  => assert() is disabled\n");
#else
  printf("\t  NDEBUG :  NOT DEFINED  => assert() is enabled\n");
#endif

  printf("\n(modes)\n");
#ifdef NO_GRAPHICS
  printf("\t  NO_GRAPHICS :  \t (defined)\n");
#endif
#ifdef RS_PC_MODE
  printf("\t  RS_PC_MODE :\t (defined)\n");
#endif
#ifdef PINC_DEVEL_MODE
  printf("\t  PINC_DEVEL_MODE :\t (defined)\n");
#endif
#ifdef NN_FAST_BUILD
  printf("\t  NN_FAST_BUILD :\t (defined)\n");
#endif
#ifdef ENABLE_ANALYTIC_PLACE
  printf("\t  ENABLE_ANALYTIC_PLACE :\t (defined)\n");
#endif

  cout << endl;

#ifdef PLN_ENABLE_TCL
  printf("\t  PLN_ENABLE_TCL :   ON  => Tcl shell enabled\n");
#else
  printf("\t  PLN_ENABLE_TCL :   OFF => Tcl shell disabled\n");
#endif

// #ifdef _PLN_VEC_BOUNDS_CHECK
//   printf("\t  _PLN_VEC_BOUNDS_CHECK :   ON  => stl_vector BC enabled\n");
// #else
//   printf("\t  _PLN_VEC_BOUNDS_CHECK :   OFF => std_vector BC disabled\n");
// #endif

#ifdef PLN_JEMALLOC
    LIST_DEC(PLN_JEMALLOC);
#endif

  cout << endl;
}

}
