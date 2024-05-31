#include "file_readers/pln_primitives.h"

namespace pln {

namespace {
  // enumNames are sorted and Prim_t is sorted, it ensures
  // correct ID -> name mapping. Always keep them in sorted order.
  static const char* enumNames[] = {
    "A_ZERO",
    "BOOT_CLOCK",
    "CARRY_CHAIN",
    "CLK_BUF",
    "DFFRE",
    "DSP19X2",
    "DSP38",
    "FIFO18KX2",
    "FIFO36K",
    "I_BUF",
    "I_BUF_DS",
    "I_DDR",
    "I_DELAY",
    "IO_BUF",
    "IO_BUF_DS",
    "I_SERDES",
    "LUT1",
    "LUT2",
    "LUT3",
    "LUT4",
    "LUT5",
    "LUT6",
    "O_BUF",
    "O_BUF_DS",
    "O_BUFT",
    "O_BUFT_DS",
    "O_DDR",
    "O_DELAY",
    "O_SERDES",
    "O_SERDES_CLK",
    "PLL",
    "TDP_RAM18KX2",
    "TDP_RAM36K",
    "X_UNKNOWN",
    "Y_UPPER_GUARD"
  };
}

CStr primt_name(Prim_t enu) noexcept {
  static_assert(sizeof(enumNames) / sizeof(enumNames[0]) == Y_UPPER_GUARD + 1);
  uint i = enu;
  assert(i <= Prim_MAX_ID);
  return enumNames[i];
}

// "A_"
static inline bool starts_w_A(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 4) < 2)
    return false;
  return z[0] == 'A' and z[1] == '_';
}

// "X_"
static inline bool starts_w_X(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 4) < 2)
    return false;
  return z[0] == 'X' and z[1] == '_';
}

// "Y_"
static inline bool starts_w_Y(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 4) < 2)
    return false;
  return z[0] == 'Y' and z[1] == '_';
}

static inline bool starts_w_CAR(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 8) < 4)
    return false;
  return z[0] == 'C' and z[1] == 'A' and z[2] == 'R';
}

// string -> enum, returns A_ZERO on error
Prim_t primt_id(CStr name) noexcept {
  if (!name or !name[0])
    return A_ZERO;
  if (starts_w_X(name))
    return X_UNKNOWN;
  if (starts_w_Y(name))
    return Y_UPPER_GUARD;
  if (starts_w_A(name))
    return A_ZERO;
  if (starts_w_CAR(name))
    return CARRY_CHAIN;

  uint X = X_UNKNOWN;
  for (uint i = 1; i < X; i++) {
    if (::strcmp(enumNames[i], name) == 0)
      return (Prim_t)i;
  }

  return A_ZERO;
}

}

