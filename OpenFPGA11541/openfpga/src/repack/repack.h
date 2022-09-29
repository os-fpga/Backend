#ifndef REPACK_H
#define REPACK_H

/********************************************************************
 * Include header files that are required by function declaration
 *******************************************************************/
#include "vpr_context.h"
#include "vpr_device_annotation.h"
#include "vpr_clustering_annotation.h"
#include "vpr_routing_annotation.h"
#include "vpr_bitstream_annotation.h"
#include "circuit_library.h"
#include "repack_option.h"

/********************************************************************
 * Function declaration
 *******************************************************************/

/* begin namespace openfpga */
namespace openfpga {

void pack_physical_pbs(const DeviceContext& device_ctx,
                       const AtomContext& atom_ctx,
                       const ClusteringContext& clustering_ctx,
                       VprDeviceAnnotation& device_annotation,
                       VprClusteringAnnotation& clustering_annotation,
                       const VprBitstreamAnnotation& bitstream_annotation,
                       const CircuitLibrary& circuit_lib,
                       const RepackOption& options);

} /* end namespace openfpga */

#endif
