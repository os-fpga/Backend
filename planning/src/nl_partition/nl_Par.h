#pragma once
// pln::Par -- Netlist Partitioning
#ifndef _pln__nl_Par_H__2f7c7a4b0bdd_
#define _pln__nl_Par_H__2f7c7a4b0bdd_

#include "util/pln_log.h"
#include "vpr_types.h"

namespace pln {

using std::string;
using std::vector;

struct Par {

  struct partition_position {
    int x1 = 0;
    int x2 = 0;
    int y1 = 0;
    int y2 = 0;
    bool is_vert_ = true;  // false means from left to right and true means from top to bottom

    partition_position() noexcept = default;

    bool isVertical() const noexcept { return is_vert_; }
    bool isHorizontal() const noexcept { return ! is_vert_; }
  };

  Par() noexcept = default;
  ~Par();

  static uint countMolecules(t_pack_molecule* mol_head);
  static string get_MtKaHyPar_path();

  bool init(t_pack_molecule* mol_head);

  bool do_part(int mol_per_partition);

  bool split(uint partion_index);

  bool write_constraints_xml() const;

  void cleanup_tmp_files() const;

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

  vector<partition_position*> pp_array_;

  uint16_t saved_ltrace_ = 0;
  string MtKaHyPar_path_;

// No copy, No move
  Par(Par&) = delete;
  Par(Par&&) = delete;
  Par& operator=(Par&) = delete;
  Par& operator=(Par&&) = delete;
};

}

#endif
