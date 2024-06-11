#pragma once
#ifndef __pln__stars__WriterVisitor_H_h_
#define __pln__stars__WriterVisitor_H_h_

#include "util/pln_log.h"

// vpr/src/base
#include "atom_netlist.h"
#include "atom_netlist_utils.h"
#include "globals.h"
#include "logic_vec.h"
#include "netlist_walker.h"
#include "vpr_types.h"
#include "vtr_version.h"

// vpr/src/timing
#include "AnalysisDelayCalculator.h"
#include "net_delay.h"

#include "RS/rsGlobal.h"
#include "RS/rsVPR.h"
#include "RS/sta_file_writer.h"
#include "RS/sta_lib_writer.h"
#include "RS/rsDB.h"

namespace pln {

using std::endl;
using std::string;
using std::stringstream;
using std::ostream;

/**
 * @brief A class which writes post-synthesis netlists (Verilog and BLIF) and
 * the SDF
 *
 * It implements the NetlistVisitor interface used by NetlistWalker (see
 * netlist_walker.h)
 */
class WriterVisitor : public NetlistVisitor {
public:
  WriterVisitor(ostream& verilog_os,
                ostream& sdf_os,
                ostream& lib_os,
                const AnalysisDelayCalculator* del_calc,
                const t_analysis_opts& opts);

  virtual ~WriterVisitor();

  // No copy, No move
  WriterVisitor(WriterVisitor& other) = delete;
  WriterVisitor(WriterVisitor&& other) = delete;
  WriterVisitor& operator=(WriterVisitor& rhs) = delete;
  WriterVisitor& operator=(WriterVisitor&& rhs) = delete;

private:  // NetlistVisitor interface functions
  void visit_top_impl(const char* top_level_name) override { top_module_name_ = top_level_name; }

  virtual void visit_atom_impl(const t_pb* atom) override;

  virtual void finish_impl() override;

protected:
  virtual void print_primary_io(int depth);

  virtual void print_assignments(int depth);

  ///@brief Writes out the verilog netlist
  void printVerilog(int depth = 0);

private:  // Internal Helper functions
  void printLib(int depth = 0);

  ///@brief Writes out the SDF
  void printSDF(int depth = 0);

  /**
   * @brief Returns the name of a circuit-level Input/Output
   *
   * The I/O is recorded and instantiated by the top level output routines
   *   @param atom  The implementation primitive representing the I/O
   *   @param dir   The IO direction
   */
  string make_io(const t_pb* atom, PortType dir);

protected:
  /**
   * @brief Returns the name of a wire connecting a primitive and global net.
   *
   * The wire is recorded and instantiated by the top level output routines.
   */
  string make_inst_wire(AtomNetId atom_net_id,   ///< The id of the net in the atom netlist
                        tatum::NodeId tnode_id,  ///< The tnode associated with the primitive pin
                        string inst_name,        ///< The name of the instance associated with the pin
                        PortType port_type,      ///< The port direction
                        int port_idx,            ///< The instance port index
                        int pin_idx);            ///< The instance pin index

  ///@brief Returns an Cell object representing the LUT
  Cell* make_lut_cell(const t_pb* atom) noexcept;

  ///@brief Returns an Cell object representing the Latch
  Cell* make_latch_instance(const t_pb* atom);

  /**
   * @brief Returns an Cell object representing the RAM
   * @note  the primtive interface to dual and single port rams is nearly
   * identical, so we using a single function to handle both
   */
  Cell* make_ram_instance(const t_pb* atom);

  ///@brief Returns an Cell object representing a Multiplier
  Cell* make_multiply_instance(const t_pb* atom);

  ///@brief Returns an Cell object representing an Adder
  Cell* make_adder_instance(const t_pb* atom);

  Cell* make_blackbox_instance(const t_pb* atom);

  ///@brief Returns the top level pb_route associated with the given pb
  const t_pb_routes& find_top_pb_route(const t_pb* curr) {
    const t_pb* top_pb = find_top_cb(curr);
    return top_pb->pb_route;
  }

