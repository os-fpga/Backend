#include "file_io/pln_blif_file.h"
#include "util/nw/Nw.h"

namespace pln {

using namespace fio;
using namespace std;
using namespace prim;

BLIF_file::~BLIF_file() {}

void BLIF_file::reset(CStr nm, uint16_t tr) noexcept {
  MMapReader::reset(nm, tr);
  inputs_.clear();
  outputs_.clear();
  nodePool_.clear();
  topInputs_.clear();
  topOutputs_.clear();
  fabricNodes_.clear();
  latches_.clear();
  constantNodes_.clear();
  rd_ok_ = chk_ok_ = false;
  inputs_lnum_ = outputs_lnum_ = 0;
  err_lnum_ = err_lnum2_ = 0;
  err_msg_.clear();
  pg_.clear();
  pg2blif_.clear();
}

// "model "
static inline bool starts_w_model(CStr z, size_t len) noexcept {
  assert(z);
  if (len < 6)
    return false;
  return z[0] == 'm' and z[1] == 'o' and z[2] == 'd' and
         z[3] == 'e' and z[4] == 'l' and ::isspace(z[5]);
}

// "inputs "
static inline bool starts_w_inputs(CStr z, size_t len) noexcept {
  assert(z);
  if (len < 7)
    return false;
  return z[0] == 'i' and z[1] == 'n' and z[2] == 'p' and
         z[3] == 'u' and z[4] == 't' and z[5] == 's' and
         ::isspace(z[6]);
}

// "outputs "
static inline bool starts_w_outputs(CStr z, size_t len) noexcept {
  assert(z);
  if (len < 8)
    return false;
  return z[0] == 'o' and z[1] == 'u' and z[2] == 't' and
         z[3] == 'p' and z[4] == 'u' and z[5] == 't' and
         z[6] == 's' and ::isspace(z[7]);
}

// "names "
static inline bool starts_w_names(CStr z, size_t len) noexcept {
  assert(z);
  if (len < 6)
    return false;
  return z[0] == 'n' and z[1] == 'a' and z[2] == 'm' and
         z[3] == 'e' and z[4] == 's' and ::isspace(z[5]);
}

// "latch "
static inline bool starts_w_latch(CStr z, size_t len) noexcept {
  assert(z);
  if (len < 6)
    return false;
  return z[0] == 'l' and z[1] == 'a' and z[2] == 't' and
         z[3] == 'c' and z[4] == 'h' and ::isspace(z[5]);
}

// "subckt "
static inline bool starts_w_subckt(CStr z, size_t len) noexcept {
  assert(z);
  if (len < 7)
    return false;
  return z[0] == 's' and z[1] == 'u' and z[2] == 'b' and
         z[3] == 'c' and z[4] == 'k' and z[5] == 't' and
         ::isspace(z[6]);
}

// "gate "
static inline bool starts_w_gate(CStr z, size_t len) noexcept {
  assert(z);
  if (len < 5)
    return false;
  return z[0] == 'g' and z[1] == 'a' and z[2] == 't' and z[3] == 'e'
         and ::isspace(z[4]);
}

// "R="
static inline bool starts_w_R_eq(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 4) < 2)
    return false;
  return z[0] == 'R' and z[1] == '=';
}

// "P="
static inline bool starts_w_P_eq(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 4) < 2)
    return false;
  return z[0] == 'P' and z[1] == '=';
}

// "T="
static inline bool starts_w_T_eq(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 4) < 2)
    return false;
  return z[0] == 'T' and z[1] == '=';
}

// "I="
static inline bool starts_w_I_eq(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 4) < 2)
    return false;
  return z[0] == 'I' and z[1] == '=';
}

// "O="
static inline bool starts_w_O_eq(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 4) < 2)
    return false;
  return z[0] == 'O' and z[1] == '=';
}

// "Y="
static inline bool starts_w_Y_eq(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 4) < 2)
    return false;
  return z[0] == 'Y' and z[1] == '=';
}

// "Q="
static inline bool starts_w_Q_eq(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 4) < 2)
    return false;
  return z[0] == 'Q' and z[1] == '=';
}

// "COUT="
static inline bool starts_w_COUT_eq(CStr z) noexcept {
  assert(z);
  if (::strnlen(z, 8) < 6)
    return false;
  return z[0] == 'C' and z[1] == 'O' and z[2] == 'U' and
         z[3] == 'T' and z[4] == '=';
}

// sometimes in eblif the gate output is not the last token. correct it.
void BLIF_file::Node::place_output_at_back(vector<string>& dat) noexcept {
  size_t dsz = dat.size();
  if (dsz < 3) return;
  CStr cs = dat.back().c_str();
  if (starts_w_R_eq(cs)) {
    std::swap(dat[dsz - 2], dat[dsz - 1]);
  }
  if (starts_w_P_eq(cs)) {
    std::swap(dat[dsz - 2], dat[dsz - 1]);
  }
  if (hasPrimType()) {
    std::stable_partition(dat.begin() + 1, dat.end(), PinPatternIsInput(ptype_));
  }
}

bool BLIF_file::readBlif() noexcept {
  inputs_.clear();
  outputs_.clear();
  nodePool_.clear();
  topInputs_.clear();
  topOutputs_.clear();
  fabricNodes_.clear();
  latches_.clear();
  constantNodes_.clear();
  rd_ok_ = chk_ok_ = false;
  err_msg_.clear();
  trace_ = 0;

  {
    CStr ts = ::getenv("pln_blif_trace");
    if (ts) {
      int tr = ::atoi(ts);
      if (tr > 0) {
        tr = std::min(tr, 1000);
        trace_ = tr;
      }
    }
  }

  if (not regularFileExists(fnm_)) {
    err_msg_ = "file does not exist";
    return false;
  }
  if (not nonEmptyFileExists(fnm_)) {
    err_msg_ = "file is empty";
    return false;
  }

  bool ok = MMapReader::read();
  if (!ok or !buf_ or !sz_) {
    err_msg_ = "file reading failed";
    return false;
  }

  ok = isGoodForParsing();
  if (!ok) {
    err_msg_ = "file is not parsable";
    return false;
  }

  ok = ::memmem(buf_, sz_, ".model", 6);
  if (!ok) {
    err_msg_ = ".model not found";
    return false;
  }

  //ok = ::memmem(buf_, sz_, ".end", 4);
  //if (!ok) {
  //  err_msg_ = ".end not found";
  //  return false;
  //}

  ok = MMapReader::makeLines(true, true);
  if (!ok) {
    err_msg_ = "failed reading lines";
    return false;
  }

  size_t lsz = lines_.size();
  if (not hasLines() or lsz < 3) {
    err_msg_ = "failed reading lines";
    return false;
  }

  size_t num_escaped = escapeNL();
  if (trace_ >= 4) {
    lprintf(" ....\\ ...  num_escaped= %zu\n", num_escaped);
  }

  std::vector<string> V; // tmp buffer for tokenizing
  V.reserve(16);

  inputs_lnum_ = outputs_lnum_ = 0;
  err_lnum_ = err_lnum2_ = 0;
  for (size_t i = 1; i < lsz; i++) {
    CStr cs = lines_[i];
    if (!cs || !cs[0]) continue;
    cs = str::trimFront(cs);
    assert(cs);
    size_t len = ::strlen(cs);
    if (len < 3) continue;
    if (cs[0] != '.') continue;
    if (!inputs_lnum_ and starts_w_inputs(cs + 1, len - 1)) {
      inputs_lnum_ = i;
      continue;
    }
    if (!outputs_lnum_ and starts_w_outputs(cs + 1, len - 1)) {
      outputs_lnum_ = i;
      continue;
    }
    if (starts_w_model(cs + 1, len - 1)) {
      V.clear();
      Fio::split_spa(cs, V);
      if (V.size() > 1)
        topModel_ = V.back();
    }
    if (inputs_lnum_ and outputs_lnum_)
      break;
  }
  if (!inputs_lnum_ and !outputs_lnum_) {
    err_msg_ = ".inputs/.outputs not found";
    return false;
  }

  if (trace_ >= 3) {
    lprintf("\t ....  topModel_= %s\n", topModel_.c_str());
    lprintf("\t ....  inputs_lnum_= %zu  outputs_lnum_= %zu\n", inputs_lnum_, outputs_lnum_);
  }

  V.clear();
  if (inputs_lnum_) {
    Fio::split_spa(lines_[inputs_lnum_], V);
    if (V.size() > 1 and V.front() == ".inputs") {
      inputs_.assign(V.begin() + 1, V.end());
      std::sort(inputs_.begin(), inputs_.end());
    }
  }

  if (outputs_lnum_) {
    V.clear();
    Fio::split_spa(lines_[outputs_lnum_], V);
    if (V.size() > 1 and V.front() == ".outputs") {
      outputs_.assign(V.begin() + 1, V.end());
      std::sort(outputs_.begin(), outputs_.end());
    }
  }

  if (inputs_.empty() and outputs_.empty()) {
    err_msg_ = ".inputs/.outputs are empty";
    return false;
  }

  rd_ok_ = true;
  return true;
}

