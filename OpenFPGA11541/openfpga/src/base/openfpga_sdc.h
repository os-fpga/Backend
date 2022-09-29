#ifndef OPENFPGA_SDC_H
#define OPENFPGA_SDC_H

/********************************************************************
 * Include header files that are required by function declaration
 *******************************************************************/
#include "command.h"
#include "command_context.h"
#include "openfpga_context.h"

/********************************************************************
 * Function declaration
 *******************************************************************/

/* begin namespace openfpga */
namespace openfpga {

int write_pnr_sdc(const OpenfpgaContext& openfpga_ctx,
                  const Command& cmd, const CommandContext& cmd_context); 

int write_configuration_chain_sdc(const OpenfpgaContext& openfpga_ctx,
                                  const Command& cmd, const CommandContext& cmd_context);

int write_sdc_disable_timing_configure_ports(const OpenfpgaContext& openfpga_ctx,
                                             const Command& cmd, const CommandContext& cmd_context);

int write_analysis_sdc(const OpenfpgaContext& openfpga_ctx,
                       const Command& cmd, const CommandContext& cmd_context);

} /* end namespace openfpga */

#endif
