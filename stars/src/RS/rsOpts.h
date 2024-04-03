#pragma once
#ifndef __pln_Opts_H__e91c943c16c2d_
#define __pln_Opts_H__e91c943c16c2d_

#include "util/pln_log.h"

namespace pln {

using std::string;
using std::vector;
using CStr = const char*;

struct rsOpts {

  CStr shortVer_ = nullptr;

  int argc_ = 0;
  const char** argv_ = nullptr;

  CStr function_ = nullptr; // {cmd, pinc, stars, partition, pack, route}
  bool have_function() const noexcept { return function_; }
  bool is_fun_cmd() const noexcept {
    return function_ && !strcmp(function_, "cmd");
  }
  bool is_fun_pinc() const noexcept {
    return function_ && !strcmp(function_, "pinc");
  }
  bool is_fun_stars() const noexcept {
    return function_ && !strcmp(function_, "stars");
  }
  bool is_fun_partition() const noexcept {
    return function_ && !strcmp(function_, "partition");
  }
  bool is_fun_pack() const noexcept {
    return function_ && !strcmp(function_, "pack");
  }
  bool is_fun_route() const noexcept {
    return function_ && !strcmp(function_, "route");
  }
  bool is_arg0_pinc() const noexcept;
  bool is_implicit_pinc() const noexcept;

  static bool isFunctionArg(CStr arg) noexcept;
  static bool ends_with_pin_c(CStr z) noexcept;
  static bool validate_pinc_args(int argc, const char** argv) noexcept;

  int vprArgc_ = 0;
  char** vprArgv_ = nullptr;

  char* deviceXML_ = nullptr;
  char* csvFile_ = nullptr;

  char* pcfFile_ = nullptr;
  char* blifFile_ = nullptr;
  char* jsonFile_ = nullptr;

  char* input_ = nullptr;
  char* output_ = nullptr;
  char* assignOrder_ = nullptr;

  int test_id_ = 0;  // TestCase ID

  int trace_ = 0;
  int traceIndex_ = 0;

  bool version_ = false;
  bool det_ver_ = false;

  bool help_ = false;
  bool check_ = false;

  bool unit1_ = false;
  bool unit2_ = false;
  bool unit3_ = false;
  bool unit4_ = false;
  bool unit5_ = false;
  bool unit6_ = false;
  bool unit7_ = false;

  rsOpts() noexcept = default;
  rsOpts(int argc, const char** argv) noexcept { parse(argc, argv); }
  ~rsOpts() { reset(); }

  void parse(int argc, const char** argv) noexcept;

  void reset() noexcept;
  void print(CStr label = nullptr) const noexcept;
  void printHelp() const noexcept;

  bool unit_specified() const noexcept {
    return unit1_ || unit2_ || unit3_ || unit4_ || unit5_ || unit6_ || unit7_;
  }
  bool ver_or_help() const noexcept { return version_ || det_ver_ || help_; }

  bool trace_specified() const noexcept { return traceIndex_ > 0; }
  bool test_id_specified() const noexcept { return test_id_ > 0; }

  bool set_VPR_TC1() noexcept;  // and2_gemini
  bool set_STA_testCase(int TC_id) noexcept;

  bool createVprArgv(vector<string>& W) noexcept;

  bool isCmdInput() const noexcept;

private:
  bool set_VPR_TC_args(CStr raw_tc) noexcept;

  void setFunction(CStr fun) noexcept;
};

}

#endif
