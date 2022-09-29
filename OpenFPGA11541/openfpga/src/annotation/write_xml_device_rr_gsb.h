#ifndef WRITE_XML_DEVICE_RR_GSB_H
#define WRITE_XML_DEVICE_RR_GSB_H

/********************************************************************
 * Include header files that are required by function declaration
 *******************************************************************/
#include <string>
#include "rr_graph_obj.h"
#include "device_grid.h"
#include "vpr_device_annotation.h"
#include "device_rr_gsb.h"
#include "rr_gsb_writer_option.h"

/********************************************************************
 * Function declaration
 *******************************************************************/

/* begin namespace openfpga */
namespace openfpga {

void write_device_rr_gsb_to_xml(const DeviceGrid& vpr_device_grid,
                                const VprDeviceAnnotation& vpr_device_annotation,
                                const RRGraph& rr_graph,
                                const DeviceRRGSB& device_rr_gsb,
                                const RRGSBWriterOption& options);

} /* end namespace openfpga */

#endif
