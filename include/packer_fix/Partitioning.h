#pragma once
#ifndef __rsbe__PARTITIONING_H
#define __rsbe__PARTITIONING_H
#include <iostream>
#include <string>
#include <vector>
#include "vpr_types.h"

class Partitioning {
public:
  Partitioning(t_pack_molecule* molecule_head);
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

#endif
