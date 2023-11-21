#pragma once
// nl::Par -- Netlist Partitioning
#ifndef __rsbe__nl_Par_H_41abdef2db313ec_
#define __rsbe__nl_Par_H_41abdef2db313ec_

#include "pinc_log.h"
#include "vpr_types.h"

namespace nlp {

class Par {
public:
  Par(t_pack_molecule* molecule_head);
  void Bi_Partion(int partion_index);
  void recursive_partitioning(int num_partitions);

// DATA:
  std::vector<t_pack_molecule*> molecules;
  int numberOfMolecules = 0;
  int numberOfNets = 0;
  int numberOfAtoms = 0;
  int* atomBlockIdToMoleculeId = nullptr;
  std::string* moleculeIdToName = nullptr;
  int* partition_array = nullptr;
  std::vector<int> partitions;
};

}

#endif
