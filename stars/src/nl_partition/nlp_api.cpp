#include "nl_partition/nl_Par.h"
#include "nl_partition/nlp_api.h"
#include "file_readers/pinc_Fio.h"
#include "globals.h"
#include "vpr_types.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <thread>

extern "C" {

uint nlp_count_molecules(t_pack_molecule* mol_head) {
  assert(mol_head);
  return pln::Par::countMolecules(mol_head);
}

}

