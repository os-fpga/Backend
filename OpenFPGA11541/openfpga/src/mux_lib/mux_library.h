/**************************************************
 * This file includes a data structure to describe
 * the multiplexer implementations in FPGA architectures
 * MuxLibrary is a collection of multiplexers 
 * with various circuit-level description (related to
 * the information available in CircuitLibrary 
 * and the input size of multiplexers)
 *************************************************/

#ifndef MUX_LIBRARY_H
#define MUX_LIBRARY_H

#include <map>
#include "mux_graph.h"
#include "mux_library_fwd.h"

/* begin namespace openfpga */
namespace openfpga {

class MuxLibrary {
  public: /* Types and ranges */
    typedef vtr::vector<MuxId, MuxId>::const_iterator mux_iterator;

    typedef vtr::Range<mux_iterator> mux_range;
  public: /* Public accessors: Aggregates */
    mux_range muxes() const;
  public:  /* Public accessors */
    /* Get a MUX graph (read-only) */
    MuxId mux_graph(const CircuitModelId& circuit_model, const size_t& mux_size) const;
    const MuxGraph& mux_graph(const MuxId& mux_id) const;
    /* Get a mux circuit model id */
    CircuitModelId mux_circuit_model(const MuxId& mux_id) const;
    /* Find the mux sizes */
    size_t max_mux_size() const;
  public:  /* Public mutators */
    /* Add a mux to the library */
    void add_mux(const CircuitLibrary& circuit_lib, const CircuitModelId& circuit_model, const size_t& mux_size); 
  public:  /* Public validators */
    bool valid_mux_id(const MuxId& mux) const;
  private:  /* Private accessors */
    bool valid_mux_lookup() const;
    bool valid_mux_circuit_model_id(const CircuitModelId& circuit_model) const;
    bool valid_mux_size(const CircuitModelId& circuit_model, const size_t& mux_size) const;
  private:  /* Private mutators: mux_lookup */
    void build_mux_lookup();
    /* Invalidate (empty) the mux fast lookup*/
    void invalidate_mux_lookup();
  private:  /* Internal data */
    /* MUX graph-based desription */
    vtr::vector<MuxId, MuxId> mux_ids_; /* Unique identifier for each mux graph */
    vtr::vector<MuxId, MuxGraph> mux_graphs_; /* Graphs describing MUX internal structures */
    vtr::vector<MuxId, CircuitModelId> mux_circuit_models_; /* circuit model id in circuit library */

    /* Local encoder description */
    //vtr::vector<MuxLocalDecoderId, Decoder> mux_local_encoders_; /* Graphs describing MUX internal structures */

    /* a fast look-up to search mux_graphs with given circuit model and mux size */
    typedef std::map<CircuitModelId, std::map<size_t, MuxId>> MuxLookup;
    mutable MuxLookup mux_lookup_; 
};

} /* end namespace openfpga */

#endif
