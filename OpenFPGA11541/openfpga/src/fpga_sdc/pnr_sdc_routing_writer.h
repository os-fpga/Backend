#ifndef PNR_SDC_ROUTING_WRITER_H
#define PNR_SDC_ROUTING_WRITER_H

/********************************************************************
 * Include header files that are required by function declaration
 *******************************************************************/
#include <string>
#include <vector>
#include "module_manager.h"
#include "device_rr_gsb.h"
#include "rr_graph_obj.h"
#include "device_grid.h"
#include "vpr_device_annotation.h"
#include "pnr_sdc_option.h"

/********************************************************************
 * Function declaration
 *******************************************************************/

/* begin namespace openfpga */
namespace openfpga {

void print_pnr_sdc_flatten_routing_constrain_sb_timing(const PnrSdcOption& options,
                                                       const ModuleManager& module_manager,
                                                       const ModuleId& top_module,
                                                       const VprDeviceAnnotation& device_annotation,
                                                       const DeviceGrid& grids,
                                                       const RRGraph& rr_graph,
                                                       const DeviceRRGSB& device_rr_gsb);

void print_pnr_sdc_compact_routing_constrain_sb_timing(const PnrSdcOption& options,
                                                       const ModuleManager& module_manager,
                                                       const ModuleId& top_module,
                                                       const VprDeviceAnnotation& device_annotation,
                                                       const DeviceGrid& grids,
                                                       const RRGraph& rr_graph,
                                                       const DeviceRRGSB& device_rr_gsb);

void print_pnr_sdc_flatten_routing_constrain_cb_timing(const PnrSdcOption& options,
                                                       const ModuleManager& module_manager, 
                                                       const ModuleId& top_module,
                                                       const VprDeviceAnnotation& device_annotation,
                                                       const DeviceGrid& grids,
                                                       const RRGraph& rr_graph,
                                                       const DeviceRRGSB& device_rr_gsb);

void print_pnr_sdc_compact_routing_constrain_cb_timing(const PnrSdcOption& options,
                                                       const ModuleManager& module_manager,
                                                       const ModuleId& top_module,
                                                       const VprDeviceAnnotation& device_annotation,
                                                       const DeviceGrid& grids,
                                                       const RRGraph& rr_graph,
                                                       const DeviceRRGSB& device_rr_gsb);

} /* end namespace openfpga */

#endif
