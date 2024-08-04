#pragma once
#ifndef _PLN_util_Nw_TABLE_h_88c68a84f705_
#define _PLN_util_Nw_TABLE_h_88c68a84f705_

#include "util/nw/Nw.h"

//  Table representation of Network
//  for CSV-io and for pretty-printing

namespace pln {

using std::string;
using std::vector;

struct NwTable {

  vector<string> header_;

  size_t nr_ = 0, nc_ = 0; // #rows and #columns, without header_

  size_t beginEdges_ = 0;  // 1st row of the edges section

  uint16_t trace_ = 0;

  NwTable() noexcept = default;
  NwTable(const NW& g) noexcept;
  ~NwTable();

  NwTable(const NwTable&) = delete;
  NwTable& operator=(const NwTable&) = delete;

  uint printMatrix(ostream& os) noexcept;

private:
  void alloc_num_matrix() noexcept;
  void alloc_str_matrix() noexcept;
  void free_num_matrix() noexcept;
  void free_str_matrix() noexcept;

  string** smat_ = nullptr;  // string matrix
  size_t** nmat_ = nullptr;  // number matrix
};

}

#endif

