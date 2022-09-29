#ifndef BUILD_DEVICE_MODULE_H
#define BUILD_DEVICE_MODULE_H

/********************************************************************
 * Include header files that are required by function declaration
 *******************************************************************/
#include "vpr_context.h"
#include "openfpga_context.h"
#include "fabric_key.h"

/********************************************************************
 * Function declaration
 *******************************************************************/

/* begin namespace openfpga */
namespace openfpga {

int build_device_module_graph(ModuleManager& module_manager,
                              DecoderLibrary& decoder_lib,
                              MemoryBankShiftRegisterBanks& blwl_sr_banks,
                              const OpenfpgaContext& openfpga_ctx,
                              const DeviceContext& vpr_device_ctx,
                              const bool& frame_view,
                              const bool& compress_routing,
                              const bool& duplicate_grid_pin,
                              const FabricKey& fabric_key,
                              const bool& generate_random_fabric_key,
                              const bool& verbose);

} /* end namespace openfpga */

#endif
