#pragma once
// pln::Par -- Netlist Partitioning
#ifndef _pln__nl_Par_H__2f7c7a4b0bdd_
#define _pln__nl_Par_H__2f7c7a4b0bdd_

#include "util/pln_log.h"
#include "vpr_types.h"

namespace pln {

using std::string;
using std::vector;

class Par {
public:
  Par() noexcept = default;
  ~Par();

  static uint countMolecules(t_pack_molecule* mol_head);

  bool init(t_pack_molecule* mol_head);

  bool do_part(int num_partitions);

  bool split(uint partion_index);

// DATA:
  vector<t_pack_molecule*>  molecules_;
  vector<string>            molNames_;

  uint numMolecules_ = 0;
  uint numNets_ = 0;
  uint numAtoms_ = 0;

  uint splitCnt_ = 0;

  // indexed by splitCnt_:
  vector<string> solverInputs_;  // hmetis files passed to solver
  vector<string> solverOutputs_; // solver output files

  int* atomBlockIdToMolId_ = nullptr;

  uint* partition_array_ = nullptr;

  vector<uint> partitions_;

  uint16_t saved_ltrace_ = 0;

// No copy, No move
  Par(Par&) = delete;
  Par(Par&&) = delete;
  Par& operator=(Par&) = delete;
  Par& operator=(Par&&) = delete;
};

}

#endif
