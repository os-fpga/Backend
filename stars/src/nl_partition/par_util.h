#pragma once
#ifndef _parCLUSTER_UTIL_H__d7a6211e247104_
#define _parCLUSTER_UTIL_H__d7a6211e247104_

#include "pinc_log.h"
#include "globals.h"
#include "atom_netlist.h"
#include "pack_types.h"
#include "echo_files.h"
#include "vpr_utils.h"
#include "constraints_report.h"

namespace nlp {

bool check_if_xml_mode_conflict(const t_packer_opts& packer_opts,
                             const t_arch* arch,
                             const vtr::vector<ClusterBlockId, std::vector<t_intra_lb_net>*>& intra_lb_routing);

}


#endif

