#pragma once
#ifndef _PLN_util_Network_Nw_4d81595e0ba86e_
#define _PLN_util_Network_Nw_4d81595e0ba86e_

#include "util/geo/xyz.h"
#include "util/pln_log.h"

#include <deque>

//  Abbreviations:
//
//    NW - Network
//
//    NI - Node Iterator
//    EI - Edge Iterator
//
//    cNI - Const Node Iterator
//    cEI - Const Edge Iterator
//

namespace pln {

using veci = std::vector<int>;
using vecu = std::vector<uint>;
using ipair = std::pair<int, int>;
using upair = std::pair<uint, uint>;
using std::ostream;

struct NW {
  struct Node;
  using NodeStor = std::deque<Node>;

  struct Edge {
    uint id_ = 0;
    uint n1_ = 0, n2_ = 0;
    uint color_ = 0;
    uint next_ = 0;

    bool inCell_ = false;

    Edge() noexcept = default;

    Edge(uint n1, uint n2, uint id) noexcept : id_(id), n1_(n1), n2_(n2) {
      assert(n1 and n2);
      assert(n1 != n2);
    }

    bool valid() const noexcept { return n1_; }
    void inval() noexcept { ::memset(this, 0, sizeof(*this)); }

    void print(ostream& os) const noexcept {
      os_printf(os, "eid:%u (%u %u) clr:%u", id_, n1_, n2_, color_);
    }
    void printl(ostream& os) const noexcept {
      print(os);
      os << std::endl;
    }
    void eprint_dot(ostream& os, char arrow, const NW& g) const noexcept;
  };
  using EdgeStor = std::deque<Edge>;

  struct Node : public XY {
    Node() noexcept = default;

    Node(const XY& a, uint64_t k, uint i, uint pa, int lb) noexcept
        : XY(a), key_(k), id_(i), par_(pa), lbl_(lb) {}

    Node(int c1, int c2, uint64_t k, uint i, uint pa, int lb) noexcept
        : XY(c1, c2), key_(k), id_(i), par_(pa), lbl_(lb) {}

    bool valid() const noexcept { return id_; }
    void inval() noexcept {
      XY::inval();
      edges_.clear();
      id_ = par_ = 0;
      key_ = 0;
    }

    std::string getName() const noexcept {
      std::string nm{"nd_"};
      nm += std::to_string(id_);
      return nm;
    }

    const XY& xy() const noexcept { return *this; }
    bool hasCoord() const noexcept { return valid() ? XY::valid() : false; }

    uint otherSide(const Edge& e) const noexcept {
      assert(id_);
      assert(e.n1_ and e.n2_);
      assert(e.n1_ != e.n2_);
      if (e.n1_ == id_) return e.n2_;
      assert(e.n2_ == id_);
      return e.n1_;
    }
    bool edgeInco(const Edge& e) const noexcept {
      assert(id_);
      assert(e.n1_ and e.n2_);
      assert(e.n1_ != e.n2_);
      if (e.n1_ == id_) return false;
      assert(e.n2_ == id_);
      return true;
    }
    bool edgeOutg(const Edge& e) const noexcept { return not edgeInco(e); }

    inline void getFanout(const NW& g, vecu& A) const noexcept;
    inline void getFanin(const NW& g, vecu& A) const noexcept;
    inline void getAdj(const NW& g, vecu& A) const noexcept;
    inline void getFanoutE(const NW& g, vecu& E) const noexcept;

    uint degree() const noexcept { return edges_.size(); }
    inline uint outDeg(const NW& g) const noexcept;
    inline uint inDeg(const NW& g) const noexcept;

    uint parent() const noexcept { return par_; }
    bool isTreeRoot() const noexcept { return !par_; }
    bool isFlagRoot() const noexcept { return root_flag_; }
    void markRoot(bool val) noexcept { root_flag_ = val; }
    void markSink(bool val) noexcept { sink_flag_ = val; }
    bool terminal() const noexcept { return sink_flag_ or root_flag_; }

    void print(ostream& os) const noexcept;
    void nprint_dot(ostream& os) const noexcept;

