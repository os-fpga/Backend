/**************************************************
 * This file includes member functions for the 
 * data structures in mux_graph.h
 *************************************************/
#include <cmath>
#include <list>
#include <map>
#include <algorithm>

/* Headers from vtrutil library */
#include "vtr_assert.h"
#include "vtr_log.h"

#include "mux_utils.h"
#include "mux_graph.h"

/* Begin namespace openfpga */
namespace openfpga {

/**************************************************
 * Member functions for the class MuxGraph
 *************************************************/

/**************************************************
 * Public Constructors 
 *************************************************/

/* Create an object based on a Circuit Model which is MUX */
MuxGraph::MuxGraph(const CircuitLibrary& circuit_lib, 
                   const CircuitModelId& circuit_model,
                   const size_t& mux_size) {
  /* Build the graph for a given multiplexer model */
  build_mux_graph(circuit_lib, circuit_model, mux_size);
} 

/**************************************************
 * Private Constructors
 *************************************************/
/* Create an empty graph */
MuxGraph::MuxGraph() {
  return;
}

/**************************************************
 * Public Accessors : Aggregates
 *************************************************/
//Accessors
MuxGraph::node_range MuxGraph::nodes() const {
  return vtr::make_range(node_ids_.begin(), node_ids_.end());
}

/* Find the non-input nodes */
std::vector<MuxNodeId> MuxGraph::non_input_nodes() const {
  /* Must be an valid graph */
  VTR_ASSERT_SAFE(valid_mux_graph());
  std::vector<MuxNodeId> node_list;
  
  /* Build the node list, level by level */
  for (size_t level = 0; level < num_node_levels(); ++level) {
    for (size_t node_type = 0; node_type < size_t(NUM_MUX_NODE_TYPES); ++node_type) {
      /* Bypass any nodes which are not OUTPUT and INTERNAL */
      if (size_t(MUX_INPUT_NODE) == node_type) { 
        continue;
      }
      /* Reach here, this is either an OUTPUT or INTERNAL node */ 
      for (auto node : node_lookup_[level][node_type]) { 
        node_list.push_back(node); 
      }
    }
  }

  return node_list;
}

MuxGraph::edge_range MuxGraph::edges() const {
  return vtr::make_range(edge_ids_.begin(), edge_ids_.end());
}

MuxGraph::mem_range MuxGraph::memories() const {
  return vtr::make_range(mem_ids_.begin(), mem_ids_.end());
}

std::vector<size_t> MuxGraph::levels() const {
  std::vector<size_t> graph_levels;
  for (size_t lvl = 0; lvl < num_levels(); ++lvl) {
    graph_levels.push_back(lvl);
  }
  return graph_levels;
}

std::vector<size_t> MuxGraph::node_levels() const {
  std::vector<size_t> graph_levels;
  for (size_t lvl = 0; lvl < num_node_levels(); ++lvl) {
    graph_levels.push_back(lvl);
  }
  return graph_levels;
}

/**************************************************
 * Public Accessors: Data query
 *************************************************/

/* Find the number of inputs in the MUX graph */
size_t MuxGraph::num_inputs() const {
  /* need to check if the graph is valid or not */
  VTR_ASSERT_SAFE(valid_mux_graph());
  /* Sum up the number of INPUT nodes in each level */
  size_t num_inputs = 0;
  for (auto node_per_level : node_lookup_) {
    num_inputs += node_per_level[MUX_INPUT_NODE].size();
  }
  return num_inputs;
}

/* Return the node ids of all the inputs of the multiplexer */
std::vector<MuxNodeId> MuxGraph::inputs() const {
  std::vector<MuxNodeId> input_nodes; 
  /* need to check if the graph is valid or not */
  VTR_ASSERT_SAFE(valid_mux_graph());
  /* Add the input nodes in each level */
  for (auto node_per_level : node_lookup_) {
    input_nodes.insert(input_nodes.end(), node_per_level[MUX_INPUT_NODE].begin(), node_per_level[MUX_INPUT_NODE].end());
  }
  return input_nodes;
}

/* Find the number of outputs in the MUX graph */
size_t MuxGraph::num_outputs() const {
  /* need to check if the graph is valid or not */
  VTR_ASSERT_SAFE(valid_mux_graph());
  /* Sum up the number of INPUT nodes in each level */
  size_t num_outputs = 0;
  for (auto node_per_level : node_lookup_) {
    num_outputs += node_per_level[MUX_OUTPUT_NODE].size();
  }
  return num_outputs;
}

/* Return the node ids of all the outputs of the multiplexer */
std::vector<MuxNodeId> MuxGraph::outputs() const {
  std::vector<MuxNodeId> output_nodes; 
  /* need to check if the graph is valid or not */
  VTR_ASSERT_SAFE(valid_mux_graph());
  /* Add the output nodes in each level */
  for (auto node_per_level : node_lookup_) {
    output_nodes.insert(output_nodes.end(), node_per_level[MUX_OUTPUT_NODE].begin(), node_per_level[MUX_OUTPUT_NODE].end());
  }
  return output_nodes;
}

/* Find the edge between two MUX nodes */
std::vector<MuxEdgeId> MuxGraph::find_edges(const MuxNodeId& from_node, const MuxNodeId& to_node) const {
  std::vector<MuxEdgeId> edges;

  VTR_ASSERT(valid_node_id(from_node));
  VTR_ASSERT(valid_node_id(to_node));

  for (const auto& edge : node_out_edges_[from_node]) {
    for (const auto& cand : edge_sink_nodes_[edge]) {
      if (cand == to_node) {
        /* This is the wanted edge, add to list */
        edges.push_back(edge);
      }
    }
  }

  return edges;
}

/* Find the number of levels in the MUX graph */
size_t MuxGraph::num_levels() const {
  /* need to check if the graph is valid or not */
  VTR_ASSERT_SAFE(valid_mux_graph());
  /* The num_levels by definition excludes the level for outputs, so a deduection is applied */
  return node_lookup_.size() - 1; 
}

/* Find the actual number of levels in the MUX graph */
size_t MuxGraph::num_node_levels() const {
  /* need to check if the graph is valid or not */
  VTR_ASSERT_SAFE(valid_mux_graph());
  return node_lookup_.size(); 
}

/* Find the number of configuration memories in the MUX graph */
size_t MuxGraph::num_memory_bits() const {
  /* need to check if the graph is valid or not */
  VTR_ASSERT_SAFE(valid_mux_graph());
  return mem_ids_.size(); 
}

/* Find the number of SRAMs at a level in the MUX graph */
size_t MuxGraph::num_memory_bits_at_level(const size_t& level) const {
  /* need to check if the graph is valid or not */
  VTR_ASSERT_SAFE(valid_level(level));
  VTR_ASSERT_SAFE(valid_mux_graph());
  return mem_lookup_[level].size(); 
}

/* Return memory id at level */
std::vector<MuxMemId> MuxGraph::memories_at_level(const size_t& level) const {
  /* need to check if the graph is valid or not */
  VTR_ASSERT_SAFE(valid_level(level));
  VTR_ASSERT_SAFE(valid_mux_graph());
  return mem_lookup_[level]; 
}

/* Find the number of nodes at a given level in the MUX graph */
size_t MuxGraph::num_nodes_at_level(const size_t& level) const {
  /* validate the level numbers */
  VTR_ASSERT_SAFE(valid_level(level));
  VTR_ASSERT_SAFE(valid_mux_graph());
  
  size_t num_nodes = 0;
  for (size_t node_type = 0; node_type < size_t(NUM_MUX_NODE_TYPES); ++node_type) {
    num_nodes += node_lookup_[level][node_type].size(); 
  }
  return num_nodes; 
}

/* Find the level of a node */
size_t MuxGraph::node_level(const MuxNodeId& node) const {
  /* validate the node */
  VTR_ASSERT(valid_node_id(node));
  return node_levels_[node]; 
}

/* Find the index of a node at its level */
size_t MuxGraph::node_index_at_level(const MuxNodeId& node) const {
  /* validate the node */
  VTR_ASSERT(valid_node_id(node));
  return node_ids_at_level_[node]; 
}

/* Find the  input edges for a node */
std::vector<MuxEdgeId> MuxGraph::node_in_edges(const MuxNodeId& node) const {
  /* validate the node */
  VTR_ASSERT(valid_node_id(node));
  return node_in_edges_[node];
}

/* Find the input nodes for a edge */
std::vector<MuxNodeId> MuxGraph::edge_src_nodes(const MuxEdgeId& edge) const {
  /* validate the edge */
  VTR_ASSERT(valid_edge_id(edge));
  return edge_src_nodes_[edge];
}

/* Find the mem that control the edge */
MuxMemId MuxGraph::find_edge_mem(const MuxEdgeId& edge) const {
  /* validate the edge */
  VTR_ASSERT(valid_edge_id(edge));
  return edge_mem_ids_[edge];
}

/* Identify if the edge is controlled by the inverted output of a mem */
bool MuxGraph::is_edge_use_inv_mem(const MuxEdgeId& edge) const {
  /* validate the edge */
  VTR_ASSERT(valid_edge_id(edge));
  return edge_inv_mem_[edge];
}

/* Find the sizes of each branch of a MUX */
std::vector<size_t> MuxGraph::branch_sizes() const {
  std::vector<size_t> branch;
  /* Visit each internal nodes/output nodes and find the the number of incoming edges */
  for (auto node : node_ids_ ) {
    /* Bypass input nodes */
    if ( (MUX_OUTPUT_NODE != node_types_[node]) 
      && (MUX_INTERNAL_NODE != node_types_[node]) ) {
      continue;
    }

    size_t branch_size = node_in_edges_[node].size();

    /* make sure the branch size is valid */
    VTR_ASSERT_SAFE(valid_mux_implementation_num_inputs(branch_size));

    /* Nodes with the same number of incoming edges, indicate the same size of branch circuit */
    std::vector<size_t>::iterator it; 
    it = std::find(branch.begin(), branch.end(), branch_size);
    /* if already exists a branch with the same size, skip updating the vector */
    if (it != branch.end()) {
      continue;
    }
    branch.push_back(branch_size);
  }

  /* Sort the branch by size */
  std::sort(branch.begin(), branch.end());

  return branch;
}

/* Find the sizes of each branch of a MUX at a given level */
std::vector<size_t> MuxGraph::branch_sizes(const size_t& level) const {
  std::vector<size_t> branch;
  /* Visit each internal nodes/output nodes and find the the number of incoming edges */
  for (auto node : node_ids_ ) {
    /* Bypass input nodes */
    if ( (MUX_OUTPUT_NODE != node_types_[node]) 
      && (MUX_INTERNAL_NODE != node_types_[node]) ) {
      continue;
    }
    /* Bypass nodes that is not at the level */
    if ( level != node_levels_[node]) {
      continue;
    }

    size_t branch_size = node_in_edges_[node].size();

    /* make sure the branch size is valid */
    VTR_ASSERT_SAFE(valid_mux_implementation_num_inputs(branch_size));

    /* Nodes with the same number of incoming edges, indicate the same size of branch circuit */
    std::vector<size_t>::iterator it; 
    it = std::find(branch.begin(), branch.end(), branch_size);
    /* if already exists a branch with the same size, skip updating the vector */
    if (it != branch.end()) {
      continue;
    }
    branch.push_back(branch_size);
  }

  /* Sort the branch by size */
  std::sort(branch.begin(), branch.end());

  return branch;
}


/* Build a subgraph from the given node
 * The strategy is very simple, we just 
 * extract a 1-level graph from here
 */
MuxGraph MuxGraph::subgraph(const MuxNodeId& root_node) const {
  /* Validate the node */
  VTR_ASSERT_SAFE(this->valid_node_id(root_node));

  /* Generate an empty graph */
  MuxGraph mux_graph;

  /* A map to record node-to-node mapping from origin graph to subgraph */
  std::map<MuxNodeId, MuxNodeId> node2node_map;

  /* A map to record edge-to-edge mapping from origin graph to subgraph */
  std::map<MuxEdgeId, MuxEdgeId> edge2edge_map;

  /* Add output nodes to subgraph */
  MuxNodeId to_node_subgraph = mux_graph.add_node(MUX_OUTPUT_NODE);
  mux_graph.node_levels_[to_node_subgraph] = 1;
  mux_graph.node_ids_at_level_[to_node_subgraph] = 0; 
  mux_graph.node_output_ids_[to_node_subgraph] = MuxOutputId(0);
  /* Update the node-to-node map */
  node2node_map[root_node] = to_node_subgraph;

  /* Add input nodes and edges to subgraph */
  size_t input_cnt = 0;
  for (auto edge_origin : this->node_in_edges_[root_node]) {
    VTR_ASSERT_SAFE(1 == edge_src_nodes_[edge_origin].size());
    /* Add nodes */
    MuxNodeId from_node_origin = this->edge_src_nodes_[edge_origin][0];
    MuxNodeId from_node_subgraph = mux_graph.add_node(MUX_INPUT_NODE);
    /* Configure the nodes */
    mux_graph.node_levels_[from_node_subgraph] = 0;
    mux_graph.node_ids_at_level_[from_node_subgraph] = input_cnt; 
    mux_graph.node_input_ids_[from_node_subgraph] = MuxInputId(input_cnt);
    input_cnt++;
    /* Update the node-to-node map */
    node2node_map[from_node_origin] = from_node_subgraph;

    /* Add edges */
    MuxEdgeId edge_subgraph = mux_graph.add_edge(node2node_map[from_node_origin], node2node_map[root_node]);
    edge2edge_map[edge_origin] = edge_subgraph; 
    /* Configure edges */
    mux_graph.edge_models_[edge_subgraph] = this->edge_models_[edge_origin];
    mux_graph.edge_inv_mem_[edge_subgraph] = this->edge_inv_mem_[edge_origin];
  } 

  /* A map to record mem-to-mem mapping from origin graph to subgraph */
  std::map<MuxMemId, MuxMemId> mem2mem_map;

  /* Add memory bits and configure edges */
  for (auto edge_origin : this->node_in_edges_[root_node]) {
    MuxMemId mem_origin = this->edge_mem_ids_[edge_origin];
    /* Try to find if the mem is already in the list */
    std::map<MuxMemId, MuxMemId>::iterator it = mem2mem_map.find(mem_origin);
    if (it != mem2mem_map.end()) {
      /* Found, we skip mem addition. But make sure we have a valid one */
      VTR_ASSERT_SAFE(MuxMemId::INVALID() != mem2mem_map[mem_origin]);
      /* configure the edge */
      mux_graph.edge_mem_ids_[edge2edge_map[edge_origin]] = mem2mem_map[mem_origin];
      continue;
    }
    /* Not found, we add a memory bit and record in the mem-to-mem map */
    MuxMemId mem_subgraph = mux_graph.add_mem();
    mux_graph.set_mem_level(mem_subgraph, 0);
    mem2mem_map[mem_origin] = mem_subgraph;
    /* configure the edge */
    mux_graph.edge_mem_ids_[edge2edge_map[edge_origin]] = mem_subgraph;
  }

  /* Since the graph is finalized, it is time to build the fast look-up */
  mux_graph.build_node_lookup();
  mux_graph.build_mem_lookup();

  return mux_graph; 
}

/* Generate MUX graphs for its branches
 * Similar to the branch_sizes() method,
 * we search all the internal nodes and 
 * find out what are the input sizes of 
 * the branches. 
 * Then we extract unique subgraphs and return 
 */
std::vector<MuxGraph> MuxGraph::build_mux_branch_graphs() const {
  std::map<size_t, bool> branch_done; /* A map showing the status of graph generation */

  std::vector<MuxGraph> branch_graphs;

  /* Visit each internal nodes/output nodes and find the the number of incoming edges */
  for (auto node : node_ids_ ) {
    /* Bypass input nodes */
    if ( (MUX_OUTPUT_NODE != node_types_[node]) 
      && (MUX_INTERNAL_NODE != node_types_[node]) ) {
      continue;
    }

    size_t branch_size = node_in_edges_[node].size();

    /* make sure the branch size is valid */
    VTR_ASSERT_SAFE(valid_mux_implementation_num_inputs(branch_size));

    /* check if the branch have been done in sub-graph extraction! */
    std::map<size_t, bool>::iterator it = branch_done.find(branch_size);
    /* if it is done, we can skip */
    if (it != branch_done.end()) {
      VTR_ASSERT(branch_done[branch_size]);
      continue;
    }

    /* Generate a subgraph and push back */
    branch_graphs.push_back(subgraph(node));

    /* Mark it is done for this branch size */
    branch_done[branch_size] = true;
  }

  return branch_graphs;
} 

/* Get the input id of a given node */
MuxInputId MuxGraph::input_id(const MuxNodeId& node_id) const {
  /* Validate node id */
  VTR_ASSERT(valid_node_id(node_id));
  /* Must be an input */
  VTR_ASSERT(MUX_INPUT_NODE == node_types_[node_id]);
  return node_input_ids_[node_id];
}

/* Identify if the node is an input of the MUX */
bool MuxGraph::is_node_input(const MuxNodeId& node_id) const {
  /* Validate node id */
  VTR_ASSERT(true == valid_node_id(node_id));
  return (MUX_INPUT_NODE == node_types_[node_id]);
}

/* Get the output id of a given node */
MuxOutputId MuxGraph::output_id(const MuxNodeId& node_id) const {
  /* Validate node id */
  VTR_ASSERT(valid_node_id(node_id));
  /* Must be an output */
  VTR_ASSERT(MUX_OUTPUT_NODE == node_types_[node_id]);
  return node_output_ids_[node_id];
}

/* Identify if the node is an output of the MUX */
bool MuxGraph::is_node_output(const MuxNodeId& node_id) const {
  /* Validate node id */
  VTR_ASSERT(true == valid_node_id(node_id));
  return (MUX_OUTPUT_NODE == node_types_[node_id]);
}

/* Get the node id of a given input */
MuxNodeId MuxGraph::node_id(const MuxInputId& input_id) const {
  /* Use the node_lookup to accelerate the search */
  for (const auto& lvl : node_lookup_) {
    for (const auto& cand_node : lvl[MUX_INPUT_NODE]) {
      if (input_id == node_input_ids_[cand_node]) {
        return cand_node; 
      }
    }
  } 

  return MuxNodeId::INVALID();
}

/* Get the node id of a given output */
MuxNodeId MuxGraph::node_id(const MuxOutputId& output_id) const {
  /* Use the node_lookup to accelerate the search */
  for (const auto& lvl : node_lookup_) {
    for (const auto& cand_node : lvl[MUX_OUTPUT_NODE]) {
      if (output_id == node_output_ids_[cand_node]) {
        return cand_node; 
      }
    }
  } 

  return MuxNodeId::INVALID();
}


/* Get the node id w.r.t. the node level and node_index at the level
 * Return an invalid value if not found 
 */
MuxNodeId MuxGraph::node_id(const size_t& node_level, const size_t& node_index_at_level) const {
  /* Ensure we have a valid node_look-up */
  VTR_ASSERT_SAFE(valid_node_lookup());

  MuxNodeId ret_node = MuxNodeId::INVALID(); 

  /* Search in the fast look up */
  if (node_level >= node_lookup_.size()) {
    return ret_node; 
  }
  
  size_t node_cnt = 0;
  /* Node level is valid, search in the node list */
  for (const auto& nodes_by_type : node_lookup_[node_level]) {
    /* Search the node_index_at_level of each node */
    for (const auto& node : nodes_by_type) {
      if (node_index_at_level != node_ids_at_level_[node]) {
        continue;
      } 
      /* Find the node, assign value and update the counter */
      ret_node = node;
      node_cnt++; 
    }
  }

  /* We should either find a node or nothing */
  VTR_ASSERT((0 == node_cnt) || (1 == node_cnt));

  return ret_node;
}

/* Decode memory bits based on an input id and an output id */
vtr::vector<MuxMemId, bool> MuxGraph::decode_memory_bits(const MuxInputId& input_id,
                                                         const MuxOutputId& output_id) const {
  /* initialize the memory bits: TODO: support default value */ 
  vtr::vector<MuxMemId, bool> mem_bits(mem_ids_.size(), false);

  /* valid the input and output */
  VTR_ASSERT_SAFE(valid_input_id(input_id));
  VTR_ASSERT_SAFE(valid_output_id(output_id));

  /* Mark all the nodes as not visited */
  vtr::vector<MuxNodeId, bool> visited(nodes().size(), false); 

  /* Create a queue for Breadth-First Search */ 
  std::list<MuxNodeId> queue; 

  /* Mark the input node as visited and enqueue it */
  visited[node_id(input_id)] = true; 
  queue.push_back(node_id(input_id)); 

  /* Create a flag to indicate if the route is success or not */
  bool route_success = false;

  while(!queue.empty()) { 
    /* Dequeue a mux node from queue, 
     * we will walk through all the fan-in of this node in this loop
     */
    MuxNodeId node_to_expand = queue.front(); 
    queue.pop_front(); 
                                                                                                               /* Get all fan-in nodes of the dequeued node
     * If the node has not been visited,  
     * then mark it visited and enqueue it
     */
    VTR_ASSERT_SAFE (1 == node_out_edges_[node_to_expand].size());
    MuxEdgeId edge = node_out_edges_[node_to_expand][0];

    /* Configure the mem bits: 
     * if inv_mem is enabled, it means 0 to enable this edge 
     * otherwise, it is 1 to enable this edge
     */
    MuxMemId mem = edge_mem_ids_[edge];
    VTR_ASSERT_SAFE (valid_mem_id(mem));
    if (true == edge_inv_mem_[edge]) {
      mem_bits[mem] = false;
    } else {
      mem_bits[mem] = true;
    }

    /* each edge must have 1 fan-out */
    VTR_ASSERT_SAFE (1 == edge_sink_nodes_[edge].size());

    /* Get the fan-out node */
    MuxNodeId next_node = edge_sink_nodes_[edge][0]; 

    /* If next node is the output node we want, we can finish here */
    if (next_node == node_id(output_id)) {
      route_success = true;
      break;
    }

    /* Add next node to the queue if not visited yet */
    if (false == visited[next_node]) { 
       visited[next_node] = true; 
       queue.push_back(next_node); 
    }
  }

  /* Routing must be success! */
  VTR_ASSERT(true == route_success);

  return mem_bits;
}

/* Find the input node that the memory bits will route an output node to 
 * This function backward propagate from the output node to an input node
 * assuming the memory bits are applied  
 */
MuxInputId MuxGraph::find_input_node_driven_by_output_node(const std::map<MuxMemId, bool>& memory_bits,
                                                           const MuxOutputId& output_id) const {
  /* Ensure that the memory bits fit the size of memory bits in this MUX */ 
  VTR_ASSERT(memory_bits.size() == mem_ids_.size());

  /* valid the output */
  VTR_ASSERT_SAFE(valid_output_id(output_id));

  /* Start from the output node */
  /* Mark all the nodes as not visited */
  vtr::vector<MuxNodeId, bool> visited(nodes().size(), false); 

  /* Create a queue for Breadth-First Search */ 
  std::list<MuxNodeId> queue; 

  /* Mark the output node as visited and enqueue it */
  visited[node_id(output_id)] = true; 
  queue.push_back(node_id(output_id)); 

  /* Record the destination input id */
  MuxInputId des_input_id = MuxInputId::INVALID();

  while(!queue.empty()) { 
    /* Dequeue a mux node from queue, 
     * we will walk through all the fan-in of this node in this loop
     */
    MuxNodeId node_to_expand = queue.front(); 
    queue.pop_front(); 
                                                                                                               /* Get all fan-in nodes of the dequeued node
     * If the node has not been visited,  
     * then mark it visited and enqueue it
     */
    MuxEdgeId next_edge = MuxEdgeId::INVALID(); 
    for (const MuxEdgeId& edge : node_in_edges_[node_to_expand]) {
      /* Configure the mem bits and find the edge that will propagate the signal 
       * if inv_mem is enabled, it means false to enable this edge 
       * otherwise, it is true to enable this edge
       */
      MuxMemId mem = edge_mem_ids_[edge];
      VTR_ASSERT_SAFE (valid_mem_id(mem));
      if (edge_inv_mem_[edge] ==  !memory_bits.at(mem)) { 
        next_edge = edge;
        break;
      }
    }
    /* We must have a valid next edge */
    VTR_ASSERT(MuxEdgeId::INVALID() != next_edge);

    /* each edge must have 1 fan-out */
    VTR_ASSERT_SAFE (1 == edge_src_nodes_[next_edge].size());

    /* Get the fan-in node */
    MuxNodeId next_node = edge_src_nodes_[next_edge][0]; 

    /* If next node is an input node, we can finish here */
    if (true == is_node_input(next_node)) {
      des_input_id = input_id(next_node);
      break;
    }

    /* Add next node to the queue if not visited yet */
    if (false == visited[next_node]) { 
       visited[next_node] = true; 
       queue.push_back(next_node); 
    }
  }

  /* Routing must be success! */
  VTR_ASSERT(MuxInputId::INVALID() != des_input_id);

  return des_input_id;
}

/**************************************************
 * Private mutators: basic operations 
 *************************************************/
/* Add a unconfigured node to the MuxGraph */
MuxNodeId MuxGraph::add_node(const enum e_mux_graph_node_type& node_type) {
  MuxNodeId node = MuxNodeId(node_ids_.size());
  /* Push to the node list */
  node_ids_.push_back(node);
  /* Resize the other node-related vectors */
  node_types_.push_back(node_type);
  node_input_ids_.push_back(MuxInputId::INVALID());
  node_output_ids_.push_back(MuxOutputId::INVALID());
  node_levels_.push_back(-1);
  node_ids_at_level_.push_back(-1);
  node_in_edges_.emplace_back();
  node_out_edges_.emplace_back();

  return node;
}

/* Add a edge connecting two nodes  */
MuxEdgeId MuxGraph::add_edge(const MuxNodeId& from_node, const MuxNodeId& to_node) {
  MuxEdgeId edge = MuxEdgeId(edge_ids_.size());
  /* Push to the node list */
  edge_ids_.push_back(edge);
  /* Resize the other node-related vectors */
  edge_models_.push_back(CircuitModelId::INVALID());
  edge_mem_ids_.push_back(MuxMemId::INVALID());
  edge_inv_mem_.push_back(false);

  /* update the edge-node connections */
  VTR_ASSERT(valid_node_id(from_node));
  edge_src_nodes_.emplace_back();
  edge_src_nodes_[edge].push_back(from_node);
  node_out_edges_[from_node].push_back(edge);

  VTR_ASSERT(valid_node_id(to_node));
  edge_sink_nodes_.emplace_back();
  edge_sink_nodes_[edge].push_back(to_node);
  node_in_edges_[to_node].push_back(edge);

  return edge;
}

/* Add a memory bit to the MuxGraph */
MuxMemId MuxGraph::add_mem() {
  MuxMemId mem = MuxMemId(mem_ids_.size());
  /* Push to the node list */
  mem_ids_.push_back(mem);
  mem_levels_.push_back(size_t(-1));
  /* Resize the other node-related vectors */

  return mem;
}

/* Configure the level of a memory */
void MuxGraph::set_mem_level(const MuxMemId& mem, const size_t& level) {
  /* Make sure we have valid edge and mem */
  VTR_ASSERT( valid_mem_id(mem) );

  mem_levels_[mem] = level;
}

/* Link an edge to a memory bit */
void MuxGraph::set_edge_mem_id(const MuxEdgeId& edge, const MuxMemId& mem) {
  /* Make sure we have valid edge and mem */
  VTR_ASSERT( valid_edge_id(edge) && valid_mem_id(mem) );

  edge_mem_ids_[edge] = mem;
}

/**************************************************
 * Private mutators: graph builders  
 *************************************************/

/* Build a graph for a multi-level multiplexer implementation
 * support both generic multi-level and tree-like multiplexers
 *
 * a N:1 multi-level MUX 
 * ----------------------
 * 
 *  input_node --->+
 *                 |
 *  input_node --->|
 *                 |--->+
 *             ... |    |
 *                 |    |
 *  input_node --->+    |---> ...
 *                      |
 *             ...  --->+        --->+
 *                                   |
 *                  ...          ... |---> output_node
 *                                   |
 *             ...  --->+        --->+
 *                      |
 *  input_node --->+    |---> ...
 *                 |    |
 *  input_node --->|    |
 *                 |--->+
 *             ... |     
 *                 |     
 *  input_node --->+     
 *
 * tree-like multiplexer graph will look like:
 * --------------------------------------------
 *
 *   input_node --->+
 *                  |--->+
 *   input_node --->+    |--->  ...
 *                       |
 *                   --->+         --->+
 *     ...            ...  ...         |----> output_node
 *              ...  --->+         --->+
 *                       |---> ...
 *   input_node --->+    |
 *                  |--->+
 *   input_node --->+    
 *
 */
void MuxGraph::build_multilevel_mux_graph(const size_t& mux_size, 
                                          const size_t& num_levels, const size_t& num_inputs_per_branch,
                                          const CircuitModelId& pgl_model) {
  /* Make sure mux_size for each branch is valid */
  VTR_ASSERT(valid_mux_implementation_num_inputs(num_inputs_per_branch));

  /* In regular cases, there is 1 mem bit for each input of a branch */
  size_t num_mems_per_level = num_inputs_per_branch;
  /* For 2-input branch, only 1 mem bit is needed for each level! */
  if (2 == num_inputs_per_branch) { 
    num_mems_per_level = 1; 
  }
  /* Number of memory bits is definite, add them */
  for (size_t ilvl = 0; ilvl < num_levels; ++ilvl) {
    for (size_t imem = 0; imem < num_mems_per_level; ++imem) {
      MuxMemId mem = add_mem();
      mem_levels_[mem] = ilvl;
    }
  }

  /* Create a fast node lookup locally.
   * Only used for building the graph
   * it sorts the nodes by levels and ids at each level
   */
  std::vector<std::vector<MuxNodeId>> node_lookup; /* [num_levels][num_nodes_per_level] */
  node_lookup.resize(num_levels + 1); 

  /* Number of outputs is definite, add and configure */
  MuxNodeId output_node = add_node(MUX_OUTPUT_NODE);
  node_levels_[output_node] = num_levels; 
  node_ids_at_level_[output_node] = 0; 
  node_output_ids_[output_node] = MuxOutputId(0); 
  /* Update node lookup */
  node_lookup[num_levels].push_back(output_node);

  /* keep a list of node ids which can be candidates for input nodes */
  std::vector<MuxNodeId> input_node_ids;
 
  /* Add internal nodes level by level, 
   * we start from the last level, following a strategy like tree growing
   */
  for (size_t lvl = num_levels - 1; ; --lvl) {
    /* Expand from the existing nodes 
     * Last level should expand from output_node 
     * Other levels will expand from internal nodes!
     */
    size_t node_cnt_per_level = 0; /* A counter to record node indices at each level */
    for (MuxNodeId seed_node : node_lookup[lvl + 1]) {
      /* Add a new node and connect to seed_node, until we reach the num_inputs_per_branch */
      for (size_t i = 0; i < num_inputs_per_branch; ++i) {
        /* We deposite a type of INTERNAL_NODE, 
         * later it will be configured to INPUT if it is in the input list 
         */
        MuxNodeId expand_node = add_node(MUX_INTERNAL_NODE); 

        /* Node level is deterministic */
        node_levels_[expand_node] = lvl; 
        node_ids_at_level_[expand_node] = node_cnt_per_level; 
        /* update level node counter */
        node_cnt_per_level++;
       
        /* Create an edge and connect the two nodes */
        MuxEdgeId edge = add_edge(expand_node, seed_node); 
        /* Configure the edge */
        edge_models_[edge] = pgl_model;

        /* Memory id depends on the level and offset in the current branch 
         * if number of inputs per branch is 2, it indicates a tree-like multiplexer, 
         * every two edges will share one memory bit 
         * otherwise, each edge corresponds to a memory bit
         */
        
        if ( 2 == num_inputs_per_branch) {
          MuxMemId mem_id = MuxMemId(lvl);
          set_edge_mem_id(edge, mem_id);
          /* If this is a second edge in the branch, we will assign it to an inverted edge */
          if (0 != i % num_inputs_per_branch) {
            edge_inv_mem_[edge] = true;
          }
        } else {
          MuxMemId mem_id = MuxMemId( lvl * num_inputs_per_branch + i );
          set_edge_mem_id(edge, mem_id);
        }

        /* Update node lookup */
        node_lookup[lvl].push_back(expand_node);

        /* Push the node to input list, and then remove the seed_node from the list */ 
        input_node_ids.push_back(expand_node);
        /* Remove the node if the seed node is the list */
        std::vector<MuxNodeId>::iterator it = find(input_node_ids.begin(), input_node_ids.end(), seed_node);
        if (it != input_node_ids.end()) {
          input_node_ids.erase(it);
        }

        /* Check the number of input nodes, if already meet the demand, we can finish here */
        if (mux_size != input_node_ids.size()) {
          continue; /* We need more inputs, keep looping */
        }

        /* The graph is done, we configure the input nodes and then we can return */
        /* We must be in level 0 !*/
        VTR_ASSERT( 0 == lvl ) ;
        for (MuxNodeId input_node : input_node_ids) {
          node_types_[input_node] = MUX_INPUT_NODE;
        }

        /* Sort the nodes by the levels and offset */
        size_t input_cnt = 0;
        for (auto lvl_nodes : node_lookup) {
          for (MuxNodeId cand_node : lvl_nodes) {
            if (MUX_INPUT_NODE != node_types_[cand_node]) {
               continue;
            }
            /* Update the input node ids */
            node_input_ids_[cand_node] = MuxInputId(input_cnt);
            /* Update the counter */
            input_cnt++;
          }
        }
        /* Make sure we visited all the inputs in the cache */
        VTR_ASSERT(input_cnt == input_node_ids.size());
        /* Finish building the graph for a multi-level multiplexer */
        return;
      }
    }
  } 
  /* Finish building the graph for a multi-level multiplexer */
} 

/* Build the graph for a given one-level multiplexer implementation 
 * a N:1 one-level MUX
 * 
 *  input_node --->+
 *                 |
 *  input_node --->|
 *                 |--> output_node
 *             ... |
 *                 |
 *  input_node --->+
 */
void MuxGraph::build_onelevel_mux_graph(const size_t& mux_size, 
                                        const CircuitModelId& pgl_model) {
  /* Make sure mux_size is valid */
  VTR_ASSERT(valid_mux_implementation_num_inputs(mux_size));

  /* We definitely know how many nodes we need, 
   * N inputs, 1 output and 0 internal nodes
   */
  MuxNodeId output_node = add_node(MUX_OUTPUT_NODE);
  node_levels_[output_node] = 1; 
  node_ids_at_level_[output_node] = 0; 
  node_output_ids_[output_node] = MuxOutputId(0); 

  for (size_t i = 0; i < mux_size; ++i) {
    MuxNodeId input_node = add_node(MUX_INPUT_NODE);
    /* All the node belong to level 0 (we have only 1 level) */
    node_input_ids_[input_node] = MuxInputId(i);
    node_levels_[input_node] = 0; 
    node_ids_at_level_[input_node] = i; 

    /* We definitely know how many edges we need, 
     * the same as mux_size, add a edge connecting two nodes
     */
    MuxEdgeId edge = add_edge(input_node, output_node); 
    /* Configure the edge */
    edge_models_[edge] = pgl_model;

    /* Create a memory bit*/
    MuxMemId mem = add_mem(); 
    mem_levels_[mem] = 0;
    /* Link the edge to a memory bit */
    set_edge_mem_id(edge, mem);
  }
  /* Finish building the graph for a one-level multiplexer */
} 

/* Convert some internal nodes to be additional outputs
 * according to the fracturable LUT port definition
 * We will iterate over each output port of a circuit model 
 * and find the frac_level and output_mask 
 * Then, the internal nodes at the frac_level will be converted
 * to output nodes with a given output_mask
 */
void MuxGraph::add_fracturable_outputs(const CircuitLibrary& circuit_lib, 
                                       const CircuitModelId& circuit_model) {
  /* Iterate over output ports */
  for (const auto& port : circuit_lib.model_ports_by_type(circuit_model, CIRCUIT_MODEL_PORT_OUTPUT, false)) {
    /* Get the fracturable_level */
    size_t frac_level = circuit_lib.port_lut_frac_level(port);
    /* Bypass invalid frac_level */
    if (size_t(-1) == frac_level) {
      continue;
    }
    /* Iterate over output masks */
    for (const auto& output_idx : circuit_lib.port_lut_output_mask(port)) {
      size_t num_matched_nodes = 0;
      /* Iterate over node and find the internal nodes, which match the frac_level and output_idx */
      for (const auto& node : node_lookup_[frac_level][MUX_INTERNAL_NODE]) {
        if (node_ids_at_level_[node] != output_idx) {
          /* Bypass condition */
          continue;
        }
        /* Reach here, this is the node we want
         * Convert it to output nodes and update the counter   
         */
        node_types_[node] = MUX_OUTPUT_NODE;
        node_output_ids_[node] = MuxOutputId(num_outputs());  
        num_matched_nodes++;
      } 
      /* Either find 1 or 0 matched nodes */
      if (0 != num_matched_nodes) {
        /* We should find only one node that matches! */
        VTR_ASSERT(1 == num_matched_nodes);
        /* Rebuild the node look-up */
        build_node_lookup();
        continue; /* Finish here, go to next */
      }
      /* Sometime the wanted node is already an output, do a double check */
      for (const auto& node : node_lookup_[frac_level][MUX_OUTPUT_NODE]) {
        if (node_ids_at_level_[node] != output_idx) {
          /* Bypass condition */
          continue;
        }
        /* Reach here, this is the node we want 
         * Just update the counter   
         */
        num_matched_nodes++;
      }
      /* We should find only one node that matches! */
      VTR_ASSERT(1 == num_matched_nodes);
    } 
  }
}

/* Build the graph for a given multiplexer model */
void MuxGraph::build_mux_graph(const CircuitLibrary& circuit_lib, 
                               const CircuitModelId& circuit_model,
                               const size_t& mux_size) {
  /* Make sure this model is a MUX */
  VTR_ASSERT((CIRCUIT_MODEL_MUX == circuit_lib.model_type(circuit_model))
          || (CIRCUIT_MODEL_LUT == circuit_lib.model_type(circuit_model)) );

  /* Make sure mux_size is valid */
  VTR_ASSERT(valid_mux_implementation_num_inputs(mux_size));

  size_t impl_mux_size = find_mux_implementation_num_inputs(circuit_lib, circuit_model, mux_size);

  /* Depends on the mux size, the implemented multiplexer structure may change! */
  enum e_circuit_model_structure impl_structure = find_mux_implementation_structure(circuit_lib, circuit_model, impl_mux_size);

  /* Branch on multiplexer structures, leading to different building strategies */
  switch (impl_structure) {
  case CIRCUIT_MODEL_STRUCTURE_TREE: {
    /* Find the number of levels */
    size_t num_levels = find_treelike_mux_num_levels(impl_mux_size);

    /* Find the number of inputs per branch, this is not final */
    size_t num_inputs_per_branch = 2;

    /* Build a multilevel mux graph */
    build_multilevel_mux_graph(impl_mux_size, num_levels, num_inputs_per_branch, circuit_lib.pass_gate_logic_model(circuit_model));
    break;
  }
  case CIRCUIT_MODEL_STRUCTURE_ONELEVEL: {
    build_onelevel_mux_graph(impl_mux_size, circuit_lib.pass_gate_logic_model(circuit_model));
    break;
  }
  case CIRCUIT_MODEL_STRUCTURE_MULTILEVEL: {
    /* Find the number of inputs per branch, this is not final */
    size_t num_inputs_per_branch = find_multilevel_mux_branch_num_inputs(impl_mux_size, circuit_lib.mux_num_levels(circuit_model));

    /* Build a multilevel mux graph */
    build_multilevel_mux_graph(impl_mux_size, circuit_lib.mux_num_levels(circuit_model), 
                               num_inputs_per_branch,
                               circuit_lib.pass_gate_logic_model(circuit_model));
    break;
  }
  default:
    VTR_LOG_ERROR("Invalid multiplexer structure for circuit model '%s'!\n",
                  circuit_lib.model_name(circuit_model).c_str());
    exit(1);
  }

  /* Since the graph is finalized, it is time to build the fast look-up */
  build_node_lookup();
  build_mem_lookup();

  /* For fracturable LUTs, we need to add more outputs to the MUX graph */
  if ( (CIRCUIT_MODEL_LUT == circuit_lib.model_type(circuit_model))
    && (true == circuit_lib.is_lut_fracturable(circuit_model)) ) {
    add_fracturable_outputs(circuit_lib, circuit_model);
  }
}

/* Build fast node lookup */
void MuxGraph::build_node_lookup() {
  /* Invalidate the node lookup if necessary */
  invalidate_node_lookup();

  /* Find the maximum number of levels */
  size_t num_levels = 0;
  for (auto node : nodes()) {
    num_levels = std::max((int)node_levels_[node], (int)num_levels);
  }

  /* Resize node_lookup */
  node_lookup_.resize(num_levels + 1);
  for (size_t lvl = 0; lvl < node_lookup_.size(); ++lvl) {
    /* Resize by number of node types */
    node_lookup_[lvl].resize(NUM_MUX_NODE_TYPES);
  }

  /* Fill the node lookup */
  for (auto node : nodes()) {
    node_lookup_[node_levels_[node]][size_t(node_types_[node])].push_back(node);
  }
}

/* Build fast mem lookup */
void MuxGraph::build_mem_lookup() {
  /* Invalidate the mem lookup if necessary */
  invalidate_mem_lookup();

  /* Find the maximum number of levels */
  size_t num_levels = 0;
  for (auto mem : memories()) {
    num_levels = std::max((int)mem_levels_[mem], (int)num_levels);
  }

  /* Resize mem_lookup */
  mem_lookup_.resize(num_levels + 1);
  for (auto mem : memories()) {
    /* Categorize mem nodes into mem_lookup */
    mem_lookup_[mem_levels_[mem]].push_back(mem);
  }
}

/* Invalidate (empty) the node fast lookup*/
void MuxGraph::invalidate_node_lookup() {
  node_lookup_.clear();
}

/* Invalidate (empty) the mem fast lookup*/
void MuxGraph::invalidate_mem_lookup() {
  mem_lookup_.clear();
}
 
/**************************************************
 * Private validators
 *************************************************/

/* valid ids */
bool MuxGraph::valid_node_id(const MuxNodeId& node) const {
  return size_t(node) < node_ids_.size() && node_ids_[node] == node;
}

bool MuxGraph::valid_edge_id(const MuxEdgeId& edge) const {
  return size_t(edge) < edge_ids_.size() && edge_ids_[edge] == edge;
}

bool MuxGraph::valid_mem_id(const MuxMemId& mem) const {
  return size_t(mem) < mem_ids_.size() && mem_ids_[mem] == mem;
}

/* validate an input id (from which data path signal will be progagated to the output) */
bool MuxGraph::valid_input_id(const MuxInputId& input_id) const {
  for (const auto& lvl : node_lookup_) {
    for (const auto& node : lvl[MUX_INPUT_NODE]) {
      if (size_t(input_id) > size_t(node_input_ids_[node])) {
        return false;
      }
    }
  } 

  return true;
}

/* validate an output id */
bool MuxGraph::valid_output_id(const MuxOutputId& output_id) const {
  for (const auto& lvl : node_lookup_) {
    for (const auto& node : lvl[MUX_OUTPUT_NODE]) {
      if (size_t(output_id) > size_t(node_output_ids_[node])) {
        return false;
      }
    }
  } 

  return true;
}

bool MuxGraph::valid_level(const size_t& level) const {
  return level < num_node_levels(); 
}

bool MuxGraph::valid_node_lookup() const {
  return node_lookup_.empty();
}

/* validate a mux graph and see if it is valid */
bool MuxGraph::valid_mux_graph() const {
  /* A valid MUX graph should be
   * 1. every node has 1 fan-out except output node
   * 2. every input can be routed to the output node 
   */
  for (const auto& node : nodes()) {
    /* output node has 0 fan-out*/
    if (MUX_OUTPUT_NODE == node_types_[node]) {
      continue;
    }
    /* other nodes should have 1 fan-out */
    if (1 != node_out_edges_[node].size()) {
      return false;
    }
  }

  /* Try to route to output */
  for (const auto& node : nodes()) {
    if (MUX_INPUT_NODE == node_types_[node]) {
      MuxNodeId next_node = node;
      while ( 0 < node_out_edges_[next_node].size() ) {
        MuxEdgeId edge = node_out_edges_[next_node][0];
        /* each edge must have 1 fan-out */
        if (1 != edge_sink_nodes_[edge].size()) {
          return false;
        }
        next_node = edge_sink_nodes_[edge][0]; 
      }
      if (MUX_OUTPUT_NODE != node_types_[next_node]) {
        return false;
      }
    }
  } 

  return true;
}

} /* End namespace openfpga*/
