#pragma once
#ifndef _rsbe_RapidSiliconBackEnd__UTILS_H
#define _rsbe_RapidSiliconBackEnd__UTILS_H

#include <vector>
#include <string>

#include "vpr_types.h"
#include "atom_netlist.h"
#include "clustered_netlist.h"
#include "netlist.h"
#include "vtr_vector.h"

#include "arch_util.h"
#include "physical_types_util.h"
#include "rr_graph_utils.h"


// Class definition for a levelized graph
//
namespace rsbe {

/**
 * @brief The Levelized class represents a graph that has been levelized, allowing efficient traversal and analysis.
 * It provides methods to perform graph levelization, check if the graph has been levelized, and access information about
 * nodes in each logic level and the total number of logic levels.
 */
class Levelized {
public:
    void levelize(); // Function to perform graph levelization

    bool is_levelized() const noexcept { return is_levelized_; }

    int num_logic_levels() const noexcept { return num_logic_levels_; }

    vtr::vector_map<int, std::vector<AtomBlockId>>* level_nodes() noexcept {
        return &level_nodes_;
    }
private:
    vtr::vector_map<int , std::vector<AtomBlockId>> level_nodes_; // Nodes in each level
    bool is_levelized_ = false; // Flag to indicate whether graph has been levelized
    int num_logic_levels_ = 0;  // Number of logic levels in the levelized graph
};

}

#endif