    bool isCell() const noexcept { return cid_; }
    void setCid(uint ci) noexcept { cid_ = ci; }

    // DATA:
    vecu edges_;

    uint64_t key_ = 0;

    uint id_ = 0;
    uint par_ = 0;
    uint cid_ = 0;

    int lbl_ = 0;
    int rad_ = 0;

    // DATA:
    bool root_flag_ = false;
    bool sink_flag_ = false;
  };

  struct cNI {
    cNI(const NW& g) noexcept : g_(g), cur_(0) { assert(!g.empty()); }

    void reset() noexcept { cur_ = 0; }
    bool valid() const noexcept { return cur_ < g_.nids_.size(); }
    cNI& operator++() noexcept {
      assert(valid());
      cur_++;
      return *this;
    }
    const Node& operator*() const noexcept {
      assert(valid());
      return g_.ndStor_[g_.nid_at(cur_)];
    }
    const Node* operator->() const noexcept {
      assert(valid());
      return &(g_.ndStor_[g_.nid_at(cur_)]);
    }

    const NW& g_;
    uint cur_ = 0;
  };

  struct NI {
    NI(NW& g) : g_(g), cur_(0) { assert(!g.empty()); }

    void reset() noexcept { cur_ = 0; }
    bool valid() const noexcept { return cur_ < g_.nids_.size(); }
    NI& operator++() noexcept {
      assert(valid());
      cur_++;
      return *this;
    }
    Node& operator*() noexcept {
      assert(valid());
      return g_.ndStor_[g_.nid_at(cur_)];
    }
    Node* operator->() noexcept {
      assert(valid());
      return &(g_.ndStor_[g_.nid_at(cur_)]);
    }

    NW& g_;
    uint cur_ = 0;
  };

  struct OutgEI;
  struct IncoEI;
  struct ChldNI;
  struct cEI;
  struct EI;

  // API:
  NW() noexcept { clear(); }

  NW(const NW& g) noexcept;
  NW& operator=(const NW& g) noexcept;

  void reserve(uint cap) noexcept {
    if (!cap) return;
    assert(cap < uint(INT_MAX));
    nids_.reserve(cap);
    rids_.reserve(cap / 2);
  }

  uint size() const noexcept {
    assert(nids_.size() <= ndStor_.size());
    return nids_.size();
  }
  int isiz() const noexcept {
    assert(nids_.size() <= ndStor_.size());
    return int(nids_.size());
  }
  uint numN() const noexcept { return size(); }
  inline uint numE() const noexcept;

  inline upair countRoots() const noexcept;

  void getNodes(vecu& V) const noexcept { V = nids_; }

  bool empty() const noexcept { return nids_.empty(); }

  inline int getMaxLabel() const noexcept;
  inline uint getMaxNid() const noexcept;

  inline ipair getMinMaxDeg() const noexcept;
  inline ipair getMinMaxLbl() const noexcept;

  Node& nodeRef(uint id) noexcept {
    assert(nids_.size() <= ndStor_.size());
    assert(id > 0 and id < ndStor_.size());
    return ndStor_[id];
  }
  const Node& nodeRef(uint id) const noexcept {
    assert(nids_.size() <= ndStor_.size());
    assert(id > 0 and id < ndStor_.size());
    return ndStor_[id];
  }
  Node& nodeRefCk(uint id) noexcept {
    assert(nids_.size() <= ndStor_.size());
    assert(id > 0 and id < ndStor_.size());
    assert(ndStor_[id].id_ == id);
    return ndStor_[id];
  }
  const Node& nodeRefCk(uint id) const noexcept {
    assert(nids_.size() <= ndStor_.size());
    assert(id > 0 and id < ndStor_.size());
    assert(ndStor_[id].id_ == id);
    return ndStor_[id];
  }

  bool hasNode(uint id) const noexcept {
    if (!id or id >= ndStor_.size()) return false;
    const Node& nd = nodeRef(id);
    return nd.valid();
  }
  bool hasEdge(uint id) const noexcept {
    if (!id or id >= edStor_.size()) return false;
    const Edge& ed = edgeRef(id);
    return ed.valid();
  }
  bool hasCoord(uint id) const noexcept {
    if (not hasNode(id)) return false;
    const Node& nd = nodeRef(id);
    if (not nd.valid()) return false;
    return nd.XY::valid();
  }

