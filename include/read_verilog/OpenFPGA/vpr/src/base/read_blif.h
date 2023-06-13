#ifndef READ_BLIF_H
#define READ_BLIF_H
#include "logic_types.h"
#include "atom_netlist_fwd.h"
#include "read_circuit.h"

bool is_string_param(const std::string& param);
bool is_binary_param(const std::string& param);
bool is_real_param(const std::string& param);
bool is_known_param(const std::string& param);

AtomNetlist read_blif(e_circuit_format circuit_format,
                      const char* blif_file,
                      const t_model* user_models,
                      const t_model* library_models);

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

#endif /*READ_BLIF_H*/