bool BLIF_file::checkBlif() noexcept {
  chk_ok_ = false;
  err_msg_.clear();
  auto& ls = lout();

  if (inputs_.empty() and outputs_.empty()) {
    err_msg_ = ".inputs/.outputs are empty";
    return false;
  }

  // 1. no duplicates in inputs_
  if (inputs_.size() > 1) {
    string* A = inputs_.data();
    size_t sz = inputs_.size();
    std::sort(A, A + sz);
    for (size_t i = 1; i < sz; i++) {
      if (A[i - 1] == A[i]) {
        err_msg_ = "duplicate input: ";
        err_msg_ += A[i];
        return false;
      }
    }
  }

  // 2. no duplicates in outputs_
  if (outputs_.size() > 1) {
    string* A = outputs_.data();
    size_t sz = outputs_.size();
    std::sort(A, A + sz);
    for (size_t i = 1; i < sz; i++) {
      if (A[i - 1] == A[i]) {
        err_msg_ = "duplicate output: ";
        err_msg_ += A[i];
        return false;
      }
    }
  }

  // 3. no overlap between inputs_ and outputs_
  if (inputs_.size() and outputs_.size()) {
    for (const string& inp : inputs_) {
      auto I = std::lower_bound(outputs_.begin(), outputs_.end(), inp);
      if (I == outputs_.end() or *I != inp) continue;
      err_msg_ = "inputs_/output_ overlap: ";
      err_msg_ += inp;
      return false;
    }
  }

  // 4. create and link nodes
  createNodes();

  if (trace_ >= 4) {
    printPrimitives(ls, false);
    if (trace_ >= 5) {
      printNodes(ls);
      flush_out(true);
    }
  }
  if (trace_ >= 7) {
    printCarryNodes(ls);
    flush_out(true);
  }

  bool link_ok = linkNodes();
  if (not link_ok) {
    string tmp = err_msg_;
    err_msg_ = "link failed at line ";
    err_msg_ += std::to_string(err_lnum_);
    err_msg_ += "  ";
    err_msg_ += tmp;
    return false;
  }

  if (trace_ >= 3) {
    flush_out(true);
    if (trace_ >= 5) {
      printNodes(ls);
      lputs("====");
    } else {
      lprintf("==== node summary after linking ====\n");
      uint nn = numNodes();
      ls << "==== nodes (" << nn << ") :" << endl;
      ls << "====    #topInputs_= " << topInputs_.size() << '\n';
      ls << "       #topOutputs_= " << topOutputs_.size() << '\n';
      ls << "      #fabricNodes_= " << fabricNodes_.size() << '\n';
      ls << "          #latches_= " << latches_.size() << '\n';
      ls << "    #constantNodes_= " << constantNodes_.size() << '\n';
      flush_out(true);
    }
    printPrimitives(ls, true);
    flush_out(true);
  }

  // 5. no undriven output ports
  for (const Node* port : topOutputs_) {
    assert(port->isRoot());
    assert(port->inDeg() == 0);
    if (port->outDeg() == 0) {
      err_msg_ = "undriven output port: ";
      err_msg_ += port->out_;
      return false;
    }
  }

  // 6. clock-data separation
  if (!topInputs_.empty() and !topOutputs_.empty()) {
    vector<const Node*> clocked;
    collectClockedNodes(clocked);
    uint clocked_sz = clocked.size();
    if (trace_ >= 3) {
      lprintf("(clock-data separation)  num_nodes= %u  num_clocked= %u\n",
              numNodes(), clocked_sz);
    }
    if (clocked_sz) {
      bool separ_ok = checkClockSepar(clocked);
      if (not separ_ok) {
        string tmp = err_msg_;
        err_msg_ = "clock-data separation issue at lines: ";
        err_msg_ += std::to_string(err_lnum_);
        err_msg_ += ", ";
        err_msg_ += std::to_string(err_lnum2_);
        err_msg_ += "  ";
        err_msg_ += tmp;
        flush_out(true);
        return false;
      }
      if (trace_ >= 4) {
        lputs("(clock-data separation)  checked OK.");
        if (trace_ >= 6) {
          flush_out(true);
          pg_.print(ls, " [[ final pinGraph ]]");
          flush_out(true);
          pg_.printEdges(ls, "[[ edges of pinGraph ]]");
          flush_out(true);
        }
      }
    }
    flush_out(true);
  }

  // -- write yaml file to check prim-DB:
  if (trace_ >= 7) {
    //string written = pr_write_yaml( DFFRE );
    //string written = pr_write_yaml( DSP19X2 );
    //string written = pr_write_yaml( DSP38 );
    //string written = pr_write_yaml( FIFO36K );
    string written = pr_write_yaml( FIFO18KX2 );
    flush_out(true);
    if (written.empty()) {
      lprintf("\t\t  FAIL: pr_write_yaml() FAILED\n\n");
    } else {
      lprintf("\t  written: %s\n\n", written.c_str());
      if (0) {
        lprintf("\n    ");
        for (int bb = 18; bb >= 0; bb--) {
          lprintf(" \"RD_DATA2[%i]\",", bb);
        }
        lputs();
        lputs();
      }
    }
  }

  chk_ok_ = true;
  return true;
}

uint BLIF_file::printInputs(std::ostream& os, CStr spacer) const noexcept {
  if (!spacer) spacer = "  ";
  uint n = numInputs();
  if (!n) {
    os << "(no inputs_)" << endl;
    return 0;
  }
  os << spacer << "--- inputs (" << n << ") :" << endl;
  for (uint i = 0; i < n; i++) os << spacer << inputs_[i];

  os << endl;
  os.flush();
  return n;
}

uint BLIF_file::printOutputs(std::ostream& os, CStr spacer) const noexcept {
  if (!spacer) spacer = "  ";
  uint n = numOutputs();
  if (!n) {
    os << "(no outputs_)" << endl;
    return 0;
  }
  os << spacer << "--- outputs (" << n << ") :" << endl;
  for (uint i = 0; i < n; i++) os << spacer << outputs_[i];

  os << endl;
  os.flush();
  return n;
}