  void markSink(uint id, bool val) noexcept {
    assert(hasNode(id));
    nodeRef(id).markSink(val);
  }

  Edge& edgeRef(uint i) noexcept {
    assert(i and i < edStor_.size());
    return edStor_[i];
  }
  const Edge& edgeRef(uint i) const noexcept {
    assert(i and i < edStor_.size());
    return edStor_[i];
  }

  inline void getIncoE(const Node& nd, vecu& E) const noexcept;
  inline void getOutgE(const Node& nd, vecu& E) const noexcept;

  inline void clear() noexcept;
  inline void clearEdges() noexcept;

  const Node* parPtr(const Node& nd) const noexcept {
    return nd.par_ > 0 ? &nodeRefCk(nd.par_) : nullptr;
  }
  Node* parPtr(const Node& nd) noexcept { return nd.par_ > 0 ? &nodeRefCk(nd.par_) : nullptr; }

  uint getOppNid(const Node& nd, uint eid) const noexcept {
    assert(eid > 0);
    const Edge& e = edgeRef(eid);
    assert(e.n1_ == nd.id_ or e.n2_ == nd.id_);
    return (e.n1_ == nd.id_ ? e.n2_ : e.n1_);
  }
  const Node* getOppPtr(const Node& nd, uint eid) const noexcept {
    const Node& op = nodeRefCk(getOppNid(nd, eid));
    return &op;
  }

  void addRoot(uint r) noexcept {
    assert(r and r <= size());
    rids_.push_back(r);
    Node& root = nodeRefCk(r);
    root.par_ = 0;
    root.markRoot(true);
  }

  uint first_rid() const noexcept { return rids_.empty() ? 0 : rids_.front(); }

  const Node& firstRoot() const noexcept {
    uint rootId = first_rid();
    assert(rootId);
    const Node& root = nodeRefCk(rootId);
    assert(root.isFlagRoot());
    return root;
  }
  Node& firstRoot() noexcept {
    uint rootId = first_rid();
    assert(rootId);
    Node& root = nodeRefCk(rootId);
    assert(root.isFlagRoot());
    return root;
  }

  void removeEdge(uint a, uint b) noexcept;

  void removeEdge(Edge& e) noexcept {
    assert(e.valid() and e.n1_ != e.n2_);
    removeEdge(e.n1_, e.n2_);
    assert(not e.valid());
  }
  void removeEdge(uint eid) noexcept {
    assert(eid);
    removeEdge(edStor_[eid]);
  }

  uint linkNodes(uint i1, uint i2, bool in_cel = false) noexcept;

  uint insK(uint64_t k) noexcept;

  uint insP(const XY& p, uint64_t k = 0) noexcept {
    if (!k) k = p.cantorHash();
    uint id = insK(k);
    nodeRef(id).setPoint(p);
    return id;
  }

  uint linK(uint64_t k1, uint64_t k2) noexcept;

  inline uint findEdge(uint a, uint b) const noexcept;

  uint findNode(uint64_t k) const noexcept {
    assert(0);
    return 0;
  }

  uint cid(const Edge& e) const noexcept {
    assert(e.n1_ and e.n2_);
    assert(e.n1_ != e.n2_);
    if (!e.inCell_) return 0;
    const Node& n1 = nodeRefCk(e.n1_);
    assert(n1.cid_ == nodeRefCk(e.n2_).cid_);
    return n1.cid_;
  }

  void labelNodes(int label) noexcept {
    for (NI I(*this); I.valid(); ++I) {
      I->lbl_ = label;
    }
  }
  inline void labelNodes(int label, uint parent) noexcept;
  inline void labelDepth() noexcept;
  inline void labelRadius() noexcept;

  inline void sort_xy() noexcept;
  inline void sort_label(vecu& V) const noexcept;
  inline void sort_cid(vecu& V) const noexcept;

