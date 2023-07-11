#pragma once
#ifndef __rsbe_staRSTA_Opts_H___
#define __rsbe_staRSTA_Opts_H___

#include "pinc_log.h"

namespace rsbe {

inline void zFree(void* p) noexcept {
  if (p) ::free(p);
}

struct rsOpts {
  using CStr = const char*;

  CStr shortVer_ = nullptr;

  int argc_ = 0;
  const char** argv_ = nullptr;

  int vprArgc_ = 0;
  char** vprArgv_ = nullptr;

  char* deviceXML_ = nullptr;
  char* csvFile_ = nullptr;

  char* pcfFile_ = nullptr;
  char* blifFile_ = nullptr;
  char* jsonFile_ = nullptr;

  char* input_ = nullptr;
  char* output_ = nullptr;

  int test_id_ = 0;  // TestCase ID

  int trace_ = 0;
  int traceIndex_ = 0;

  bool version_ = false;
  bool dev_ver_ = false;

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
  bool ver_or_help() const noexcept { return version_ || dev_ver_ || help_; }

  bool trace_specified() const noexcept { return traceIndex_ > 0; }
  bool test_id_specified() const noexcept { return test_id_ > 0; }

  bool set_PINC_test() noexcept;

  bool set_VPR_TC1() noexcept;  // and2_gemini
  bool set_VPR_TC2() noexcept;  // flop2flop

private:
  bool sprintFiles(CStr subdir, CStr stem) noexcept;
};

}

#endif