uint BLIF_file::printNodes(std::ostream& os) const noexcept {
  uint n = numNodes();
  if (!n) {
    os << "(no nodes_)" << endl;
    return 0;
  }
  os << "--- nodes (" << n << ") :" << endl;
  os << "---    #topInputs_= " << topInputs_.size() << '\n';
  os << "      #topOutputs_= " << topOutputs_.size() << '\n';
  os << "     #fabricNodes_= " << fabricNodes_.size() << '\n';
  os << "         #latches_= " << latches_.size() << '\n';
  os << "   #constantNodes_= " << constantNodes_.size() << '\n';
  os << endl;

  if (trace_ < 4) {
    os.flush();
    return n;
  }

  os << "--- nodes (" << n << ") :" << endl;
  for (uint i = 1; i <= n; i++) {
    const Node& nd = nodePool_[i];
    CStr pts = nd.cPrimType();
    assert(pts);

    if (trace_ >= 5)
      lputs();

    os_printf(os,
              "  |%u| L:%u  %s  ptype:%s  inDeg=%u outDeg=%u  par=%u  out:%s  mog:%i/%i  ",
              i, nd.lnum_, nd.kw_.c_str(), pts,
              nd.inDeg(), nd.outDeg(), nd.parent_,
              nd.cOut(), nd.isMog(), nd.isVirtualMog());
    if (nd.data_.empty()) {
      os << endl;
    } else {
      const string* A = nd.data_.data();
      size_t sz = nd.data_.size();
      prnArray(os, A, sz, "  ");
    }

    if (trace_ >= 5) {
      const string* A = nd.inPins_.data();
      size_t sz = nd.inPins_.size();
      os_printf(os, "    ##inPins=%zu  ", sz);
      prnArray(os, A, sz, "  ");
      //
      A = nd.inSigs_.data();
      sz = nd.inSigs_.size();
      os_printf(os, "    ##inSigs=%zu  ", sz);
      prnArray(os, A, sz, "  ");
    }
  }

  os << endl;
  os.flush();
  return n;
}

uint BLIF_file::printPrimitives(std::ostream& os, bool instCounts) const noexcept {
  os << endl;
  os_printf(os, "======== primitive types (%u) :\n", Prim_MAX_ID - 1);
  char ncs_buf[80] = {};

  if (instCounts) {
    char ic_buf[80] = {};
    uint n_clock_inst = 0;
    std::array<uint, Prim_MAX_ID> IC = countTypes();
    for (uint t = 1; t < Prim_MAX_ID; t++) {
      Prim_t pt = Prim_t(t);
      CStr pn = pr_enum2str(pt);
      assert(pn and pn[0]);
      uint n_clocks = pr_num_clocks(pt);
      ncs_buf[0] = 0;
      ic_buf[0] = 0;
      if (IC[t]) {
        ::sprintf(ic_buf, "   inst-count= %u", IC[t]);
      }
      if (n_clocks and IC[t]) {
        ::sprintf(ncs_buf, "   #clock_pins= %u", n_clocks);
        n_clock_inst += IC[t];
      }
      os_printf(os, "    [%u]  %s   %s%s\n",
                t, pn, ic_buf, ncs_buf);
    }
    flush_out(true);
    lprintf("==== number of instances with clock pins = %u\n", n_clock_inst);
  }
  else {
    for (uint t = 1; t < Prim_MAX_ID; t++) {
      Prim_t pt = Prim_t(t);
      CStr pn = pr_enum2str(pt);
      assert(pn and pn[0]);
      uint n_outputs = pr_num_outputs(pt);
      uint n_clocks = pr_num_clocks(pt);
      ncs_buf[0] = 0;
      if (n_clocks) {
        ::sprintf(ncs_buf, "   #clock_pins= %u", n_clocks);
      }
      os_printf(os, "    [%u]  %s    i:%u  o:%u   %s   %s\n",
                t, pn, pr_num_inputs(pt), n_outputs, ncs_buf,
                pr_is_core_fabric(pt) ? "core" : "");
    }
  }

  os << endl;
  os.flush();
  return Prim_MAX_ID;
}

void BLIF_file::collectClockedNodes(vector<const Node*>& V) noexcept {
  V.clear();
  uint nn = numNodes();
  if (nn == 0)
    return;

  V.reserve(20);
  for (uint i = 1; i <= nn; i++) {
    const Node& nd = nodePool_[i];
    uint t = nd.ptype_;
    if (!t or t >= Prim_MAX_ID)
      continue;
    uint n_clocks = pr_num_clocks(nd.ptype_);
    if (n_clocks)
      V.push_back(&nd);
  }
}

std::array<uint, Prim_MAX_ID> BLIF_file::countTypes() const noexcept {
  std::array<uint, Prim_MAX_ID> A;
  for (uint t = 0; t < Prim_MAX_ID; t++)
    A[t] = 0;
  uint nn = numNodes();
  if (nn == 0)
    return A;

  for (uint i = 1; i <= nn; i++) {
    const Node& nd = nodePool_[i];
    uint t = nd.ptype_;
    if (t > 0 and t < Prim_MAX_ID)
      A[t]++;
  }

  return A;
}

uint BLIF_file::countCarryNodes() const noexcept {
  uint nn = numNodes();
  if (nn == 0)
    return 0;

  uint cnt = 0;
  for (uint i = 1; i <= nn; i++) {
    const Node& nd = nodePool_[i];
    if (nd.ptype_ == CARRY)
      cnt++;
  }

  return cnt;
}

uint BLIF_file::printCarryNodes(std::ostream& os) const noexcept {
  uint nn = numNodes();
  if (!nn) {
    os << "(no nodes_)" << endl;
    return 0;
  }

  uint cnt = countCarryNodes();
  if (!cnt) {
    os << "(no CarryNodes)" << endl;
    return 0;
  }

  os << "--- CarryNodes (" << cnt << ") :" << endl;

  for (uint i = 1; i <= nn; i++) {
    const Node& nd = nodePool_[i];
    if (nd.ptype_ != CARRY)
      continue;
    CStr pts = nd.cPrimType();
    assert(pts);
    os_printf(os,
              "  |%u| L:%u  %s  ptype:%s  inDeg=%u outDeg=%u  par=%u  out:%s  mog:%i/%i  ",
              i, nd.lnum_, nd.kw_.c_str(), pts,
              nd.inDeg(), nd.outDeg(), nd.parent_,
              nd.cOut(), nd.isMog(), nd.isVirtualMog());
    if (nd.data_.empty()) {
      os << endl;
    } else {
      const string* A = nd.data_.data();
      size_t sz = nd.data_.size();
      prnArray(os, A, sz, "  ");
      lputs();
    }
  }
  os << "^^^ CarryNodes (" << cnt << ") ^^^" << endl;

  flush_out(true);
  return cnt;
}

static bool s_is_MOG(const BLIF_file::Node& nd,
                     vector<string>& terms) noexcept {
  terms.clear();
  const vector<string>& data = nd.data_;
  uint dsz = data.size();
  assert(dsz < 1000000);
  if (dsz < 4)
    return false;

  if (nd.ptype_ == O_SERDES_CLK)
    return false;

  uint pt = nd.ptype_;
  if (pt >= LUT1 and pt <= LUT6)
    return false;

  if (nd.ptype_ == I_SERDES) {
    for (const string& t : data) {
      if (is_I_SERDES_output_term(t))
        terms.push_back(t);
    }
    return true;
  }
  if (nd.ptype_ == O_SERDES) {
    for (const string& t : data) {
      if (is_O_SERDES_output_term(t))
        terms.push_back(t);
    }
    return true;
  }
  if (nd.ptype_ == TDP_RAM36K) {
    for (const string& t : data) {
      if (is_TDP_RAM36K_output_term(t))
        terms.push_back(t);
    }
    return true;
  }
  if (nd.ptype_ == TDP_RAM18KX2) {
    for (const string& t : data) {
      if (is_TDP_RAM18KX_output_term(t))
        terms.push_back(t);
    }
    return true;
  }
  if (nd.ptype_ == PLL) {
    for (const string& t : data) {
      if (is_PLL_output_term(t))
        terms.push_back(t);
    }
    return true;
  }

  bool has_O = false, has_Y = false, has_Q = false,
       has_COUT = false;
  uint sum = 0;

  for (uint i = dsz - 1; i > 1; i--) {
    CStr cs = data[i].c_str();
    if (!has_O) {
      has_O = starts_w_O_eq(cs);
      if (has_O)
        terms.emplace_back(cs);
    }
    if (!has_Y) {
      has_Y = starts_w_Y_eq(cs);
      if (has_Y)
        terms.emplace_back(cs);
    }
    if (!has_Q) {
      has_Q = starts_w_Q_eq(cs);
      if (has_Q)
        terms.emplace_back(cs);
    }
    if (!has_COUT) {
      has_COUT = starts_w_COUT_eq(cs);
      if (has_COUT)
        terms.emplace_back(cs);
    }
  }

  sum = uint(has_O) + uint(has_Y) + uint(has_Q) + uint(has_COUT);
  if (sum < 2) {
    terms.clear();
    return false;
  }
  return true;
}

