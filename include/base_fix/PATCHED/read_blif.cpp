/**
 * @file
 * @brief BLIF Netlist Loader
 *
 * This loader handles loading a post-technology mapping fully flattened (i.e not
 * hierarchical) netlist in Berkely Logic Interchange Format (BLIF) file, and
 * builds a netlist data structure (AtomNetlist) from it.
 *
 * BLIF text parsing is handled by the blifparse library, while this file is responsible
 * for creating the netlist data structure.
 *
 * The main object of interest is the BlifAllocCallback struct, which implements the
 * blifparse callback interface.  The callback methods are then called when basic blif
 * primitives are encountered by the parser.  The callback methods then create the associated
 * netlist data structures.
 */
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
#include <unordered_set>
#include <cctype> //std::isdigit

#include "blifparse.hpp"
#include "atom_netlist.h"

#include "vtr_assert.h"
#include "vtr_util.h"
#include "vtr_log.h"
#include "vtr_logic.h"
#include "vtr_time.h"
#include "vtr_digest.h"

#include "vpr_types.h"
#include "vpr_error.h"
#include "globals.h"
#include "read_blif_RS.h"
#include "BlifAllocCallback.h"
#include "arch_types.h"
#include "echo_files.h"
#include "hash.h"
#include "edif_blif.hpp"

bool is_string_param(const std::string& param) {
    /* Empty param is considered a string */
    if (param.empty()) {
        return true;
    }

    /* There have to be at least 2 characters (the quotes) */
    if (param.length() < 2) {
        return false;
    }

    /* The first and the last characters must be quotes */
    size_t len = param.length();
    if (param[0] != '"' || param[len - 1] != '"') {
        return false;
    }

    /* There mustn't be any other quotes except for escaped ones */
    for (size_t i = 1; i < (len - 1); ++i) {
        if (param[i] == '"' && param[i - 1] != '\\') {
            return false;
        }
    }

    /* This is a string param */
    return true;
}

bool is_binary_param(const std::string& param) {
    /* Must be non-empty */
    if (param.empty()) {
        return false;
    }

    /* The string must contain only '0' and '1' */
    for (size_t i = 0; i < param.length(); ++i) {
        if (param[i] != '0' && param[i] != '1') {
            return false;
        }
    }

    /* This is a binary word param */
    return true;
}

bool is_real_param(const std::string& param) {
    /* Must be non-empty */
    if (param.empty()) {
        return false;
    }

    /* The string must match the regular expression */
    static const std::regex real_number_expr("[+-]?([0-9]*\\.[0-9]+)|([0-9]+\\.[0-9]*)");
    if (!std::regex_match(param, real_number_expr)) {
        return false;
    }

    /* This is a real number param */
    return true;
}

AtomNetlist read_blif(e_circuit_format circuit_format,
                      const char* blif_file,
                      const t_model* user_models,
                      const t_model* library_models) {
    AtomNetlist netlist;
    std::string netlist_id = vtr::secure_digest_file(blif_file);

    BlifAllocCallback alloc_callback(circuit_format, netlist, netlist_id, user_models, library_models);
    blifparse::blif_parse_filename(blif_file, alloc_callback);

    return netlist;
}
