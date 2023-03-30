#pragma once

#include "util/cmd_line.h"
#include "pin_loc/pin_location.h"

// wraper function to do the real job, it is used by openfpga shell as well
// this gurantees same behavior
int pin_constrain_location(const pinc::cmd_line& cmd);