static void s_remove_MOG_terms(BLIF_file::Node& nd) noexcept {
  vector<string>& data = nd.data_;
  uint dsz = data.size();
  assert(dsz < 1000000);
  if (dsz < 4)
    return;

  if (nd.ptype_ == I_SERDES) {
    for (uint i = dsz - 1; i > 1; i--) {
      const string& t = data[i];
      if (is_I_SERDES_output_term(t)) {
        data.erase(data.begin() + i);
        continue;
      }
    }
    return;
  }
  if (nd.ptype_ == O_SERDES) {
    for (uint i = dsz - 1; i > 1; i--) {
      const string& t = data[i];
      if (is_O_SERDES_output_term(t)) {
        data.erase(data.begin() + i);
        continue;
      }
    }
    return;
  }
  if (nd.ptype_ == TDP_RAM36K) {
    for (uint i = dsz - 1; i > 1; i--) {
      const string& t = data[i];
      if (is_TDP_RAM36K_output_term(t)) {
        data.erase(data.begin() + i);
        continue;
      }
    }
    return;
  }
  if (nd.ptype_ == TDP_RAM18KX2) {
    for (uint i = dsz - 1; i > 1; i--) {
      const string& t = data[i];
      if (is_TDP_RAM18KX_output_term(t)) {
        data.erase(data.begin() + i);
        continue;
      }
    }
    return;
  }
  if (nd.ptype_ == PLL) {
    for (uint i = dsz - 1; i > 1; i--) {
      const string& t = data[i];
      if (is_PLL_output_term(t)) {
        data.erase(data.begin() + i);
        continue;
      }
    }
    return;
  }

  for (uint i = dsz - 1; i > 1; i--) {
    CStr cs = data[i].c_str();
    if (starts_w_O_eq(cs)) {
      data.erase(data.begin() + i);
      continue;
    }
    if (starts_w_Y_eq(cs)) {
      data.erase(data.begin() + i);
      continue;
    }
    if (starts_w_Q_eq(cs)) {
      data.erase(data.begin() + i);
      continue;
    }
    if (starts_w_COUT_eq(cs)) {
      data.erase(data.begin() + i);
      continue;
    }
  }
}

bool BLIF_file::createNodes() noexcept {
  nodePool_.clear();
  topInputs_.clear();
  topOutputs_.clear();
  fabricNodes_.clear();
  latches_.clear();
  constantNodes_.clear();
  if (!rd_ok_) return false;
  if (inputs_.empty() and outputs_.empty()) return false;

  size_t lsz = lines_.size();
  if (not hasLines() or lsz < 3) return false;
  if (lsz >= UINT_MAX) return false;

  nodePool_.reserve(2 * lsz + 8);
  nodePool_.emplace_back();  // put a fake node

  char buf[2048] = {};
  vector<string> V;
  V.reserve(16);
  inputs_lnum_ = outputs_lnum_ = 0;
  err_lnum_ = 0;
  for (uint L = 1; L < lsz; L++) {
    V.clear();
    CStr cs = lines_[L];
    if (!cs || !cs[0]) continue;
    cs = str::trimFront(cs);
    assert(cs);
    size_t len = ::strlen(cs);
    if (len < 3) continue;
    if (cs[0] != '.') continue;
    if (!inputs_lnum_ and starts_w_inputs(cs + 1, len - 1)) {
      inputs_lnum_ = L;
      continue;
    }
    if (!outputs_lnum_ and starts_w_outputs(cs + 1, len - 1)) {
      outputs_lnum_ = L;
      continue;
    }

    if (trace_ >= 6) lprintf("\t .....  line-%u  %s\n", L, cs);

    // -- parse .names
    if (starts_w_names(cs + 1, len - 1)) {
      Fio::split_spa(lines_[L], V);
      if (V.size() > 1 and V.front() == ".names") {
        nodePool_.emplace_back(".names", L);
        Node& nd = nodePool_.back();
        nd.data_.assign(V.begin() + 1, V.end());
      }
      continue;
    }

    // -- parse .latch
    if (starts_w_latch(cs + 1, len - 1)) {
      Fio::split_spa(lines_[L], V);
      if (V.size() > 1 and V.front() == ".latch") {
        nodePool_.emplace_back(".latch", L);
        Node& nd = nodePool_.back();
        nd.data_.assign(V.begin() + 1, V.end());
      }
      continue;
    }

    // -- parse .subckt
    if (starts_w_subckt(cs + 1, len - 1)) {
      Fio::split_spa(lines_[L], V);
      if (V.size() > 1 and V.front() == ".subckt") {
        nodePool_.emplace_back(".subckt", L);
        Node& nd = nodePool_.back();
        nd.data_.assign(V.begin() + 1, V.end());
        nd.ptype_ = pr_str2enum(nd.data_front());
        nd.place_output_at_back(nd.data_);
      }
      continue;
    }

    // -- parse .gate
    if (starts_w_gate(cs + 1, len - 1)) {
      Fio::split_spa(lines_[L], V);
      if (V.size() > 1 and V.front() == ".gate") {
        nodePool_.emplace_back(".gate", L);
        Node& nd = nodePool_.back();
        nd.data_.assign(V.begin() + 1, V.end());
        nd.ptype_ = pr_str2enum(nd.data_front());
        nd.place_output_at_back(nd.data_);
      }
      continue;
    }
  }

  if (!inputs_lnum_ and !outputs_lnum_) {
    err_msg_ = ".inputs/.outputs not found";
    return false;
  }
  uint nn = numNodes();
  if (not nn) {
    err_msg_ = "no nodes created";
    return false;
  }

  // -- replace MOGs by virtual SOGs
  num_MOGs_ = 0;
  for (uint i = 1; i <= nn; i++) {
    V.clear();
    Node& nd = nodePool_[i];
    if (nd.kw_ != ".subckt" and nd.kw_ != ".gate")
      continue;
    if (nd.data_.size() < 4)
      continue;
    if (nd.is_IBUF() or nd.is_OBUF())
      continue;
    assert(!nd.is_mog_);

    s_is_MOG(nd, V);
    bool is_mog = V.size() > 1;
    if (is_mog) {
      if (trace_ >= 5) {
        lprintf("\t\t .... MOG-type: %s\n", nd.cPrimType());
        lprintf("\t\t .... [terms] V.size()= %zu\n", V.size());
        logVec(V, "  [V-terms] ");
        lputs();
      }

      s_remove_MOG_terms(nd);
      uint startVirtual = nodePool_.size();
      for (uint j = 1; j < V.size(); j++) {
        nodePool_.emplace_back(nd);
        nodePool_.back().virtualOrigin_ = i;
        nodePool_.back().is_mog_ = true;
      }
      nd.data_.push_back(V.front());
      nd.is_mog_ = true;
      // give one output term to each virtual MOG:
      uint V_index = 1;
      for (uint k = startVirtual; k < nodePool_.size(); k++) {
        assert(V_index < V.size());
        Node& k_node = nodePool_[k];
        k_node.data_.push_back(V[V_index++]);
      }
      num_MOGs_++;
    }
  }

  nn = numNodes();
  assert(nn);

  fabricNodes_.reserve(nn);

  // -- finish and index nodes:
  for (uint i = 1; i <= nn; i++) {
    V.clear();
    Node& nd = nodePool_[i];
    if (nd.kw_ == ".names" or nd.kw_ == ".latch") {
      if (nd.data_.size() > 1) {
        if (nd.kw_ == ".names") {
          nd.out_ = nd.data_.back();
          fabricNodes_.push_back(&nd);
        } else if (nd.kw_ == ".latch") {
          nd.out_ = nd.data_.back();
          latches_.push_back(&nd);
        }
      } else if (nd.data_.size() == 1) {
        constantNodes_.push_back(&nd);
      }
      continue;
    }
    if (nd.kw_ == ".subckt" or nd.kw_ == ".gate") {
      if (nd.data_.size() > 1) {
        const string& last = nd.data_.back();
        size_t llen = last.length();
        if (!last.empty() and llen < 2047) {
          // replace '=' by 'space' and tokenize:
          ::strcpy(buf, last.c_str());
          for (uint k = 0; k < llen; k++) {
            if (buf[k] == '=') buf[k] = ' ';
          }
          Fio::split_spa(buf, V);
          if (not V.empty()) nd.out_ = V.back();
        }
        // fill inPins_, inSigs_:
        nd.inPins_.clear();
        nd.inSigs_.clear();
        const string* DA = nd.data_.data();
        size_t DA_sz = nd.data_.size();
        if (DA_sz > 2 and nd.kw_ == ".subckt") {
          // skip 1st (type) and last (output) terms in data_
          nd.inSigs_.reserve(DA_sz - 1);
          for (uint di = 1; di < DA_sz - 1; di++) {
            nd.inSigs_.push_back(DA[di]);
          }
        }
        if (not nd.inSigs_.empty()) {
          std::sort(nd.inSigs_.begin(), nd.inSigs_.end());
          if (0) {
            // if we get inPins_ from Prim-DB, sometimes they won't match
            // the signals. The blif may be generated with an old version
            // of Prim-DB. But the checker should be robust.
            pr_get_inputs(nd.ptype_, nd.inPins_);
            assert(nd.inPins_.size() == nd.inSigs_.size());
          }
          nd.inPins_.clear();
          nd.inPins_.reserve(nd.inSigs_.size());
          // strip the name before '=' in inSigs_, e.g. A[1]=sig_a --> sig_a
          for (string& ss : nd.inSigs_) {
            assert(!ss.empty());
            V.clear();
            ::strcpy(buf, ss.c_str());
            size_t len = ss.length();
            for (uint k = 0; k < len; k++) {
              if (buf[k] == '=') buf[k] = ' ';
            }
            Fio::split_spa(buf, V);
            if (V.size() > 1) {
              ss = V.back();
              nd.inPins_.push_back(V.front());
            }
          }
          assert(nd.inPins_.size() == nd.inSigs_.size());
          // std::sort(nd.inPins_.begin(), nd.inPins_.end());
        }
      }
      fabricNodes_.push_back(&nd);
      continue;
    }
  }

  std::sort(fabricNodes_.begin(), fabricNodes_.end(), Node::CmpOut{});

  V.clear();
  topInputs_.clear();
  topOutputs_.clear();

  // put nodes for toplevel inputs:
  if (inputs_lnum_ and inputs_.size()) {
    topInputs_.reserve(inputs_.size());
    for (const string& inp : inputs_) {
      nodePool_.emplace_back("_topInput_", inputs_lnum_);
      Node& nd = nodePool_.back();
      nd.data_.push_back(inp);
      nd.out_ = inp;
      nd.is_top_ = -1;
      topInputs_.push_back(&nd);
    }
    std::sort(topInputs_.begin(), topInputs_.end(), Node::CmpOut{});
  }
  // put nodes for toplevel outputs:
  if (outputs_lnum_ and outputs_.size()) {
    topOutputs_.reserve(outputs_.size());
    for (const string& out : outputs_) {
      nodePool_.emplace_back("_topOutput_", outputs_lnum_);
      Node& nd = nodePool_.back();
      nd.data_.push_back(out);
      nd.out_ = out;
      nd.is_top_ = 1;
      topOutputs_.push_back(&nd);
    }
    std::sort(topOutputs_.begin(), topOutputs_.end(), Node::CmpOut{});
  }

  // index nd.id_
  nn = numNodes();
  for (uint i = 0; i <= nn; i++) {
    Node& nd = nodePool_[i];
    nd.id_ = i;
  }

  return true;
}

