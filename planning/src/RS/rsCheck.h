#pragma once
//  API entry points for checkers
#ifndef _pln_rs_rsCheck_H_h_
#define _pln_rs_rsCheck_H_h_

#include "RS/rsOpts.h"

namespace pln {

bool do_check(const rsOpts& opts, bool blif_vs_csv);

bool do_check_blif(CStr cfn,
                   std::vector<std::string>& badInputs,
                   std::vector<std::string>& badOutputs);

bool do_cleanup_blif(
                  CStr cfn,
                  std::vector<uspair>& corrected // (lnum, removed_net)
                  );

}

#endif

