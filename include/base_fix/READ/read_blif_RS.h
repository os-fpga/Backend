//
//  Rapid Silicon addition to read_blif.h
//

#pragma once
#ifndef __READ_BLIF_H_RS_h
#define __READ_BLIF_H_RS_h

#include "read_blif.h"

AtomNetlist read_blif_from_vrilog(e_circuit_format circuit_format,
                      const char* blif_file,
                      const t_model* user_models,
                      const t_model* library_models,
                      t_vpr_setup& vpr_setup,
                      const char* top_mod);

AtomNetlist read_blif_from_edif(e_circuit_format circuit_format,
                      const char* blif_file,
                      const t_model* user_models,
                      const t_model* library_models);

extern bool isNestEncrypted;
extern char* intf_mod_str;
extern char* top_mod_str;

#endif