// returns pin index in data_ or -1
int BLIF_file::Node::in_contact(const string& x) const noexcept {
  if (x.empty() or data_.empty()) return -1;
  size_t dsz = data_.size();
  if (dsz == 1) return x == data_.front();
  for (size_t i = 0; i < dsz - 1; i++) {
    assert(not data_[i].empty());
    CStr cs = data_[i].c_str();
    CStr p = ::strrchr(cs, '=');
    if (p)
      p++;
    else
      p = cs;
    if (x == p)  // compare the part afer '='
      return i;
  }
  return -1;
}

string BLIF_file::Node::firstInputPin() const noexcept {
  if (data_.size() < 2) return {};
  if (kw_ == ".names") {
    if (data_[0] != out_) return data_[0];
  }
  if (kw_ == ".latch") {
    if (data_[1] != out_) return data_[1];
  }
  if (data_.size() < 3) return {};
  if (kw_ == ".subckt" or kw_ == ".gate") {
    CStr dat1 = data_[1].c_str();
    if (starts_w_I_eq(dat1)) {
      return dat1 + 2;
    }
  }
  return {};
}

void BLIF_file::Node::allInputPins(vector<string>& V) const noexcept {
  V.clear();

  if (data_.size() < 2) return;
  if (kw_ == ".names") {
    if (data_[0] != out_) {
      V.push_back(data_[0]);
      return;
    }
  }
  if (kw_ == ".latch") {
    if (data_[1] != out_) {
      V.push_back(data_[1]);
      return;
    }
  }
  if (data_.size() < 3) return;
  if (kw_ == ".subckt" or kw_ == ".gate") {
    CStr dat1 = data_[1].c_str();
    if (starts_w_I_eq(dat1)) {
      V.push_back(dat1 + 2); // TMP. WRONG.
      return;
    }
  }
  return;
}

// void BLIF_file::Node:: allInputSignals(vector<string>& V) const noexcept;

BLIF_file::Node* BLIF_file::findOutputPort(const string& contact) noexcept {
  assert(not contact.empty());
  if (topOutputs_.empty()) return nullptr;

  // TMP linear
  for (Node* x : topOutputs_) {
    if (x->out_ == contact) return x;
  }
  return nullptr;
}

BLIF_file::Node* BLIF_file::findInputPort(const string& contact) noexcept {
  assert(not contact.empty());
  if (topInputs_.empty()) return nullptr;

  // TMP linear
  for (Node* x : topInputs_) {
    if (x->out_ == contact) return x;
  }
  return nullptr;
}

// searches inputs
BLIF_file::Node* BLIF_file::findFabricParent(uint of, const string& contact, int& pin) noexcept {
  pin = -1;
  assert(not contact.empty());
  if (fabricNodes_.empty()) return nullptr;

  // TMP linear
  for (Node* x : fabricNodes_) {
    if (x->id_ == of) continue;
    int pinIdx = x->in_contact(contact);
    if (pinIdx >= 0) {
      pin = pinIdx;
      return x;
    }
  }
  return nullptr;
}

void BLIF_file::getFabricParents(uint of, const string& contact, vector<upair>& PAR) noexcept {
  assert(not contact.empty());
  if (fabricNodes_.empty()) return;

  // TMP linear
  for (const Node* x : fabricNodes_) {
    if (x->id_ == of) continue;
    const Node& nx = *x;
    if (nx.inSigs_.empty())
      continue;
    size_t in_sz = nx.inSigs_.size();
    assert(nx.inPins_.size() == in_sz);
    for (uint k = 0; k < in_sz; k++) {
      if (nx.inSigs_[k] == contact) {
        PAR.emplace_back(nx.id_, k);
      }
    }
  }
}

