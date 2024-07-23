#pragma once
#ifndef _pln_frd_primitives_H_e192faa77f7aec_
#define _pln_frd_primitives_H_e192faa77f7aec_

#include "util/pln_log.h"

#undef A_ZERO
#undef CARRY
#undef PLL

namespace pln {
namespace prim {

enum Prim_t {

  A_ZERO         = 0,

  BOOT_CLOCK     = 1,
  CARRY          = 2,
  CLK_BUF        = 3,
  DFFNRE         = 4,
  DFFRE          = 5,
  DSP19X2        = 6,
  DSP38          = 7,
  FIFO18KX2      = 8,
  FIFO36K        = 9,
  I_BUF         = 10,
  I_BUF_DS      = 11,
  I_DDR         = 12,
  I_DELAY       = 13,
  IO_BUF        = 14,
  IO_BUF_DS     = 15,
  I_SERDES      = 16,
  LUT1          = 17,
  LUT2          = 18,
  LUT3          = 19,
  LUT4          = 20,
  LUT5          = 21,
  LUT6          = 22,
  O_BUF         = 23,
  O_BUF_DS      = 24,
  O_BUFT        = 25,
  O_BUFT_DS     = 26,
  O_DDR         = 27,
  O_DELAY       = 28,
  O_SERDES      = 29,
  O_SERDES_CLK  = 30,
  PLL           = 31,
  TDP_RAM18KX2  = 32,
  TDP_RAM36K    = 33,

  X_UNKNOWN     = 34,

  Y_UPPER_GUARD = 35
};

// valid IDs are from 1 to Prim_MAX_ID inclusive
//
constexpr uint Prim_MAX_ID = X_UNKNOWN;


// enum -> string
CStr pr_enum2str(Prim_t enu) noexcept;

// string -> enum, returns A_ZERO on error
Prim_t pr_str2enum(CStr name) noexcept;


bool pr_cpin_is_output(Prim_t primId, CStr pinName) noexcept;
bool pr_cpin_is_clock(Prim_t primId, CStr pinName) noexcept;

inline bool pr_pin_is_output(Prim_t primId, const std::string& pinName) noexcept {
  return pr_cpin_is_output(primId, pinName.c_str());
}
inline bool pr_pin_is_clock(Prim_t primId, const std::string& pinName) noexcept {
  return pr_cpin_is_clock(primId, pinName.c_str());
}

uint pr_num_outputs(Prim_t primId) noexcept;
uint pr_num_clocks(Prim_t primId) noexcept;

bool is_I_SERDES_output_term(const std::string& term) noexcept;
bool is_O_SERDES_output_term(const std::string& term) noexcept;

bool is_TDP_RAM36K_output_term(const std::string& term) noexcept;
bool is_TDP_RAM18KX_output_term(const std::string& term) noexcept;

bool is_PLL_output_term(const std::string& term) noexcept;

}}

#endif