  void getTopo(vecu& topo) noexcept;
  uint getRadius() noexcept;

  inline bool selfCheck() const noexcept { return true; }
  inline bool checkConn() const noexcept { return true; }

  bool verifyOneRoot() const noexcept;

  uint print(ostream& os, CStr msg = nullptr) const noexcept;
  uint dump(CStr msg = nullptr) const noexcept;
  uint printNodes(ostream& os, CStr msg, uint16_t forDot) const noexcept;
  uint dumpNodes(CStr msg = nullptr) const noexcept;

  uint printSum(ostream& os, uint16_t forDot) const noexcept;

  uint printMetis(ostream& os, bool nodeTable) const noexcept;
  uint dumpMetis(bool nodeTable) const noexcept;
  bool writeMetis(CStr fn, bool nodeTable) const noexcept;

  uint printDot(ostream& os, CStr nwNm, bool nodeTable = false) const noexcept;
  uint dumpDot(CStr nwNm) const noexcept;
  bool writeDot(CStr fn, CStr nwNm, bool nodeTable = false) const noexcept;

  void beComplete() noexcept;

  static constexpr CStr s_metisComment = R"(%% )";
  static constexpr CStr s_dotComment = R"(// )";

  struct Cmpi_xy {
    const NW& g_;
    Cmpi_xy(const NW& g) noexcept : g_(g) {}
    bool operator()(uint i, uint j) const noexcept {
      const XY& ni = g_.ndStor_[i];
      const XY& nj = g_.ndStor_[j];
      return ni < nj;
    }
  };
  struct Cmpi_key {
    const NW& g_;
    Cmpi_key(const NW& g) noexcept : g_(g) {}
    bool operator()(uint i, uint j) const noexcept {
      const Node& ni = g_.ndStor_[i];
      const Node& nj = g_.ndStor_[j];
      return ni.key_ < nj.key_;
    }
  };
  struct Cmpi_label {
    const NW& g_;
    Cmpi_label(const NW& g) noexcept : g_(g) {}
    bool operator()(uint i, uint j) const noexcept {
      const Node& ni = g_.ndStor_[i];
      const Node& nj = g_.ndStor_[j];
      return ni.lbl_ < nj.lbl_;
    }
  };
  struct Cmpi_cid {
    const NW& g_;
    Cmpi_cid(const NW& g) noexcept : g_(g) {}
    bool operator()(uint i, uint j) const noexcept {
      const Node& ni = g_.ndStor_[i];
      const Node& nj = g_.ndStor_[j];
      return ni.cid_ < nj.cid_;
    }
  };

  uint nid_at(uint i) const noexcept {
    assert(i < nids_.size());
    return nids_[i];
  }
  uint& nid_at(uint i) noexcept {
    assert(i < nids_.size());
    return nids_[i];
  }

  inline uint addNode(const XY& p, uint64_t k) noexcept;
  uint addNode(uint64_t k) noexcept { return addNode(XY{INT_MIN, INT_MIN}, k); }

  void rmNode(uint x) noexcept;

  void dfsSetDepth(const Node& fr, Node& to) noexcept {}
  void dfsSetRadius(const Node& fr, Node& to) noexcept {}
  bool dfsOrientEdges(const Node& fr, Node& to) noexcept { return 0; }

  uint bsearchNode(const uint* A, uint end, uint64_t k) const noexcept {
    assert(0);
    return 0;
  }

  static void dot_comment(ostream& os, uint16_t dotMode, CStr msg = nullptr) noexcept {
    if (!dotMode) return;
    if (dotMode == 1) {
      os << s_dotComment;
    } else {
      assert(dotMode == 2);
      os << s_metisComment;
    }
    if (msg) os << msg;
  }

  // DATA:
  vecu nids_;
  vecu rids_;
  NodeStor ndStor_;
  EdgeStor edStor_;

