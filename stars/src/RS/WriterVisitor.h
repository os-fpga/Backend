#pragma once
#ifndef __rsbe__stars__WriterVisitor_H_h_
#define __rsbe__stars__WriterVisitor_H_h_

#include "pinc_log.h"

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

#include "rsGlobal.h"
#include "rsVPR.h"
#include "sta_file_writer.h"
#include "sta_lib_data.h"
#include "sta_lib_writer.h"
#include "rsDB.h"

namespace rsbe {

using std::cout;
using std::endl;
using std::string;
using std::stringstream;
using std::ostream;
using namespace pinc;

/**
 * @brief A class which writes post-synthesis netlists (Verilog and BLIF) and
 * the SDF
 *
 * It implements the NetlistVisitor interface used by NetlistWalker (see
 * netlist_walker.h)
 */
class StaWriterVisitor : public NetlistVisitor {
public:
  StaWriterVisitor(ostream& verilog_os,
                   ostream& sdf_os,
                   ostream& lib_os,
                   std::shared_ptr<const AnalysisDelayCalculator> delay_calc,
                   const t_analysis_opts& opts);

  virtual ~StaWriterVisitor();

  // No copy, No move
  StaWriterVisitor(StaWriterVisitor& other) = delete;
  StaWriterVisitor(StaWriterVisitor&& other) = delete;
  StaWriterVisitor& operator=(StaWriterVisitor& rhs) = delete;
  StaWriterVisitor& operator=(StaWriterVisitor&& rhs) = delete;

private:  // NetlistVisitor interface functions
  void visit_top_impl(const char* top_level_name) override { top_module_name_ = top_level_name; }

  virtual void visit_atom_impl(const t_pb* atom) override;

  virtual void finish_impl() override;

protected:
  virtual void print_primary_io(int depth);

  virtual void print_assignments(int depth) {
    verilog_os_ << "\n";
    verilog_os_ << indent(depth + 1) << "//IO assignments\n";
    for (auto& assign : assignments_) {
      assign.print_verilog(verilog_os_, indent(depth + 1));
    }
  }

  ///@brief Writes out the verilog netlist
  void print_verilog(int depth = 0);

private:  // Internal Helper functions
  void print_lib(int depth = 0);

  ///@brief Writes out the SDF
  void print_sdf(int depth = 0);

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

  ///@brief Returns an Instance object representing the LUT
  std::shared_ptr<Instance> make_lut_instance(const t_pb* atom);

  ///@brief Returns an Instance object representing the Latch
  std::shared_ptr<Instance> make_latch_instance(const t_pb* atom);

  /**
   * @brief Returns an Instance object representing the RAM
   * @note  the primtive interface to dual and single port rams is nearly
   * identical, so we using a single function to handle both
   */
  std::shared_ptr<Instance> make_ram_instance(const t_pb* atom);

  ///@brief Returns an Instance object representing a Multiplier
  std::shared_ptr<Instance> make_multiply_instance(const t_pb* atom);

  ///@brief Returns an Instance object representing an Adder
  std::shared_ptr<Instance> make_adder_instance(const t_pb* atom);

