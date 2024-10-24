#pragma once
#ifndef _pln_BLIF_FILE_H_e6f4df41a2f915079_
#define _pln_BLIF_FILE_H_e6f4df41a2f915079_

#include "file_io/pln_Fio.h"
#include "file_io/pln_primitives.h"
#include "util/geo/xyz.h"
#include "util/nw/Nw.h"

#include <unordered_map>

namespace pln {

using std::string;
using std::vector;

struct BLIF_file : public fio::MMapReader
{
  struct NodeDescriptor {
    uint lnum_ = 0;
    uint  nid_ = 0;
    int  term_ = -1; // index in realData_

    NodeDescriptor() noexcept = default;
    NodeDescriptor(uint l, uint n, int t) noexcept
      : lnum_(l), nid_(n), term_(t)
    {}

    bool valid() const noexcept { return lnum_; }
    void inval() noexcept { lnum_ = 0; term_ = -1; }
    void reset() noexcept { lnum_ = 0; nid_ = 0; term_ = -1; }
  };

  // Nodes are rooted at output ports.
  // Input ports are leaves.
  // Output ports are roots of fanin cones.
  struct BNode {
    uint id_ = 0;      // index+1 in pool
    uint nw_id_ = 0;   // node-id in NW, for ports only

    uint lnum_ = 0;    // line number in blif
    uint parent_ = 0;
    uint depth_ = 0;
    vector<uint> chld_;

    string kw_;               // keyword: .names, .latch, .subckt, .gate, etc.

    vector<string> realData_; // everything on the line ater kw, tokenized.
                              // populated only for real MOGs, otherwise use data_

    vector<string> data_;     // like realData_, but has only 1 output term (virtual SOG)

    vector<uint> dangTerms_;  // realData_ indexes of dangling bits

    vector<string> inPins_;   // input pins from Prim-DB
    vector<string> inSigs_;   // input signals from blif-file

    vector<string> disabledClocks_; // clock input pins of RAM18KX2 that are connected to $false
                                    // and should not participate in pinGraph

    string out_;              // SOG output net (=signal) (real or virtual)

    uint virtualOrigin_ = 0;  // node-ID from which this virtual MOG is created

    prim::Prim_t  ptype_ = prim::A_ZERO;

    uint64_t cell_hc_ = 0, out_hc_ = 0;

    int16_t is_top_ = 0;     // -1:top input  1:top output
    bool is_mog_ = false;
    bool isClockLess_ = false;

    bool is_wire_ = false, is_const_ = false;  // .names

  public:
    BNode() noexcept = default;
    BNode(CStr keyword, uint L) noexcept : lnum_(L) {
      if (keyword) kw_ = keyword;
    }

    const string& getInPin(uint pinIndex) const noexcept {
      assert(inPins_.size() == inSigs_.size());
      assert(pinIndex < inPins_.size());
      return inPins_[pinIndex];
    }
    const string& getInSig(uint pinIndex) const noexcept {
      assert(inPins_.size() == inSigs_.size());
      assert(pinIndex < inSigs_.size());
      return inSigs_[pinIndex];
    }

    void setOutHash() noexcept {
      assert(id_);
      out_hc_ = is_top_ ? id_ : str::hashf(out_.c_str());
    }
    uint64_t outHash() const noexcept {
      assert(id_);
      if (is_top_)
        return id_;
      return out_hc_ ? out_hc_ : str::hashf(out_.c_str());
    }

    void setCellHash() noexcept {
      assert(id_);
      if (is_top_) {
        cell_hc_ = id_;
        return;
      }
      cell_hc_ = hashComb(id_, out_);
    }
    uint64_t cellHash() const noexcept {
      assert(id_);
      if (is_top_)
        return id_;
      if (cell_hc_) return cell_hc_;
      return hashComb(id_, out_);
    }
    uint64_t cellHash0() const noexcept {
      assert(id_);
      if (is_top_)
        return id_;
      return hashComb(id_, out_);
    }

    bool isTopPort() const noexcept { return is_top_ != 0; }
    bool isTopInput() const noexcept { return is_top_ < 0; }
    bool isTopOutput() const noexcept { return is_top_ > 0; }

    bool isRoot() const noexcept { return parent_ == 0; }
    bool isLeaf() const noexcept { return chld_.empty(); }

    bool isMog() const noexcept { return is_mog_; }
    bool isVirtualMog() const noexcept { return is_mog_ and virtualOrigin_ > 0; }

    bool isLatch() const noexcept { return kw_ == ".latch"; }

    bool valid() const noexcept { return id_ > 0; }
    void inval() noexcept { id_ = 0; }