  uint16_t trace_ = 0;
};

inline void NW::clear() noexcept {
  nids_.clear();
  rids_.clear();
  ndStor_.clear();
  edStor_.clear();
  ndStor_.emplace_back();
  edStor_.emplace_back();
}

inline uint NW::addNode(const XY& p, uint64_t k) noexcept {
  if (ndStor_.empty()) ndStor_.emplace_back();

  uint id = ndStor_.size();
  ndStor_.emplace_back(p, k, id, 0, false);
  nids_.push_back(id);
  return id;
}

inline ipair NW::getMinMaxDeg() const noexcept {
  ipair deg{0, 0};
  if (empty()) return deg;

  const Node& n0 = nodeRefCk(nids_[0]);
  deg.first = n0.degree();
  deg.second = deg.first;
  for (cNI I(*this); I.valid(); ++I) {
    const Node& nd = *I;
    int d = nd.degree();
    assert(d >= 0);
    if (d < deg.first) deg.first = d;
    if (d > deg.second) deg.second = d;
  }
  return deg;
}

inline ipair NW::getMinMaxLbl() const noexcept {
  ipair L{0, 0};
  if (empty()) return L;

  const Node& n0 = nodeRefCk(nids_[0]);
  L.first = n0.lbl_;
  L.second = n0.lbl_;
  for (cNI I(*this); I.valid(); ++I) {
    const Node& nd = *I;
    int x = nd.lbl_;
    if (x < L.first) L.first = x;
    if (x > L.second) L.second = x;
  }
  return L;
}

struct NW::OutgEI {
  OutgEI(const NW& g, uint nid) noexcept : g_(g), nid_(nid) { reset(); }
  OutgEI(const NW& g, const Node& nd) noexcept : g_(g), nid_(nd.id_) { reset(); }
  void reset() noexcept {
    cur_ = 0;
    esz_ = g_.nodeRef(nid_).edges_.size();
    skipInco();
  }
  void reset(uint nid) noexcept {
    cur_ = 0;
    nid_ = nid;
    esz_ = g_.nodeRef(nid_).edges_.size();
    skipInco();
  }

  bool valid() const noexcept { return cur_ < esz_; }

  OutgEI& operator++() {
    assert(valid());
    cur_++;
    skipInco();
    return *this;
  }
  const Edge& operator*() const noexcept {
    assert(valid());
    return g_.edStor_[g_.nodeRef(nid_).edges_[cur_]];
  }
  const Edge* operator->() const noexcept {
    assert(valid());
    return &(g_.edStor_[g_.nodeRef(nid_).edges_[cur_]]);
  }

  void skipInco() noexcept {
    const Node& nd = g_.nodeRef(nid_);
    while (cur_ < esz_ and nd.edgeInco(g_.edStor_[nd.edges_[cur_]])) cur_++;
  }

  const NW& g_;
  uint nid_ = 0;
  uint cur_ = 0;
  uint esz_ = 0;
};

struct NW::IncoEI {
  IncoEI(const NW& g, uint nid) noexcept : g_(g), nid_(nid) { reset(); }
  IncoEI(const NW& g, const Node& nd) noexcept : g_(g), nid_(nd.id_) { reset(); }

  void reset() noexcept {
    cur_ = 0;
    esz_ = g_.nodeRef(nid_).edges_.size();
    skipOutgEdges();
  }
  void reset(uint nid) noexcept {
    cur_ = 0;
    nid_ = nid;
    esz_ = g_.nodeRef(nid_).edges_.size();
    skipOutgEdges();
  }

  bool valid() const noexcept { return cur_ < esz_; }

  IncoEI& operator++() {
    assert(valid());
    cur_++;
    skipOutgEdges();
    return *this;
  }
  const Edge& operator*() const noexcept {
    assert(valid());
    return g_.edStor_[g_.nodeRef(nid_).edges_[cur_]];
  }
  const Edge* operator->() const noexcept {
    assert(valid());
    return &(g_.edStor_[g_.nodeRef(nid_).edges_[cur_]]);
  }

  void skipOutgEdges() noexcept {
    const Node& nd = g_.nodeRef(nid_);
    while (cur_ < esz_ and nd.edgeOutg(g_.edStor_[nd.edges_[cur_]])) cur_++;
  }

