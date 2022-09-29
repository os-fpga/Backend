/*********************************************************************
 * This file includes top-level function to generate Verilog primitive modules
 * and print them to files
 ********************************************************************/

/* Headers from vtrutil library */
#include "vtr_assert.h"
#include "vtr_log.h"

#include "verilog_submodule_utils.h"
#include "verilog_essential_gates.h"
#include "verilog_decoders.h"
#include "verilog_mux.h"
#include "verilog_lut.h"
#include "verilog_wire.h"
#include "verilog_memory.h"
#include "verilog_shift_register_banks.h"
#include "verilog_writer_utils.h"

#include "verilog_constants.h"
#include "verilog_submodule.h"

/* begin namespace openfpga */
namespace openfpga {

/*********************************************************************
 * Top-level function to generate primitive modules:
 * 1. Logic gates: AND/OR, inverter, buffer and transmission-gate/pass-transistor
 * 2. Routing multiplexers
 * 3. Local encoders for routing multiplexers
 * 4. Wires
 * 5. Configuration memory blocks
 * 6. Verilog template
 ********************************************************************/
void print_verilog_submodule(ModuleManager& module_manager, 
                             NetlistManager& netlist_manager,
                             const MemoryBankShiftRegisterBanks& blwl_sr_banks,
                             const MuxLibrary& mux_lib,
                             const DecoderLibrary& decoder_lib,
                             const CircuitLibrary& circuit_lib, 
                             const std::string& submodule_dir, 
                             const std::string& submodule_dir_name, 
                             const FabricVerilogOption& fpga_verilog_opts) {

  print_verilog_submodule_essentials(const_cast<const ModuleManager&>(module_manager), 
                                     netlist_manager,
                                     submodule_dir,
                                     submodule_dir_name,
                                     circuit_lib,
                                     fpga_verilog_opts);

  /* Decoders for architecture */
  print_verilog_submodule_arch_decoders(const_cast<const ModuleManager&>(module_manager),
                                        netlist_manager, 
                                        decoder_lib, 
                                        submodule_dir,
                                        submodule_dir_name,
                                        fpga_verilog_opts);

  /* Routing multiplexers */
  /* NOTE: local decoders generation must go before the MUX generation!!! 
   *       because local decoders modules will be instanciated in the MUX modules 
   */
  print_verilog_submodule_mux_local_decoders(const_cast<const ModuleManager&>(module_manager),
                                             netlist_manager, 
                                             mux_lib, circuit_lib, 
                                             submodule_dir,
                                             submodule_dir_name,
                                             fpga_verilog_opts);
  print_verilog_submodule_muxes(module_manager, netlist_manager, mux_lib, circuit_lib,
                                submodule_dir,
                                submodule_dir_name,
                                fpga_verilog_opts);
 
  /* LUTes */
  print_verilog_submodule_luts(const_cast<const ModuleManager&>(module_manager),
                               netlist_manager, circuit_lib,
                               submodule_dir,
                               submodule_dir_name,
                               fpga_verilog_opts);

  /* Hard wires */
  print_verilog_submodule_wires(const_cast<const ModuleManager&>(module_manager),
                                netlist_manager, circuit_lib,
                                submodule_dir,
                                submodule_dir_name,
                                fpga_verilog_opts);

  /* Memories */
  print_verilog_submodule_memories(const_cast<const ModuleManager&>(module_manager),
                                   netlist_manager,
                                   mux_lib, circuit_lib, 
                                   submodule_dir,
                                   submodule_dir_name,
                                   fpga_verilog_opts);

  /* Shift register banks */
  print_verilog_submodule_shift_register_banks(const_cast<const ModuleManager&>(module_manager),
                                               netlist_manager,
                                               blwl_sr_banks, 
                                               submodule_dir,
                                               submodule_dir_name,
                                               fpga_verilog_opts);


  /* Dump template for all the modules */
  if (true == fpga_verilog_opts.print_user_defined_template()) { 
    print_verilog_submodule_templates(const_cast<const ModuleManager&>(module_manager),
                                      circuit_lib,
                                      submodule_dir,
                                      fpga_verilog_opts);
  }
}

} /* end namespace openfpga */