// matches out_
BLIF_file::Node* BLIF_file::findFabricDriver(uint of, const string& contact) noexcept {
  assert(not contact.empty());
  if (fabricNodes_.empty()) return nullptr;

  // TMP linear
  for (Node* x : fabricNodes_) {
    if (x->id_ == of) continue;
    if (x->out_contact(contact)) return x;
  }
  return nullptr;
}

// finds TopInput or FabricDriver
BLIF_file::Node* BLIF_file::findDriverNode(uint of, const string& contact) noexcept {
  assert(not contact.empty());
  if (fabricNodes_.empty()) return nullptr;

  // TMP linear
  size_t sz = topInputs_.size();
  if (sz) {
    Node** A = topInputs_.data();
    for (size_t i = 0; i < sz; i++) {
      Node* np = A[i];
      assert(np);
      if (np->out_ == contact) {
        return np;
      }
    }
  }

  return findFabricDriver(of, contact);
}

void BLIF_file::link2(Node& from, Node& to) noexcept {
  assert(from.id_);
  assert(to.id_);
  assert(!to.parent_);
  to.parent_ = from.id_;
  from.chld_.push_back(to.id_);
}

bool BLIF_file::linkNodes() noexcept {
  if (!rd_ok_) return false;
  if (inputs_.empty() and outputs_.empty()) return false;
  if (fabricNodes_.empty()) return false;

  err_msg_.clear();

  // 1. start with fabricNodes_ which are rooted at output ports
  for (Node* fab_nd : fabricNodes_) {
    assert(fab_nd);
    Node& nd = *fab_nd;
    if (nd.out_.empty()) {
      err_msg_ = str::concat("incomplete fabric cell:  ", nd.kw_);
      if (!nd.data_.empty()) {
        err_msg_ += str::concat("  ", nd.data_.front());
      }
      err_lnum_ = nd.lnum_;
      return false;
    }
    Node* port = findOutputPort(nd.out_);
    if (port) {
      link2(*port, nd);
    }
  }

  // 1a. input ports should not contact fabric outputs
  if (not topInputs_.empty()) {
    for (Node* in_nd : topInputs_) {
      Node& nd = *in_nd;
      assert(!nd.out_.empty());
      Node* par = findFabricDriver(nd.id_, nd.out_);
      if (par) {
        err_msg_.reserve(224);
        err_msg_ = "input port contacts fabric driver:  port= ",
        err_msg_ += nd.out_;
        err_msg_ += "  fab-driver= ";
        err_msg_ += par->cPrimType();
        err_msg_ += " @ line ";
        err_msg_ += std::to_string(par->lnum_);
        err_lnum_ = nd.lnum_;
        return false;
      }
    }
  }

  if (::getenv("pln_check_output_port_loopback")) {
    // 1b. output ports should not contact fabric inputs
    if (not topOutputs_.empty()) {
      for (Node* out_nd : topOutputs_) {
        Node& nd = *out_nd;
        assert(!nd.out_.empty());
        int pinIndex = -1;
        Node* par = findFabricParent(nd.id_, nd.out_, pinIndex);
        if (par) {
          assert(pinIndex >= 0);
          assert(uint(pinIndex) < par->data_.size());
          const string& pinToken = par->data_[pinIndex];
          if (trace_ >= 20)
            lout() << pinToken << endl;
          if (not par->isLatch()) {
            // skipping latches for now
            err_msg_.reserve(224);
            err_msg_ = "output port contacts fabric input:  port= ",
            err_msg_ += nd.out_;
            err_msg_ += "  fab-input= ";
            err_msg_ += par->cPrimType();
            err_msg_ += " @ line ";
            err_msg_ += std::to_string(par->lnum_);
            err_lnum_ = nd.lnum_;
            return false;
          }
        }
      }
    }
  }

  // 2. every node except topOutputs_ should have parent_
  for (Node* fab_nd : fabricNodes_) {
    Node& nd = *fab_nd;
    assert(!nd.out_.empty());
    if (nd.parent_) continue;
    int pinIndex = -1;
    Node* par = findFabricParent(nd.id_, nd.out_, pinIndex);
    if (!par) {
      err_msg_ = "dangling cell output: ";
      err_msg_ += nd.out_;
      if (trace_ >= 2) {
        err_msg_ += "  nid# ";
        err_msg_ += std::to_string(nd.id_);
      }
      err_lnum_ = nd.lnum_;
      return false;
    }
    link2(*par, nd);
  }

  // 3. link input ports
  if (not topInputs_.empty()) {
    for (Node* in_nd : topInputs_) {
      Node& nd = *in_nd;
      assert(!nd.out_.empty());
      int pinIndex = -1;
      Node* par = findFabricParent(nd.id_, nd.out_, pinIndex);
      if (!par) {
        err_msg_ = "dangling input port: ";
        err_msg_ += nd.out_;
        if (trace_ >= 2) {
          err_msg_ += "  nid# ";
          err_msg_ += std::to_string(nd.id_);
        }
        err_lnum_ = nd.lnum_;
        return false;
      }
      link2(*par, nd);
    }
  }

  // 4. set hashes
  uint nn = numNodes();
  for (uint i = 1; i <= nn; i++) {
    Node& nd = nodePool_[i];
    assert(nd.id_ == i);
    nd.setCellHash();
    nd.setOutHash();
  }

  // 5. every node except topInputs_ should be driven
  string inp1;
  for (Node* fab_nd : fabricNodes_) {
    Node& nd = *fab_nd;
    assert(!nd.out_.empty());
    inp1 = nd.firstInputPin();
    if (inp1.empty())
      continue;
    if (inp1 == "$true" or inp1 == "$false" or inp1 == "$undef")
      continue;
    Node* in_port = findInputPort(inp1);
    if (in_port)
      continue;
    Node* drv_cell = findFabricDriver(nd.id_, inp1);
    if (!drv_cell) {
      err_msg_ = "undriven cell input: ";
      err_msg_ += inp1;
      if (trace_ >= 2) {
        err_msg_ += "  nid# ";
        err_msg_ += std::to_string(nd.id_);
      }
      err_lnum_ = nd.lnum_;
      return false;
    }
  }

  return true;
}

