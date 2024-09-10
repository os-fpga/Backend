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

void NW::setNodeName(uint id, CStr nm) noexcept {
  assert(hasNode(id));

  assert(nm);

  //if (nodeRef(id).isNamed())
  //  lputs1();
  // ---
  //if (id == 17)
  //  lputs1();

  if (nm) {
    lprintf(" (setNodeName)   %u --> %s\n", id, nm);
    nodeRefCk(id).name_ = nm;
    return;
  }

  nodeRefCk(id).name_.clear();
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

}

