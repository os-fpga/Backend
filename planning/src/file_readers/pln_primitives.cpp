#include "file_readers/pln_primitives.h"
#include <regex>

namespace pln {

using std::vector;
using std::string;

namespace {

  // _enumNames are sorted and Prim_t is sorted, it ensures
  // correct ID -> name mapping. Always keep them in sorted order.
  static const char* _enumNames[] = {
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

  static vector<string> _id2outputs[] = {
    {}, // 0

    { "O" },         // BOOT_CLOCK
    { "O", "COUT" }, // CARRY_CHAIN

    { "O" }, // CLK_BUF

    { "Q" }, // DFFRE

    { "Z1", "DLY_B1", "Z2", "DLY_B2" },  // DSP19X2  = 5,

    { "Z", "DLY_B" },    // DSP38  = 6,

    // FIFO18KX2  = 7,
    { "RD_DATA1", "EMPTY1", "FULL1", "ALMOST_EMPTY1", "ALMOST_FULL1",
      "PROG_EMPTY1", "PROG_FULL1", "OVERFLOW1", "UNDERFLOW1",
      "RD_DATA2", "EMPTY2", "FULL2", "ALMOST_EMPTY2", "ALMOST_FULL2",
      "PROG_EMPTY2", "PROG_FULL2", "OVERFLOW2", "UNDERFLOW2"  },

    // FIFO36K  = 8,
    { "RD_DATA", "EMPTY", "FULL", "ALMOST_EMPTY", "ALMOST_FULL",
      "PROG_EMPTY", "PROG_FULL", "OVERFLOW", "UNDERFLOW"  },

    { "O" },  // I_BUF      = 9,
    { "O" },  // I_BUF_DS   = 10,

    { "Q" },  // I_DDR      = 11,

    { "O", "DLY_TAP_VALUE" },   // I_DELAY  = 12,

    { "O" }, // IO_BUF        = 13,
    { "O" }, // IO_BUF_DS     = 14,


    /* I_SERDES
     CLK_OUT:
       dir: output
       desc: Fabric clock output
     Q[WIDTH-1:0]:
       dir: output
       desc: Data output
     DATA_VALID:
       dir: output
       desc: DATA_VALID output
     DPA_LOCK:
       dir: output
       desc: DPA_LOCK output
     DPA_ERROR:
       dir: output
       desc: DPA_ERROR output
    */
    { "CLK_OUT", "Q", "DATA_VALID", "DPA_LOCK", "DPA_ERROR" },


    { "Y" },   // LUT1  = 16,
    { "Y" },   // LUT2  = 17,
    { "Y" },   // LUT3  = 18,
    { "Y" },   // LUT4  = 19,
    { "Y" },   // LUT5  = 20,
    { "Y" },   // LUT6  = 21,

    { "O" },                 // O_BUF       = 22,
    { "O", "O_P", "O_N" },   // O_BUF_DS    = 23,

    { "O" },                // O_BUFT       = 24,
    { "O", "O_P", "O_N" },  // O_BUFT_DS    = 25,

    { "Q" },  // O_DDR   = 26,

    { "O", "DLY_TAP_VALUE" },  // O_DELAY   = 27,

    /* O_SERDES
     OE_OUT:
       dir: output
       desc: Output tri-state enable output (to O_BUFT or inferred tri-state signal)
     Q:
       dir: output
       desc: Data output (Connect to output port, buffer or O_DELAY)
     CHANNEL_BOND_SYNC_OUT:
       dir: output
    */
    { "OE_OUT", "Q", "CHANNEL_BOND_SYNC_OUT", "DLY_TAP_VALUE" },


    /* O_SERDES_CLK
     CLK_EN:
       dir: input
       desc: Gates output OUTPUT_CLK
     OUTPUT_CLK:
       dir: output
       desc: Clock output (Connect to output port, buffer or O_DELAY)
       type: reg
       default: 1'b0
     PLL_LOCK:
       dir: input
       desc: PLL lock input
     PLL_CLK:
       dir: input
       desc: PLL clock input
    */
    { "OUTPUT_CLK" },


    /* PLL
     CLK_OUT:
       dir: output
     CLK_OUT_DIV2:
       dir: output
     CLK_OUT_DIV3:
       dir: output
     CLK_OUT_DIV4:
       dir: output
     SERDES_FAST_CLK:
       dir: output
     LOCK:
       dir: output
    */
    { "CLK_OUT", "CLK_OUT_DIV2",
      "CLK_OUT_DIV3", "CLK_OUT_DIV4",
      "SERDES_FAST_CLK", "LOCK" },


    // TDP_RAM18KX2  = 31,
    { "RDATA_A1", "RDATA_B1", "RDATA_A2", "RDATA_B2",
      "RPARITY_A1", "RPARITY_B1", "RPARITY_A2", "RPARITY_B2" },

    // TDP_RAM36K  = 32,
    { "RDATA_A", "RPARITY_A", "RDATA_B", "RPARITY_B" },

    {}, // X_UNKNOWN     = 33,
    {}, // Y_UPPER_GUARD = 34
    {}
};

}

static inline bool _vec_contains(const vector<string>& V, CStr name) noexcept {
  for (const string& s : V) {
    if (s == name)
      return true;
  }
  return false;
}

bool prim_cpin_is_output(Prim_t primId, CStr pinName) noexcept {
  uint i = primId;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or !pinName or !pinName[0] or i > Prim_MAX_ID)
    return false;
  const vector<string>& V = _id2outputs[i];
  return _vec_contains(V, pinName);
}

CStr primt_name(Prim_t enu) noexcept {
  static_assert(sizeof(_enumNames) / sizeof(_enumNames[0]) == Y_UPPER_GUARD + 1);
  uint i = enu;
  assert(i <= Prim_MAX_ID);
  return _enumNames[i];
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

bool is_I_SERDES_output_term(const std::string& term) noexcept {
  assert(!term.empty());
  if (term.empty()) return false;

  static std::regex re_iserdes_out{
      R"(CLK_OUT=|Q=|DATA_VALID=|DPA_LOCK=|DPA_ERROR=|Q\[\d+\]=)" };
  
  std::cmatch m;
  bool b = false;

  try {
    b = std::regex_search(term.c_str(), m, re_iserdes_out);
  } catch (...) {
    assert(0);
    b = false;
  }

  //if (b)
  //  lprintf("__I_SERDES_output REGEX matched: %s\n", term.c_str());

  return b;
}

bool is_O_SERDES_output_term(const std::string& term) noexcept {
  assert(!term.empty());
  if (term.empty()) return false;

  static std::regex re_oserdes_out{
      R"(OE_OUT=|Q=|CHANNEL_BOND_SYNC_OUT=|Q\[\d+\]=)" };

  bool b = false;
  std::cmatch m;

  try {
    b = std::regex_search(term.c_str(), m, re_oserdes_out);
  } catch (...) {
    assert(0);
    b = false;
  }

  //if (b)
  //  lprintf("__O_SERDES_output REGEX matched: %s\n", term.c_str());

  return b;
}

bool is_TDP_RAM36K_output_term(const std::string& term) noexcept {
  assert(!term.empty());
  if (term.empty()) return false;

  static std::regex re_RAM36K_out{
      R"(RDATA_A=|RPARITY_A=|RDATA_B=|RPARITY_B=|RDATA_A\[\d+\]=|RPARITY_A\[\d+\]=|RDATA_B\[\d+\]=|RPARITY_B\[\d+\]=)"
  };

  std::cmatch m;
  bool b = false;

  try {
    b = std::regex_search(term.c_str(), m, re_RAM36K_out);
  } catch (...) {
    assert(0);
    b = false;
  }

  //if (b)
  //  lprintf("__RAM36K_output REGEX matched: %s\n", term.c_str());

  return b;
}

bool is_TDP_RAM18KX_output_term(const std::string& term) noexcept {
  assert(!term.empty());
  if (term.empty()) return false;

static CStr p =
R"(RDATA_A1=|RDATA_A1\[\d+\]=|RDATA_A2=|RDATA_A2\[\d+\]=|RDATA_B1=|RDATA_B1\[\d+\]=|RDATA_B2=|RDATA_B2\[\d+\]=|RPARITY_A1\[\d+\]=|RPARITY_A2\[\d+\]=|RPARITY_B1\[\d+\]=|RPARITY_B2\[\d+\]=)";

  static std::regex re_RAM18KX_out{ p };

  std::cmatch m;
  bool b = false;

  try {
    b = std::regex_search(term.c_str(), m, re_RAM18KX_out);
  } catch (...) {
    assert(0);
    b = false;
  }

  //if (b)
  //  lprintf("__RAM18KX_output REGEX matched: %s\n", term.c_str());

  return b;
}

bool is_PLL_output_term(const std::string& term) noexcept {
  assert(!term.empty());
  if (term.empty()) return false;

  // { "CLK_OUT", "CLK_OUT_DIV2",
  //   "CLK_OUT_DIV3", "CLK_OUT_DIV4",
  //   "SERDES_FAST_CLK", "LOCK" }

static CStr p =
R"(CLK_OUT=|CLK_OUT_DIV2=|CLK_OUT_DIV3=|CLK_OUT_DIV4=|SERDES_FAST_CLK=|LOCK=)";

  static std::regex re_PLL_out{ p };

  std::cmatch m;
  bool b = false;

  try {
    b = std::regex_search(term.c_str(), m, re_PLL_out);
  } catch (...) {
    assert(0);
    b = false;
  }

  //if (b)
  //  lprintf("__PLL_output REGEX matched: %s\n", term.c_str());

  return b;
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

  if (::strcmp(name, "CARRY") == 0) // TMP
    return CARRY_CHAIN;
  if (starts_w_CAR(name))
    return CARRY_CHAIN;

  uint X = X_UNKNOWN;
  for (uint i = 1; i < X; i++) {
    if (::strcmp(_enumNames[i], name) == 0)
      return (Prim_t)i;
  }

  return A_ZERO;
}

}

