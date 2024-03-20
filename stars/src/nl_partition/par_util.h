#pragma once
#ifndef _pln_parCLUSTER_UTIL_H__259961046e_
#define _pln_parCLUSTER_UTIL_H__259961046e_

#include "pinc_log.h"
#include "globals.h"
#include "atom_netlist.h"
#include "pack_types.h"
#include "echo_files.h"
#include "vpr_utils.h"
#include "constraints_report.h"

namespace pln {

bool check_if_xml_mode_conflict(const t_packer_opts& packer_opts,
                             const t_arch* arch,
                             const vtr::vector<ClusterBlockId, std::vector<t_intra_lb_net>*>& intra_lb_routing);

}


#endif