  std::shared_ptr<Instance> make_blackbox_instance(const t_pb* atom);

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
  LogicVec load_lut_mask(size_t num_inputs,   // LUT size
                         const t_pb* atom) {  // LUT primitive
    auto& atom_ctx = g_vpr_ctx.atom();

    const t_model* model = atom_ctx.nlist.block_model(atom_ctx.lookup.pb_atom(atom));
    assert(model->name == string(MODEL_NAMES));

#ifdef DEBUG_LUT_MASK
    cout << "Loading LUT mask for: " << atom->name << endl;
#endif

    // Figure out how the inputs were permuted (compared to the input netlist)
    std::vector<int> permute = determine_lut_permutation(num_inputs, atom);

    // Retrieve the truth table
    const auto& truth_table = atom_ctx.nlist.block_truth_table(atom_ctx.lookup.pb_atom(atom));

    // Apply the permutation
    auto permuted_truth_table = permute_truth_table(truth_table, num_inputs, permute);

    // Convert to lut mask
    LogicVec lut_mask = truth_table_to_lut_mask(permuted_truth_table, num_inputs);

#ifdef DEBUG_LUT_MASK
    cout << "\tLUT_MASK: " << lut_mask << endl;
#endif

    return lut_mask;
  }

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
  std::vector<int> determine_lut_permutation(size_t num_inputs, const t_pb* atom_pb) {
    auto& atom_ctx = g_vpr_ctx.atom();

    std::vector<int> permute(num_inputs, OPEN);

#ifdef DEBUG_LUT_MASK
    cout << "\tInit Permute: {";
    for (size_t i = 0; i < permute.size(); i++) {
      cout << permute[i] << ", ";
    }
    cout << "}" << endl;
#endif

    // Determine the permutation
    //
    // We walk through the logical inputs to this atom (i.e. in the original
    // truth table/netlist) and find the corresponding input in the
    // implementation atom (i.e. in the current netlist)
    auto ports = atom_ctx.nlist.block_input_ports(atom_ctx.lookup.pb_atom(atom_pb));
    if (ports.size() == 1) {
      const t_pb_graph_node* gnode = atom_pb->pb_graph_node;
      assert(gnode->num_input_ports == 1);
      assert(gnode->num_input_pins[0] >= (int)num_inputs);

      AtomPortId port_id = *ports.begin();

      for (size_t ipin = 0; ipin < num_inputs; ++ipin) {
        AtomNetId impl_input_net_id =
            find_atom_input_logical_net(atom_pb, ipin);  // The net currently connected to input j

        // Find the original pin index
        const t_pb_graph_pin* gpin = &gnode->input_pins[0][ipin];
        BitIndex orig_index = atom_pb->atom_pin_bit_index(gpin);

        if (impl_input_net_id) {
          // If there is a valid net connected in the implementation
          AtomNetId logical_net_id = atom_ctx.nlist.port_net(port_id, orig_index);

          // Fatal error should be flagged when the net marked in
          // implementation does not match the net marked in input netlist
          if (impl_input_net_id != logical_net_id) {
            VPR_FATAL_ERROR(VPR_ERROR_IMPL_NETLIST_WRITER,
                            "Unmatch:\n\tlogical net is '%s' at pin "
                            "'%lu'\n\timplmented net is '%s' at pin '%s'\n",
                            atom_ctx.nlist.net_name(logical_net_id).c_str(), size_t(orig_index),
                            atom_ctx.nlist.net_name(impl_input_net_id).c_str(), gpin->to_string().c_str());
          }

          // Mark the permutation.
          //  The net originally located at orig_index in the atom netlist
          //  was moved to ipin in the implementation
          permute[orig_index] = ipin;
        }
      }
    } else {
      // May have no inputs on a constant generator
      assert(ports.size() == 0);
    }

    // Fill in any missing values in the permutation (i.e. zeros)
    std::set<int> perm_indicies(permute.begin(), permute.end());
    size_t unused_index = 0;
    for (size_t i = 0; i < permute.size(); i++) {
      if (permute[i] == OPEN) {
        while (perm_indicies.count(unused_index)) {
          unused_index++;
        }
        permute[i] = unused_index;
        perm_indicies.insert(unused_index);
      }
    }

#ifdef DEBUG_LUT_MASK
    cout << "\tPermute: {";
    for (size_t k = 0; k < permute.size(); k++) {
      cout << permute[k] << ", ";
    }
    cout << "}" << endl;

    cout << "\tBLIF = Input ->  Rotated" << endl;
    cout << "\t------------------------" << endl;
#endif
    return permute;
  }

