#pragma once
/**
 * @file
 * @brief BLIF Netlist Loader
 *
 * This loader handles loading a post-technology mapping fully flattened (i.e not
 * hierarchical) netlist in Berkely Logic Interchange Format (BLIF) file, and
 * builds a netlist data structure (AtomNetlist) from it.
 *
 * BLIF text parsing is handled by the blifparse library, while this file is responsible
 * for creating the netlist data structure.
 *
 * The main object of interest is the BlifAllocCallback struct, which implements the
 * blifparse callback interface.  The callback methods are then called when basic blif
 * primitives are encountered by the parser.  The callback methods then create the associated
 * netlist data structures.
 */
#ifndef __VPR_src_base_BlifAllocCallback_H_
#define __VPR_src_base_BlifAllocCallback_H_

#include <cstdio>
#include <cstring>
#include <ctime>
#include <sstream>
#include <unordered_set>
#include <cctype>

#include "blifparse.hpp"
#include "atom_netlist.h"

#include "vtr_assert.h"
#include "vtr_util.h"
#include "vtr_log.h"
#include "vtr_logic.h"
#include "vtr_time.h"
#include "vtr_digest.h"

#include "vpr_types.h"
#include "vpr_error.h"
#include "globals.h"
#include "read_blif.h"
#include "arch_types.h"
#include "echo_files.h"
#include "simple_netlist.h"
#include "veri_prune.h"
#include "edif_blif.hpp"

struct BlifAllocCallback : public blifparse::Callback {
  public:
    BlifAllocCallback(e_circuit_format blif_format, AtomNetlist& main_netlist,
                      const std::string netlist_id, const t_model* user_models, const t_model* library_models);

    virtual ~BlifAllocCallback();

    static constexpr const char* OUTPAD_NAME_PREFIX = "out:";

  public: //Callback interface
    void start_parse() override {}

    void finish_parse() override;

    void begin_model(std::string model_name) override;

    void inputs(std::vector<std::string> input_names) override;

    void outputs(std::vector<std::string> output_names) override;

    void names(std::vector<std::string> nets, std::vector<std::vector<blifparse::LogicValue>> so_cover) override;

    void latch(std::string input, std::string output, blifparse::LatchType type, std::string control, blifparse::LogicValue init) override;

    void subckt(std::string subckt_model, std::vector<std::string> ports, std::vector<std::string> nets) override;

    void blackbox() override;

    void end_model() override;

    //BLIF Extensions
    void conn(std::string src, std::string dst) override;

    void cname(std::string cell_name) override {
        if (blif_format_ != e_circuit_format::EBLIF) {
            parse_error(lineno_, ".cname", "Supported only in extended BLIF format");
        }

        //Re-name the block
        curr_model().set_block_name(curr_block(), cell_name);
    }

    void attr(std::string name, std::string value) override {
        if (blif_format_ != e_circuit_format::EBLIF) {
            parse_error(lineno_, ".attr", "Supported only in extended BLIF format");
        }

        curr_model().set_block_attr(curr_block(), name, value);
    }

    static bool is_known_param(const std::string& param) {
        /* Must be non-empty */
        if (param.empty()) {
            return false;
        }

        /* The parameter must not contain 'x' and 'X' */
        for (size_t i = 0; i < param.length(); ++i) {
            if (param[i] == 'x' || param[i] == 'X') {
                return false;
            }
        }
        return true;
    }

    void param(std::string name, std::string value) override;

    //Utilities
    void filename(std::string fname) override { filename_ = fname; }

    void lineno(int line_num) override { lineno_ = line_num; }

    void parse_error(const int curr_lineno, const std::string& near_text, const std::string& msg) override;

  public:
    //Retrieve the netlist
    size_t determine_main_netlist_index();

  private:
    const t_model* find_model(std::string name);

    const t_model_ports* find_model_port(const t_model* blk_model, std::string port_name);

    /** @brief Splits the index off a signal name and returns the base signal name
     *         (excluding the index) and the index as an integer. For example
     *
     * "my_signal_name[2]"   -> "my_signal_name", 2
     */
    std::pair<std::string, int> split_index(const std::string& signal_name);

    ///@brief Retieves a reference to the currently active .model
    AtomNetlist& curr_model() {
        if (blif_models_.empty() || ended_) {
            vpr_throw(VPR_ERROR_BLIF_F, filename_.c_str(), lineno_, "Expected .model");
        }

        return blif_models_[blif_models_.size() - 1];
    }

    void set_curr_model_blackbox(bool val) {
        VTR_ASSERT(blif_models_.size() == blif_models_black_box_.size());
        blif_models_black_box_[blif_models_black_box_.size() - 1] = val;
    }

    bool verify_blackbox_model(AtomNetlist& blif_model);

    ///@brief Returns a different unique subck name each time it is called
    std::string unique_subckt_name() {
        return "unnamed_subckt" + std::to_string(unique_subckt_name_counter_++);
    }

    /** @brief Sets the current block which is being processed
     *
     * Used to determine which block a .cname, .param, .attr apply to
     */
    void set_curr_block(AtomBlockId blk) {
        curr_block_ = blk;
    }

    ///@brief Gets the current block which is being processed
    AtomBlockId curr_block() const {
        return curr_block_;
    }

    /**
     * @brief Merges all the recorded net pairs which need to be merged
     *
     * This should only be called at the end of a .model to ensure that
     * all the associated driver/sink pins have been delcared and connected
     * to their nets
     */
    void merge_conn_nets();

  private:
    bool ended_ = true; //Initially no active .model
    std::string filename_ = "";
    int lineno_ = -1;

    std::vector<AtomNetlist> blif_models_;
    std::vector<bool> blif_models_black_box_;

    AtomNetlist& main_netlist_;    ///<User object we fill
    const std::string netlist_id_; ///<Unique identifier based on the contents of the blif file
    const t_model* user_arch_models_ = nullptr;
    const t_model* library_arch_models_ = nullptr;
    const t_model* inpad_model_;
    const t_model* outpad_model_;

    size_t unique_subckt_name_counter_ = 0;

    AtomBlockId curr_block_;
    std::vector<std::pair<AtomNetId, AtomNetId>> curr_nets_to_merge_;

    e_circuit_format blif_format_ = e_circuit_format::BLIF;
};

#endif

