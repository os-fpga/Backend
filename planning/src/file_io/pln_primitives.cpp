#include "file_io/pln_primitives.h"
#include <regex>

namespace pln {
namespace prim {

using std::vector;
using std::string;

  // _enumNames are sorted and Prim_t is sorted, it ensures
  // correct ID -> name mapping. Always keep them in sorted order.
  static const char* _enumNames[] = {
    "A_ZERO",
    "BOOT_CLOCK",
    "CARRY",
    "CLK_BUF",
    "DFFNRE",
    "DFFRE",
    "DSP19X2",
    "DSP38",
    "FCLK_BUF",
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
    { "O", "COUT" }, // CARRY

    { "O" }, // CLK_BUF

    { "Q" }, // DFFNRE

    { "Q" }, // DFFRE

    { "Z1", "DLY_B1", "Z2", "DLY_B2" },  // DSP19X2

    { "Z", "DLY_B" },    // DSP38

    { "O" },  // FCLK_BUF

    // FIFO18KX2
    { "RD_DATA1", "EMPTY1", "FULL1", "ALMOST_EMPTY1", "ALMOST_FULL1",
      "PROG_EMPTY1", "PROG_FULL1", "OVERFLOW1", "UNDERFLOW1",
      "RD_DATA2", "EMPTY2", "FULL2", "ALMOST_EMPTY2", "ALMOST_FULL2",
      "PROG_EMPTY2", "PROG_FULL2", "OVERFLOW2", "UNDERFLOW2"  },

    // FIFO36K
    { "RD_DATA", "EMPTY", "FULL", "ALMOST_EMPTY", "ALMOST_FULL",
      "PROG_EMPTY", "PROG_FULL", "OVERFLOW", "UNDERFLOW"  },

    { "O" },  // I_BUF
    { "O" },  // I_BUF_DS

    { "Q" },  // I_DDR

    { "O", "DLY_TAP_VALUE" },   // I_DELAY

    { "O" }, // IO_BUF
    { "O" }, // IO_BUF_DS

    // I_SERDES
    { "CLK_OUT", "Q", "DATA_VALID", "DPA_LOCK", "DPA_ERROR" },

    { "Y" },   // LUT1
    { "Y" },   // LUT2
    { "Y" },   // LUT3
    { "Y" },   // LUT4
    { "Y" },   // LUT5
    { "Y" },   // LUT6

    { "O" },                 // O_BUF
    { "O", "O_P", "O_N" },   // O_BUF_DS

    { "O" },                // O_BUFT
    { "O", "O_P", "O_N" },  // O_BUFT_DS

    { "Q" },  // O_DDR

    { "O", "DLY_TAP_VALUE" },  // O_DELAY

    // O_SERDES
    { "OE_OUT", "Q", "CHANNEL_BOND_SYNC_OUT", "DLY_TAP_VALUE" },

    { "OUTPUT_CLK" },   // O_SERDES_CLK

    // PLL
    { "CLK_OUT", "CLK_OUT_DIV2",
      "CLK_OUT_DIV3", "CLK_OUT_DIV4",
      "SERDES_FAST_CLK", "LOCK" },

    // TDP_RAM18KX2
    { "RDATA_A1", "RDATA_B1", "RDATA_A2", "RDATA_B2",
      "RPARITY_A1", "RPARITY_B1", "RPARITY_A2", "RPARITY_B2" },

    // TDP_RAM36K
    { "RDATA_A", "RPARITY_A", "RDATA_B", "RPARITY_B" },

    {}, // X_UNKNOWN
    {}, // Y_UPPER_GUARD
    {}
  }; // _id2outputs