  /**
   * @brief Helper function for load_lut_mask() which determines if the
   *        names is encodeing the ON (returns true) or OFF (returns false)
   * set.
   */
  bool names_encodes_on_set(vtr::t_linked_vptr* names_row_ptr) {
    // Determine the truth (output value) for this row
    // By default we assume the on-set is encoded to correctly handle
    // constant true/false
    //
    // False output:
    //      .names j
    //
    // True output:
    //      .names j
    //      1
    bool encoding_on_set = true;

    // We may get a nullptr if the output is always false
    if (names_row_ptr) {
      // Determine whether the truth table stores the ON or OFF set
      //
      //  In blif, the 'output values' of a .names must be either '1' or '0',
      //  and must be consistent within a single .names -- that is a single
      //  .names can encode either the ON or OFF set (of which only one will
      //  be encoded in a single .names)
      //
      const string names_first_row = (const char*)names_row_ptr->data_vptr;
      auto names_first_row_output_iter = names_first_row.end() - 1;

      if (*names_first_row_output_iter == '1') {
        encoding_on_set = true;
      } else if (*names_first_row_output_iter == '0') {
        encoding_on_set = false;
      } else {
        VPR_FATAL_ERROR(VPR_ERROR_IMPL_NETLIST_WRITER,
                        "Invalid .names truth-table character '%c'. Must be "
                        "one of '1', '0' or '-'. \n",
                        *names_first_row_output_iter);
      }
    }

    return encoding_on_set;
  }

  /**
   * @brief Helper function for load_lut_mask()
   *
   * Converts the given names_row string to a LogicVec
   */
  LogicVec names_row_to_logic_vec(const string names_row, size_t num_inputs, bool encoding_on_set) {
    // Get an iterator to the last character (i.e. the output value)
    auto output_val_iter = names_row.end() - 1;

    // Sanity-check, the 2nd last character should be a space
    auto space_iter = names_row.end() - 2;
    assert(*space_iter == ' ');

    // Extract the truth (output value) for this row
    if (*output_val_iter == '1') {
      assert(encoding_on_set);
    } else if (*output_val_iter == '0') {
      assert(!encoding_on_set);
    } else {
      VPR_FATAL_ERROR(VPR_ERROR_IMPL_NETLIST_WRITER, "Invalid .names encoding both ON and OFF set\n");
    }

    // Extract the input values for this row
    LogicVec input_values(num_inputs, vtr::LogicValue::FALSE);
    size_t i = 0;
    // Walk through each input in the input cube for this row
    while (names_row[i] != ' ') {
      vtr::LogicValue input_val = vtr::LogicValue::UNKOWN;
      if (names_row[i] == '1') {
        input_val = vtr::LogicValue::TRUE;
      } else if (names_row[i] == '0') {
        input_val = vtr::LogicValue::FALSE;
      } else if (names_row[i] == '-') {
        input_val = vtr::LogicValue::DONT_CARE;
      } else {
        VPR_FATAL_ERROR(VPR_ERROR_IMPL_NETLIST_WRITER,
                        "Invalid .names truth-table character '%c'. Must be "
                        "one of '1', '0' or '-'. \n",
                        names_row[i]);
      }

      input_values[i] = input_val;
      i++;
    }
    return input_values;
  }

  ///@brief Returns the total number of input pins on the given pb
  int find_num_inputs(const t_pb* pb) {
    int count = 0;
    for (int i = 0; i < pb->pb_graph_node->num_input_ports; i++) {
      count += pb->pb_graph_node->num_input_pins[i];
    }
    return count;
  }
  ///@brief Returns the logical net ID
  AtomNetId find_atom_input_logical_net(const t_pb* atom, int atom_input_idx) {
    const t_pb_graph_node* pb_node = atom->pb_graph_node;

    int cluster_pin_idx = pb_node->input_pins[0][atom_input_idx].pin_count_in_cluster;
    const auto& top_pb_route = find_top_pb_route(atom);
    AtomNetId atom_net_id;
    if (top_pb_route.count(cluster_pin_idx)) {
      atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;
    }
    return atom_net_id;
  }

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

    return rsbe::get_delay_ps(delay_sec);  // Class overload hides file-scope by default
  }

private:                    // Data
  string top_module_name_;  ///< Name of the top level module (i.e. the circuit)
protected:
  std::vector<string> inputs_;           ///< Name of circuit inputs
  std::vector<string> outputs_;          ///< Name of circuit outputs
  std::vector<Assignment> assignments_;  ///< Set of assignments (i.e. net-to-net connections)
  std::vector<std::shared_ptr<Instance>> cell_instances_;  ///< Set of cell instances

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

  std::shared_ptr<const AnalysisDelayCalculator> delay_calc_;
  struct t_analysis_opts opts_;
};

}

#endif

