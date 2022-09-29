#ifndef VERILOG_MODULE_WRITER_H
#define VERILOG_MODULE_WRITER_H

/********************************************************************
 * Include header files that are required by function declaration
 *******************************************************************/
#include <fstream>
#include "module_manager.h"
#include "verilog_port_types.h"

/********************************************************************
 * Function declaration
 *******************************************************************/

/* begin namespace openfpga */
namespace openfpga {

void write_verilog_module_to_file(std::fstream& fp,
                                  const ModuleManager& module_manager,
                                  const ModuleId& module_id,
                                  const bool& use_explicit_port_map,
                                  const e_verilog_default_net_type& default_net_type);

} /* end namespace openfpga */

#endif
