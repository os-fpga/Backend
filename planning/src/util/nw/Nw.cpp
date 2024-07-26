#include "util/nw/Nw.h"

//  ====  NW - Network (aka Graph)

//  Abbreviations:
//
//    NI - Node Iterator
//    EI - Edge Iterator
//
//    cNI - Const Node Iterator
//    cEI - Const Edge Iterator
//

namespace pln {

uint NW::insK(uint64_t k) noexcept {
  assert(k);
  if (empty()) return addNode(k);

  uint newNid = 0;
  addNode(k);
  Node* p = &(ndStor_.back());

  vecu::iterator E = nids_.end() - 1;
  vecu::iterator I = std::lower_bound(nids_.begin(), E, p->id_, Cmpi_key(*this));
  if (I == E) {
    newNid = p->id_;
  } else if (ndStor_[*I].key_ == k) {
    newNid = nid_at(I - nids_.begin());
    ndStor_.pop_back();
    nids_.pop_back();
  } else {
    newNid = p->id_;
    nids_.pop_back();
    nids_.insert(I, newNid);
    assert(nodeRef(newNid).key_ == k);
    assert(nids_.size() <= ndStor_.size());
  }

  assert(nodeRef(newNid).key_ == k);
  return newNid;
}

uint NW::linkNodes(uint i1, uint i2, bool in_cel) noexcept {
  assert(i1 and i2);
  assert(i1 != i2);
  assert(hasNode(i1) and hasNode(i2));

  Node& nd1 = nodeRefCk(i1);
  Node& nd2 = nodeRefCk(i2);

  uint ee = findEdge(i1, i2);
  if (ee) {
    nd2.par_ = i1;
    return ee;
  }

  uint eIndex = edStor_.size();

  assert(nd1.id_ and nd2.id_);
  assert(nd1.id_ != nd2.id_);

  edStor_.emplace_back(nd1.id_, nd2.id_, eIndex);
  edStor_.back().next_ = 0;
  edStor_.back().inCell_ = in_cel;

  if (edStor_.size() > 2) {
    assert(edStor_[eIndex - 1].valid());
    edStor_[eIndex - 1].next_ = eIndex;
  }

  nd1.edges_.push_back(eIndex);
  nd2.edges_.push_back(eIndex);
  nd2.par_ = i1;
  return eIndex;
}

static void unlink_edge(NW& g, NW::Edge& e) noexcept {
  assert(e.valid());
  assert(g.hasEdge(e.id_));

  using edS_t = std::deque<NW::Edge>;
  edS_t& edS = g.edStor_;
  assert(edS.size());
  assert(edS.back().valid());

  uint eid = e.id_;
  uint e_next = e.next_;
  e.inval();

  auto prev_edge = [](edS_t& S, uint z) -> uint
  {
    assert(z > 0);
    for (uint i = z - 1; i > 0; i--) {
      if (S[i].valid() && S[i].next_ == z) return i;
    }
    return 0;
  };

  if (eid == edS.size() - 1) {
    edS.pop_back();
    while (!edS.empty() && !edS.back().valid()) edS.pop_back();
    if (!edS.empty()) {
      assert(edS.back().valid());
      edS.back().next_ = 0;
      assert(edS.back().id_ == edS.size() - 1);
    }
  } else if (eid > 0) {
    uint e_prev = prev_edge(edS, eid);
    if (e_prev > 0) {
      assert(edS[e_prev].valid());
      edS[e_prev].next_ = e_next;
    }
  }
}

void NW::rmNode(uint x) noexcept {
  assert(!empty());
  Node& rm = nodeRefCk(x);
  assert(rm.valid());

  int sz1 = isiz();
  for (int i = sz1 - 1; i >= 0; i--) {
    if (nids_[i] == x) {
      nids_.erase(nids_.begin() + i);
      break;
    }
  }
  assert(int(nids_.size()) == sz1 - 1);

  for (int i = rm.degree() - 1; i >= 0; i--) {
    const Edge& ei = edStor_[rm.edges_[i]];
    assert(ei.valid());
    Node& u = ndStor_[rm.otherSide(ei)];
    for (int j = u.degree() - 1; j >= 0; j--) {
      Edge& ej = edStor_[u.edges_[j]];
      assert(ej.valid());
      if (ej.n1_ == x or ej.n2_ == x) {
        u.edges_.erase( u.edges_.begin() + j );
        unlink_edge(*this, ej);
      }
    }
  }
  rm.inval();
}

void NW::getTopo(vecu& topo) noexcept {
  topo.clear();
  if (empty()) return;
  labelDepth();
  getNodes(topo);
  sort_label(topo);
}

uint NW::getRadius() noexcept {
  if (empty()) return 0;
  labelRadius();
  int ml = getMaxLabel();
  return ml >= 0 ? ml : 0;
}

using Nd = NW::Node;

static void dfs_setD(NW& g, const Nd& fr, Nd& to) noexcept {
  assert(to.lbl_ == UINT_MAX);
  assert(fr.lbl_ != UINT_MAX);
  assert(to.par_ > 0 and to.par_ == fr.id_);

  to.lbl_ = fr.lbl_ + 1;

  for (int i = to.degree() - 1; i >= 0; i--) {
    uint ei = to.edges_[i];
    NW::Edge& e = g.edgeRef(ei);
    Nd& other = g.nodeRef(to.otherSide(e));
    if (other.id_ == fr.id_) continue;
    dfs_setD(g, to, other);
  }
}

void NW::labelDepth() noexcept {
  assert(not empty());
  uint rootId = first_rid();
  assert(rootId);
  assert(verifyOneRoot());

  Node& root = nodeRefCk(rootId);

  for (NI I(*this); I.valid(); ++I) I->lbl_ = UINT_MAX;
  root.lbl_ = 0;
  if (size() == 1) {
    return;
  }
  if (size() == 2) {
    for (NI I(*this); I.valid(); ++I) {
      Node& nd = *I;
      if (nd.id_ != rootId)
        nd.lbl_ = 1;
    }
    return;
  }

  for (int i = root.degree() - 1; i >= 0; i--) {
    Edge& e = edStor_[root.edges_[i]];
    Node& to = ndStor_[root.otherSide(e)];
    dfs_setD(*this, root, to);
  }
}

upair NW::getMinMaxDeg() const noexcept {
  upair dg{0, 0};
  if (empty()) return dg;

  const Node& n0 = nodeRefCk(nids_[0]);
  dg.first = n0.degree();
  dg.second = dg.first;
  for (cNI I(*this); I.valid(); ++I) {
    const Node& nd = *I;
    uint d = nd.degree();
    if (d < dg.first) dg.first = d;
    if (d > dg.second) dg.second = d;
  }
  return dg;
}

upair NW::getMinMaxLbl() const noexcept {
  upair L{0, 0};
  if (empty()) return L;

  const Node& n0 = nodeRefCk(nids_[0]);
  L.first = n0.lbl_;
  L.second = n0.lbl_;
  for (cNI I(*this); I.valid(); ++I) {
    const Node& nd = *I;
    uint x = nd.lbl_;
    if (x < L.first) L.first = x;
    if (x > L.second) L.second = x;
  }
  return L;
}

// ==== DEBUG

void NW::dot_comment(ostream& os, uint16_t dotMode, CStr msg) noexcept {
  if (!dotMode) return;
  if (dotMode == 1) {
    os << s_dotComment;
  } else {
    assert(dotMode == 2);
    os << s_metisComment;
  }
  if (msg) os << msg;
}

bool NW::verifyOneRoot() const noexcept {
  if (empty()) return true;
  assert(!rids_.empty());

  upair cnt = countRoots();
  uint tree_roots = cnt.first;
  uint mark_roots = cnt.second;
  assert(tree_roots == 1);
  assert(mark_roots == 1);

  if (tree_roots != 1) return false;
  if (mark_roots != 1) return false;

  return true;
}

void NW::Node::print(ostream& os) const noexcept {
  using std::endl;
  os << "id:" << id_;
  if (XY::valid())
    os << " (" << x_ << ' ' << y_ << ')';
  else
    os << " ( )";

  os << " key=" << key_ << " deg=" << degree();
  if (isTreeRoot()) {
    if (isFlagRoot())
      os << " R+";
    else
      os << " R-";
  } else if (isFlagRoot()) {
    os << " r-";
  }
  if (sink_flag_) os << " S";
  os_printf(os, " par=%u lbl=%u", par_, lbl_);
  if (not name_.empty())
    os << " nm= " << name_;
  os << endl;
}

void NW::Node::nprint_dot(ostream& os) const noexcept {
  os << "  " << getName();

  os_printf(os, "  // deg= %u;\n", degree());
}

void NW::Edge::eprint_dot(ostream& os, char arrow, const NW& g) const noexcept {
  assert(arrow == '>' or arrow == '-');

  os << g.nodeRef(n1_).getName();

  os << " -" << arrow << ' ';

  os << g.nodeRef(n2_).getName();

  if (inCell_) {
    os << " [ style=dashed";
    os << " ] ";
  }

  os << ";\n";
}

uint NW::print(ostream& os, CStr msg) const noexcept {
  using std::endl;
  if (msg) os << msg << endl;

  if (empty()) {
    os << " empty" << endl;
    return 0;
  }

  uint r0 = first_rid();
  os << "nn=" << size() << " ne=" << numE() << " nr=" << rids_.size() << " root0:" << r0 << endl;

  for (cNI I(*this); I.valid(); ++I) {
    const Node& nd = *I;
    nd.print(os);
  }

  os << " nids_:" << endl;
  for (uint i = 0; i < nids_.size(); i++) os << i << ": " << nid_at(i) << endl;

  return size();
}
uint NW::dump(CStr msg) const noexcept { return print(lout(), msg); }

uint NW::printNodes(ostream& os, CStr msg, uint16_t forDot) const noexcept {
  using std::endl;
  if (msg) {
    os_printf(os, "%s %s\n", s_dotComment, msg);
  }

  if (empty()) {
    os_printf(os, "%s empty\n", s_dotComment);
    return 0;
  }

  for (cNI I(*this); I.valid(); ++I) {
    const Node& nd = *I;
    if (forDot == 1)
      os << s_dotComment << ' ';
    nd.print(os);
  }

  return size();
}
uint NW::dumpNodes(CStr msg) const noexcept { return printNodes(lout(), msg, 0); }

uint NW::printEdges(ostream& os, CStr msg) const noexcept {
  using std::endl;
  if (msg) os << msg << endl;

  uint esz = numE();
  os_printf(os, "  nw:%s  esz= %u\n", nw_name_.c_str(), esz);
  if (esz) {
    for (cEI I(*this); I.valid(); ++I) {
      I->print(os);
      os << endl;
    }
  }
  return esz;
}
uint NW::dumpEdges(CStr msg) const noexcept { return printEdges(lout(), msg); }

uint NW::printSum(ostream& os, uint16_t forDot) const noexcept {
  dot_comment(os, forDot);
  if (empty()) {
    os_printf(os, "  nw:%s  (empty NW)\n)", nw_name_.c_str());
    return 0;
  }
  os_printf(os, "nw:%s\n", nw_name_.c_str());

  upair mmD = getMinMaxDeg();
  upair mmL = getMinMaxLbl();
  upair rcnt = countRoots();

  os_printf(os, "nn= %u  ne= %u  nr= %zu  r0= %u",
            numN(), numE(), rids_.size(), first_rid());

  os_printf(os, "  mm-deg: (%u,%u)", mmD.first, mmD.second);
  os_printf(os, "  mm-lbl: (%u,%u)\n", mmL.first, mmL.second);

  dot_comment(os, forDot);
  os_printf(os, "nr=(%u,%u)\n", rcnt.first, rcnt.second);

  return size();
}

uint NW::printMetis(ostream& os, bool nodeTable) const noexcept {
  using std::endl;
  if (!printSum(os, 2)) return 0;
  if (nodeTable) {
    printNodes(os, nullptr, 2);
    os_printf(os, "%s====\n", s_metisComment);
  }
  os << numN() << ' ' << numE() << endl;

  vecu A;
  for (cNI I(*this); I.valid(); ++I) {
    A.clear();
    const Node& nd = *I;
    nd.getAdj(*this, A);
    os << nd.id_;
    for (uint a : A) os << ' ' << a;
    os << endl;
  }
  return size();
}
uint NW::dumpMetis(bool nodeTable) const noexcept { return printMetis(lout(), nodeTable); }

bool NW::writeMetis(CStr fn, bool nodeTable) const noexcept {
  assert(fn);
  if (!fn or !fn[0]) return false;
  if (empty()) return false;

  bool ok = true;
  uint status = 0;

  std::ofstream f(fn);
  if (f.is_open()) {
    status = printMetis(f, nodeTable);
    if (trace_ >= 3) lprintf(" written %s  status: %u\n\n", fn, status);
    f.close();
  } else {
    if (trace_ >= 2) lprintf("\n [Error Metis] not f.is_open(),  fn: %s\n\n", fn);
    ok = false;
  }

  return (ok and status > 0);
}

uint NW::printDot(ostream& os, CStr nwNm, bool nodeTable) const noexcept {
  if (!printSum(os, 1)) return 0;
  if (nodeTable) {
    printNodes(os, nullptr, 1);
    os_printf(os, "%s====\n", s_dotComment);
  }

  if (!nwNm or !nwNm[0]) {
    if (not nw_name_.empty())
      nwNm = nw_name_.c_str();
  }
  os_printf(os, "digraph %s {\n", (nwNm and nwNm[0]) ? nwNm : "G");

  // os << "  node [rankdir=LR];\n";
  os << "  node [shape = circle];\n";

  for (cNI I(*this); I.valid(); ++I) {
    I->nprint_dot(os);
  }
  os << std::endl;

  for (cEI I(*this); I.valid(); ++I) {
    I->eprint_dot(os, '>', *this);
  }

  os_printf(os, "}\n");
  return size();
}
uint NW::dumpDot(CStr nwNm) const noexcept { return printDot(lout(), nwNm, false); }

bool NW::writeDot(CStr fn, CStr nwNm, bool nodeTable) const noexcept {
  assert(fn);
  if (!fn or !fn[0]) return false;
  if (empty()) return false;

  bool ok = true;
  uint status = 0;

  std::ofstream f(fn);
  if (f.is_open()) {
    status = printDot(f, nwNm, nodeTable);
    if (trace_ >= 3) lprintf(" written %s  status: %u\n\n", fn, status);
    f.close();
  } else {
    if (trace_ >= 2) lprintf("\n [Error Dot-file] not f.is_open(),  fn: %s\n\n", fn);
    ok = false;
  }

  return (ok and status > 0);
}

void NW::beComplete() noexcept {
  assert(selfCheck());
  clearEdges();

  if (size() < 2) return;

  uint maxId = getMaxNid();
  for (uint i = 1; i <= maxId; i++) {
    if (!hasNode(i)) continue;
    const Node& ndi = nodeRef(i);
    for (uint j = i + 1; j <= maxId; j++) {
      if (!hasNode(j)) continue;
      const Node& ndj = nodeRef(j);
      linkNodes(ndi.id_, ndj.id_);
    }
  }
  assert(selfCheck());
}

}