  ///@brief Returns the tnode ID of the given atom's connected cluster pin
  tatum::NodeId find_tnode(const t_pb* atom, int cluster_pin_idx) {
    auto& atom_ctx = g_vpr_ctx.atom();

    AtomBlockId blk_id = atom_ctx.lookup.pb_atom(atom);
    ClusterBlockId clb_index = atom_ctx.lookup.atom_clb(blk_id);

    auto key = std::make_pair(clb_index, cluster_pin_idx);
    auto iter = pin_id_to_tnode_lookup_.find(key);
    assert(iter != pin_id_to_tnode_lookup_.end());

    tatum::NodeId tnode_id = iter->second;
    assert(tnode_id);

    return tnode_id;
  }

private:
  ///@brief Returns the top complex block which contains the given pb
  const t_pb* find_top_cb(const t_pb* curr) {
    // Walk up through the pb graph until curr
    // has no parent, at which point it will be the top pb
    const t_pb* parent = curr->parent_pb;
    while (parent != nullptr) {
      curr = parent;
      parent = curr->parent_pb;
    }
    return curr;
  }

  ///@brief Returns a LogicVec representing the LUT mask of the given LUT atom
  LogicVec load_lut_mask(uint num_inputs,    // LUT size
                         const t_pb* atom);  // LUT primitive

  /**
   * @brief Helper function for load_lut_mask() which determines how the LUT
   * inputs were permuted compared to the input BLIF
   *
   * Since the LUT inputs may have been rotated from the input blif
   * specification we need to figure out this permutation to reflect the
   * physical implementation connectivity.
   *
   * We return a permutation map (which is a list of swaps from index to
   * index) which is then applied to do the rotation of the lutmask.
   *
   * The net in the atom netlist which was originally connected to pin i, is
   * connected to pin permute[i] in the implementation.
   */
  std::vector<int> determine_lut_permutation(uint num_inputs, const t_pb* atom_pb);

  ///@brief Returns the total number of input pins on the given pb
  static uint count_atom_inputs(const t_pb& pb) noexcept {
    uint count = 0;
    for (int i = 0; i < pb.pb_graph_node->num_input_ports; i++) {
      count += pb.pb_graph_node->num_input_pins[i];
    }
    return count;
  }

  ///@brief Returns the logical net ID
  AtomNetId find_atom_input_logical_net(const t_pb* atom, int atom_input_idx);

  ///@brief Returns the name of the routing segment between two wires
  string interconnect_name(string driver_wire, string sink_wire) {
    string name = join_identifier("routing_segment", driver_wire);
    name = join_identifier(name, "to");
    name = join_identifier(name, sink_wire);

    return name;
  }

  ///@brief Returns the delay in pico-seconds from source_tnode to sink_tnode
  double get_delay_ps(tatum::NodeId source_tnode, tatum::NodeId sink_tnode) {
    auto& timing_ctx = g_vpr_ctx.timing();

    tatum::EdgeId edge = timing_ctx.graph->find_edge(source_tnode, sink_tnode);
    assert(edge);

    double delay_sec = delay_calc_->max_edge_delay(*timing_ctx.graph, edge);

    return pln::get_delay_ps(delay_sec);  // Class overload hides file-scope by default
  }

private:                    // Data
  string top_module_name_;  ///< Name of the top level module (i.e. the circuit)
protected:
  std::vector<string> inputs_;           ///< Name of circuit inputs
  std::vector<string> outputs_;          ///< Name of circuit outputs
  std::vector<Assignment> assignments_;  ///< Set of assignments (i.e. net-to-net connections)

  std::vector<Cell*> all_cells_;         ///< Set of cell instances

private:
  // Drivers of logical nets.
  // Key: logic net id, Value: pair of wire_name and tnode_id
  std::map<AtomNetId, std::pair<string, tatum::NodeId>> logical_net_drivers_;

  // Sinks of logical nets.
  // Key: logical net id, Value: vector wire_name tnode_id pairs
  std::map<AtomNetId, std::vector<std::pair<string, tatum::NodeId>>> logical_net_sinks_;
  std::map<string, float> logical_net_sink_delays_;

  // Output streams
protected:
  ostream& verilog_os_;

private:
  ostream& sdf_os_;
  ostream& lib_os_;

  // Look-up from pins to tnodes
  std::map<std::pair<ClusterBlockId, int>, tatum::NodeId> pin_id_to_tnode_lookup_;

  const AnalysisDelayCalculator*  delay_calc_ = nullptr;
  t_analysis_opts  opts_;
};

}

#endif

