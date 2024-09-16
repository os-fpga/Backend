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
    "I_FAB",
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
    "O_FAB",
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

    // DSP19X2
    {
      // Z1[18:0]:
      "Z1[17]", "Z1[16]", "Z1[15]", "Z1[14]", "Z1[13]", "Z1[12]", "Z1[11]", "Z1[10]",
      "Z1[9]", "Z1[8]", "Z1[7]", "Z1[6]", "Z1[5]", "Z1[4]", "Z1[3]", "Z1[2]",
      "Z1[1]", "Z1[0]",

      // DLY_B1[8:0]:
      "DLY_B1[7]", "DLY_B1[6]", "DLY_B1[5]", "DLY_B1[4]", "DLY_B1[3]", "DLY_B1[2]",
      "DLY_B1[1]", "DLY_B1[0]"

      // Z2[18:0]:
      "Z2[17]", "Z2[16]", "Z2[15]", "Z2[14]", "Z2[13]", "Z2[12]", "Z2[11]", "Z2[10]",
      "Z2[9]", "Z2[8]", "Z2[7]", "Z2[6]", "Z2[5]", "Z2[4]", "Z2[3]", "Z2[2]",
      "Z2[1]", "Z2[0]",

      // DLY_B2[8:0]:
      "DLY_B2[7]", "DLY_B2[6]", "DLY_B2[5]", "DLY_B2[4]", "DLY_B2[3]", "DLY_B2[2]",
      "DLY_B2[1]", "DLY_B2[0]"
    },

    // DSP38
    {
      // Z[37:0]
      "Z[36]", "Z[35]", "Z[34]", "Z[33]", "Z[32]", "Z[31]", "Z[30]", "Z[29]", "Z[28]", "Z[27]", "Z[26]",
      "Z[25]", "Z[24]", "Z[23]", "Z[22]", "Z[21]", "Z[20]", "Z[19]", "Z[18]", "Z[17]", "Z[16]", "Z[15]",
      "Z[14]", "Z[13]", "Z[12]", "Z[11]", "Z[10]", "Z[9]", "Z[8]", "Z[7]",
      "Z[6]", "Z[5]", "Z[4]", "Z[3]", "Z[2]", "Z[1]", "Z[0]",

      // DLY_B[17:0]
      "DLY_B[16]", "DLY_B[15]", "DLY_B[14]", "DLY_B[13]", "DLY_B[12]", "DLY_B[11]", "DLY_B[10]",
      "DLY_B[9]", "DLY_B[8]", "DLY_B[7]", "DLY_B[6]", "DLY_B[5]", "DLY_B[4]", "DLY_B[3]",
      "DLY_B[2]", "DLY_B[1]", "DLY_B[0]"
    },

    { "O" },  // FCLK_BUF

  // FIFO18KX2
  {
    "EMPTY1", "FULL1",
    "ALMOST_EMPTY1", "ALMOST_FULL1",
    "PROG_EMPTY1", "PROG_FULL1",
    "OVERFLOW1", "UNDERFLOW1",

    "EMPTY2", "FULL2",
    "ALMOST_EMPTY2", "ALMOST_FULL2",
    "PROG_EMPTY2", "PROG_FULL2",
    "OVERFLOW2", "UNDERFLOW2",

    // RD_DATA1[DATA_READ_WIDTH1-1:0]:
    "RD_DATA1[18]", "RD_DATA1[17]", "RD_DATA1[16]", "RD_DATA1[15]", "RD_DATA1[14]",
    "RD_DATA1[13]", "RD_DATA1[12]", "RD_DATA1[11]", "RD_DATA1[10]", "RD_DATA1[9]",
    "RD_DATA1[8]", "RD_DATA1[7]", "RD_DATA1[6]", "RD_DATA1[5]", "RD_DATA1[4]",
    "RD_DATA1[3]", "RD_DATA1[2]", "RD_DATA1[1]", "RD_DATA1[0]",

    // RD_DATA2[DATA_READ_WIDTH2-1:0]:
    "RD_DATA2[18]", "RD_DATA2[17]", "RD_DATA2[16]", "RD_DATA2[15]", "RD_DATA2[14]",
    "RD_DATA2[13]", "RD_DATA2[12]", "RD_DATA2[11]", "RD_DATA2[10]", "RD_DATA2[9]",
    "RD_DATA2[8]", "RD_DATA2[7]", "RD_DATA2[6]", "RD_DATA2[5]", "RD_DATA2[4]",
    "RD_DATA2[3]", "RD_DATA2[2]", "RD_DATA2[1]", "RD_DATA2[0]"
  },

  // FIFO36K
  {
    // RD_DATA[DATA_WRITE_WIDTH-1:0]:
    "RD_DATA[35]", "RD_DATA[34]", "RD_DATA[33]", "RD_DATA[32]", "RD_DATA[31]", "RD_DATA[30]",
    "RD_DATA[29]", "RD_DATA[28]", "RD_DATA[27]", "RD_DATA[26]", "RD_DATA[25]", "RD_DATA[24]",
    "RD_DATA[23]", "RD_DATA[22]", "RD_DATA[21]", "RD_DATA[20]", "RD_DATA[19]", "RD_DATA[18]",
    "RD_DATA[17]", "RD_DATA[16]", "RD_DATA[15]", "RD_DATA[14]", "RD_DATA[13]", "RD_DATA[12]",
    "RD_DATA[11]", "RD_DATA[10]", "RD_DATA[9]", "RD_DATA[8]", "RD_DATA[7]", "RD_DATA[6]",
    "RD_DATA[5]", "RD_DATA[4]", "RD_DATA[3]", "RD_DATA[2]", "RD_DATA[1]", "RD_DATA[0]",

    "EMPTY", "FULL", "ALMOST_EMPTY", "ALMOST_FULL",
    "PROG_EMPTY", "PROG_FULL",
    "OVERFLOW", "UNDERFLOW"
  },

    { "O" },  // I_BUF
    { "O" },  // I_BUF_DS

    { "Q" },  // I_DDR

    { "O", "DLY_TAP_VALUE" },  // I_DELAY

    { "O" },  // I_FAB

    { "O" },  // IO_BUF
    { "O" },  // IO_BUF_DS

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

    { "O" },  // O_FAB

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
    // A1[9:0]:
    "A1[8]", "A1[7]", "A1[6]", "A1[5]", "A1[4]", "A1[3]", "A1[2]", "A1[1]", "A1[0]",

    // B1[8:0]:
    "B1[7]", "B1[6]", "B1[5]", "B1[4]", "B1[3]", "B1[2]", "B1[1]", "B1[0]",

    // A2[9:0]:
    "A2[8]", "A2[7]", "A2[6]", "A2[5]", "A2[4]", "A2[3]", "A2[2]", "A2[1]", "A2[0]",

    // B2[8:0]:
    "B2[7]", "B2[6]", "B2[5]", "B2[4]", "B2[3]", "B2[2]", "B2[1]", "B2[0]",

    "CLK", "RESET",

    // ACC_FIR[4:0]:
    "ACC_FIR[3]", "ACC_FIR[2]", "ACC_FIR[1]", "ACC_FIR[0]",

    // FEEDBACK[2:0]:
    "FEEDBACK[1]", "FEEDBACK[0]",

    "LOAD_ACC",
    "UNSIGNED_A", "UNSIGNED_B",
    "SATURATE",

    // SHIFT_RIGHT[4:0]:
    "SHIFT_RIGHT[3]", "SHIFT_RIGHT[2]", "SHIFT_RIGHT[1]", "SHIFT_RIGHT[0]",

    "ROUND", "SUBTRACT"
  },

  // DSP38
  {
    // A[19:0]:
    "A[18]", "A[17]", "A[16]", "A[15]", "A[14]", "A[13]", "A[12]", "A[11]", "A[10]",
    "A[9]", "A[8]", "A[7]", "A[6]", "A[5]", "A[4]", "A[3]", "A[2]", "A[1]", "A[0]",

    // B[17:0]:
    "B[16]", "B[15]", "B[14]", "B[13]", "B[12]", "B[11]", "B[10]", "B[9]", "B[8]",
    "B[7]", "B[6]", "B[5]", "B[4]", "B[3]", "B[2]", "B[1]", "B[0]",

    // ACC_FIR[5:0]:
    "ACC_FIR[4]", "ACC_FIR[3]", "ACC_FIR[2]", "ACC_FIR[1]", "ACC_FIR[0]",

    "CLK", "RESET",

    // FEEDBACK[2:0]:
    "FEEDBACK[1]", "FEEDBACK[0]",

    "LOAD_ACC",
    "SATURATE",

    // SHIFT_RIGHT[5:0]
    "SHIFT_RIGHT[4]", "SHIFT_RIGHT[3]", "SHIFT_RIGHT[2]", "SHIFT_RIGHT[1]", "SHIFT_RIGHT[0]",

    "ROUND",
    "SUBTRACT",
    "UNSIGNED_A",
    "UNSIGNED_B"
  },

    { "I" },  // FCLK_BUF

  // FIFO18KX2
  {
    "RESET1",
    "WR_CLK1", "RD_CLK1",
    "WR_EN1", "RD_EN1",

    "RESET2",
    "WR_CLK2", "RD_CLK2",
    "WR_EN2", "RD_EN2",

    // WR_DATA1[DATA_WRITE_WIDTH1-1:0]:
    "WR_DATA1[18]", "WR_DATA1[17]", "WR_DATA1[16]", "WR_DATA1[15]", "WR_DATA1[14]",
    "WR_DATA1[13]", "WR_DATA1[12]", "WR_DATA1[11]", "WR_DATA1[10]", "WR_DATA1[9]",
    "WR_DATA1[8]", "WR_DATA1[7]", "WR_DATA1[6]", "WR_DATA1[5]", "WR_DATA1[4]",
    "WR_DATA1[3]", "WR_DATA1[2]", "WR_DATA1[1]", "WR_DATA1[0]",

    // WR_DATA2[DATA_WRITE_WIDTH2-1:0]:
    "WR_DATA2[18]", "WR_DATA2[17]", "WR_DATA2[16]", "WR_DATA2[15]", "WR_DATA2[14]",
    "WR_DATA2[13]", "WR_DATA2[12]", "WR_DATA2[11]", "WR_DATA2[10]", "WR_DATA2[9]",
    "WR_DATA2[8]", "WR_DATA2[7]", "WR_DATA2[6]", "WR_DATA2[5]", "WR_DATA2[4]",
    "WR_DATA2[3]", "WR_DATA2[2]", "WR_DATA2[1]", "WR_DATA2[0]"
  },

  // FIFO36K
  { "RESET", "WR_CLK", "RD_CLK", "WR_EN", "RD_EN",

    // WR_DATA[DATA_WRITE_WIDTH-1:0]:
    "WR_DATA[35]", "WR_DATA[34]", "WR_DATA[33]", "WR_DATA[32]", "WR_DATA[31]", "WR_DATA[30]",
    "WR_DATA[29]", "WR_DATA[28]", "WR_DATA[27]", "WR_DATA[26]", "WR_DATA[25]", "WR_DATA[24]",
    "WR_DATA[23]", "WR_DATA[22]", "WR_DATA[21]", "WR_DATA[20]", "WR_DATA[19]", "WR_DATA[18]",
    "WR_DATA[17]", "WR_DATA[16]", "WR_DATA[15]", "WR_DATA[14]", "WR_DATA[13]", "WR_DATA[12]",
    "WR_DATA[11]", "WR_DATA[10]", "WR_DATA[9]", "WR_DATA[8]", "WR_DATA[7]", "WR_DATA[6]",
    "WR_DATA[5]", "WR_DATA[4]", "WR_DATA[3]", "WR_DATA[2]", "WR_DATA[1]", "WR_DATA[0]"
  },

    { "I", "EN" },           // I_BUF
    { "I_P", "I_N", "EN" },  // I_BUF_DS

    { "D", "R", "E", "C" },  // I_DDR

    // I_DELAY
    { "I", "DLY_LOAD", "DLY_ADJ",
      "DLY_INCDEC", "CLK_IN" },

    { "I" },  // I_FAB

    { "I" },  // IO_BUF
    { "I" },  // IO_BUF_DS

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

    { "I" },  // O_FAB

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

    {  }, // I_FAB
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
    {  },   // O_FAB

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