bool BLIF_file::checkClockSepar(vector<const Node*>& clocked) noexcept {
  auto& ls = lout();
  if (::getenv("pln_dont_check_clock_separation"))
    return true;
  if (clocked.empty())
    return true;

  bool ok = createPinGraph();
  if (!ok) {
    flush_out(true); err_puts();
    lprintf2("[Error] could not create Pin Graph\n");
    err_puts(); flush_out(true);
    return false;
  }
  if (pg_.size() < 3)
    return true;

  flush_out(true);

  if (trace_ >= 3) {
    lprintf("   (Pin Graph)  number of clock nodes = %u\n",
            pg_.countClockNodes());
  }

  if (not pg_.hasClockNodes()) {
    return true;
  }

  if (trace_ >= 4) {
    lputs("   (Pin Graph)  *** summary before coloring ***");
    pg_.printSum(ls, 0);
    lputs();
  }

  // -- paint clock nodes and their incoming edges Red
  for (NW::NI I(pg_); I.valid(); ++I) {
    NW::Node& nd = *I;
    if (nd.isClk()) {
      nd.paintRed();
      for (NW::IncoEI J(pg_, nd); J.valid(); ++J) {
        uint eid = J->id_;
        pg_.edgeRef(eid).paintRed();
      }
    }
  }

  if (trace_ >= 6) {
    lputs("   (Pin Graph)  *** summary after A ***");
    pg_.printSum(ls, 0);
    lputs();
  }

  // -- paint CLK_BUF and iport nodes contacted by red edges
  for (NW::cEI E(pg_); E.valid(); ++E) {
    const NW::Edge& ed = *E;
    if (ed.isRed()) {
      NW::Node& nd1 = pg_.nodeRef(ed.n1_);
      NW::Node& nd2 = pg_.nodeRef(ed.n2_);
      assert(pg2blif_.count(nd1.id_));
      assert(pg2blif_.count(nd2.id_));
      const Node& bnd1 = bnodeRef(map_pg2blif(nd1.id_));
      const Node& bnd2 = bnodeRef(map_pg2blif(nd2.id_));
      if (bnd1.is_CLK_BUF() or bnd1.isTopInput())
        nd1.paintRed();
      if (bnd2.is_CLK_BUF() or bnd2.isTopInput())
        nd2.paintRed();
    }
  }

  if (trace_ >= 6) {
    lputs("   (Pin Graph)  *** summary after B ***");
    pg_.printSum(ls, 0);
    lputs();
  }

  // -- for red input ports, paint their outgoing edges Red
  for (const Node* p : topInputs_) {
    const Node& port = *p;
    assert(port.nw_id_);
    NW::Node& nw_node = pg_.nodeRefCk(port.nw_id_);
    if (nw_node.isRed()) {
      for (NW::OutgEI J(pg_, nw_node); J.valid(); ++J) {
        uint eid = J->id_;
        pg_.edgeRef(eid).paintRed();
      }
    }
  }

  if (trace_ >= 4) {
    lputs("   (Pin Graph)  *** summary after coloring ***");
    pg_.printSum(ls, 0);
    lputs();
  }

  if (trace_ >= 4)
    writePinGraph("pin_graph_2.dot");

  bool color_ok = true;
  CStr viol_prefix = "    ===>>> clock color violation";

  // -- check that end-points of red edges are red
  for (NW::cEI E(pg_); E.valid(); ++E) {
    const NW::Edge& ed = *E;
    if (not ed.isRed())
      continue;
    const NW::Node& p1 = pg_.nodeRef(ed.n1_);
    const NW::Node& p2 = pg_.nodeRef(ed.n2_);
    if (!p1.isRed() or !p2.isRed()) {
      color_ok = false;
      if (pg2blif_.count(p1.id_) and pg2blif_.count(p2.id_)) {
        uint b1 = map_pg2blif(p1.id_);
        uint b2 = map_pg2blif(p2.id_);
        string nm1 = p1.getName();
        string nm2 = p2.getName();
        const Node& bnode1 = bnodeRef(b1);
        const Node& bnode2 = bnodeRef(b2);
        err_lnum_  = bnode1.lnum_;
        err_lnum2_ = bnode2.lnum_;
        flush_out(true);
        lprintf("%s: pin-graph nodes:  #%u:%s - #%u:%s\n",
                viol_prefix, p1.id_, nm1.c_str(), p2.id_, nm2.c_str());
        flush_out(true);
        lprintf("%s: blif lines:  %u - %u\n",
                viol_prefix, err_lnum_, err_lnum2_);
        char B[512] = {};
        ::sprintf(B, "      line %u :  %s  ", err_lnum_, bnode1.kw_.c_str());
        logVec(bnode1.data_, B);
        ::sprintf(B, "      line %u :  %s  ", err_lnum2_, bnode2.kw_.c_str());
        logVec(bnode2.data_, B);
        flush_out(true);
      }
      break;
    }
  }

  return color_ok;
}

struct qTup {
  uint inpNid_ = 0;
  uint cellId_ = 0;
  uint inputPin_ = 0;
  CStr outNet_ = nullptr;

  qTup() noexcept = default;

  qTup(uint inid, uint cid, uint ip, CStr z) noexcept
    : inpNid_(inid),
      cellId_(cid),
      inputPin_(ip),
      outNet_(z) {
    assert(inid);
    assert(cid);
    assert(z and z[0]);
  }
};

