#include "file_readers/pln_blif_file.h"
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
  err_lnum_ = 0;
  err_msg_.clear();
  pg_.clear();
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
  err_lnum_ = 0;
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
  if (trace_ >= 6) {
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
        err_msg_ = "clock-data separation issue at line ";
        err_msg_ += std::to_string(err_lnum_);
        err_msg_ += "  ";
        err_msg_ += tmp;
        flush_out(true);
        return false;
      }
      if (trace_ >= 4)
        lputs("(clock-data separation)  checked OK.");
    }
    flush_out(true);
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
      os_printf(os, "    [%u]  %s    i:%u  o:%u   %s\n",
                t, pn, pr_num_inputs(pt), n_outputs, ncs_buf);
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
        if (!last.empty() && llen < 2047) {
          // replace '=' by 'space' and tokenize:
          ::strcpy(buf, last.c_str());
          for (uint k = 0; k < llen; k++) {
            if (buf[k] == '=') buf[k] = ' ';
          }
          Fio::split_spa(buf, V);
          if (not V.empty()) nd.out_ = V.back();
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
BLIF_file::Node* BLIF_file::findFabricParent(uint of, const string& contact, int& pinIndex) noexcept {
  pinIndex = -1;
  assert(not contact.empty());
  if (fabricNodes_.empty()) return nullptr;

  // TMP linear
  for (Node* x : fabricNodes_) {
    if (x->id_ == of) continue;
    int pinIdx = x->in_contact(contact);
    if (pinIdx >= 0) {
      pinIndex = pinIdx;
      return x;
    }
  }
  return nullptr;
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
          // lputs9();
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
  if (not ::getenv("pln_check_clock_separation"))
    return true;
  if (clocked.empty())
    return true;

  bool ok = createPinGraph();
  if (!ok) {
    flush_out(true); err_puts();
    lprintf2("[Error] NOT createPinGraph()\n");
    err_puts(); flush_out(true);
  }

#if 0
  NW g;
  auto& ls = lout();
  char nm_buf[512] = {};

  for (const Node* np : clocked) {
    const Node& nd = *np;
    if (not pr_num_clocks(nd.ptype_))
      continue;
    if (nd.ptype_ == CLK_BUF)
      continue;
    if (nd.parent_ == 0)
      continue;
    lprintf("...... checking clock-separ for node #%u of type %s\n",
             nd.id_, nd.cPrimType());
    lprintf("\t\t  parent= %u\n", nd.parent_);

    g.clear();

    uint64_t rhash = nd.hashCode();
    uint g_rid = g.addNode(rhash);
    g.addRoot(g_rid);

    const Node& par = nodeRef(nd.parent_);

    uint g_pid = g.insK(par.hashCode());
    g.markSink(g_pid, true);

    g.linkNodes(g_rid, g_pid, false);

    ::snprintf(nm_buf, 510, "%s_%uL", nd.cPrimType(), nd.lnum_);
    g.setNodeName(g_rid, nm_buf);

    ::snprintf(nm_buf, 510, "%s_%uL", par.cPrimType(), par.lnum_);
    g.setNodeName(g_pid, nm_buf);

    uint bh_id = g.insK(111);
    g.markSink(bh_id, true);
    g.setNodeName(bh_id, "BH1");

    g.linkNodes(g_pid, bh_id, false);

    g.labelDepth();

    g.dump("\t *** g separ ***");
    lprintf("\t  g. numN()= %u   numE()= %u\n", g.numN(), g.numE());
    g.printSum(ls, 0);

    lputs("\n\t***** break; // TMP");
    break; // TMP
  }
#endif //////////0000000000

  return true;
}

bool BLIF_file::createPinGraph() noexcept {
  auto& ls = lout();
  pg_.clear();
  if (!rd_ok_) return false;
  if (topInputs_.empty() and topOutputs_.empty()) return false;
  if (fabricNodes_.empty()) return false;

  err_msg_.clear();
  uint64_t key = 0;
  uint nid = 0, kid = 0, eid = 0;
  vector<string> INP;

  // -- create pg-nodes for topInputs_
  for (const Node* p : topInputs_) {
    const Node& port = *p;
    assert(!port.out_.empty());
    nid = pg_.insK(port.id_);
    assert(nid);
    pg_.setNodeName(nid, port.out_);
  }

  // -- create pg-nodes for topOutputs_
  for (const Node* p : topOutputs_) {
    const Node& port = *p;
    assert(!port.out_.empty());
    nid = pg_.insK(port.id_);
    assert(nid);
    pg_.setNodeName(nid, port.out_);
  }

  // -- link from input ports to fabric
  lputs9();
  for (Node* p : topInputs_) {
    INP.clear();
    Node& port = *p;
    assert(!port.out_.empty());

    if (trace_ >= 5) {
      lprintf("    TopInput:  lnum_= %u   %s\n",
              port.lnum_, port.out_.c_str());
    }

    int pinIndex = -1;
    Node* par = findFabricParent(port.id_, port.out_, pinIndex);
    if (!par) {
      continue;
    }

    pr_get_inputs(par->ptype_, INP);

    if (trace_ >= 5) {
      lprintf("    FabricParent par:  lnum_= %u  kw_= %s  ptype_= %s  #inputs= %u\n",
              par->lnum_, par->kw_.c_str(), par->cPrimType(),
              pr_num_inputs(par->ptype_));
      logVec(INP, "    [par_inputs] ");
      lputs();
    }

    assert(par->cell_hc_);
    key = hashComb(par->cell_hc_, pinIndex);
    assert(key);
    kid = pg_.insK(key);
    assert(kid);

    eid = pg_.linK(port.id_, kid);
    assert(eid);
  }

  if (1) {

    pg_.dump("\t *** pg_ separ ***");
    lprintf("\t  pg_. numN()= %u   numE()= %u\n", pg_.numN(), pg_.numE());
    lputs();
    pg_.printSum(ls, 0);
    lputs();
    pg_.dumpEdges("|edges|");

  }

  return false;
}

}

