#pragma once
// extern "C" API for using with libPlanner_dll.so
#ifndef _nlp__nlpAPI_H__
#define _nlp__nlpAPI_H__

#include "util/pln_log.h"
#include "vpr_types.h"

extern "C" {

uint nlp_count_molecules(t_pack_molecule* mol_head);

}

// function pointer types:
using MolPtr_to_uint_F = uint (*) (t_pack_molecule*);

#endif
