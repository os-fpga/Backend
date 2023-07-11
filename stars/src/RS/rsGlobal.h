#pragma once
/*
 * Global variables
 * Key global variables that are used everywhere in stars.
 */

#ifndef __rsbe__RS__STARS_GLOBALS_H_
#define __rsbe__RS__STARS_GLOBALS_H_

// VPR header: vpr/src/base/vpr_types.h
#include "vpr_types.h"

namespace rsbe {

t_vpr_setup* get_vprSetup() noexcept;

}

#endif

