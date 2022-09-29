/*********************************************************************
 * This function includes the writer for generating exchangeable
 * information, in order to interface different simulators   
 ********************************************************************/
#include <cmath>
#include <ctime>
#include <map>
#define MINI_CASE_SENSITIVE
#include "ini.h"

/* Headers from vtrutil library */
#include "vtr_assert.h"
#include "vtr_time.h"

/* Headers from openfpgautil library */
#include "openfpga_scale.h"
#include "openfpga_digest.h"
#include "openfpga_reserved_words.h"

#include "openfpga_naming.h"

#include "simulation_utils.h"

#include "verilog_constants.h"
#include "verilog_simulation_info_writer.h"

/* begin namespace openfpga */
namespace openfpga {

/*********************************************************************
 * Top-level function to write an ini file which contains exchangeable
 * information, in order to interface different Verilog simulators
 ********************************************************************/
void print_verilog_simulation_info(const std::string& ini_fname,
                                   const VerilogTestbenchOption& options,
                                   const std::string& circuit_name,
                                   const std::string& src_dir,
                                   const AtomContext& atom_ctx,
                                   const PlacementContext& place_ctx,
                                   const IoLocationMap& io_location_map,
                                   const ModuleManager& module_manager,
                                   const e_config_protocol_type& config_protocol_type,
                                   const size_t& num_program_clock_cycles,
                                   const int& num_operating_clock_cycles,
                                   const float& prog_clock_freq,
                                   const float& op_clock_freq) {

  std::string timer_message = std::string("Write exchangeable file containing simulation information '") + ini_fname + std::string("'");

  std::string ini_dir_path = format_dir_path(find_path_dir_name(ini_fname));

  /* Create directories */
  create_directory(ini_dir_path);

  /* Start time count */
  vtr::ScopedStartFinishTimer timer(timer_message);

  /* Use default name if user does not provide one */
  VTR_ASSERT(true != ini_fname.empty());

  mINI::INIStructure ini;

  /* Compute simulation time period: full testbench and pre-configured testbench has different length 
   * Currently, we only support the two types. And one of them must be enabled when outputting this file
   */
  float simulation_time_period = 0.;
  if (options.print_top_testbench()) {
    simulation_time_period = find_simulation_time_period(options.time_unit(),
                                                         num_program_clock_cycles,
                                                         1. / prog_clock_freq,
                                                         num_operating_clock_cycles,
                                                         1. / op_clock_freq);
  } else {
    VTR_ASSERT(options.print_preconfig_top_testbench());
    simulation_time_period = find_operating_phase_simulation_time(num_operating_clock_cycles,
                                                                  1. / op_clock_freq,
                                                                  options.time_unit());
  }

  /* Identify the testbench file name depending on the type */
  std::string top_tb_name;
  if (options.print_top_testbench()) {
    top_tb_name = circuit_name + std::string(AUTOCHECK_TOP_TESTBENCH_VERILOG_FILE_POSTFIX);
  } else {
    VTR_ASSERT(options.print_preconfig_top_testbench());
    top_tb_name = circuit_name + std::string(FORMAL_RANDOM_TOP_TESTBENCH_POSTFIX);
  }

  /* Basic information */
  ini["SIMULATION_DECK"]["PROJECTNAME "] = "ModelSimProject";
  ini["SIMULATION_DECK"]["BENCHMARK "] = circuit_name;
  ini["SIMULATION_DECK"]["TOP_TB"] = top_tb_name;
  ini["SIMULATION_DECK"]["SIMTIME "] = std::to_string(simulation_time_period);
  ini["SIMULATION_DECK"]["UNIT "] = unit_to_string(options.time_unit());
  ini["SIMULATION_DECK"]["VERILOG_PATH "] = std::string(src_dir);
  ini["SIMULATION_DECK"]["VERILOG_FILE1"] = std::string(DEFINES_VERILOG_FILE_NAME);
  ini["SIMULATION_DECK"]["VERILOG_FILE2"] = std::string(circuit_name + std::string(TOP_VERILOG_TESTBENCH_INCLUDE_NETLIST_FILE_NAME_POSTFIX));
  ini["SIMULATION_DECK"]["CONFIG_PROTOCOL"] = std::string(CONFIG_PROTOCOL_TYPE_STRING[config_protocol_type]);

  /* Information required by UVM */
  if (CONFIG_MEM_FRAME_BASED == config_protocol_type) {
    /* Find the top_module */
    ModuleId top_module = module_manager.find_module(generate_fpga_top_module_name());
    VTR_ASSERT(true == module_manager.valid_module_id(top_module));

    /* Address port */
    ModulePortId addr_port_id = module_manager.find_module_port(top_module,
                                                                std::string(DECODER_ADDRESS_PORT_NAME));
    BasicPort addr_port = module_manager.module_port(top_module, addr_port_id);

    ini["SIMULATION_DECK"]["ADDR_WIDTH"] = std::to_string(addr_port.get_width());

    /* I/O port */
    std::vector<BasicPort> module_io_ports = module_manager.module_ports_by_type(top_module, ModuleManager::MODULE_GPIO_PORT);
    size_t total_gpio_width = 0;
    for (const BasicPort& module_io_port : module_io_ports) {
      total_gpio_width += module_io_port.get_width();
    }
    ini["SIMULATION_DECK"]["GPIO_WIDTH"] = std::to_string(total_gpio_width);

    /* I/O direction map: 
     * - '0' output 
     * - '1' input
     * For unused ports, by default we assume it is configured as inputs
     * TODO: this should be reworked to be consistent with bitstream
     */
    for (const BasicPort& module_io_port : module_manager.module_ports_by_type(top_module, ModuleManager::MODULE_GPIO_PORT)) {
      std::string io_direction(module_io_port.get_width(), '1');
      for (const AtomBlockId& atom_blk : atom_ctx.nlist.blocks()) {
        /* Bypass non-I/O atom blocks ! */
        if ( (AtomBlockType::INPAD != atom_ctx.nlist.block_type(atom_blk))
          && (AtomBlockType::OUTPAD != atom_ctx.nlist.block_type(atom_blk)) ) {
          continue;
        }

        /* Find the index of the mapped GPIO in top-level FPGA fabric */
        size_t io_index = io_location_map.io_index(place_ctx.block_locs[atom_ctx.lookup.atom_clb(atom_blk)].loc.x,
                                                   place_ctx.block_locs[atom_ctx.lookup.atom_clb(atom_blk)].loc.y,
                                                   place_ctx.block_locs[atom_ctx.lookup.atom_clb(atom_blk)].loc.z,
                                                   module_io_port.get_name());

        if (size_t(-1) == io_index) {
          continue;
        }

        if (AtomBlockType::INPAD == atom_ctx.nlist.block_type(atom_blk)) {
          io_direction[io_index] = '1';
        } else {
          VTR_ASSERT(AtomBlockType::OUTPAD == atom_ctx.nlist.block_type(atom_blk));
          io_direction[io_index] = '0';
        }
   
        std::string io_tag = "IO" + module_io_port.get_name();
    
        /* Organize the vector to string */   
        ini["SIMULATION_DECK"][io_tag] = io_direction;
      }
    }
  }

  mINI::INIFile file(ini_fname);
  file.generate(ini, true);
}

} /* end namespace openfpga */
