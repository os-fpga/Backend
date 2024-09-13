#include "util/nw/Nw_table.h"
#include "file_io/pln_Fio.h"

//
// --- Network NW can be stored in a custom CSV format.
//
//   Basically, a csv-row can describe either a node or an edge.
//   Some fields are common for nodes and edges: id, color, flags, etc.
//   Some fields are node-specific or edge-specific.
//   Usually, the CSV file contains a section of node-records followed by
//   a section of edge-records, though any order of row-records is allowed.
//   In the "nodes" section, the edge-specific fields are empty.
//   In the "edges" section, the node-specific fields are empty.
//
// Column labels:
//   A: "NdEd" node or edge? 'N' for node, 'E' for edge
//   B: Id
//   C: color
//   D: depth
//   E:
//   F: flags
//   G:
//   H:
//   I: n1
//   J: n2
//   K: key
//   L: label
//   M:
//   N: name
//   O:
//   P: parent
//   Q:
//   R:
//   S:
//   T: comment
//   U:
//   V:
//   W: weight
//   X: X-coord
//   Y: Y-coord
//   Z: Z-coord
//
// Empty fields are reserved for future extensions.
// Our CSV reader interprets lines starting with # as comments.
//

namespace pln {

static const char* _colLabels[] = {

  "NdEd",    // A-0
  "Id",      // B-1
  "color",   // C-2
  "depth",   // D-3
  nullptr,   // E-4
  "flags",   // F-5
  nullptr,   // G-6
  nullptr,   // H-7
  "n1",      // I-8
  "n2",      // J-9
  "key",     // K-10
  "label",   // L
  nullptr,   // M
  "name",    // N
  nullptr,   // O
  "parent",  // P
  nullptr,   // Q
  nullptr,   // R
  nullptr,   // S
  "comment", // T
  nullptr,   // U
  nullptr,   // V
  "weight",  // W
      "X",   // X
      "Y",   // Y
      "Z",   // Z

 "impossible1",
 "impossible2",
  nullptr,
  nullptr
};

using std::endl;

NwTable::NwTable(const NW& g) noexcept {
  if (g.size() < 2)
    return;

  size_t nn = g.numN(), ne = g.numN();
  nc_ = 27;
  nr_ = nn + ne;
  beginEdges_ = nn;

  alloc_num_matrix();
  alloc_str_matrix();

  header_.reserve(nc_ + 1);
  header_.resize(26);
  assert(_colLabels[0]);
  header_[0] = _colLabels[0];

  for (uint i = 1; i < 26; i++) {
    CStr label = _colLabels[i];
    if (label)
      header_[i] = label;
  }

  // nodes:
  uint row_cnt = 0;
  for (NW::cNI I(g); I.valid(); ++I, row_cnt++) {
    const NW::Node& ii = *I;
    smat_[row_cnt][0] = "Nd";
    nmat_[row_cnt][1] = ii.id_;
    nmat_[row_cnt][2] = ii.color_;
    nmat_[row_cnt][3] = ii.par_;

    nmat_[row_cnt][10] = ii.key_;
  }

  // edges:
  beginEdges_ = row_cnt;
  for (NW::cEI I(g); I.valid(); ++I, row_cnt++) {
    const NW::Edge& e = *I;
    smat_[row_cnt][0] = "Ed";
    nmat_[row_cnt][1] = e.id_;
    nmat_[row_cnt][2] = e.color_;

    nmat_[row_cnt][8] = e.n1_;
    nmat_[row_cnt][9] = e.n2_;
  }
}

NwTable::~NwTable() {
  free_num_matrix();
  free_str_matrix();
}

void NwTable::alloc_num_matrix() noexcept {
  assert(!nmat_);
  assert(nr_ > 0 && nc_ > 1);

  nmat_ = new size_t*[nr_ + 2];
  for (size_t r = 0; r < nr_ + 2; r++) {
    nmat_[r] = new size_t[nc_ + 2];
    ::memset(nmat_[r], 0, (nc_ + 2) * sizeof(size_t));
  }
}

void NwTable::alloc_str_matrix() noexcept {
  assert(!smat_);
  assert(nr_ > 0 && nc_ > 1);

  smat_ = new string*[nr_ + 2];
  for (size_t r = 0; r < nr_ + 2; r++) {
    smat_[r] = new string[nc_ + 2];
  }
}

void NwTable::free_num_matrix() noexcept {
  if (!nmat_) {
    nr_ = nc_ = 0;
    return;
  }
  if (nr_ < 2) {
    nmat_ = nullptr;
    return;
  }

  for (size_t r = 0; r < nr_ + 2; r++)
    delete[] nmat_[r];

  delete[] nmat_;
  nmat_ = nullptr;
}

void NwTable::free_str_matrix() noexcept {
  if (!smat_) {
    nr_ = nc_ = 0;
    return;
  }
  if (nr_ < 2) {
    smat_ = nullptr;
    return;
  }

  for (size_t r = 0; r < nr_ + 2; r++)
    delete[] smat_[r];

  delete[] smat_;
  smat_ = nullptr;
}

uint NwTable::printMatrix(ostream& os) noexcept {
  if (!nc_ or !nr_ or !nmat_)
    return 0;
  assert(smat_);
  assert(beginEdges_ > 0);

  // nodes:
  for (size_t r = 0; r < beginEdges_; r++) {
    size_t* row_nums = nmat_[r];
    string* row_strs = smat_[r];
    assert(row_nums);
    assert(row_strs);
    for (size_t c = 0; c < nc_; c++) {
      if (c) os << ',';
      if (row_strs[c].empty())
        os << row_nums[c];
      else
        os << row_strs[c];
    }
    os << endl;
  }

  // edges:
  for (size_t r = beginEdges_; r < nr_; r++) {
    size_t* row_nums = nmat_[r];
    string* row_strs = smat_[r];
    assert(row_nums);
    assert(row_strs);
    for (size_t c = 0; c < nc_; c++) {
      if (c) os << ',';
      if (row_strs[c].empty())
        os << row_nums[c];
      else
        os << row_strs[c];
    }
    os << endl;
  }

  return nr_;
}

uint NW::printCsv(ostream& os) const noexcept {
  if (size() < 2)
    return 0;
  for (uint i = 0; i < 26; i++) {
    CStr label = _colLabels[i];
    if (!label) {
      os_printf(os, ",col%u", i);
      continue;
    }
    assert(label[0]);
    if (i) os << ',';
    os << label;
  }
  os << endl;

  NwTable nwt(*this);
  nwt.printMatrix(os);

  return numN() + numE();
}

uint NW::dumpCsv() const noexcept {
  return printCsv(lout());
}

bool NW::writeCsv(CStr fn) const noexcept {
  assert(fn);
  if (!fn or !fn[0]) return false;
  if (empty()) return false;

  bool ok = true;
  uint status = 0;

  std::ofstream f(fn);
  if (f.is_open()) {
    status = printCsv(f);
    if (trace_ >= 3) lprintf(" written %s  status: %u\n\n", fn, status);
    f.close();
  } else {
    if (trace_ >= 2) lprintf("\n [Error Csv] not f.is_open(),  fn: %s\n\n", fn);
    ok = false;
  }

  return (ok and status > 0);
}

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

void NW::Node::print(ostream& os) const noexcept {
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
  os_printf(os, " par=%u lbl=%u prim=%u", par_, lbl_, prim_);
  if (not name_.empty())
    os << " nm= " << name_;
  os << endl;
}

static void be_dot_friendly(char* buf) noexcept {
  assert(buf);
  // replace [] by __
  // replace $ by S
  for (char* p = buf; *p; p++) {
    if (*p == '[' or *p == ']') {
      *p = '_';
      continue;
    }
    if (*p == '$') {
      *p = 'S';
    }
  }

  // shorten too long names
  size_t len = ::strlen(buf);
  if (len > 32) {
    size_t H = str::hashf(buf);
    ::sprintf(buf + 28, "_%zX", H);
  }

  // if buf has . then quote:
  if (::strpbrk(buf, ".:/!")) {
    len = ::strlen(buf);
    buf[len + 1] = 0;
    buf[len + 2] = 0;
    buf[len + 3] = 0;
    buf[len + 4] = 0;
    ::memmove(buf + 1, buf, len);
    buf[0] = '"';
    len++;
    buf[len] = '"';
  }
}

void NW::Node::nprint_dot(ostream& os) const noexcept {
  char name_buf[2048] = {};
  getName(name_buf);
  be_dot_friendly(name_buf);

  char attrib[512] = {};

  if (inp_flag_ or clk_flag_) {

    if (inp_flag_) {
      CStr cs = isRed() ? "red" : "gray";
      ::sprintf(attrib, "[ shape=box, color=%s, style=filled ];", cs);
    }
    else if (clk_flag_) {
      CStr cs = isRed() ? "maroon" : "orange";
      ::sprintf(attrib, "[ shape=ellipse, color=%s, style=filled ];", cs);
      if (prim_ == 18) {
        // I_SERDES
        ::sprintf(attrib, "[ shape=polygon,sides=4,skew=.3, color=%s, style=filled ];", cs);
      }
    }

  }
  else {

    if (isRed()) {
      ::strcpy(attrib, "[ shape=octagon, color=purple, fillcolor=pink, style=filled ];");
    }
    else {
      ::strcpy(attrib, "[ shape=record, style=rounded ];");
      if (viol_flag_) {
        ::strcpy(attrib, "[ shape=doubleoctagon, color=blueviolet, fillcolor=violet, style=filled ];");
      }
    }

  }

  os_printf(os, "%s  %s  // deg= %u;\n",
            name_buf, attrib, degree());
}

void NW::Edge::eprint_dot(ostream& os, char arrow, const NW& g) const noexcept {
  assert(arrow == '>' or arrow == '-');
  char buf[4100] = {};
  const Node& nd1 = g.nodeRef(n1_);
  nd1.getName(buf);
  be_dot_friendly(buf);
  os_printf(os, "%s -%c ", buf, arrow);

  buf[0] = 0;
  const Node& nd2 = g.nodeRef(n2_);
  nd2.getName(buf);
  be_dot_friendly(buf);
  os << buf;

  char attrib[512] = {};
  ::sprintf(attrib, " [%s %s",
      inCell_ ? "style=dashed" : "",
      color_ ? "color=" : ""
      );
  if (color_) {
    if (nd1.viol_flag_ and nd2.viol_flag_)
      ::strcat(attrib, "cyan3");
    else
      ::strcat(attrib, i2color(color_));
  }
  ::strcat(attrib, " ]");

  os_printf(os, "%s;\n", attrib);
}

uint NW::print(ostream& os, CStr msg) const noexcept {
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
  uint numClkNodes = countClockNodes();

  dot_comment(os, forDot);
  os_printf(os, "nn= %u  ne= %u  nr= %zu  r0= %u  #clkn= %u",
            numN(), numE(), rids_.size(), first_rid(), numClkNodes);

  os_printf(os, "  mm-deg: (%u,%u)", mmD.first, mmD.second);
  os_printf(os, "  mm-lbl: (%u,%u)\n", mmL.first, mmL.second);

  uint numNamedNodes = countNamedNodes();
  uint numRedNodes = countRedNodes();
  uint numRedEdges = countRedEdges();

  dot_comment(os, forDot);
  os_printf(os, "#NamedNodes=%u  #RedNodes= %u  #RedEdges= %u\n",
            numNamedNodes, numRedNodes, numRedEdges);

  dot_comment(os, forDot);
  os_printf(os, "nr=(%u,%u)\n", rcnt.first, rcnt.second);

  return size();
}

uint NW::printMetis(ostream& os, bool nodeTable) const noexcept {
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

uint NW::printDot(ostream& os, CStr nwNm, bool nodeTable, bool noDeg0) const noexcept {
  if (!printSum(os, 1)) return 0;
  if (nodeTable) {
    printNodes(os, nullptr, 1);
    os_printf(os, "%s====\n", s_dotComment);
  }

  if (!nwNm or !nwNm[0]) {
    if (not nw_name_.empty())
      nwNm = nw_name_.c_str();
  }
  os_printf(os, "\ndigraph %s {\n", (nwNm and nwNm[0]) ? nwNm : "G");
 	os_printf(os, "  label=\"%s\";\n", (nwNm and nwNm[0]) ? nwNm : "G");

  // os << "  node [rankdir=LR];\n";
  os << "  node [shape = record];\n";

  for (cNI I(*this); I.valid(); ++I) {
    const Node& ii = *I;
    if (noDeg0 and ii.degree() == 0 and not ii.isClk())
      continue;
    ii.nprint_dot(os);
  }
  os << endl;

  for (cEI I(*this); I.valid(); ++I) {
    I->eprint_dot(os, '>', *this);
  }

  os_printf(os, "}\n");
  return size();
}
uint NW::dumpDot(CStr nwNm) const noexcept { return printDot(lout(), nwNm, false, false); }

bool NW::writeDot(CStr fn, CStr nwNm, bool nodeTable, bool noDeg0) const noexcept {
  assert(fn);
  if (!fn or !fn[0]) return false;
  if (empty()) return false;

  bool ok = true;
  uint status = 0;

  std::ofstream f(fn);
  if (f.is_open()) {
    status = printDot(f, nwNm, nodeTable, noDeg0);
    if (trace_ >= 3) lprintf(" written %s  status: %u\n\n", fn, status);
    f.close();
  } else {
    if (trace_ >= 2) lprintf("\n [Error Dot-file] not f.is_open(),  fn: %s\n\n", fn);
    ok = false;
  }

  return (ok and status > 0);
}

static const char* _colorNames[] = {
  "black",
  "red",
  "orange",
  "yellow",
  "green",
  "blue",
  "firebrick",
  "darkslategray",
  "purple",
  "aquamarine3",
  "chocolate4",
  "cadetblue4",
  "darkolivegreen4",
  "darkorchid4",
  "cyan",
  "goldenrod3",
  "black",
  "black",
  "black"
};

CStr NW::i2color(uint i) noexcept {
  if (i == c_Red)
    return "darkorange3";
  return _colorNames[ i % (c_MAX_COLOR + 1) ];
}

}

