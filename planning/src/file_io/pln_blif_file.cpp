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
  fabricRealNodes_.clear();
  dangOutputs_.clear();
  latches_.clear();
  constantNodes_.clear();
  rd_ok_ = chk_ok_ = false;
  inputs_lnum_ = outputs_lnum_ = 0;
  err_lnum_ = err_lnum2_ = 0;
  err_msg_.clear();
  pg_.clear();
  pg2blif_.clear();

  for (uint t = 0; t < Prim_MAX_ID; t++)
    typeHistogram_[t] = 0;
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

// "param "
static inline bool starts_w_param(CStr z, size_t len) noexcept {
  assert(z);
  if (len < 6)
    return false;
  return z[0] == 'p' and z[1] == 'a' and z[2] == 'r' and
         z[3] == 'a' and z[4] == 'm' and ::isspace(z[5]);
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
void BLIF_file::BNode::place_output_at_back(vector<string>& dat) noexcept {
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

CStr BLIF_file::BNode::cPrimType() const noexcept {
  if (is_wire_)
    return "WIRE";
  if (is_const_)
    return "CONST";
  return ptype_ == prim::A_ZERO ? "{e}" : pr_enum2str(ptype_);
}

CStr BLIF_file::BNode::cPortName() const noexcept {
  if (isTopPort() and not data_.empty())
    return data_.front().c_str();
  return "{np}";
}

bool BLIF_file::BNode::isDanglingTerm(uint term) const noexcept {
  if (dangTerms_.empty())
    return false;
  const uint* A = dangTerms_.data();
  for (int64_t i = int64_t(dangTerms_.size()) - 1; i >= 0; i--) {
    if (A[i] == term)
      return true;
  }
  return false;
}

int BLIF_file::findTermByNet(const vector<string>& D, const string& net) noexcept {
  assert(not net.empty());
  assert(not D.empty());
  if (net.empty() or D.empty())
    return -1;

  int64_t sz = D.size();
  assert(sz < INT_MAX);

  //lputs();
  //logVec(D, " _bnode.D ");
  //lputs();

  for (int i = sz - 1; i >= 0; i--) {
    CStr term = D[i].c_str();
    CStr p = ::strchr(term, '=');
    if (p and net == p+1)
      return i;
  }
  return -1;
}

bool BLIF_file::readBlif() noexcept {
  inputs_.clear();
  outputs_.clear();
  nodePool_.clear();
  topInputs_.clear();
  topOutputs_.clear();
  fabricNodes_.clear();
  fabricRealNodes_.clear();
  latches_.clear();
  constantNodes_.clear();
  topModel_.clear();
  pinGraphFile_.clear();

  for (uint t = 0; t < prim::Prim_MAX_ID; t++)
    typeHistogram_[t] = 0;

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
  if (trace_ >= 6) {
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

bool BLIF_file::checkBlif(vector<string>& badInputs,
                          vector<string>& badOutputs) noexcept {
  chk_ok_ = false;
  err_msg_.clear();
  badInputs.clear();
  badOutputs.clear();

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

  typeHistogram_ = countTypes();

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

#ifndef NDEBUG
  // confirm that all fab node have output:
  for (const BNode* fab_nd : fabricNodes_) {
    assert(fab_nd);
    assert(not fab_nd->out_.empty());
  }
#endif

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
      ls << "====      #topInputs_= " << topInputs_.size() << '\n';
      ls << "         #topOutputs_= " << topOutputs_.size() << '\n';
      ls << "        #fabricNodes_= " << fabricNodes_.size() << '\n';
      ls << "    #fabricRealNodes_= " << fabricRealNodes_.size() << '\n';
      ls << "            #latches_= " << latches_.size() << '\n';
      ls << "      #constantNodes_= " << constantNodes_.size() << '\n';
      flush_out(true);
    }
    printPrimitives(ls, true);
    flush_out(true);
  }

  // 5. no undriven output ports
  for (const BNode* port : topOutputs_) {
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
    if (trace_ >= 6)
      pg_.setTrace(trace_);
    vector<BNode*> clocked;
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
  if (trace_ >= 8) {
    //string written = pr_write_yaml( FIFO36K );
    //string written = pr_write_yaml( FIFO18KX2 );
    //string written = pr_write_yaml( TDP_RAM36K );
    string written = pr_write_yaml( TDP_RAM18KX2 );
    flush_out(true);
    if (written.empty()) {
      lprintf("\t\t  FAIL: pr_write_yaml() FAILED\n\n");
    } else {
      lprintf("\t  written: %s\n\n", written.c_str());
      if (0) {
        lprintf("\n    ");
        for (int bb = 15; bb >= 0; bb--) {
          lprintf(" \"RDATA_A[%i]\",", bb);
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
  os << "---      #topInputs_= " << topInputs_.size() << '\n';
  os << "        #topOutputs_= " << topOutputs_.size() << '\n';
  os << "       #fabricNodes_= " << fabricNodes_.size() << '\n';
  os << "   #fabricRealNodes_= " << fabricRealNodes_.size() << '\n';
  os << "           #latches_= " << latches_.size() << '\n';
  os << "     #constantNodes_= " << constantNodes_.size()
     << "  cnt " << countConstNodes() << '\n';
  os << "         #wireNodes_= " << countWireNodes() << '\n';
  os << endl;

  if (trace_ < 4) {
    os.flush();
    return n;
  }

  os << "----- nodes (" << n << ") :" << endl;
  for (uint i = 1; i <= n; i++) {
    const BNode& nd = nodePool_[i];
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

    bool hasSigs = bool(nd.inPins_.size()) or bool(nd.inSigs_.size());
    if (trace_ >= 5 and hasSigs) {
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

void BLIF_file::collectClockedNodes(vector<BNode*>& V) noexcept {
  V.clear();
  uint nn = numNodes();
  if (nn == 0)
    return;

  V.reserve(20);
  for (uint i = 1; i <= nn; i++) {
    BNode& nd = nodePool_[i];
    uint t = nd.ptype_;
    if (!t or t >= Prim_MAX_ID)
      continue;
    if (nd.is_clockLess_DSP())
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
    const BNode& nd = nodePool_[i];
    uint t = nd.ptype_;
    if (t > 0 and t < Prim_MAX_ID)
      A[t]++;
  }

  return A;
}

uint BLIF_file::countLUTs() const noexcept {
  uint nn = numNodes();
  if (nn == 0)
    return 0;

  uint cnt = 0;
  for (uint i = 1; i <= nn; i++) {
    const BNode& nd = nodePool_[i];
    if (nd.is_LUT())
      cnt++;
  }

  return cnt;
}

uint BLIF_file::countFFs() const noexcept {
  uint nn = numNodes();
  if (nn == 0)
    return 0;

  uint cnt = 0;
  for (uint i = 1; i <= nn; i++) {
    const BNode& nd = nodePool_[i];
    if (nd.is_FF())
      cnt++;
  }

  return cnt;
}

uint BLIF_file::countCBUFs() const noexcept {
  uint nn = numNodes();
  if (nn == 0)
    return 0;

  uint cnt = 0;
  for (uint i = 1; i <= nn; i++) {
    const BNode& nd = nodePool_[i];
    if (nd.is_CLK_BUF())
      cnt++;
  }

  return cnt;
}

void BLIF_file::countBUFs(uint& nIBUF, uint& nOBUF, uint& nCBUF) const noexcept {
  nIBUF = nOBUF = nCBUF = 0;
  uint nn = numNodes();
  if (nn == 0)
    return;

  for (uint i = 1; i <= nn; i++) {
    const BNode& nd = nodePool_[i];
    if (nd.isTopPort())
      continue;
    if (nd.is_IBUF()) {
      nIBUF++;
      continue;
    }
    if (nd.is_OBUF()) {
      nOBUF++;
      continue;
    }
    if (nd.is_CLK_BUF()) {
      nCBUF++;
    }
  }
}

void BLIF_file::countMOGs(uint& nISERD,
                          uint& nDSP38, uint& nDSP19,
                          uint& nRAM36, uint& nRAM18,
                          uint& nCARRY
                          ) const noexcept {
  nISERD = nDSP38 = nDSP19 = nRAM36 = nRAM18 = nCARRY = 0;
  uint nn = numNodes();
  if (nn == 0)
    return;

  assert(not fabricRealNodes_.empty());

  for (const BNode* x : fabricRealNodes_) {
    const BNode& nd = *x;
    assert(not nd.isTopPort());
    assert(not nd.isVirtualMog());
    Prim_t pt = nd.ptype_;
    if (pt == I_SERDES) {
      nISERD++;
      continue;
    }
    if (pt == DSP38) {
      nDSP38++;
      continue;
    }
    if (pt == DSP19X2) {
      nDSP19++;
      continue;
    }
    if (pt == TDP_RAM36K) {
      nRAM36++;
      continue;
    }
    if (pt == TDP_RAM18KX2) {
      nRAM18++;
      continue;
    }
    if (pt == CARRY) {
      nCARRY++;
    }
  }
}

uint BLIF_file::countCarryNodes() const noexcept {
  uint nn = numNodes();
  if (nn == 0)
    return 0;

  uint cnt = 0;
  for (uint i = 1; i <= nn; i++) {
    const BNode& nd = nodePool_[i];
    if (nd.ptype_ == CARRY)
      cnt++;
  }

  return cnt;
}

uint BLIF_file::countWireNodes() const noexcept {
  uint nn = numNodes();
  if (nn == 0)
    return 0;

  uint cnt = 0;
  for (uint i = 1; i <= nn; i++) {
    const BNode& nd = nodePool_[i];
    if (nd.is_wire_)
      cnt++;
  }

  return cnt;
}

uint BLIF_file::countConstNodes() const noexcept {
  uint nn = numNodes();
  if (nn == 0)
    return 0;

  uint cnt = 0;
  for (uint i = 1; i <= nn; i++) {
    const BNode& nd = nodePool_[i];
    if (nd.is_const_)
      cnt++;
  }

  return cnt;
}

uint BLIF_file::numWarnings() const noexcept {
  assert(dangOutputs_.size() < size_t(INT_MAX));
  return dangOutputs_.size();
}

BLIF_file::NodeDescriptor BLIF_file::hasDanglingBit(uint lnum) const noexcept {
  assert(dangOutputs_.size() < size_t(INT_MAX));
  NodeDescriptor ret;
  if (!lnum or dangOutputs_.empty())
    return ret;

  // TMP linear
  for (const NodeDescriptor& desc : dangOutputs_) {
    if (desc.lnum_ == lnum) {
      ret = desc;
      break;
    }
  }
  return ret;
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
    const BNode& nd = nodePool_[i];
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

static bool s_is_MOG(const BLIF_file::BNode& nd,
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
  if (nd.ptype_ == DSP38) {
    for (const string& t : data) {
      if (is_DSP38_output_term(t))
        terms.push_back(t);
    }
    return true;
  }
  if (nd.ptype_ == DSP19X2) {
    for (const string& t : data) {
      if (is_DSP19X2_output_term(t))
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

static void s_remove_MOG_terms(BLIF_file::BNode& nd) noexcept {
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
  fabricRealNodes_.clear();
  dangOutputs_.clear();
  latches_.clear();
  constantNodes_.clear();
  if (!rd_ok_) return false;
  if (inputs_.empty() and outputs_.empty()) return false;

  size_t lsz = lines_.size();
  if (not hasLines() or lsz < 3) return false;
  if (lsz >= UINT_MAX) return false;

  vector<string> V;
  V.reserve(16);
  inputs_lnum_ = outputs_lnum_ = 0;
  err_lnum_ = 0;

  // estimate #MOG-bnodes to reserve memory
  {
    size_t MOG_bnodes = 0;

    for (uint L = 1; L < lsz; L++) {
      V.clear();
      CStr cs = lines_[L];
      if (!cs || !cs[0]) continue;
      cs = str::trimFront(cs);
      assert(cs);
      size_t len = ::strlen(cs);
      if (len < 3) continue;
      if (cs[0] != '.') continue;
      if (not starts_w_subckt(cs + 1, len - 1))
        continue;
      Fio::split_spa(lines_[L], V);
      //if (L == 48) {
      //  string delWire1151 = "$delete_wire$1151";
      //  lputs8();
      //  int dTerm = findTermByNet(V, delWire1151);
      //  lprintf("  dTerm= %i\n", dTerm);
      //}
      if (V.size() > 1 and V.front() == ".subckt") {
        Prim_t pt = pr_str2enum( V[1].c_str() );
        if (pr_is_MOG(pt)) {
          MOG_bnodes++;
          MOG_bnodes += pr_num_outputs(pt);
        }
      }
    }

    if (trace_ >= 5)
      lprintf("  estimated #MOG bnodes = %zu\n", MOG_bnodes);

    MOG_bnodes++;
    nodePool_.reserve(lsz * 4 + MOG_bnodes * 8);
    nodePool_.emplace_back();  // put a fake node
  }

  char buf[8192] = {};
  V.clear();
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

    if (trace_ >= 8) lprintf("\t .....  line-%u  %s\n", L, cs);

    // -- parse .names
    if (starts_w_names(cs + 1, len - 1)) {
      Fio::split_spa(lines_[L], V);
      if (V.size() > 1 and V.front() == ".names") {
        nodePool_.emplace_back(".names", L);
        BNode& nd = nodePool_.back();
        nd.data_.assign(V.begin() + 1, V.end());
        nd.out_ = nd.data_.back();
        if (V.size() == 2) {
          nd.is_const_ = true;
        } else if (V.size() == 3) {
          nd.is_wire_ = true;
          nd.inSigs_.push_back(V[1]);
          nd.inPins_.push_back("wire_in");
        }
      }
      continue;
    }

    // -- parse .latch
    if (starts_w_latch(cs + 1, len - 1)) {
      Fio::split_spa(lines_[L], V);
      if (V.size() > 1 and V.front() == ".latch") {
        nodePool_.emplace_back(".latch", L);
        BNode& nd = nodePool_.back();
        nd.data_.assign(V.begin() + 1, V.end());
      }
      continue;
    }

    // -- parse .subckt
    if (starts_w_subckt(cs + 1, len - 1)) {
      Fio::split_spa(lines_[L], V);
      if (V.size() > 1 and V.front() == ".subckt") {
        nodePool_.emplace_back(".subckt", L);
        BNode& nd = nodePool_.back();
        nd.data_.assign(V.begin() + 1, V.end());
        nd.ptype_ = pr_str2enum(nd.data_front());
        nd.place_output_at_back(nd.data_);
        if (pr_is_DSP(nd.ptype_)) {
          vector<string> TK;
          // search for .param DSP_MODE "MULTIPLY"
          // to flag clock-less DSP
          uint m = 0;
          for (uint L2 = L + 1; L2 < lines_.size(); L2++, m++) {
            if (m > 50)
              break;
            CStr ps = lines_[L2];
            if (!ps || !ps[0]) continue;
            ps = str::trimFront(ps);
            assert(ps);
            size_t ps_len = ::strlen(ps);
            if (ps_len < 3) continue;
            if (ps[0] != '.') continue;
            if (starts_w_subckt(ps + 1, ps_len - 1))
              break;
            if (starts_w_names(ps + 1, ps_len - 1))
              break;
            if (starts_w_param(ps + 1, ps_len - 1)) {
              Fio::split_spa(ps, TK);
              if (TK.size() == 3 and TK[1] == "DSP_MODE" and TK[2].length() > 3) {
                CStr tk = TK[2].c_str();
                if (tk[0] == '"')
                  tk++;
                if (::memcmp(tk, "MULTIPLY", 8) == 0) {
                  nd.isClockLess_ = true;
                  if (trace_ >= 5)
                    lprintf("\t  marked clock-less DSP at line %u\n", L);
                  break;
                }
              }
            }
          }
        }
      }
      continue;
    }

    // -- parse .gate
    if (starts_w_gate(cs + 1, len - 1)) {
      Fio::split_spa(lines_[L], V);
      if (V.size() > 1 and V.front() == ".gate") {
        nodePool_.emplace_back(".gate", L);
        BNode& nd = nodePool_.back();
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
    BNode& nd = nodePool_[i];
    if (nd.kw_ != ".subckt" and nd.kw_ != ".gate")
      continue;
    if (nd.data_.size() < 3)
      continue;
    if (nd.is_IBUF() or nd.is_OBUF() or nd.is_LUT())
      continue;
    assert(!nd.is_mog_);

    s_is_MOG(nd, V);
    bool is_mog = V.size() > 1;
    if (is_mog) {
      if (trace_ >= 5) {
        lprintf("\t\t ____ MOG-type: %s\n", nd.cPrimType());
        lprintf("\t\t .... [terms] V.size()= %zu\n", V.size());
        logVec(V, "  [V-terms] ");
        lputs();
      }

      vector<string> dataCopy { nd.data_ };
      s_remove_MOG_terms(nd);
      uint startVirtual = nodePool_.size();
      for (uint j = 1; j < V.size(); j++) {
        nodePool_.emplace_back(nd);
        nodePool_.back().virtualOrigin_ = i;
        nodePool_.back().is_mog_ = true;
      }
      nd.realData_.swap(dataCopy);
      nd.data_.push_back(V.front());
      nd.is_mog_ = true;
      // give one output term to each virtual MOG:
      uint V_index = 1;
      for (uint k = startVirtual; k < nodePool_.size(); k++) {
        assert(V_index < V.size());
        BNode& k_node = nodePool_[k];
        k_node.data_.push_back(V[V_index++]);
      }
      num_MOGs_++;
    }
  }

  nn = numNodes();
  assert(nn);

  fabricNodes_.reserve(nn + 4);

  // -- finish and index nodes:
  for (uint i = 1; i <= nn; i++) {
    V.clear();
    BNode& nd = nodePool_[i];
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
        if (!last.empty() and llen < 4095) {
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

  std::sort(fabricNodes_.begin(), fabricNodes_.end(), BNode::CmpOut{});

  fabricRealNodes_.clear();
  fabricRealNodes_.reserve(fabricNodes_.size());
  for (BNode* x : fabricNodes_) {
    if (not x->isVirtualMog()) {
      fabricRealNodes_.push_back(x);
    }
  }

  V.clear();
  topInputs_.clear();
  topOutputs_.clear();

  // put nodes for toplevel inputs:
  if (inputs_lnum_ and inputs_.size()) {
    topInputs_.reserve(inputs_.size());
    for (const string& inp : inputs_) {
      nodePool_.emplace_back("_topInput_", inputs_lnum_);
      BNode& nd = nodePool_.back();
      nd.data_.push_back(inp);
      nd.out_ = inp;
      nd.is_top_ = -1;
      topInputs_.push_back(&nd);
    }
    std::sort(topInputs_.begin(), topInputs_.end(), BNode::CmpOut{});
  }
  // put nodes for toplevel outputs:
  if (outputs_lnum_ and outputs_.size()) {
    topOutputs_.reserve(outputs_.size());
    for (const string& out : outputs_) {
      nodePool_.emplace_back("_topOutput_", outputs_lnum_);
      BNode& nd = nodePool_.back();
      nd.data_.push_back(out);
      nd.out_ = out;
      nd.is_top_ = 1;
      topOutputs_.push_back(&nd);
    }
    std::sort(topOutputs_.begin(), topOutputs_.end(), BNode::CmpOut{});
  }

  // index nd.id_
  nn = numNodes();
  for (uint i = 0; i <= nn; i++) {
    BNode& nd = nodePool_[i];
    nd.id_ = i;
  }

  // possibly adjust "clockness" flag of some RAM18KX2 clock inputs.
  // RAM18KX2 instances may have unused halves with clock inputs
  // (CLK_A1/2, CLK_B1/2) connected to $false.
  // This seems to be allowed. EDA-3235.
  uint RAM18KX2_cnt_total = 0, RAM18KX2_cnt_disabled = 0;
  for (BNode* x : fabricRealNodes_) {
    BNode& nd = *x;
    assert(not nd.isTopPort());
    assert(not nd.isVirtualMog());
    if (nd.ptype_ != prim::TDP_RAM18KX2)
      continue;
    RAM18KX2_cnt_total++;
    if (trace_ >= 5) {
      lprintf("\nRAM18KX2 instance #%u L=%u  dat_sz= %zu\n",
        nd.id_, nd.lnum_, nd.realData_.size());
      if (trace_ >= 6)
        logVec(nd.realData_, " dat ");
    }
    for (const string& term : nd.realData_) {
      if (term == "CLK_A1=$false") {
        nd.disabledClocks_.emplace_back("CLK_A1");
        continue;
      }
      if (term == "CLK_B1=$false") {
        nd.disabledClocks_.emplace_back("CLK_B1");
        continue;
      }
      if (term == "CLK_A2=$false") {
        nd.disabledClocks_.emplace_back("CLK_A2");
        continue;
      }
      if (term == "CLK_B2=$false") {
        nd.disabledClocks_.emplace_back("CLK_B2");
      }
    }
    if (not nd.disabledClocks_.empty())
      RAM18KX2_cnt_disabled++;
  }

  if (trace_ >= 4) {
    lprintf("DONE BLIF_file::createNodes()");
    lprintf("   total #RAM18KX2 instances = %u", RAM18KX2_cnt_total);
    if (RAM18KX2_cnt_total) {
      lprintf("   #RAM18KX2 instances w disabled clocks = %u",
              RAM18KX2_cnt_disabled);
    }
    flush_out(true);
  }

  return true;
}

// returns pin index in data_ or -1
int BLIF_file::BNode::in_contact(const string& x) const noexcept {
  if (x.empty() or data_.empty()) return -1;
  size_t dsz = data_.size();
  if (dsz == 1)
    return x == data_.front();
  size_t ssz =inSigs_.size();
  if (!ssz)
    return -1;
  assert(ssz < dsz);
  for (size_t i = 0; i < ssz; i++) {
    const string& sig = inSigs_[i];
    assert(not sig.empty());
    if (x == sig)
      return i;
  }

  return -1;
}

string BLIF_file::BNode::firstInputNet() const noexcept {
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

void BLIF_file::BNode::allInputPins(vector<string>& V) const noexcept {
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

BLIF_file::BNode* BLIF_file::findOutputPort(const string& contact) noexcept {
  assert(not contact.empty());
  if (topOutputs_.empty()) return nullptr;

  // TMP linear
  for (BNode* x : topOutputs_) {
    if (x->out_ == contact) return x;
  }
  return nullptr;
}

BLIF_file::BNode* BLIF_file::findInputPort(const string& contact) noexcept {
  assert(not contact.empty());
  if (topInputs_.empty()) return nullptr;

  // TMP linear
  for (BNode* x : topInputs_) {
    if (x->out_ == contact) return x;
  }
  return nullptr;
}

// searches inputs
BLIF_file::BNode* BLIF_file::findFabricParent(uint of, const string& contact, int& pin) noexcept {
  pin = -1;
  assert(not contact.empty());
  if (fabricNodes_.empty()) return nullptr;

  assert(not fabricRealNodes_.empty());

  // TMP linear
  for (BNode* x : fabricRealNodes_) {
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

  assert(not fabricRealNodes_.empty());

  // TMP linear
  for (const BNode* x : fabricRealNodes_) {
    if (x->id_ == of) continue;
    const BNode& nx = *x;
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
BLIF_file::BNode* BLIF_file::findFabricDriver(uint of, const string& contact) noexcept {
  assert(not contact.empty());
  if (fabricNodes_.empty()) return nullptr;

  // TMP linear
  for (BNode* x : fabricNodes_) {
    if (x->id_ == of) continue;
    if (x->out_contact(contact)) return x;
  }
  return nullptr;
}

// finds TopInput or FabricDriver
BLIF_file::BNode* BLIF_file::findDriverNode(uint of, const string& contact) noexcept {
  assert(not contact.empty());
  if (fabricNodes_.empty()) return nullptr;

  // TMP linear
  size_t sz = topInputs_.size();
  if (sz) {
    BNode** A = topInputs_.data();
    for (size_t i = 0; i < sz; i++) {
      BNode* np = A[i];
      assert(np);
      if (np->out_ == contact) {
        return np;
      }
    }
  }

  return findFabricDriver(of, contact);
}

void BLIF_file::link2(BNode& from, BNode& to) noexcept {
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
  for (BNode* fab_nd : fabricNodes_) {
    assert(fab_nd);
    BNode& nd = *fab_nd;
    if (nd.out_.empty()) {
      err_msg_ = str::concat("incomplete fabric cell:  ", nd.kw_);
      if (!nd.data_.empty()) {
        err_msg_ += str::concat("  ", nd.data_.front());
      }
      err_lnum_ = nd.lnum_;
      return false;
    }
    BNode* port = findOutputPort(nd.out_);
    if (port) {
      link2(*port, nd);
    }
  }

  // 1a. input ports should not contact fabric outputs
  if (not topInputs_.empty()) {
    for (BNode* in_nd : topInputs_) {
      BNode& nd = *in_nd;
      assert(!nd.out_.empty());
      BNode* par = findFabricDriver(nd.id_, nd.out_);
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
      for (BNode* out_nd : topOutputs_) {
        BNode& nd = *out_nd;
        assert(!nd.out_.empty());
        int pinIndex = -1;
        BNode* par = findFabricParent(nd.id_, nd.out_, pinIndex);
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
  for (BNode* fab_nd : fabricNodes_) {
    BNode& nd = *fab_nd;
    assert(!nd.out_.empty());
    if (nd.parent_)
      continue;
    // if (nd.lnum_ == 48)
    //   lputs7();
    int pinIndex = -1;
    BNode* par = findFabricParent(nd.id_, nd.out_, pinIndex);
    if (!par) {
      if (nd.is_RAM() or nd.is_DSP() or nd.is_CARRY()) {
        const string& net = nd.out_;
        uint rid = nd.realId(*this);
        BNode& realNd = bnodeRef(rid);
        const vector<string>& realData = realNd.realData_;
        assert(not realData.empty());
        int dataTerm = findTermByNet(realData, net);
        assert(dataTerm >= 0);
        assert(dataTerm < int64_t(realData.size()));
        if (dataTerm < 0)
          continue;
        // RAM or DSP or CARRY output bits may be unused
        if (trace_ >= 4) {
          lprintf("skipping dangling cell output issue for %s at line %u\n",
                  realNd.cPrimType(), realNd.lnum_);
          lprintf("  dangling net: %s  term# %i %s\n",
                  net.c_str(), dataTerm, realData[dataTerm].c_str());
          lputs();
        }
        realNd.dangTerms_.push_back(dataTerm);
        dangOutputs_.emplace_back(realNd.lnum_, realNd.id_, dataTerm);
        continue;
      }
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
    for (BNode* in_nd : topInputs_) {
      BNode& nd = *in_nd;
      assert(!nd.out_.empty());
      int pinIndex = -1;
      BNode* par = findFabricParent(nd.id_, nd.out_, pinIndex);
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
    BNode& nd = nodePool_[i];
    assert(nd.id_ == i);
    nd.setCellHash();
    nd.setOutHash();
  }

  // 5. every node except topInputs_ should be driven
  string inp1;
  for (BNode* fab_nd : fabricNodes_) {
    BNode& nd = *fab_nd;
    assert(!nd.out_.empty());
    inp1 = nd.firstInputNet();
    if (inp1.empty())
      continue;
    if (inp1 == "$true" or inp1 == "$false" or inp1 == "$undef")
      continue;
    BNode* in_port = findInputPort(inp1);
    if (in_port)
      continue;
    BNode* drv_cell = findFabricDriver(nd.id_, inp1);
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

bool BLIF_file::checkClockSepar(vector<BNode*>& clocked) noexcept {
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
      const BNode& bnd1 = bnodeRef(map_pg2blif(nd1.id_));
      const BNode& bnd2 = bnodeRef(map_pg2blif(nd2.id_));
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
  for (const BNode* p : topInputs_) {
    const BNode& port = *p;
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

  bool color_ok = true;
  CStr viol_prefix = "    ===!!! clock-data separation error";

  // -- check that end-points of red edges are red
  for (NW::cEI E(pg_); E.valid(); ++E) {
    const NW::Edge& ed = *E;
    if (not ed.isRed())
      continue;
    NW::Node& p1 = pg_.nodeRef(ed.n1_);
    NW::Node& p2 = pg_.nodeRef(ed.n2_);
    if (!p1.isRed() or !p2.isRed()) {
      color_ok = false;
      p1.markViol(true);
      p2.markViol(true);
      if (pg2blif_.count(p1.id_) and pg2blif_.count(p2.id_)) {
        uint b1 = map_pg2blif(p1.id_);
        uint b2 = map_pg2blif(p2.id_);
        string nm1 = p1.getName();
        string nm2 = p2.getName();
        const BNode& bnode1 = bnodeRef(b1);
        const BNode& bnode2 = bnodeRef(b2);
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

  if (color_ok)
    pg_.setNwName("pin_graph_OK");
  else
    pg_.setNwName("pin_graph_VIOL");

  if (not ::getenv("pln_blif_dont_write_pinGraph")) {
    pinGraphFile_ = writePinGraph("_PinGraph.dot");
  }

  if (!color_ok) {
    flush_out(true); err_puts();
    lprintf2("[Error] clock-data separation error\n");
    err_puts(); flush_out(true);
    return false;
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

static CStr s_possible_cdrivers = "{iport, CLK_BUF, I_SERDES}";

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
  for (BNode* p : topInputs_) {
    BNode& port = *p;
    assert(!port.out_.empty());
    nid = pg_.insK(port.id_);
    assert(nid);
    pg_.nodeRef(nid).markInp(true);
    assert(not pg_.nodeRef(nid).isNamed());
    pg_.setNodeName3(nid, port.id_, port.lnum_, port.out_.c_str());
    port.nw_id_ = nid;
    pg2blif_.emplace(nid, port.id_);
  }

  // -- create pg-nodes for topOutputs_
  for (BNode* p : topOutputs_) {
    BNode& port = *p;
    assert(!port.out_.empty());
    nid = pg_.insK(port.id_);
    assert(nid);
    pg_.nodeRef(nid).markOut(true);
    assert(not pg_.nodeRef(nid).isNamed());
    pg_.setNodeName3(nid, port.id_, port.lnum_, port.out_.c_str());
    port.nw_id_ = nid;
    pg2blif_.emplace(nid, port.id_);
  }

  uint max_nid1 = pg_.getMaxNid();
  uint64_t max_key1 = pg_.getMaxKey();
  if (trace_ >= 4) {
    if (trace_ >= 5) lputs();
    lprintf(" (pg_ after inserting top-port nodes)");
    lprintf("  max_nid= %u  max_key= %zu\n", max_nid1, max_key1);
    if (trace_ >= 5) {
      pg_.printNodes(lout(), " pg_ after inserting top-port nodes ", false);
    }
  }
  max_key1++; // max_key1 is used to offset hashCantor

  vector<qTup> Q;
  Q.reserve(topInputs_.size());

  // -- link from input ports to fabric
  for (BNode* p : topInputs_) {
    INP.clear();
    BNode& port = *p;
    assert(!port.out_.empty());

    PAR.clear();
    getFabricParents(port.id_, port.out_, PAR);

    if (trace_ >= 5) {
      lputs();
      lprintf("  TopInput:  id_= %u  lnum_= %u   %s   PAR.size()= %zu\n",
              port.id_, port.lnum_, port.out_.c_str(), PAR.size());
      if (trace_ >= 6)
        lprintf("      %s\n", port.cPortName());
    }

    // if (port.id_ == 24)
    //  lputs1();

    for (const upair& pa : PAR) {
      if (pa.first == port.id_)
        continue;
      const BNode& par = bnodeRef(pa.first);
      uint pinIndex = pa.second;

      INP.clear();
      pr_get_inputs(par.ptype_, INP);

      const string& pinName = par.getInPin(pinIndex);
      bool is_clock = pr_pin_is_clock(par.ptype_, pinName)
                        and not par.isDisabledClock(pinName, *this);

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

      uint par_realId = par.realId(*this);
      key = hashCantor(par_realId, pinIndex+1) + max_key1;
      assert(key);
      kid = pg_.findNode(key);
      if (kid) {
        // virtual MOG effect
        if (trace_ >= 7) {
          lprintf(" (virtual MOG effect)  node exists:  key=%zu  nid=%u\n",
                  key, kid);
        }
        continue;
      }
      kid = pg_.insK(key);
      assert(kid);
      assert(key != port.id_);      // bc port.id_ is a key for port
      assert(pg_.hasKey(port.id_)); //
      pg2blif_.emplace(kid, par_realId);

      pg_.nodeRef(kid).setCid(par_realId);
      ::snprintf(nm_buf, 510, "%s_%uL_%up%u",
                 par.hasPrimType() ? par.cPrimType() : "NP",
                 par.lnum_, par_realId, pinIndex);

      assert(not pg_.nodeRef(kid).inp_flag_);
      pg_.setNodeName4(kid, par_realId, par.lnum_, pinIndex+1, par.cPrimType());
      pg_.nodeRef(kid).markClk(is_clock);

      if (trace_ >= 7)
        lprintf(" ALT nodeName  %s  -->  %s\n", pg_.cnodeName(kid), nm_buf);

      eid = pg_.linK(port.id_, key);
      assert(eid);

      if (trace_ >= 8)
        pg_.printEdge(eid);

      Q.emplace_back(kid, par_realId, pinIndex, par.out_.c_str());
    }
  }

  // if (0) {
  //  writePinGraph("111_G_afterLinkTopInp.dot", true, false);
  // }

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
      BNode& bnode = bnodeRef(q.cellId_);
      assert(not bnode.isTopPort());

      uint64_t qk = bnode.cellHash0();
      assert(qk);
      kid = pg_.insK(qk);
      eid = pg_.linkNodes(q.inpNid_, kid, true);

      ::snprintf(nm_buf, 510, "nd%u_L%u_Q%u",
                kid, bnode.lnum_, q.cellId_);

      assert(not pg_.nodeRef(kid).inp_flag_);
      pg_.setNodeName3(kid, bnode.id_, bnode.lnum_, bnode.out_.c_str());
      bnode.nw_id_ = kid;
      pg2blif_.emplace(kid, bnode.id_);

      if (trace_ >= 6)
        lprintf(" ALT nodeName  %s  -->  %s\n", pg_.cnodeName(kid), nm_buf);
    }
  }

  // -- create pg-nodes for sequential input pins
  vector<BNode*> clocked;
  collectClockedNodes(clocked);
  if (trace_ >= 4)
    lprintf("  createPinGraph:  clocked.size()= %zu\n", clocked.size());
  if (not clocked.empty()) {
    string inp1;
    for (BNode* cnp : clocked) {
      BNode& cn = *cnp;
      assert(cn.hasPrimType());
      if (cn.ptype_ == prim::CLK_BUF or cn.ptype_ == prim::FCLK_BUF)
        continue;

      for (uint i = 0; i < cn.inPins_.size(); i++) {
        const string& inp = cn.inPins_[i];
        assert(not inp.empty());
        if (not pr_pin_is_clock(cn.ptype_, inp))
          continue;
        if (cn.isDisabledClock(inp, *this)) {
          if (trace_ >= 6)
            lprintf("    createPinGraph: skipping disabled clock-input-pin %s\n",
                    inp.c_str());
          continue;
        }

        uint cn_realId = cn.realId(*this);
        key = hashCantor(cn_realId, i + 1) + max_key1;
        assert(key);
        // if (key == 110)
        //  lputs3();
        kid = pg_.findNode(key);
        if (kid) {
          lprintf("\t\t ___ found   nid %u '%s'   for key %zu",
                  kid, pg_.cnodeName(kid), key);
        }
        else {
          kid = pg_.insK(key);
          assert(kid);
        }
        pg_.nodeRef(kid).markClk(true);

        ::snprintf(nm_buf, 510, "nd%u_L%u_cn",
                   kid, cn.lnum_);

        assert(not pg_.nodeRef(kid).inp_flag_);
        pg_.setNodeName4(kid, cn_realId, cn.lnum_, i+1, cn.cPrimType());
        pg2blif_.emplace(kid, cn_realId);

        const string& inet = cn.inSigs_[i];
        assert(not inet.empty());
        const BNode* driver = findDriverNode(cn_realId, inet);
        if (!driver) {
          flush_out(true); err_puts();
          lprintf2("[Error] no driver for clock node #%u %s pin:%s  line:%u\n",
                   cn_realId, cn.cPrimType(), inp.c_str(), cn.lnum_);
          err_puts(); flush_out(true);
          err_lnum_ = cn.lnum_;
          err_lnum2_ = cn.lnum_;
          return false;
        }
        uint driver_realId = driver->realId(*this);

        if (trace_ >= 6) {
          lputs();
          lprintf(" from cn#%u %s ",
                cn_realId, cn.cPrimType() );
          lprintf( "  CLOCK_TRACE driver->  id_%u  %s  out_net %s\n",
                driver_realId, driver->cPrimType(), driver->out_.c_str() );
        }

        if (not driver->canDriveClockNode()) {
          flush_out(true); err_puts();
          lprintf2("[Error] bad driver (%s) for clock node #%u  must be %s\n",
                    driver->cPrimType(), cn_realId, s_possible_cdrivers);
          err_puts(); flush_out(true);
          return false;
        }

        uint64_t opin_key = driver->cellHash0();
        assert(opin_key);
        uint opin_nid = pg_.findNode(opin_key);
        if (opin_nid) {
          assert(pg_.nodeRef(opin_nid).isNamed());
          assert(map_pg2blif(opin_nid) == driver_realId);
        }
        else {
          opin_nid = pg_.insK(opin_key);

          ::snprintf(nm_buf, 510, "nd%u_L%u_",
                     opin_nid, driver->lnum_);
          if (driver->ptype_ == prim::CLK_BUF) {
            ::strcat(nm_buf, "CBUF_");
            string outPin = pr_first_output(driver->ptype_);
            assert(not outPin.empty());
            ::strcat(nm_buf, outPin.c_str());
          }

          assert(not pg_.nodeRef(opin_nid).inp_flag_);
          pg_.setNodeName3(opin_nid, driver_realId,
                           driver->lnum_, driver->out_.c_str());
          pg2blif_.emplace(opin_nid, driver_realId);
        }

        assert(opin_nid and kid);
        eid = pg_.linkNodes(opin_nid, kid, false);
        assert(eid);

        if (driver->is_CLK_BUF()) {
          // step upward again

          const BNode& dn = *driver;

          inp1 = dn.firstInputNet();
          if (trace_ >= 5) {
            lprintf("\t\t    CLOCK_TRACE_inp1  %s\n", inp1.c_str());
          }
          if (inp1.empty()) {
            lprintf("\n\t\t\t return false at %s : %u\n", __FILE__, __LINE__);
            return false;
          }
          if (inp1 == "$true" or inp1 == "$false" or inp1 == "$undef") {
            lprintf("\n\t\t\t return false at %s : %u\n", __FILE__, __LINE__);
            return false;
          }

          uint64_t ipin_key = hashCantor(dn.realId(*this), 1) + max_key1;
          assert(ipin_key);
          uint ipin_nid = pg_.findNode(ipin_key);
          if (ipin_nid) {
            assert(pg_.nodeRef(ipin_nid).isNamed());
            assert(map_pg2blif(ipin_nid) == dn.id_);
          }
          else {
            ipin_nid = pg_.insK(ipin_key);

            ::snprintf(nm_buf, 510, "nd%u_L%u_",
                       ipin_nid, dn.lnum_);
            if (dn.ptype_ == prim::CLK_BUF) {
              ::strcat(nm_buf, "iCBUF_");
              string inpPin = pr_first_input(dn.ptype_);
              assert(not inpPin.empty());
              ::strcat(nm_buf, inpPin.c_str());
            }
            assert(not pg_.nodeRef(ipin_nid).inp_flag_);
            pg_.setNodeName3(ipin_nid, dn.id_, dn.lnum_, dn.out_.c_str());
            pg2blif_.emplace(ipin_nid, dn.id_);
          }

          // connect input and output of 'dn' (cell-arc):
          assert(ipin_nid and opin_nid);
          assert(pg_.hasNode(ipin_nid));
          assert(pg_.hasNode(opin_nid));
          eid = pg_.linkNodes(ipin_nid, opin_nid, true);
          pg_.edgeRef(eid).paintRed();

          // driver-of-driver
          BNode* drv_drv = findDriverNode(dn.id_, inp1);

          if (!drv_drv) {
            lputs();
            flush_out(true); err_puts();
            lprintf2("[Error] no driver for clock-buf node #%u %s  line:%u\n",
                     dn.id_, dn.cPrimType(), dn.lnum_);
            err_lnum_ = dn.lnum_;
            err_puts(); flush_out(true);
            err_lnum_ = dn.lnum_;
            err_lnum2_ = dn.lnum_;
            return false;
          }

          if (trace_ >= 5) {
            lputs();
            lprintf("    CLOCK_TRACE drv_drv->  id_%u  %s  out_net %s  line:%u\n",
                    drv_drv->id_, drv_drv->cPrimType(),
                    drv_drv->out_.c_str(), drv_drv->lnum_);
          }

          if (not drv_drv->canDriveClockNode()) {
            flush_out(true); err_puts();
            lprintf2("[Error] bad driver (%s) for clock-buf node #%u line:%u  must be %s\n",
                      drv_drv->cPrimType(), dn.id_, dn.lnum_, s_possible_cdrivers);
            err_puts(); flush_out(true);
            err_lnum_ = dn.lnum_;
            err_lnum2_ = drv_drv->lnum_;
            return false;
          }

          // -- create NW-Node for drv_drv->out_
          //    and connect it to the iput of CLK_BUF 'dn'

          assert(drv_drv->out_ == inp1);
          uint64_t drv_drv_outKey = 0;
          uint drv_drv_outNid = 0;
          nid = kid = 0;
          uint drv_drv_realId = drv_drv->realId(*this);

          if (drv_drv->isTopInput()) {
            assert(drv_drv->nw_id_);
            drv_drv_outNid = drv_drv->nw_id_;
            assert(pg_.hasNode(drv_drv_outNid));
            assert(pg2blif_.count(drv_drv_outNid));
            assert(map_pg2blif(drv_drv_outNid) == drv_drv_realId);
            drv_drv_outKey = pg_.nodeRefCk(drv_drv_outNid).key_;
            assert(drv_drv_outKey);
          }
          else {

            drv_drv_outKey = drv_drv->cellHash0();
            assert(drv_drv_outKey);
            drv_drv_outNid = pg_.insK(drv_drv_outKey);
            assert(drv_drv_outNid);
            pg_.nodeRef(drv_drv_outNid).markClk(true);
            pg2blif_.emplace(drv_drv_outNid, drv_drv_realId);

            if (not pg_.nodeRefCk(drv_drv_outNid).isNamed()) {
              ::snprintf(nm_buf, 510, "nd%u_L%u_",
                         drv_drv_outNid, drv_drv->lnum_);
              if (drv_drv->is_CLK_BUF()) {
                ::strcat(nm_buf, "ddCBUF_");
                string outPin = pr_first_output(driver->ptype_);
                assert(not outPin.empty());
                ::strcat(nm_buf, outPin.c_str());
              }

              assert(not pg_.nodeRef(drv_drv_outNid).inp_flag_);
              pg_.setNodeName3(drv_drv_outNid, drv_drv_realId,
                               drv_drv->lnum_, drv_drv->out_.c_str());
            }

          }

          assert(ipin_nid);
          assert(pg_.hasNode(ipin_nid));
          assert(pg_.nodeRefCk(ipin_nid).isNamed());

          assert(drv_drv_outKey);
          assert(drv_drv_outNid);
          assert(pg_.nodeRefCk(drv_drv_outNid).isNamed());

          // eid = pg_.linK(drv_drv_outKey, drv_drv_outKey);
          eid = pg_.linkNodes(drv_drv_outNid, ipin_nid, false);
          assert(eid);
          pg_.edgeRef(eid).paintRed();
          if (trace_ >= 12)
            lprintf("\t\t\t eid= %u\n", eid);
        }
      }
    }
  }

  // mark prim-types in NW:
  for (const auto I : pg2blif_) {
    uint nw_id = I.first;
    uint bl_id = I.second;
    const BNode& bnode = bnodeRef(bl_id);
    assert(not bnode.isVirtualMog());
    if (bnode.hasPrimType())
      pg_.nodeRefCk(nw_id).markPrim(bnode.ptype_);
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

string BLIF_file::writePinGraph(CStr fn0, bool nodeTable, bool noDeg0) const noexcept {
  auto& ls = lout();
  if (trace_ >= 5) {
    flush_out(true);
    if (trace_ >= 6)
      pg_.print(ls, "\t *** pin_graph for clock-data separation ***");
    else
      lputs("\t --- pin_graph for clock-data separation ---");
    lprintf("\t  pg_. numN()= %u   numE()= %u\n", pg_.numN(), pg_.numE());
    lputs();
    pg_.printSum(ls, 0);
    lputs();
    if (trace_ >= 6)
      pg_.printEdges(ls, "|edges|");
    flush_out(true);
  }

  if (!fn0 or !fn0[0])
    fn0 = "pinGraph.dot";

  string fn = str::concat(topModel_.c_str(), "_", fn0);

  bool wrDot_ok = pg_.writeDot(fn.c_str(), nullptr, nodeTable, noDeg0);
  if (!wrDot_ok) {
    flush_out(true); err_puts();
    lprintf2("[Error] could not write pin_graph to file '%s'\n", fn.c_str());
    err_puts(); flush_out(true);
  }

  lprintf("(writePinGraph)  status:%s  file: %s\n",
          wrDot_ok ? "OK" : "FAIL",
          fn.c_str());

  flush_out(true);
  if (wrDot_ok)
    return fn;
  return {};
}


string BLIF_file::writeBlif(const string& toFn, bool cleanUp,
                            vector<uspair>& corrected) noexcept {
  corrected.clear();
  if (toFn.empty())
    return {};
  if (!fsz_ || !sz_ || !buf_ || fnm_.empty())
    return {};
  if (not hasLines())
    return {};

  string fn2 = toFn; // (toFn == fnm_ ? str::concat("2_", toFn) : toFn);

  if (trace_ >= 4) {
    lout() << "pln_blif_file: writing BLIF to " << fn2
           << "\n  CWD= " << get_CWD()
           << "\n  clean-up mode : " << int(cleanUp) << endl;
  }

  CStr cnm = fn2.c_str();
  FILE* f = ::fopen(cnm, "w");
  if (!f) {
    if (trace_ >= 3) {
      flush_out(true);
      lprintf2("[Error] writeBlif() could not open file for writing: %s\n", cnm);
      flush_out(true);
    }
    return {};
  }

  constexpr uint buf_CAP = 1048574; // 1 MiB
  char buf[buf_CAP + 2] = {};
  size_t cnt = 0, n = lines_.size();
  if (n < 2) {
    ::fclose(f);
    return {};
  }
  bool error = false;

  ::fprintf(f, "### written by PLANNER %s\n\n", pln_get_version());
  if (::ferror(f)) {
    if (trace_ >= 3) {
      flush_out(true);
      lprintf2("ERROR writeBlif() error during writing: %s\n", cnm);
      flush_out(true);
    }
    error = true;
    n = 0; // skips the loop
  }

  for (size_t lineNum = 0; lineNum < n; lineNum++) {
    CStr cs = lines_[lineNum];
    if (!cs || !cs[0])
      continue;

    ::strncpy(buf, cs, buf_CAP);

    if (cleanUp) {
      NodeDescriptor ddesc = hasDanglingBit(lineNum);
      if (ddesc.valid()) {
        assert(ddesc.nid_ > 0);
        const BNode& dNode = bnodeRef(ddesc.nid_);
        assert(not dNode.isVirtualMog());
        assert(not dNode.realData_.empty());
        assert(not dNode.dangTerms_.empty());
        // build 'buf' skipping dangling terms
        uint skipCnt = 0;
        buf[0] = 0; buf[1] = 0;
        size_t rdsz = dNode.realData_.size();
        if (trace_ >= 4) {
          lprintf("  wrBlif: cell %s at line %zu has dangling bit, filtering..\n",
                  dNode.cPrimType(), lineNum);
          if (trace_ >= 5) {
            lprintf("    dangling cell realData_.size()= %zu\n", rdsz);
          }
        }
        char* tail = ::stpcpy(buf, ".subckt");
        for (size_t i = 0; i < rdsz; i++) {
          if (dNode.isDanglingTerm(i)) {
            if (trace_ >= 6)
              lprintf("\t    (wrBlif) skipping dangling term %zu\n", i);
            corrected.emplace_back(lineNum, dNode.realData_[i]);
            skipCnt++;
            continue;
          }
          CStr ts = dNode.realData_[i].c_str();
          tail = ::stpcpy(tail, " ");
          tail = ::stpcpy(tail, ts);
        }
        if (trace_ >= 4) {
          lprintf("  wrBlif: filtered dangling bits (%u) for cell %s at line %zu\n",
                  skipCnt, dNode.cPrimType(), lineNum);
        }
      }
    }

    ::fputs(buf, f);
    ::fputc('\n', f);

    if (::ferror(f)) {
      if (trace_ >= 3) {
        flush_out(true);
        lprintf2("ERROR writeBlif() error during writing: %s\n", cnm);
        flush_out(true);
      }
      error = true;
      break;
    }
    cnt++;
  }

  ::fclose(f);

  flush_out(trace_ >= 5);

  if (error) {
    flush_out(true); err_puts();
    lprintf2("[Error] writeBlif ERROR writing: %s\n", cnm);
    err_puts(); flush_out(true);
  }
  else if (trace_ >= 4) {
    lprintf("  writeBlif OK written #lines= %zu\n", cnt);
  }

  flush_out(trace_ >= 5);

  if (error)
    return {};
  return fn2;
}

}