    bool hasPrimType() const noexcept {
      uint pt = ptype_;
      return pt > 0 and pt < prim::Prim_MAX_ID;
    }

    uint realId(const BLIF_file& bf) const noexcept {
      if (isVirtualMog())
        return bf.bnodeRef(virtualOrigin_).id_;
      return id_;
    }

    uint deg() const noexcept { return uint(!isRoot()) + chld_.size(); }
    uint inDeg() const noexcept { return uint(!isRoot()); }
    uint outDeg() const noexcept { return chld_.size(); }

    bool out_contact(const string& x) const noexcept {
      assert(!x.empty());
      assert(!out_.empty());
      return x == out_;
    }

    // returns pin index in chld_ or -1
    int in_contact(const string& x) const noexcept;

    void place_output_at_back(vector<string>& dat) noexcept;

    CStr data_front() const noexcept {
      return data_.size() > 1 ? data_.front().c_str() : nullptr;
    }

    bool is_str_IBUF() const noexcept {
      return data_.size() > 1 and data_.front() == "I_BUF";
    }
    bool is_str_OBUF() const noexcept {
      return data_.size() > 1 and data_.front() == "O_BUF";
    }

    bool is_IBUF() const noexcept {
      using namespace prim;
      return ptype_ == I_BUF or ptype_ == I_BUF_DS;
    }
    bool is_OBUF() const noexcept {
      using namespace prim;
      return ptype_ == O_BUF or ptype_ == O_BUF_DS or
             ptype_ == O_BUFT or ptype_ == O_BUFT_DS;
    }
    bool is_CLK_BUF() const noexcept {
      return ptype_ == prim::CLK_BUF;
    }
    bool is_LUT() const noexcept {
      return prim::pr_is_LUT(ptype_);
    }
    bool is_FF() const noexcept {
      return prim::pr_is_DFF(ptype_);
    }
    bool is_RAM() const noexcept {
      return prim::pr_is_RAM(ptype_);
    }
    bool is_DSP() const noexcept {
      return prim::pr_is_DSP(ptype_);
    }
    bool is_clockLess_DSP() const noexcept {
      return isClockLess_ and prim::pr_is_DSP(ptype_);
    }

    bool canDriveClockNode() const noexcept {
      return isTopInput() or is_CLK_BUF() or ptype_ == prim::I_SERDES;
    }

    string firstInputNet() const noexcept;

    void allInputPins(vector<string>& V) const noexcept;

    void allInputSignals(vector<string>& V) const noexcept;

    CStr cOut() const noexcept { return out_.empty() ? "{e}" : out_.c_str(); }

    CStr cPrimType() const noexcept;

    bool isDanglingTerm(uint term) const noexcept;

    bool isDisabledClock(const string& pinName, const BLIF_file& bf) const noexcept {
      assert(not pinName.empty());
      if (pinName.empty())
        return false;
      if (ptype_ != prim::TDP_RAM18KX2)
        return false;
      const BNode* base = isVirtualMog() ? &bf.bnodeRef(virtualOrigin_) : this;
      for (const string& x : base->disabledClocks_) {
        if (x == pinName)
          return true;
      }
      return false;
    }

    struct CmpOut {
      bool operator()(const BNode* a, const BNode* b) const noexcept {
        return a->out_ < b->out_;
      }
    };
    struct PinIsInput {
      prim::Prim_t pt_ = prim::A_ZERO;
      PinIsInput(prim::Prim_t p) noexcept
        : pt_(p) {}
      bool operator()(const string& pinName) const noexcept {
        return not pr_pin_is_output(pt_, pinName); 
      }
    };

    struct PinPatternIsInput {
      prim::Prim_t pt_ = prim::A_ZERO;
      PinPatternIsInput(prim::Prim_t p) noexcept
        : pt_(p) {}
      bool operator()(const string& pat) const noexcept {
        size_t len = pat.length();
        if (len < 2 or len > 256)
          return false;
        char buf[258];
        ::strcpy(buf, pat.c_str());
        // cut at '='
        for (uint i = 0; i < len; i++) {
          if (buf[i] == '=') {
            buf[i] = 0;
            break;
          }
        }
        return not pr_cpin_is_output(pt_, buf);
      }
    };

  }; // BNode

  BNode& bnodeRef(uint id) noexcept {
    assert(id > 0 and id < nodePool_.size());
    return nodePool_[id];
  }
  const BNode& bnodeRef(uint id) const noexcept {
    assert(id > 0 and id < nodePool_.size());
    return nodePool_[id];
  }

