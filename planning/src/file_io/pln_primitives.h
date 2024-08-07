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
  FCLK_BUF       = 8,
  FIFO18KX2      = 9,
  FIFO36K       = 10,
  I_BUF         = 11,
  I_BUF_DS      = 12,
  I_DDR         = 13,
  I_DELAY       = 14,
  IO_BUF        = 15,
  IO_BUF_DS     = 16,
  I_SERDES      = 17,
  LUT1          = 18,
  LUT2          = 19,
  LUT3          = 20,
  LUT4          = 21,
  LUT5          = 22,
  LUT6          = 23,
  O_BUF         = 24,
  O_BUF_DS      = 25,
  O_BUFT        = 26,
  O_BUFT_DS     = 27,
  O_DDR         = 28,
  O_DELAY       = 29,
  O_SERDES      = 30,
  O_SERDES_CLK  = 31,
  PLL           = 32,
  TDP_RAM18KX2  = 33,
  TDP_RAM36K    = 34,

  X_UNKNOWN     = 35,

  Y_UPPER_GUARD = 36
};

// valid IDs are from 1 to Prim_MAX_ID inclusive
//
constexpr uint Prim_MAX_ID = X_UNKNOWN;


// enum -> string
CStr pr_enum2str(Prim_t enu) noexcept;

// string -> enum, returns A_ZERO on error
Prim_t pr_str2enum(CStr name) noexcept;


bool pr_cpin_is_output(Prim_t pt, CStr pinName) noexcept;
bool pr_cpin_is_clock(Prim_t pt, CStr pinName) noexcept;

inline bool pr_pin_is_output(Prim_t pt, const std::string& pinName) noexcept {
  return pr_cpin_is_output(pt, pinName.c_str());
}
inline bool pr_pin_is_clock(Prim_t pt, const std::string& pinName) noexcept {
  return pr_cpin_is_clock(pt, pinName.c_str());
}

uint pr_num_inputs(Prim_t pt) noexcept;
uint pr_num_outputs(Prim_t pt) noexcept;
uint pr_num_clocks(Prim_t pt) noexcept;

void pr_get_inputs(Prim_t pt, std::vector<std::string>& INP);
void pr_get_outputs(Prim_t pt, std::vector<std::string>& OUT);

bool is_I_SERDES_output_term(const std::string& term) noexcept;
bool is_O_SERDES_output_term(const std::string& term) noexcept;

bool is_TDP_RAM36K_output_term(const std::string& term) noexcept;
bool is_TDP_RAM18KX_output_term(const std::string& term) noexcept;

bool is_PLL_output_term(const std::string& term) noexcept;

// DEBUG:
std::string pr_write_yaml(Prim_t pt) noexcept;

}}

#endif

