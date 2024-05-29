#pragma once
#ifndef _pln_frd_BLIF_FILE_H_e6f4ecdf41a2f915079_
#define _pln_frd_BLIF_FILE_H_e6f4ecdf41a2f915079_

#include "file_readers/pinc_Fio.h"

namespace pln {

using std::string;
using std::vector;

struct BLIF_file : public fio::MMapReader
{
  // Nodes are rooted at output ports.
  // Input ports are leaves.
  // Output ports are roots of fanin cones.
  struct Node {
    uint id_ = 0;    // index+1 in pool
    uint lnum_ = 0;  // line number in blif
    uint parent_ = 0;
    uint depth_ = 0;
    vector<uint> chld_;

    string kw_;            // keyword: .names, .subckt, etc.
    vector<string> data_;  // everything on the line ater kw, tokenized

    // vector<string> mog_outs_; // pin names O, Y, Q are considered to be outputs.
    //                           // MOG-nodes are transformed to bunches of SOG-nodes.

    string out_; // SOG output (real or virtual)

    uint virtualOrigin_ = 0; // node-ID from which this virtual MOG is created

    int16_t is_top_ = 0;  // -1:top input  1:top output
    bool is_mog_ = false;

  public:
    Node() noexcept = default;
    Node(CStr keyword, uint L) noexcept : lnum_(L) {
      if (keyword) kw_ = keyword;
    }

    bool isTopPort() const noexcept { return is_top_ != 0; }
    bool isTopInput() const noexcept { return is_top_ < 0; }
    bool isTopOutput() const noexcept { return is_top_ > 0; }

    bool isRoot() const noexcept { return parent_ == 0; }
    bool isLeaf() const noexcept { return chld_.empty(); }

    bool isMog() const noexcept { return is_mog_; }
    bool isVirtualMog() const noexcept { return is_mog_ and virtualOrigin_ > 0; }

    bool valid() const noexcept { return id_ > 0; }
    void inval() noexcept { id_ = 0; }

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

    bool is_IBUF() const noexcept {
      return data_.size() > 1 and data_.front() == "I_BUF";
    }
    bool is_OBUF() const noexcept {
      return data_.size() > 1 and data_.front() == "O_BUF";
    }

    string firstInputPin() const noexcept;

    CStr cOut() const noexcept { return out_.empty() ? "{e}" : out_.c_str(); }
    CStr cType() const noexcept { return data_.empty() ? "{e}" : data_.front().c_str(); }

    struct CmpOut {
      bool operator()(const Node* a, const Node* b) const noexcept {
        return a->out_ < b->out_;
      }
    };
  }; // Node

  string topModel_;

  vector<string> inputs_;
  vector<string> outputs_;

  size_t inputs_lnum_ = 0, outputs_lnum_ = 0, err_lnum_ = 0;

  string err_msg_;
  uint16_t trace_ = 0;

  bool rd_ok_ = false;
  bool chk_ok_ = false;
  uint num_MOGs_ = 0;

public:
  BLIF_file() noexcept = default;
  BLIF_file(CStr nm) noexcept : fio::MMapReader(nm) {}
  BLIF_file(const string& nm) noexcept : fio::MMapReader(nm) {}

  virtual ~BLIF_file();

  virtual void reset(CStr nm, uint16_t tr = 0) noexcept override;

  bool readBlif() noexcept;
  bool checkBlif() noexcept;

  uint numInputs() const noexcept { return inputs_.size(); }
  uint numOutputs() const noexcept { return outputs_.size(); }
  uint numNodes() const noexcept { return nodePool_.empty() ? 0 : nodePool_.size() - 1; }

  uint printInputs(std::ostream& os, CStr spacer = nullptr) const noexcept;
  uint printOutputs(std::ostream& os, CStr spacer = nullptr) const noexcept;
  uint printNodes(std::ostream& os) const noexcept;

private:
  bool createNodes() noexcept;
  bool linkNodes() noexcept;
  void link(Node& from, Node& to) noexcept;

  Node* findOutputPort(const string& contact) noexcept;
  Node* findInputPort(const string& contact) noexcept;

  Node* findFabricParent(uint of, const string& contact, int& pinIndex) noexcept;  // searches inputs
  Node* findFabricDriver(uint of, const string& contact) noexcept;                 // matches out_

  std::vector<Node> nodePool_;  // nodePool_[0] is a fake node "parent of root"

  std::vector<Node*> topInputs_, topOutputs_;
  std::vector<Node*> fabricNodes_, constantNodes_;
};

}

#endif
