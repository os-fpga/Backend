#pragma once
/*
 * Global variables
 * Key global variables that are used everywhere in stars.
 */

#ifndef __pln__RS__STARS_GLOBALS_H_
#define __pln__RS__STARS_GLOBALS_H_

// VPR header: vpr/src/base/vpr_types.h
#include "vpr_types.h"

//#define RSBE_DEAL_PINC 1
#undef RSBE_DEAL_PINC
#define RSBE_DEAL_VPR  1
// else: stars


namespace rsbe {

t_vpr_setup* get_vprSetup() noexcept;

}

#endif
