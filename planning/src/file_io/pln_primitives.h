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
  I_FAB         = 15,
  IO_BUF        = 16,
  IO_BUF_DS     = 17,
  I_SERDES      = 18,
  LUT1          = 19,
  LUT2          = 20,
  LUT3          = 21,
  LUT4          = 22,
  LUT5          = 23,
  LUT6          = 24,
  O_BUF         = 25,
  O_BUF_DS      = 26,
  O_BUFT        = 27,
  O_BUFT_DS     = 28,
  O_DDR         = 29,
  O_DELAY       = 30,
  O_FAB         = 31,
  O_SERDES      = 32,
  O_SERDES_CLK  = 33,
  PLL           = 34,
  TDP_RAM18KX2  = 35,
  TDP_RAM36K    = 36,

  X_UNKNOWN     = 37,

  Y_UPPER_GUARD = 38
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

inline bool pr_is_LUT(Prim_t t) noexcept  { return t >= LUT1 and t <= LUT6; }
inline bool pr_is_DSP(Prim_t t) noexcept  { return t == DSP19X2 or t == DSP38; }
inline bool pr_is_DFF(Prim_t t) noexcept  { return t == DFFNRE or t == DFFRE; }
inline bool pr_is_FIFO(Prim_t t) noexcept { return t == FIFO18KX2 or t == FIFO36K; }

inline bool pr_is_RAM(Prim_t t) noexcept {
  return t == TDP_RAM18KX2 or t == TDP_RAM36K;
}

inline bool pr_is_MOG(Prim_t t) noexcept {
  if (pr_is_LUT(t) or pr_is_DFF(t))
    return false;
  if (pr_is_DSP(t) or pr_is_RAM(t))
    return true;
  return pr_num_outputs(t) > 1;
}

bool pr_is_core_fabric(Prim_t t) noexcept;

void pr_get_inputs(Prim_t pt, std::vector<std::string>& INP) noexcept;
void pr_get_outputs(Prim_t pt, std::vector<std::string>& OUT) noexcept;

std::string pr_first_input(Prim_t pt) noexcept;
std::string pr_first_output(Prim_t pt) noexcept;

bool is_I_SERDES_output_term(const std::string& term) noexcept;
bool is_O_SERDES_output_term(const std::string& term) noexcept;

bool is_TDP_RAM36K_output_term(const std::string& term) noexcept;
bool is_TDP_RAM18KX_output_term(const std::string& term) noexcept;

bool is_PLL_output_term(const std::string& term) noexcept;

bool is_DSP38_output_term(const std::string& term) noexcept;
bool is_DSP19X2_output_term(const std::string& term) noexcept;

// DEBUG:
std::string pr_write_yaml(Prim_t pt) noexcept;

}}

#endif

