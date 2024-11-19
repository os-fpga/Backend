#pragma once
//
//  'planning' exec modes - entry points to various operations.
//  'planning' main() calls one of these deal_ functions.
//

#include "RS/rsEnv.h"

namespace pln {

bool deal_check(const rsOpts& opts);

bool deal_cleanup(const rsOpts& opts);

bool deal_stars(const rsOpts& opts, bool orig_args);

bool deal_vpr(const rsOpts& opts, bool orig_args);

bool deal_cmd(rsOpts& opts);

bool deal_pinc(const rsOpts& opts, bool orig_args);

void deal_help(const rsOpts& opts);

void deal_units(const rsOpts& opts);

bool deal_shell(const rsOpts& opts);

bool validate_partition_opts(const rsOpts& opts);

}

