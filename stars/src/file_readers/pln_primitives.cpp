#include "file_readers/pln_primitives.h"

namespace pln {


CStr primt_name(Prim_t enu) noexcept {
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
  static_assert(sizeof(enumNames) / sizeof(enumNames[0]) == Y_UPPER_GUARD + 1);

  uint i = enu;
  assert(i <= Prim_MAX_ID);

  return enumNames[i];
}


}

