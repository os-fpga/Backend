/**************************************************
 * This file includes only declarations for 
 * the data structures to describe multiplexer structures
 * Please refer to mux_graph.h for more details
 *************************************************/
#ifndef MUX_GRAPH_FWD_H
#define MUX_GRAPH_FWD_H

#include "vtr_strong_id.h"

/* begin namespace openfpga */
namespace openfpga {


/* Strong Ids for MUXes */
struct mux_node_id_tag;
struct mux_edge_id_tag;
struct mux_mem_id_tag;
struct mux_input_id_tag;
struct mux_output_id_tag;

typedef vtr::StrongId<mux_node_id_tag> MuxNodeId;
typedef vtr::StrongId<mux_edge_id_tag> MuxEdgeId;
typedef vtr::StrongId<mux_mem_id_tag> MuxMemId;
typedef vtr::StrongId<mux_input_id_tag> MuxInputId;
typedef vtr::StrongId<mux_output_id_tag> MuxOutputId;

class MuxGraph;

} /* end namespace openfpga */


#endif 
