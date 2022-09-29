#ifndef VERILOG_DECODERS_H
#define VERILOG_DECODERS_H

/********************************************************************
 * Include header files that are required by function declaration
 *******************************************************************/
#include <fstream>
#include <string>
#include <vector>

#include "circuit_library.h"
#include "mux_graph.h"
#include "mux_library.h"
#include "decoder_library.h"
#include "module_manager.h"
#include "netlist_manager.h"
#include "verilog_port_types.h"
#include "fabric_verilog_options.h"

/********************************************************************
 * Function declaration
 *******************************************************************/

/* begin namespace openfpga */
namespace openfpga {

void print_verilog_submodule_mux_local_decoders(const ModuleManager& module_manager,
                                                NetlistManager& netlist_manager,
                                                const MuxLibrary& mux_lib,
                                                const CircuitLibrary& circuit_lib,
                                                const std::string& submodule_dir,
                                                const std::string& submodule_dir_name,
                                                const FabricVerilogOption& options);

void print_verilog_submodule_arch_decoders(const ModuleManager& module_manager,
                                           NetlistManager& netlist_manager,
                                           const DecoderLibrary& decoder_lib,
                                           const std::string& submodule_dir,
                                           const std::string& submodule_dir_name,
                                           const FabricVerilogOption& options);


} /* end namespace openfpga */

#endif
