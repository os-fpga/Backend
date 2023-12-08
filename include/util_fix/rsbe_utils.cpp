#include <cstring>
#include <unordered_set>
#include <regex>
#include <algorithm>
#include <sstream>

#include "vtr_assert.h"
#include "vtr_log.h"
#include "vtr_memory.h"

#include "vpr_types.h"
#include "vpr_error.h"

#include "physical_types.h"
#include "globals.h"
#include "vpr_utils.h"
#include "cluster_placement.h"
#include "place_macro.h"
#include "string.h"
#include "pack_types.h"
#include "device_grid.h"
#include "timing_fail_error.h"
#include "route_constraint.h"
#include "re_cluster_util.h"


namespace rsbe {

// Function to perform graph levelization
void Levelized::levelize() {
    //Levelizes the graph
    const auto& atom_nlist = g_vpr_ctx.atom().nlist;
    const auto atom_lookup = g_vpr_ctx.atom().lookup;

    const auto tg_ = g_vpr_ctx.timing().graph;
    // vtr::vector_map<int , std::vector<AtomBlockId>> level_nodes; //Nodes in each level

    //Allocate space for the first level
    level_nodes_.resize(1);

    //Copy the number of input edges per-node
    //These will be decremented to know when all a node's upstream parents have been
    //placed in a previous level (indicating that the node goes in the current level)
    //
    //Also initialize the first level (nodes with no fanin)
    std::vector<int> node_fanin_remaining(atom_nlist.blocks().size());
    for (auto node_id : atom_nlist.blocks()) {
        size_t node_fanin = 0;
        for (auto in_pin : atom_nlist.block_input_pins(node_id)) {
            auto sink_tnode = atom_lookup.atom_pin_tnode(in_pin);
            auto net_id = atom_nlist.pin_net(in_pin);
            auto source_pin = atom_nlist.net_driver(net_id);
            auto source_tnode = atom_lookup.atom_pin_tnode(source_pin);
            auto edge = tg_->find_edge(source_tnode, sink_tnode);
            if (tg_->edge_disabled(edge)) continue;
            ++node_fanin;
        }
        node_fanin_remaining[size_t(node_id)] = node_fanin;

        //Initialize the first level
        if (node_fanin == 0) {
            level_nodes_[0].push_back(node_id);

            // if (atom_nlist.block_type(node_id) == AtomBlockType::INPAD) {
            //     //We require that all primary inputs (i.e. top-level circuit inputs) to
            //     //be SOURCEs. Due to disconnected nodes we may have non-SOURCEs which
            //     //otherwise appear in the first level.
            //     primary_inputs.push_back(node_id);
            // }
        }
    }

    //Walk the graph from primary inputs (no fanin) to generate a topological sort
    //
    //We inspect the output edges of each node and decrement the fanin count of the
    //target node.  Once the fanin count for a node reaches zero it can be added
    //to the current level.
    int level_idx = 0;

    std::vector<AtomBlockId> last_level;
    vtr::vector<AtomBlockId, bool> visited(atom_nlist.blocks().size(), false);

    bool inserted_node_in_level = true;
    while (inserted_node_in_level) { //If nothing was inserted we are finished
        inserted_node_in_level = false;

        for (const auto node_id : level_nodes_[level_idx]) {
            //Inspect the fanout
            for (auto out_pin : atom_nlist.block_output_pins(node_id)) {
                auto source_tnode = atom_lookup.atom_pin_tnode(out_pin);
                auto net_id = atom_nlist.pin_net(out_pin);
                for (auto sink_pin : atom_nlist.net_sinks(net_id)) {
                    auto sink_tnode = atom_lookup.atom_pin_tnode(sink_pin);
                    auto edge = tg_->find_edge(source_tnode, sink_tnode);
                    if (tg_->edge_disabled(edge))
                        continue;
                    auto blk_id = atom_nlist.pin_block(sink_pin);
                    size_t bid = size_t(blk_id);
                    VTR_ASSERT((node_fanin_remaining[bid] > 0 && !visited[blk_id]) ||
                               (node_fanin_remaining[bid] <= 0 && visited[blk_id]));
                    node_fanin_remaining[bid]--;

                    if (node_fanin_remaining[bid] == 0){
                        if (atom_nlist.block_type(node_id) != AtomBlockType::OUTPAD){
                            level_nodes_.resize(level_idx+2);
                            level_nodes_[level_idx+1].push_back(blk_id);
                            inserted_node_in_level = true;
                            visited[blk_id] = true;
                        }
                        else{
                            VTR_ASSERT(atom_nlist.block_output_pins(blk_id).size() == 0);
                            last_level.push_back(blk_id);
                        }
                    }
                }

            }
        }

        if (inserted_node_in_level) {
            level_idx++;
        }
    }

    //Add the last level to the end of the levelization
    level_nodes_.emplace_back(last_level);
    level_idx++;

    //Add SINK type nodes in the last level to logical outputs
    //Note that we only do this for sinks, since non-sink nodes may end up
    //in the last level (e.g. due to breaking combinational loops)
    // auto is_sink = [this](NodeId id) {
    //     return this->node_type(id) == NodeType::SINK;
    // };
    // std::copy_if(last_level.begin(), last_level.end(), std::back_inserter(logical_outputs_), is_sink);

    //Mark the levelization as valid
    is_levelized_ = true;
    num_logic_levels_ = level_idx;
}

}  // namespace rsbe

