#include "util/nw/Nw.h"

namespace pln {

uint NW::Node::outDeg(const NW& g) const noexcept {
  if (edges_.empty()) return 0;

  uint deg = 0;
  for (uint eid : edges_) {
    const Edge& ed = g.edgeRef(eid);
    assert(ed.valid());
    if (ed.n1_ == id_) deg++;
  }

  return deg;
}

uint NW::Node::inDeg(const NW& g) const noexcept {
  if (edges_.empty()) return 0;

  uint deg = 0;
  for (uint eid : edges_) {
    const Edge& ed = g.edgeRef(eid);
    assert(ed.valid());
    if (ed.n2_ == id_) deg++;
  }

  return deg;
}

void NW::Node::getFanoutE(const NW& g, vecu& E) const noexcept {
  for (OutgEI I(g, id_); I.valid(); ++I) {
    const Edge& e = *I;
    assert(e.n1_ == id_);
    E.push_back(e.id_);
  }
}

void NW::Node::getFanout(const NW& g, vecu& A) const noexcept {
  for (OutgEI I(g, id_); I.valid(); ++I) {
    const Edge& e = *I;
    assert(e.n1_ == id_);
    uint v = otherSide(e);
    assert(g.hasNode(v));
    A.push_back(v);
  }
}

void NW::Node::getFanin(const NW& g, vecu& A) const noexcept {
  for (IncoEI I(g, id_); I.valid(); ++I) {
    const Edge& e = *I;
    assert(e.n2_ == id_);
    uint v = otherSide(e);
    assert(g.hasNode(v));
    A.push_back(v);
  }
}

void NW::Node::getAdj(const NW& g, vecu& A) const noexcept {
  A.reserve(edges_.size());
  getFanin(g, A);
  getFanout(g, A);
}

void NW::getIncoE(const Node& nd, vecu& E) const noexcept {
  E.clear();
  if (empty()) return;
  for (int i = nd.degree() - 1; i >= 0; i--) {
    uint ei = nd.edges_[i];
    const Edge& e = edgeRef(ei);
    if (not nd.edgeInco(e)) continue;
    E.push_back(ei);
  }
}

void NW::getOutgE(const Node& nd, vecu& E) const noexcept {
  E.clear();
  if (empty()) return;
  for (int i = nd.degree() - 1; i >= 0; i--) {
    uint ei = nd.edges_[i];
    const Edge& e = edgeRef(ei);
    if (nd.edgeInco(e)) continue;
    E.push_back(ei);
  }
}

static constexpr uint STOP_ON_NAMING_NODE = 29;

void NW::setNodeName(uint id, CStr nm) noexcept {
  assert(hasNode(id));
  // assert(nm);

  if (STOP_ON_NAMING_NODE) {
    if (id == STOP_ON_NAMING_NODE) {
      lputs1();
    }
  }

  if (nm) {
    if (trace_ >= 6)
      lprintf(" (setNodeName)   %u --> %s\n", id, nm);
    nodeRefCk(id).name_ = nm;
    return;
  }

  nodeRefCk(id).name_.clear();
}

void NW::setNodeName3(uint id, uint B, uint L, CStr a) noexcept {
  char buf[128];
  Node& nd = nodeRefCk(id);
  char c = 'n';
  if (nd.inp_flag_)
    c = 'i';
  else if (nd.out_flag_)
    c = 'o';
  ::sprintf(buf, "%c%uB%uL%u_", c, id, B, L);
  nd.name_ = str::concat(buf, a);

  if (trace_ >= 6)
    lprintf(" (setNodeName3)   %u --> %s\n", id, nd.name_.c_str());

  if (STOP_ON_NAMING_NODE) {
    if (id == STOP_ON_NAMING_NODE) {
      lputs1();
    }
  }
}

void NW::setNodeName4(uint id, uint B, uint L, uint ipin, CStr a) noexcept {
  char buf[128];
  Node& nd = nodeRefCk(id);
  char c = 'n';
  if (nd.inp_flag_)
    c = 'i';
  else if (nd.out_flag_)
    c = 'o';
  ::sprintf(buf, "%c%uB%uL%u_p%u", c, id, B, L, ipin);

  nd.name_ = str::concat(buf, a);

  if (trace_ >= 6)
    lprintf(" (setNodeName4)   %u --> %s\n", id, nd.name_.c_str());

  if (STOP_ON_NAMING_NODE) {
    if (id == STOP_ON_NAMING_NODE) {
      lputs1();
    }
  }
}

uint NW::countRedEdges() const noexcept {
  uint cnt = 0;
  for (cEI I(*this); I.valid(); ++I) {
    if (I->isRed())
      cnt++;
  }
  return cnt;
}

void NW::clearEdges() noexcept {
  edStor_.clear();
  edStor_.emplace_back();
  if (empty()) return;
  for (NI I(*this); I.valid(); ++I) {
    I->edges_.clear();
  }
}

uint NW::getMaxNid() const noexcept {
  if (empty()) return 0;
  uint maxId = 0;
  for (cNI I(*this); I.valid(); ++I) {
    const Node& nd = *I;
    if (nd.id_ > maxId) maxId = nd.id_;
  }
  return maxId;
}

uint64_t NW::getMaxKey() const noexcept {
  if (empty()) return 0;
  uint64_t maxK = 0;
  for (cNI I(*this); I.valid(); ++I) {
    const Node& nd = *I;
    if (nd.key_ > maxK) maxK = nd.key_;
  }
  return maxK;
}

}

