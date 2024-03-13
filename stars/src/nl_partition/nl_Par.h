#pragma once
// nl::Par -- Netlist Partitioning
#ifndef __rsbe__nl_Par_H_41abdef2db313ec_
#define __rsbe__nl_Par_H_41abdef2db313ec_

#include "pinc_log.h"
#include "vpr_types.h"

namespace nlp {

using std::string;
using std::vector;

class Par {
public:
  Par() noexcept = default;
  ~Par();

  static uint countMolecules(t_pack_molecule* molecule_head);

  bool init(t_pack_molecule* molecule_head);

  bool do_partitioning(int num_partitions);

  bool split(uint partion_index);

// DATA:
  vector<t_pack_molecule*> molecules_;
  uint numMolecules_ = 0;
  uint numNets_ = 0;
  uint numAtoms_ = 0;
  uint splitCnt_ = 0;
  int* atomBlockIdToMolId_ = nullptr;
  string* molIdToName_ = nullptr;
  uint* partition_array_ = nullptr;
  vector<uint> partitions_;

// No copy, No move
  Par(Par&) = delete;
  Par(Par&&) = delete;
  Par& operator=(Par&) = delete;
  Par& operator=(Par&&) = delete;
};

}

#endif
