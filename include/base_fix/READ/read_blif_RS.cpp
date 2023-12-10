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
#include "simple_netlist.h"
#include "veri_prune.h"
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
    simple_netlist n_l;
    std::string blif_file_ = blif_file;
    std::string arch_file = vpr_setup.FileNameOpts.ArchFile;
    std::stringstream ss(arch_file);
    std::vector<std::string> tokens;
    std::string token;

    while (std::getline(ss, token, '/')) {
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }
    tokens.pop_back();
    std::string directoryName =  tokens.back();

    gb_constructs gb;
    prune_verilog(blif_file, gb, directoryName);
    if (gb.contains_io_prem) {
        n_l.create_ps_json = false;
        mod_str = gb.mod_str;
        std::filesystem::path pathObj(blif_file_);
        blif_file_ = pathObj.filename().string();
        blif_file_.insert(blif_file_.find_last_of("."), "_");
        std::string directory = std::filesystem::current_path().string();
        blif_file_ = directory + "/" +blif_file_;
        std::ofstream new_file(blif_file_.c_str());
        new_file << mod_str;
        new_file.close();

        std::string interface_data_dump_file(blif_file_);
        if (interface_data_dump_file.size() > 2 &&
            'v' == interface_data_dump_file.back() &&
            '.' == interface_data_dump_file[interface_data_dump_file.size() - 2]) {
          interface_data_dump_file.pop_back();
          interface_data_dump_file.pop_back();
        }
        interface_data_dump_file += "interface.json";
        std::ofstream interface_structure_file;
        interface_structure_file.open(interface_data_dump_file);
        interface_structure_file << gb.interface_data_dump;
        interface_structure_file.close();

        intf_mod_str = gb.intf_mod_str;
        top_mod_str = gb.top_mod_str;
    }

    std::string netlist_id = vtr::secure_digest_file(blif_file_.c_str());

    BlifAllocCallback alloc_callback(circuit_format, netlist, netlist_id, user_models, library_models);

    const char* key_file = "private_key.pem";
    if (std::string(top_mod) == "") {
        top_mod = nullptr;
    }

    FILE *infile = tmpfile();
    parse_verilog(blif_file_.c_str(), n_l, key_file, top_mod);
    {
        std::stringstream ss1;
        n_l.b_print(ss1);
        fputs(ss1.str().c_str(), infile);
    }
    rewind(infile);
    if (infile != NULL)
    {
        // Parse the file
        blif_parse_file(infile, alloc_callback, blif_file_.c_str());

        std::fclose(infile);
    }
    else
    {
        // blif_error_wrap(alloc_callback, 0, "", "Could not open file '%s'.\n", blif_file);
    }
        if(n_l.encrypted){
        isNestEncrypted = true;
    }

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

