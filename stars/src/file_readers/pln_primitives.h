#pragma once
#ifndef _pln_frd_primitives_H_e192faa77f7aec_
#define _pln_frd_primitives_H_e192faa77f7aec_

#include "util/pln_log.h"

namespace pln {

enum Prim_t {

  A_ZERO         = 0,

  BOOT_CLOCK     = 1,
  CARRY_CHAIN    = 2,
  CLK_BUF        = 3,
  DFFRE          = 4,
  DSP19X2        = 5,
  DSP38          = 6,
  FIFO18KX2      = 7,
  FIFO36K        = 8,
  I_BUF          = 9,
  I_BUF_DS      = 10,
  I_DDR         = 11,
  I_DELAY       = 12,
  IO_BUF        = 13,
  IO_BUF_DS     = 14,
  I_SERDES      = 15,
  LUT1          = 16,
  LUT2          = 17,
  LUT3          = 18,
  LUT4          = 19,
  LUT5          = 20,
  LUT6          = 21,
  O_BUF         = 22,
  O_BUF_DS      = 23,
  O_BUFT        = 24,
  O_BUFT_DS     = 25,
  O_DDR         = 26,
  O_DELAY       = 27,
  O_SERDES      = 28,
  O_SERDES_CLK  = 29,
  PLL           = 30,
  TDP_RAM18KX2  = 31,
  TDP_RAM36K    = 32,

  X_UNKNOWN     = 33,

  Y_UPPER_GUARD = 34
};

// valid IDs are from 1 to Prim_MAX_ID inclusive
//
constexpr uint Prim_MAX_ID = X_UNKNOWN;


// enum -> string
CStr primt_name(Prim_t enu) noexcept;


// string -> enum, returns A_ZERO on error
Prim_t primt_id(CStr name) noexcept;


bool prim_cpin_is_output(Prim_t primId, CStr pinName) noexcept;

inline bool prim_pin_is_output(Prim_t primId, const std::string& pinName) noexcept {
  return prim_cpin_is_output(primId, pinName.c_str());
}


}

#endif