  const NW& g_;
  uint nid_ = 0;
  uint cur_ = 0;
  uint esz_ = 0;
};

struct NW::EI {
  EI(NW& g) noexcept : stor_(g.edStor_), cur_(1) {
    while (valid() and !stor_[cur_].valid()) cur_++;
  }
  bool valid() const noexcept { return cur_ and cur_ < stor_.size(); }

  EI& operator++() noexcept {
    assert(valid());
    cur_ = stor_[cur_].next_;
    return *this;
  }
  Edge& operator*() noexcept {
    assert(valid());
    return stor_[cur_];
  }
  Edge* operator->() noexcept {
    assert(valid());
    return &(stor_[cur_]);
  }

  EdgeStor& stor_;
  uint cur_ = 0;
};

struct NW::cEI {
  cEI(const NW& g) noexcept : stor_(g.edStor_), cur_(1) {
    while (valid() and !stor_[cur_].valid()) cur_++;
  }

  bool valid() const noexcept { return cur_ and cur_ < stor_.size(); }

  cEI& operator++() noexcept {
    assert(valid());
    cur_ = stor_[cur_].next_;
    return *this;
  }
  const Edge& operator*() const noexcept {
    assert(valid());
    return stor_[cur_];
  }
  const Edge* operator->() const noexcept {
    assert(valid());
    return &(stor_[cur_]);
  }

