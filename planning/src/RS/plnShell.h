#pragma once
//
//  class Shell - Tcl_Interp wrapper
//  based on FOEDAG/src/Tcl/TclInterpreter.h
//

#ifndef _pln_Shell_H_0df6e17bc86545_
#define _pln_Shell_H_0df6e17bc86545_

#include <tcl.h>
#include "RS/rsEnv.h"

namespace pln {

class Shell {
public:
  using string = std::string;
  // using TCallback0 = std::function<void()>;
  // using TCallback1 = std::function<void(const string& arg1)>;
  // using TCallback2 = std::function<void(const string& arg1, const string& arg2)>;

  struct Terminal;

  Shell(Tcl_Interp* ip, const rsOpts& opts) noexcept;
  ~Shell();

  bool loop();
  bool inExit() const noexcept { return false; }

  int evalFile(CStr fn, string& result);
  int evalCmd(CStr cmd, string& result);
  int evalCmd(CStr cmd);

  void setResult(const string& result);

  void addCmd(CStr cmdName, Tcl_CmdProc proc,
              ClientData clientData, Tcl_CmdDeleteProc* delProc);

  Tcl_Interp* interp() noexcept { return interp_; }

  static Shell* instance() noexcept { return instance_; }
  static Shell& inst() noexcept {
    assert(instance_);
    return *instance_;
  }

private:
  string tcHistoryScript();
  string tcStackTrace(int code) const noexcept;

  const rsOpts& opts_;
  Tcl_Interp* interp_ = nullptr;
  static Shell* instance_;
};

struct Shell::Terminal {
  Terminal(Tcl_Interp* inter) noexcept;
  ~Terminal();

  void rlLoop();

  bool evalCo(CStr lbuf);

private:
  int execCmdObj(CStr cbuf);
  void resetCmdObj() noexcept;

  // DATA:
  Tcl_Interp* interp_ = nullptr;
  Tcl_Obj* cmdObj_ = nullptr;

  bool isTerm_ = false, isEOF_ = false;
  bool contPrompt_ = false;

  char* partLine_ = nullptr;
};

}

#endif