  static vector<string> _id2inputs[] = {
    {}, // 0

    {  },                // BOOT_CLOCK
    { "P", "G", "CIN" }, // CARRY

    { "I" }, // CLK_BUF

    { "D", "R", "E", "C" }, // DFFNRE

    { "D", "R", "E", "C" }, // DFFRE

  // DSP19X2
  {
  "A1[8]", "A1[7]", "A1[6]", "A1[5]", "A1[4]", "A1[3]", "A1[2]", "A1[1]", "A1[0]",
  "B1[9]", "B1[8]", "B1[7]", "B1[6]", "B1[5]", "B1[4]", "B1[3]", "B1[2]", "B1[1]", "B1[0]",
  "CLK", "RESET", "LOAD_ACC", "UNSIGNED_A", "UNSIGNED_B",
  "SATURATE", "ROUND", "SUBTRACT"
  },

  // DSP38
  {
  "A[19]", "A[18]", "A[17]", "A[16]", "A[15]", "A[14]", "A[13]", "A[12]", "A[11]", "A[10]",
  "A[9]", "A[8]", "A[7]", "A[6]", "A[5]", "A[4]", "A[3]", "A[2]", "A[1]", "A[0]",
  "B[17]", "B[16]", "B[15]", "B[14]", "B[13]", "B[12]", "B[11]", "B[10]", "B[9]", "B[8]",
  "B[7]", "B[6]", "B[5]", "B[4]", "B[3]", "B[2]", "B[1]", "B[0]",
  "CLK",
  },

    { "I" },  // FCLK_BUF

    // FIFO18KX2
    {  },

    // FIFO36K
    {  },

    { "I", "EN" },           // I_BUF
    { "I_P", "I_N", "EN" },  // I_BUF_DS

    { "D", "R", "E", "C" },  // I_DDR

    // I_DELAY
    { "I", "DLY_LOAD", "DLY_ADJ",
      "DLY_INCDEC", "CLK_IN" },

    { "I" }, // IO_BUF
    { "I" }, // IO_BUF_DS

    // I_SERDES
    { "D", "RST", "BITSLIP_ADJ", "EN", "CLK_IN",
      "PLL_LOCK", "PLL_CLK" },

    { "A" },                     // LUT1
    { "A[1]", "A[0]" },          // LUT2
    { "A[2]", "A[1]", "A[0]"},   // LUT3
    { "A[3]", "A[2]", "A[1]", "A[0]" },                 // LUT4
    { "A[4]", "A[3]", "A[2]", "A[1]", "A[0]" },         // LUT5
    { "A[5]", "A[4]", "A[3]", "A[2]", "A[1]", "A[0]" }, // LUT6

    { "I" },   // O_BUF
    { "I" },   // O_BUF_DS

    { "I", "T" },   // O_BUFT
    { "I", "T" },   // O_BUFT_DS

    // O_DDR
    { "D[1]", "D[0]", "R", "E", "C" },

    // O_DELAY
    { "I", "DLY_LOAD", "DLY_ADJ",
      "DLY_INCDEC", "CLK_IN" },

    // O_SERDES  // TMP. INCOMPLETE: D is a bus
    { "D", "RST", "DATA_VALID", "CLK_IN", "OE_IN",
      "CHANNEL_BOND_SYNC_IN", "PLL_LOCK", "PLL_CLK" },

    // O_SERDES_CLK
    { "CLK_EN", "PLL_LOCK", "PLL_CLK" },

    // PLL
    { "PLL_EN", "CLK_IN" },

    // TDP_RAM18KX2  // TMP. INCOMPLETE
    { "WEN_A1", "WEN_B1", "REN_A1", "REN_B1",
      "CLK_A1", "CLK_B1" },

    // TDP_RAM36K  // TMP. INCOMPLETE
    { "WEN_A", "WEN_B", "REN_A", "REN_B",
      "CLK_A", "CLK_B" },

    {}, // X_UNKNOWN
    {}, // Y_UPPER_GUARD
    {}
  }; // _id2inputs

  static vector<string> _id2clocks[] = {
    {}, // 0

    { "O" },  // BOOT_CLOCK
    {  },     // CARRY

    { "I", "O" }, // CLK_BUF

    { "C" },    // DFFNRE

    { "C" },    // DFFRE

    { "CLK" },  // DSP19X2

    { "CLK" },  // DSP38

    { "I", "O" },  // FCLK_BUF

    // FIFO18KX2
    { "WR_CLK1", "RD_CLK1",
      "WR_CLK2", "RD_CLK2" },

    // FIFO36K
    { "WR_CLK", "RD_CLK" },

    {  },  // I_BUF
    {  },  // I_BUF_DS

    { "C" },  // I_DDR

    { "CLK_IN" }, // I_DELAY

    {  }, // IO_BUF
    {  }, // IO_BUF_DS

    // I_SERDES
    { "CLK_IN", "CLK_OUT", "PLL_CLK" },

    {  },   // LUT1
    {  },   // LUT2
    {  },   // LUT3
    {  },   // LUT4
    {  },   // LUT5
    {  },   // LUT6

    {  },   // O_BUF
    {  },   // O_BUF_DS

    {  },   // O_BUFT
    {  },   // O_BUFT_DS

    {  },   // O_DDR

    {  },   // O_DELAY

    { "CLK_IN", "PLL_CLK" },    // O_SERDES

    { "PLL_CLK" },              // O_SERDES_CLK

    { "CLK_OUT", "FAST_CLK" },  // PLL

    // TDP_RAM18KX2
    { "CLK_A1", "CLK_B1",
      "CLK_A2", "CLK_B2" },
    
    { "CLK_A", "CLK_B" },       // TDP_RAM36K

    {}, // X_UNKNOWN
    {}, // Y_UPPER_GUARD
    {}
  }; // _id2clocks

static inline bool _vec_contains(const vector<string>& V, CStr name) noexcept {
  for (const string& s : V) {
    if (s == name)
      return true;
  }
  return false;
}

bool pr_cpin_is_output(Prim_t pt, CStr pinName) noexcept {
  uint i = pt;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or !pinName or !pinName[0] or i > Prim_MAX_ID)
    return false;
  const vector<string>& V = _id2outputs[i];
  return _vec_contains(V, pinName);
}

bool pr_cpin_is_clock(Prim_t pt, CStr pinName) noexcept {
  uint i = pt;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or !pinName or !pinName[0] or i > Prim_MAX_ID)
    return false;
  const vector<string>& V = _id2clocks[i];
  return _vec_contains(V, pinName);
}

