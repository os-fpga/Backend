//
//  Rapid Silicon addition to read_blif.cpp
//

#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
#include <fstream>
#include <unordered_set>
#include <cctype>
#include <filesystem>

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
#include "edif_blif.hpp"

bool isNestEncrypted = false;
char* intf_mod_str = nullptr;
char* top_mod_str = nullptr;

static char *mod_str = nullptr;

AtomNetlist read_blif_from_vrilog(e_circuit_format circuit_format,
                                  const char *blif_file,
                                  const t_model *user_models,
                                  const t_model *library_models,
                                  t_vpr_setup& vpr_setup,
                                  const char* top_mod)
{
    AtomNetlist netlist;

    return netlist;
}

AtomNetlist read_blif_from_edif(e_circuit_format circuit_format,
                                  const char *blif_file,
                                  const t_model *user_models,
                                  const t_model *library_models)
{
    AtomNetlist netlist;
    std::string netlist_id = vtr::secure_digest_file(blif_file);

    BlifAllocCallback alloc_callback(circuit_format, netlist, netlist_id, user_models, library_models);

    FILE *infile = tmpfile();

    edif_blif(blif_file,infile);

    rewind(infile);
    if (infile != NULL)
    {
        // Parse the file
        std::cout << "The input file is not empty " << std::endl;
        blif_parse_file(infile, alloc_callback, blif_file);
        std::fclose(infile);
    }
    else
    {
        // blif_error_wrap(alloc_callback, 0, "", "Could not open file '%s'.\n", blif_file);
    }

    return netlist;
}