bool BLIF_file::createPinGraph() noexcept {
  pg_.clear();
  pg2blif_.clear();
  if (!rd_ok_) return false;
  if (topInputs_.empty() and topOutputs_.empty()) return false;
  if (fabricNodes_.empty()) return false;

  err_msg_.clear();
  uint64_t key = 0;
  uint nid = 0, kid = 0, eid = 0;
  vector<string> INP;
  vector<upair> PAR;
  char nm_buf[520] = {};

  pg2blif_.reserve(2 * nodePool_.size() + 1);

  // -- create pg-nodes for topInputs_
  for (Node* p : topInputs_) {
    Node& port = *p;
    assert(!port.out_.empty());
    nid = pg_.insK(port.id_);
    assert(nid);
    pg_.setNodeName(nid, port.out_);
    pg_.nodeRef(nid).markInp(true);
    port.nw_id_ = nid;
    pg2blif_.emplace(nid, port.id_);
  }

  // -- create pg-nodes for topOutputs_
  for (Node* p : topOutputs_) {
    Node& port = *p;
    assert(!port.out_.empty());
    nid = pg_.insK(port.id_);
    assert(nid);
    pg_.setNodeName(nid, port.out_);
    pg_.nodeRef(nid).markOut(true);
    port.nw_id_ = nid;
    pg2blif_.emplace(nid, port.id_);
  }

  vector<qTup> Q;
  Q.reserve(topInputs_.size());

  // -- link from input ports to fabric
  for (Node* p : topInputs_) {
    INP.clear();
    Node& port = *p;
    assert(!port.out_.empty());

    PAR.clear();
    getFabricParents(port.id_, port.out_, PAR);

    if (trace_ >= 5) {
      lputs();
      lprintf("    TopInput:  lnum_= %u   %s   PAR.size()= %zu\n",
              port.lnum_, port.out_.c_str(), PAR.size());
    }

    for (const upair& pa : PAR) {
      if (pa.first == port.id_)
        continue;
      const Node& par = bnodeRef(pa.first);
      uint pinIndex = pa.second;

      INP.clear();
      pr_get_inputs(par.ptype_, INP);

      const string& pinName = par.getInPin(pinIndex);
      bool is_clock = pr_pin_is_clock(par.ptype_, pinName);

      if (trace_ >= 5) {
        lprintf("      FabricParent par:  lnum_= %u  kw_= %s  ptype_= %s  pin[%u nm= %s%s]\n",
                par.lnum_, par.kw_.c_str(), par.cPrimType(),
                pinIndex, pinName.c_str(), is_clock ? " <C>" : "");
        logVec(INP, "      [par_inputs] ");
        logVec(par.inPins_, "         [inPins_] ");
        if (trace_ >= 7)
          logVec(par.inSigs_, "        [inSigs_] ");
        lputs();
      }

      assert(par.cell_hc_);
      key = hashComb(par.cell_hc_, pinIndex);
      assert(key);
      assert(not pg_.hasKey(key));
      kid = pg_.insK(key);
      assert(kid);
      assert(key != port.id_);      // bc port.id_ is a key for port
      assert(pg_.hasKey(port.id_)); //
      pg2blif_.emplace(kid, par.id_);

      pg_.nodeRef(kid).setCid(par.id_);
      ::snprintf(nm_buf, 510, "%s_%uL_%up%u",
                 par.hasPrimType() ? par.cPrimType() : "NP",
                 par.lnum_, par.id_, pinIndex);
      pg_.setNodeName(kid, nm_buf);
      pg_.nodeRef(kid).markClk(is_clock);

      eid = pg_.linK(port.id_, key);
      assert(eid);

      Q.emplace_back(kid, par.id_, pinIndex, par.out_.c_str());
    }
  }

  // -- link cell-edges and next level
  if (trace_ >= 5) {
    lprintf("|Q| .sz= %zu\n", Q.size());
    for (uint i = 0; i < Q.size(); i++) {
      const qTup& q = Q[i];
      lprintf("   |%u|  ( nid:%u  %u.%u  %s )\n",
              i, q.inpNid_, q.cellId_, q.inputPin_, q.outNet_);
    }
  }
  if (not Q.empty()) {
    for (uint i = 0; i < Q.size(); i++) {
      const qTup& q = Q[i];
      uint64_t qk = hashComb(q.cellId_, q.outNet_);
      assert(qk);
      kid = pg_.insK(qk);
      eid = pg_.linkNodes(q.inpNid_, kid, true);

      ::snprintf(nm_buf, 510, "nd%u_L%u_Q%u",
                kid, bnodeRef(q.cellId_).lnum_, q.cellId_);
      pg_.setNodeName(kid, nm_buf);
      pg2blif_.emplace(kid, q.cellId_);
    }
  }

  // -- create pg-nodes for sequential input pins
  vector<const Node*> clocked;
  collectClockedNodes(clocked);
  if (trace_ >= 4)
    lprintf("  createPinGraph:  clocked.size()= %zu\n", clocked.size());
  if (not clocked.empty()) {
    string inp1;
    for (const Node* cnp : clocked) {
      const Node& cn = *cnp;
      assert(cn.hasPrimType());
      if (cn.ptype_ == prim::CLK_BUF or cn.ptype_ == prim::FCLK_BUF)
        continue;
      // lputs9();
      for (uint i = 0; i < cn.inPins_.size(); i++) {
        const string& inp = cn.inPins_[i];
        assert(not inp.empty());
        if (not pr_pin_is_clock(cn.ptype_, inp))
          continue;
        assert(cn.cell_hc_);
        key = hashComb(cn.cell_hc_, inp);
        assert(key);
        assert(not pg_.hasKey(key));
        kid = pg_.insK(key);
        assert(kid);
        pg_.nodeRef(kid).markClk(true);

        ::snprintf(nm_buf, 510, "nd%u_L%u_cn",
                   kid, cn.lnum_);
        pg_.setNodeName(kid, nm_buf);
        pg2blif_.emplace(kid, cn.id_);

        const string& inet = cn.inSigs_[i];
        assert(not inet.empty());
        const Node* driver = findDriverNode(cn.id_, inet);
        if (!driver) {
          flush_out(true); err_puts();
          lprintf2("[Error] no driver for clock node #%u %s  line:%u\n",
                   cn.id_, cn.cPrimType(), cn.lnum_);
          err_puts(); flush_out(true);
          return false;
        }

        if (trace_ >= 5) {
          lputs9();
          lprintf(" from cn#%u %s ",
                cn.id_, cn.cPrimType() );
          lprintf( "  CLOCK_TRACE driver->  id_%u  %s  out_net %s\n",
                driver->id_, driver->cPrimType(), driver->out_.c_str() );
        }

        if (not driver->is_CLK_BUF() and not driver->isTopInput()) {
          flush_out(true); err_puts();
          lprintf2("[Error] bad driver for clock node #%u  must be CLK_BUF or iport\n",
                    cn.id_);
          err_puts(); flush_out(true);
          return false;
        }

        uint64_t qk = hashComb(driver->id_, driver->out_);
        assert(qk);
        uint pid = pg_.insK(qk);

        ::snprintf(nm_buf, 510, "nd%u_L%u_",
                   pid, driver->lnum_);
        if (driver->ptype_ == prim::CLK_BUF)
          ::strcat(nm_buf, "CBUF");
        pg_.setNodeName(pid, nm_buf);
        pg2blif_.emplace(pid, driver->id_);

        eid = pg_.linK(qk, key);
        assert(eid);

        if (driver->is_CLK_BUF()) {
          // step upward again

          inp1 = driver->firstInputPin();
          if (inp1.empty()) {
            lprintf("\n\t\t\t return false at %s : %u\n", __FILE__, __LINE__);
            return false;
          }
          if (inp1 == "$true" or inp1 == "$false" or inp1 == "$undef") {
            lprintf("\n\t\t\t return false at %s : %u\n", __FILE__, __LINE__);
            return false;
          }

          if (trace_ >= 5) {
            lprintf( "\t\t    CLOCK_TRACE_inp1  %s\n",
                  inp1.c_str() );
          }

          const Node& dn = *driver;
          Node* drv_drv = findDriverNode(dn.id_, inp1); // driver-of-driver

          if (!drv_drv) {
            lputs8();
            flush_out(true); err_puts();
            lprintf2("[Error] no driver for clock-buf node #%u %s  line:%u\n",
                     dn.id_, dn.cPrimType(), dn.lnum_);
            err_puts(); flush_out(true);
            return false;
          }

          if (trace_ >= 4) {
            lputs();
            lprintf("    CLOCK_TRACE drv_drv->  id_%u  %s  out_net %s  line:%u\n",
                    drv_drv->id_, drv_drv->cPrimType(),
                    drv_drv->out_.c_str(), drv_drv->lnum_);
          }

          if (not drv_drv->is_CLK_BUF() and not drv_drv->isTopInput()) {
            flush_out(true); err_puts();
            lprintf2("[Error] bad drv_drv for clock-buf node #%u line:%u  must be CLK_BUF or iport\n",
                      dn.id_, dn.lnum_);
            err_puts(); flush_out(true);
            return false;
          }

          qk = 0;

          assert(dn.cell_hc_);
          key = hashComb(dn.cell_hc_, inp);
          assert(key);
          kid = pg_.insK(key);
          assert(kid);
          pg_.nodeRef(kid).markClk(true);
          pg2blif_.emplace(kid, dn.id_);

          if (drv_drv->isTopInput()) {
            assert(drv_drv->nw_id_);
            uint nw_id = drv_drv->nw_id_;
            assert(pg_.hasNode(nw_id));
            assert(pg2blif_.count(nw_id));
            assert(map_pg2blif(nw_id) == drv_drv->id_);
            qk = pg_.nodeRefCk(nw_id).key_;
            assert(qk);
          }
          else {

            qk = hashComb(drv_drv->id_, drv_drv->out_);
            assert(qk);
            bool qk_inserted = not pg_.hasKey(qk);
            pid = pg_.insK(qk);

            if (qk_inserted) {
              ::snprintf(nm_buf, 510, "nd%u_L%u_",
                         pid, drv_drv->lnum_);
              if (drv_drv->is_CLK_BUF())
                ::strcat(nm_buf, "CBUF");
              pg_.setNodeName(pid, nm_buf);
            }
            pg2blif_.emplace(pid, drv_drv->id_);
          }

          assert(qk);
          eid = pg_.linK(qk, key);
          assert(eid);
          if (trace_ >= 11)
            lprintf("\t\t\t eid= %u\n", eid);
        }
      }
    }
  }

  pg_.setNwName("pin_graph");

  // writePinGraph("pin_graph_1.dot");

#ifndef NDEBUG
  // -- verify that all NW IDs are mapped to BNodes:
  for (NW::cNI I(pg_); I.valid(); ++I) {
    const NW::Node& nd = *I;
    if (not pg2blif_.count(nd.id_)) {
      lputs9("\n  !!!  ASSERT  !!!");
      lprintf("  !!!  nd.id_= %u\n", nd.id_);
      lprintf("  "); nd.print(lout());
      flush_out(true);
    }
    assert(pg2blif_.count(nd.id_));
  }
#endif

  return true;
}

bool BLIF_file::writePinGraph(CStr fn) const noexcept {
  auto& ls = lout();
  flush_out(true);
  pg_.print(ls, "\t *** pin_graph for clock-data separation ***");
  lprintf("\t  pg_. numN()= %u   numE()= %u\n", pg_.numN(), pg_.numE());
  lputs();
  pg_.printSum(ls, 0);
  lputs();
  pg_.printEdges(ls, "|edges|");
  flush_out(true);

  if (!fn or !fn[0])
    fn = "pinGraph.dot";

  bool wrDot_ok = pg_.writeDot(fn, nullptr, true, true);
  if (!wrDot_ok) {
    flush_out(true); err_puts();
    lprintf2("[Error] could not write pin_graph to file '%s'\n", fn);
    err_puts(); flush_out(true);
  }

  lprintf("(writePinGraph)  status:%i  file: %s\n",
          wrDot_ok, fn);

  flush_out(true);
  return wrDot_ok;
}

}