CStr pr_enum2str(Prim_t enu) noexcept {
  static_assert(sizeof(_enumNames) / sizeof(_enumNames[0]) == Y_UPPER_GUARD + 1);
  uint i = enu;
  assert(i <= Prim_MAX_ID);
  return _enumNames[i];
}

uint pr_num_outputs(Prim_t pt) noexcept {
  uint i = pt;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or i > Prim_MAX_ID)
    return 0;
  const vector<string>& V = _id2outputs[i];
  return V.size();
}

uint pr_num_inputs(Prim_t pt) noexcept {
  uint i = pt;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or i > Prim_MAX_ID)
    return 0;
  const vector<string>& V = _id2inputs[i];
  return V.size();
}

uint pr_num_clocks(Prim_t pt) noexcept {
  uint i = pt;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or i > Prim_MAX_ID)
    return 0;
  const vector<string>& V = _id2clocks[i];
  return V.size();
}

void pr_get_inputs(Prim_t pt, vector<string>& INP) {
  INP.clear();
  uint i = pt;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or i > Prim_MAX_ID)
    return;
  INP = _id2inputs[i];
}

void pr_get_outputs(Prim_t pt, vector<string>& OUT) {
  OUT.clear();
  uint i = pt;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or i > Prim_MAX_ID)
    return;
  OUT = _id2outputs[i];
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

bool is_I_SERDES_output_term(const string& term) noexcept {
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

bool is_O_SERDES_output_term(const string& term) noexcept {
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

bool is_TDP_RAM36K_output_term(const string& term) noexcept {
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

bool is_TDP_RAM18KX_output_term(const string& term) noexcept {
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

bool is_PLL_output_term(const string& term) noexcept {
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
Prim_t pr_str2enum(CStr name) noexcept {
  if (!name or !name[0])
    return A_ZERO;
  if (starts_w_X(name))
    return X_UNKNOWN;
  if (starts_w_Y(name))
    return Y_UPPER_GUARD;
  if (starts_w_A(name))
    return A_ZERO;
  if (starts_w_CAR(name))
    return CARRY;

  for (uint i = 1; i < Prim_MAX_ID; i++) {
    if (::strcmp(_enumNames[i], name) == 0)
      return (Prim_t)i;
  }

  return A_ZERO;
}

// ==== DEBUG:

string pr_write_yaml(Prim_t pt) noexcept {
  using std::endl;
  CStr primName = pr_enum2str(pt);
  assert(primName);
  assert(primName[0]);
  if (!primName or !primName[0])
    return {};

  string fn = str::concat(primName, "_pln.yaml");

  flush_out(true);
  lprintf("  pr_write_yaml( %s )  to file:  %s\n", primName, fn.c_str());

  std::ofstream fos(fn);
  if (not fos.is_open()) {
    flush_out(true);
    lprintf("pr_write_yaml: could not open file for writing:  %s\n\n", fn.c_str());
    return {};
  }

  fos << "name: " << primName << endl;
  fos << "desc: ";
  if (pt >= LUT1 and pt <= LUT6)
    os_printf(fos, "%u-input lookup table (LUT)", pr_num_inputs(pt));
  fos << endl;

  fos << "category: core_fabric" << endl;
  fos << endl;

  fos << "ports:" << endl;

  vector<string> V;

  pr_get_inputs(pt, V);
  for (const string& port : V) {
    os_printf(fos, "   %s:\n", port.c_str());
    os_printf(fos, "     dir: input\n");
    os_printf(fos, "     desc:");
    if (pr_pin_is_clock(pt, port))
      fos << " Clock";
    fos << endl;
  }
  pr_get_outputs(pt, V);
  for (const string& port : V) {
    os_printf(fos, "   %s:\n", port.c_str());
    os_printf(fos, "     dir: output\n");
    os_printf(fos, "     desc:");
    if (pr_pin_is_clock(pt, port))
      fos << " Clock";
    fos << endl;
  }

  fos << endl;

  return fn;
}

}}

