#include "RS/plnShell.h"
#include "RS/rsDeal.h"
#include "RS/rsEnv.h"
#include "file_io/pln_Fio.h"

#include "globals.h"
#include "read_options.h"

#include "RS/rsCheck.h"
#include "RS/rsVPR.h"
#include "RS/sta_file_writer.h"

#include <unistd.h>

#ifdef PLN_ENABLE_TCL
#include <readline/history.h>
#include <readline/readline.h>
#endif

namespace pln {

using std::cout;
using std::endl;
using std::string;

static constexpr size_t CMD_BUF_CAP = 1048574;  // ~ 1 MiB

bool deal_shell(const rsOpts& opts) {
  assert(opts.argv_);
  assert(opts.argv_[0]);
  assert(opts.argc_ > 0);

  uint16_t tr = ltrace();
  const rsEnv& env = rsEnv::inst();
  const string& a0 = env.abs_arg0_;
  assert(!a0.empty());

  bool ok = false;
  flush_out(true);
  if (tr >= 4) {
    lprintf("  abs_arg0: %s\n", a0.c_str());
    flush_out(true);
  }

#ifdef PLN_ENABLE_TCL
  Tcl_FindExecutable(a0.c_str());

  Tcl_Interp* ip = Tcl_CreateInterp();
  assert(ip);
  int init_status = Tcl_Init(ip);
  if (init_status != TCL_OK) {
    flush_out(true);
    err_puts();
    lprintf2("[Error] Tcl_Init failed\n");
    err_puts();
    flush_out(true);
    return false;
  }

  Shell sh(ip, opts);

  ok = sh.loop();
  if (!ok) {
    lprintf2("[Error] sh.loop() failed\n");
  }

  Tcl_Finalize();
#endif

  flush_out(true);
  return ok;
}

#ifdef PLN_ENABLE_TCL
static int help_proc(void* clientData, Tcl_Interp* ip, int argc, CStr argv[]) {
  lprintf("help_proc: need help\n");

  return 0;
}

static int reportApp_proc(void* clientData, Tcl_Interp* ip, int argc, CStr argv[]) {
  const rsEnv& env = rsEnv::inst();
  lprintf("  ==== reportApp ====\n");
  lprintf("  PLANNER ver. %s\n", env.shortVerCS());
  lprintf("  ====\n");

  return 0;
}
#endif

Shell* Shell::instance_ = nullptr;

// constructor adds commands to interp_
Shell::Shell(Tcl_Interp* ip, const rsOpts& opts) noexcept : opts_(opts), interp_(ip) {
#ifdef PLN_ENABLE_TCL
  assert(interp_);
  assert(!instance_);
  instance_ = this;

  addCmd("help", help_proc, nullptr, nullptr);
  addCmd("report_app", reportApp_proc, nullptr, nullptr);
#endif
}

Shell::~Shell() {
#ifdef PLN_ENABLE_TCL
  // if (interp_)
  //   Tcl_DeleteInterp(interp_);
#endif
  instance_ = nullptr;
}

int Shell::evalFile(CStr fn, string& result) {
  int code = 0;

#ifdef PLN_ENABLE_TCL
  assert(fn and fn[0]);
  assert(interp_);
  result.clear();
  if (!fn or !fn[0]) {
    result = "(evalFile: bad filename)";
    return TCL_ERROR;
  }

  code = Tcl_EvalFile(interp_, fn);

  if (code >= TCL_ERROR)
    result = tcStackTrace(code);
  else
    result = Tcl_GetStringResult(interp_);
#endif

  return code;
}

int Shell::evalCmd(CStr cmd, string& result) {
  int code = 0;

#ifdef PLN_ENABLE_TCL
  assert(cmd and cmd[0]);
  assert(interp_);
  result.clear();

  code = Tcl_Eval(interp_, cmd);

  if (code >= TCL_ERROR)
    result = tcStackTrace(code);
  else
    result = Tcl_GetStringResult(interp_);
#endif

  return code;
}

int Shell::evalCmd(CStr cmd) {
  int code = 0;
#ifdef PLN_ENABLE_TCL
  assert(cmd and cmd[0]);
  assert(interp_);
  code = Tcl_Eval(interp_, cmd);
#endif
  return code;
}

void Shell::addCmd(CStr cmdName, Tcl_CmdProc proc,
                   ClientData clientData, Tcl_CmdDeleteProc* delProc) {
#ifdef PLN_ENABLE_TCL
  assert(cmdName and cmdName[0]);
  assert(interp_);
  Tcl_CreateCommand(interp_, cmdName, proc, clientData, delProc);
#endif
}

string Shell::tcStackTrace(int code) const noexcept {
  string output;

#ifdef PLN_ENABLE_TCL
  assert(interp_);
  Tcl_Obj* options = Tcl_GetReturnOptions(interp_, code);
  Tcl_Obj* key = Tcl_NewStringObj("-errorinfo", -1);
  Tcl_Obj* stackTrace = nullptr;

  Tcl_IncrRefCount(key);
  Tcl_DictObjGet(nullptr, options, key, &stackTrace);
  Tcl_DecrRefCount(key);

  output = Tcl_GetString(stackTrace);
  Tcl_DecrRefCount(options);
#endif

  return output;
}

bool Shell::loop() {
#ifdef PLN_ENABLE_TCL
  assert(interp_);

  if (opts_.input_) {
    // evaluate the script passed to 'planning'
    string output;
    int code = evalFile(opts_.input_, output);
    if (code == TCL_OK) {
      lout() << output << endl;
    } else {
      flush_out(true);
      lprintf2("[Error] (tcl) could not evaluate command line input: %s\n",
               opts_.input_);
      flush_out(true);
      return false;
    }
  }

  // start interactive UI (readline loop)
  Terminal t(interp_);
  t.rlLoop();
#endif

  return true;
}

Shell::Terminal::Terminal(Tcl_Interp* ip) noexcept
  : interp_(ip), cmdObj_(nullptr),
    isTerm_(false), isEOF_(false),
    contPrompt_(false) {

  partLine_ = (char*)::calloc(CMD_BUF_CAP + 2, 1);
  assert(partLine_);

#ifdef PLN_ENABLE_TCL
  isTerm_ = ::isatty(0);
  if (isTerm_) Tcl_SetVar(interp_, "::tcl_interactive", "1", TCL_GLOBAL_ONLY);

  resetCmdObj();
#endif
}

Shell::Terminal::~Terminal() {
  p_free(partLine_);
}

bool Shell::Terminal::evalCo(CStr lbuf) {
  if (!lbuf) return false;
  if (!lbuf[0]) return false;

  int ecode = 0;

#ifdef PLN_ENABLE_TCL
  char cbuf[CMD_BUF_CAP + 2];
  cbuf[0] = 0;
  cbuf[1] = 0;
  cbuf[CMD_BUF_CAP - 1] = 0;
  cbuf[CMD_BUF_CAP] = 0;
  cbuf[CMD_BUF_CAP + 1] = 0;

  if (partLine_[0]) {
    // continue incomplete line
    ::strncpy(cbuf, partLine_, CMD_BUF_CAP);
    str::chomp(cbuf);
    ::strcat(cbuf, " ");
    ::strcat(cbuf, lbuf);
    partLine_[0] = 0;
  } else {
    ::strcpy(cbuf, lbuf);
  }

  ecode = execCmdObj(cbuf);
#endif

  return ecode == TCL_OK;
}

int Shell::Terminal::execCmdObj(CStr cbuf) {
  assert(partLine_);
  int code = 0;

#ifdef PLN_ENABLE_TCL
  Tcl_SetStringObj(cmdObj_, cbuf, ::strlen(cbuf));

  if (not Tcl_CommandComplete(cbuf)) {
    contPrompt_ = true;
    ::strcpy(partLine_, cbuf);
    ::strcat(partLine_, "\n");
    return TCL_OK;
  }
  contPrompt_ = false;

  code = Tcl_RecordAndEvalObj(interp_, cmdObj_, TCL_EVAL_GLOBAL);
  resetCmdObj();

  Tcl_Obj* objResult = Tcl_GetObjResult(interp_);
  Tcl_IncrRefCount(objResult);
  int len = 0;
  CStr csResult = Tcl_GetStringFromObj(objResult, &len);

  if (code != TCL_OK) {
    if (csResult and len > 0) {
      lprintf2("[Error] (Tcl) %s\n", csResult);
    }
  }
  else if (isTerm_) {
    if (csResult and len > 0 and Tcl_GetStdChannel(TCL_STDOUT)) {
      lprintf("%s\n", csResult);
    }
  }
  Tcl_DecrRefCount(objResult);
  Tcl_ResetResult(interp_);
#endif

  return code;
}

void Shell::Terminal::resetCmdObj() noexcept {
#ifdef PLN_ENABLE_TCL
  if (cmdObj_) Tcl_DecrRefCount(cmdObj_);
  cmdObj_ = Tcl_NewObj();
  assert(cmdObj_);
  Tcl_IncrRefCount(cmdObj_);
#endif
}

void Shell::Terminal::rlLoop() {
#ifdef PLN_ENABLE_TCL
  const Shell& sh = Shell::inst();

  char lbuf[CMD_BUF_CAP + 2];
  lbuf[0] = 0;
  lbuf[1] = 0;
  lbuf[CMD_BUF_CAP - 1] = 0;
  lbuf[CMD_BUF_CAP] = 0;
  lbuf[CMD_BUF_CAP + 1] = 0;

  while (!sh.inExit()) {
    if (not Tcl_GetStdChannel(TCL_STDIN)) break;
    CStr line = ::readline(contPrompt_ ? ">> " : "pln> ");
    if (not line) break;
    if (not line[0]) continue;
    ::strncpy(lbuf, line, CMD_BUF_CAP);
    str::chomp(lbuf);
    evalCo(lbuf);
  }
#endif
}

}

