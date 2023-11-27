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
  Par(t_pack_molecule* molecule_head);
  ~Par();

  void Bi_Partion(uint partion_index);

  bool recursive_partitioning(int num_partitions);

// DATA:
  vector<t_pack_molecule*> molecules_;
  uint numMolecules_ = 0;
  uint numNets_ = 0;
  uint numAtoms_ = 0;
  int* atomBlockIdToMolId_ = nullptr;
  string* molIdToName_ = nullptr;
  uint* partition_array_ = nullptr;
  vector<uint> partitions_;
};

}

#endif