  string topModel_;
  string pinGraphFile_;

  vector<string> inputs_;
  vector<string> outputs_;

  size_t inputs_lnum_ = 0, outputs_lnum_ = 0;
  mutable uint err_lnum_ = 0, err_lnum2_ = 0;

  string err_msg_;
  uint16_t trace_ = 0;

  bool rd_ok_ = false;
  bool chk_ok_ = false;
  uint num_MOGs_ = 0;

  std::array<uint, prim::Prim_MAX_ID> typeHistogram_;

public:
  BLIF_file() noexcept = default;
  BLIF_file(CStr nm) noexcept : fio::MMapReader(nm) {}
  BLIF_file(const string& nm) noexcept : fio::MMapReader(nm) {}

  virtual ~BLIF_file();

  virtual void reset(CStr nm, uint16_t tr = 0) noexcept override;

  bool readBlif() noexcept;

  string writeBlif(const string& toFn, bool cleanUp,
                   std::vector<uspair>& corrected) noexcept;

  bool checkBlif(vector<string>& badInputs, vector<string>& badOutputs) noexcept;

  uint numInputs() const noexcept { return inputs_.size(); }
  uint numOutputs() const noexcept { return outputs_.size(); }
  uint numNodes() const noexcept { return nodePool_.empty() ? 0 : nodePool_.size() - 1; }
  uint countLUTs() const noexcept;
  uint countFFs() const noexcept;
  uint countCBUFs() const noexcept;
  void countBUFs(uint& nIBUF, uint& nOBUF, uint& nCBUF) const noexcept;

  void countMOGs(uint& nISERD, uint& nDSP38, uint& nDSP19X,
                 uint& nRAM36, uint& nRAM18) const noexcept;

  uint typeHist(prim::Prim_t t) const noexcept { return typeHistogram_[t]; }

  void collectClockedNodes(vector<BNode*>& V) noexcept;
  std::array<uint, prim::Prim_MAX_ID> countTypes() const noexcept;

  uint countCarryNodes() const noexcept;
  uint countWireNodes() const noexcept;
  uint countConstNodes() const noexcept;

  uint numWarnings() const noexcept;
  NodeDescriptor hasDanglingBit(uint lnum) const noexcept;

  uint printInputs(std::ostream& os, CStr spacer = nullptr) const noexcept;
  uint printOutputs(std::ostream& os, CStr spacer = nullptr) const noexcept;
  uint printNodes(std::ostream& os) const noexcept;
  uint printPrimitives(std::ostream& os, bool instCounts) const noexcept;

  uint printCarryNodes(std::ostream& os) const noexcept;

private:
  bool createNodes() noexcept;
  bool linkNodes() noexcept;
  void link2(BNode& from, BNode& to) noexcept;

  bool createPinGraph() noexcept;
  bool linkPinGraph() noexcept;

  string writePinGraph(CStr fn0) const noexcept;

  bool checkClockSepar(vector<BNode*>& clocked) noexcept;

  BNode* findOutputPort(const string& contact) noexcept;
  BNode* findInputPort(const string& contact) noexcept;

  BNode* findFabricParent(uint of, const string& contact, int& pinIndex) noexcept;  // searches inputs
  BNode* findFabricDriver(uint of, const string& contact) noexcept;                 // matches out_

  BNode* findDriverNode(uint of, const string& contact) noexcept;

  // collects matching input pins from all cells
  // pair: 1st - nodeId, 2nd - pinIndex
  void getFabricParents(uint of, const string& contact, vector<upair>& PAR) noexcept;

  uint map_pg2blif(uint pg_nid) const noexcept {
    assert(pg_nid);
    assert(not pg2blif_.empty());
    const auto F = pg2blif_.find(pg_nid);
    assert(F != pg2blif_.end());
    if (F == pg2blif_.end())
      return 0;
    return F->second;
  }

  static int findTermByNet(const vector<string>& D, const string& net) noexcept; // index in BNode::data_

// DATA:
  std::vector<BNode> nodePool_;  // nodePool_[0] is a fake node "parent of root"

  std::vector<BNode*> topInputs_, topOutputs_;
  std::vector<BNode*> fabricNodes_, constantNodes_;
  std::vector<BNode*> fabricRealNodes_; // skip virtual SOGs

  std::vector<NodeDescriptor> dangOutputs_; // (lnum, nid, term)

  std::vector<BNode*> latches_; // latches are not checked for now

  std::unordered_map<uint, uint> pg2blif_; // map IDs from NW to BLIF_file::BNode::id_

  NW pg_; // Pin Graph
};

}

#endif
