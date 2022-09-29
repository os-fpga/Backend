#ifndef RR_GRAPH_H
#define RR_GRAPH_H

/* Include track buffers or not. Track buffers isolate the tracks from the
 * input connection block. However, they are difficult to lay out in practice,
 * and so are not currently used in commercial architectures. */
#define INCLUDE_TRACK_BUFFERS false

#include "device_grid.h"
#include "clb2clb_directs.h"

enum e_graph_type {
    GRAPH_GLOBAL, /* One node per channel with wire capacity > 1 and full connectivity */
    GRAPH_BIDIR,  /* Detailed bidirectional graph */
    GRAPH_UNIDIR, /* Detailed unidir graph, untilable */
    /* RESEARCH TODO: Get this option debugged */
    GRAPH_UNIDIR_TILEABLE /* Detail unidir graph with wire groups multiples of 2*L */
};
typedef enum e_graph_type t_graph_type;

/* Warnings about the routing graph that can be returned.
 * This is to avoid output messages during a value sweep */
enum {
    RR_GRAPH_NO_WARN = 0x00,
    RR_GRAPH_WARN_FC_CLIPPED = 0x01,
    RR_GRAPH_WARN_CHAN_WIDTH_CHANGED = 0x02
};

void create_rr_graph(const t_graph_type graph_type,
                     const std::vector<t_physical_tile_type>& block_types,
                     const DeviceGrid& grid,
                     t_chan_width nodes_per_chan,
                     const int num_arch_switches,
                     t_det_routing_arch* det_routing_arch,
                     std::vector<t_segment_inf>& segment_inf,
                     const enum e_base_cost_type base_cost_type,
                     const bool trim_empty_channels,
                     const bool trim_obs_channels,
                     const enum e_clock_modeling clock_modeling,
                     const t_direct_inf* directs,
                     const int num_directs,
                     int* Warnings);

void free_rr_graph();

//Returns a brief one-line summary of an RR node
std::string describe_rr_node(const RRNodeId& inode);

void init_fan_in(std::vector<t_rr_node>& L_rr_node, const int num_rr_nodes);

// Sets the spec for the rr_switch based on the arch switch
void load_rr_switch_from_arch_switch(int arch_switch_idx,
                                     int rr_switch_idx,
                                     int fanin,
                                     const float R_minW_nmos,
                                     const float R_minW_pmos);

t_non_configurable_rr_sets identify_non_configurable_rr_sets();

void rr_graph_externals(const std::vector<t_segment_inf>& segment_inf,
                        int max_chan_width,
                        int wire_to_rr_ipin_switch,
                        enum e_base_cost_type base_cost_type);

void alloc_and_load_rr_switch_inf(const int num_arch_switches,
                                  const float R_minW_nmos,
                                  const float R_minW_pmos,
                                  const int wire_to_arch_ipin_switch,
                                  int* wire_to_rr_ipin_switch);

t_clb_to_clb_directs* alloc_and_load_clb_to_clb_directs(const t_direct_inf* directs, const int num_directs, const int delayless_switch);

std::vector<vtr::Matrix<int>> alloc_and_load_actual_fc(const std::vector<t_physical_tile_type>& types,
                                                       const int max_pins,
                                                       const std::vector<t_segment_inf>& segment_inf,
                                                       const int* sets_per_seg_type,
                                                       const int max_chan_width,
                                                       const e_fc_type fc_type,
                                                       const enum e_directionality directionality,
                                                       bool* Fc_clipped);

t_rr_switch_inf create_rr_switch_from_arch_switch(int arch_switch_idx,
                                                  const float R_minW_nmos,
                                                  const float R_minW_pmos);

#endif