  const EdgeStor& stor_;
  uint cur_ = 0;
};

struct NW::ChldNI : public NW::OutgEI {
  ChldNI(const NW& g, uint nid) noexcept : OutgEI(g, nid) {}
  ChldNI& operator++() noexcept {
    OutgEI::operator++();
    return *this;
  }
  const Node& operator*() noexcept {
    assert(valid());
    const Node& nd = g_.nodeRefCk(nid_);
    const Edge& e = g_.edStor_[nd.edges_[cur_]];
    return g_.ndStor_[nd.otherSide(e)];
  }
  const Node* operator->() noexcept {
    assert(valid());
    const Node& nd = g_.nodeRefCk(nid_);
    const Edge& e = g_.edStor_[nd.edges_[cur_]];
    return &(g_.ndStor_[nd.otherSide(e)]);
  }
};

inline uint NW::Node::outDeg(const NW& g) const noexcept {
  if (edges_.empty()) return 0;

  uint deg = 0;
  for (uint eid : edges_) {
    const Edge& ed = g.edgeRef(eid);
    assert(ed.valid());
    if (ed.n1_ == id_) deg++;
  }

  return deg;
}

inline uint NW::Node::inDeg(const NW& g) const noexcept {
  if (edges_.empty()) return 0;

  uint deg = 0;
  for (uint eid : edges_) {
    const Edge& ed = g.edgeRef(eid);
    assert(ed.valid());
    if (ed.n2_ == id_) deg++;
  }

  return deg;
}

inline void NW::Node::getFanoutE(const NW& g, vecu& E) const noexcept {
  for (OutgEI I(g, id_); I.valid(); ++I) {
    const Edge& e = *I;
    assert(e.n1_ == id_);
    E.push_back(e.id_);
  }
}

inline void NW::Node::getFanout(const NW& g, vecu& A) const noexcept {
  for (OutgEI I(g, id_); I.valid(); ++I) {
    const Edge& e = *I;
    assert(e.n1_ == id_);
    uint v = otherSide(e);
    assert(g.hasNode(v));
    A.push_back(v);
  }
}

inline void NW::Node::getFanin(const NW& g, vecu& A) const noexcept {
  for (IncoEI I(g, id_); I.valid(); ++I) {
    const Edge& e = *I;
    assert(e.n2_ == id_);
    uint v = otherSide(e);
    assert(g.hasNode(v));
    A.push_back(v);
  }
}

inline void NW::Node::getAdj(const NW& g, vecu& A) const noexcept {
  A.reserve(edges_.size());
  getFanin(g, A);
  getFanout(g, A);
}

inline void NW::getIncoE(const Node& nd, vecu& E) const noexcept {
  E.clear();
  if (empty()) return;
  for (int i = nd.degree() - 1; i >= 0; i--) {
    uint ei = nd.edges_[i];
    const Edge& e = edgeRef(ei);
    if (not nd.edgeInco(e)) continue;
    E.push_back(ei);
  }
}

inline void NW::getOutgE(const Node& nd, vecu& E) const noexcept {
  E.clear();
  if (empty()) return;
  for (int i = nd.degree() - 1; i >= 0; i--) {
    uint ei = nd.edges_[i];
    const Edge& e = edgeRef(ei);
    if (nd.edgeInco(e)) continue;
    E.push_back(ei);
  }
}

inline uint NW::numE() const noexcept {
  uint cnt = 0;
  for (cEI I(*this); I.valid(); ++I) cnt++;
  return cnt;
}

inline void NW::clearEdges() noexcept {
  edStor_.clear();
  edStor_.emplace_back();
  if (empty()) return;
  for (NI I(*this); I.valid(); ++I) {
    I->edges_.clear();
  }
}

inline uint NW::findEdge(uint a, uint b) const noexcept {
  if (!hasNode(a) or !hasNode(b)) return 0;
  assert(a != b);

  const Node& A = nodeRefCk(a);
  for (int i = A.degree() - 1; i >= 0; i--) {
    uint ei = A.edges_[i];
    const Edge& e = edStor_[ei];
    if (A.otherSide(e) == b) return ei;
  }
  return 0;
}

inline upair NW::countRoots() const noexcept {
  uint cnt = 0, mark_cnt = 0;
  if (empty()) return {cnt, mark_cnt};
  for (cNI I(*this); I.valid(); ++I) {
    const Node& nd = *I;
    if (nd.isTreeRoot()) cnt++;
    if (nd.isFlagRoot()) mark_cnt++;
  }
  return {cnt, mark_cnt};
}

inline int NW::getMaxLabel() const noexcept {
  if (empty()) return 0;
  int maxLabel = 0;
  for (cNI I(*this); I.valid(); ++I) {
    const Node& nd = *I;
    if (nd.lbl_ > maxLabel) maxLabel = nd.lbl_;
  }
  return maxLabel;
}

inline uint NW::getMaxNid() const noexcept {
  if (empty()) return 0;
  uint maxId = 0;
  for (cNI I(*this); I.valid(); ++I) {
    const Node& nd = *I;
    if (nd.id_ > maxId) maxId = nd.id_;
  }
  return maxId;
}

inline void NW::sort_xy() noexcept {
  std::sort(nids_.begin(), nids_.end(), Cmpi_xy(*this));
}

inline void NW::sort_label(vecu& V) const noexcept {
  if (V.size() < 2) return;
  std::stable_sort(V.begin(), V.end(), Cmpi_label(*this));
}

inline void NW::sort_cid(vecu& V) const noexcept {
  if (V.size() < 2) return;
  std::stable_sort(V.begin(), V.end(), Cmpi_cid(*this));
}

inline void NW::labelNodes(int label, uint parent) noexcept {
  for (NI I(*this); I.valid(); ++I) {
    I->lbl_ = label;
    I->par_ = parent;
  }
}

inline void NW::labelDepth() noexcept {
  uint rootId = first_rid();
  assert(rootId);
  assert(verifyOneRoot());

  Node& root = nodeRefCk(rootId);

  for (NI I(*this); I.valid(); ++I) I->lbl_ = -1;
  root.lbl_ = 0;

  for (int i = root.degree() - 1; i >= 0; i--) {
    Edge& e = edStor_[root.edges_[i]];
    Node& to = ndStor_[root.otherSide(e)];
    dfsSetDepth(root, to);
  }
}

inline void NW::labelRadius() noexcept {
  uint rootId = first_rid();
  assert(rootId);
  assert(hasNode(rootId));
  assert(verifyOneRoot());

  Node& root = nodeRef(rootId);

  for (NI I(*this); I.valid(); ++I) I->lbl_ = -1;
  root.lbl_ = 0;

  for (int i = root.degree() - 1; i >= 0; i--) {
    Edge& e = edStor_[root.edges_[i]];
    Node& to = ndStor_[root.otherSide(e)];
    dfsSetRadius(root, to);
  }
}

}

#endif

