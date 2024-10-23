#pragma once
//  API entry points for checkers
#ifndef _pln_rs_rsCheck_H_h_
#define _pln_rs_rsCheck_H_h_

#include "RS/rsOpts.h"

namespace pln {

bool do_check(const rsOpts& opts, bool blif_vs_csv);

bool do_check_blif(
                  CStr fileName,
                  std::vector<std::string>& badInputs,
                  std::vector<std::string>& badOutputs,
                  std::vector<uspair>& corrected,

                  bool cleanup = false // cleanup => checker may modify the blif

                  );


bool do_cleanup_blif(
                  CStr fileName,
                  std::vector<uspair>& corrected // (lnum, removed_net)
                  );


}

#endif

