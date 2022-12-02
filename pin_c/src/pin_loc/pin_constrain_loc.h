#pragma once

#include "cmd_line.h"
#include "pin_location.h"

// wraper function to do the real job, it is used by openfpga shell as well
// this gurantees same behavior
int pin_constrain_location(const pinc::cmd_line& cmd);

/*
int pin_constrain_location(cmd_line &cmd) {
  pin_location pl(cmd);
  // pl.get_cmd().print_options();
  if (!pl.reader_and_writer()) {
    return 1;
  }
  return 0;
}
*/