bool pr_is_core_fabric(Prim_t t) noexcept {
  if (pr_is_LUT(t) or pr_is_DSP(t) or pr_is_DFF(t))
    return true;
  if (t == CARRY or pr_is_FIFO(t) or pr_is_RAM(t) or
      t == I_FAB or t == O_FAB) {
    return true;
  }

  return false;
}

void pr_get_inputs(Prim_t pt, vector<string>& INP) noexcept {
  INP.clear();
  uint i = pt;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or i > Prim_MAX_ID)
    return;
  INP = _id2inputs[i];
}

void pr_get_outputs(Prim_t pt, vector<string>& OUT) noexcept {
  OUT.clear();
  uint i = pt;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or i > Prim_MAX_ID)
    return;
  OUT = _id2outputs[i];
}

std::string pr_first_input(Prim_t pt) noexcept {
  uint i = pt;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or i > Prim_MAX_ID)
    return {};

  const vector<string>& A = _id2inputs[i];
  if (A.empty())
    return {};
  return A.front();
}

std::string pr_first_output(Prim_t pt) noexcept {
  uint i = pt;
  assert(i <= Prim_MAX_ID);
  if (i == 0 or i > Prim_MAX_ID)
    return {};

  const vector<string>& A = _id2outputs[i];
  if (A.empty())
    return {};
  return A.front();
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

bool is_DSP38_output_term(const string& term) noexcept {
  assert(!term.empty());
  if (term.empty()) return false;

  static std::regex re_dsp38_out{
      R"(Z\[\d+\]=|DLY_B\[\d+\]=)" };

  std::cmatch m;
  bool b = false;

  try {
    b = std::regex_search(term.c_str(), m, re_dsp38_out);
  } catch (...) {
    assert(0);
    b = false;
  }

  //if (b)
  //  lprintf("__DSP38_output REGEX matched: %s\n", term.c_str());

  return b;
}

bool is_DSP19X2_output_term(const string& term) noexcept {
  assert(!term.empty());
  if (term.empty()) return false;

  static std::regex re_dsp19_out{
      R"(Z1\[\d+\]=|DLY_B1\[\d+\]=|Z2\[\d+\]=|DLY_B2\[\d+\]=)" };

  std::cmatch m;
  bool b = false;

  try {
    b = std::regex_search(term.c_str(), m, re_dsp19_out);
  } catch (...) {
    assert(0);
    b = false;
  }

  //if (b)
  //  lprintf("__DSP19X2_output REGEX matched: %s\n", term.c_str());

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
  if (pr_is_LUT(pt))
    os_printf(fos, "%u-input lookup table (LUT)", pr_num_inputs(pt));
  fos << endl;

  os_printf(fos, "category: %s\n", pr_is_core_fabric(pt) ? "core_fabric" : "");

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

