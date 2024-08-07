/*
 * Main clustering algorithm
 * Author(s): Vaughn Betz (first revision - VPack), Alexander Marquardt (second revision - T-VPack), Jason Luu (third revision - AAPack)
 * June 8, 2011
 */

/*
 * The clusterer uses several key data structures:
 *
 *      t_pb_type (and related types):
 *          Represent the architecture as described in the architecture file.
 *
 *      t_pb_graph_node (and related types):
 *          Represents a flattened version of the architecture with t_pb_types
 *          expanded (according to num_pb) into unique t_pb_graph_node instances,
 *          and the routing connectivity converted to a graph of t_pb_graph_pin (nodes)
 *          and t_pb_graph_edge.
 *
 *      t_pb:
 *          Represents a clustered instance of a t_pb_graph_node containing netlist primitives
 *
 *  t_pb_type and t_pb_graph_node (and related types) describe the targeted FPGA architecture, while t_pb represents
 *  the actual clustering of the user netlist.
 *
 *  For example:
 *      Consider an architecture where CLBs contain 4 BLEs, and each BLE is a LUT + FF pair.
 *      We wish to map a netlist of 400 LUTs and 400 FFs.
 *
 *      A BLE corresponds to one t_pb_type (which has num_pb = 4).
 *
 *      Each of the 4 BLE positions corresponds to a t_pb_graph_node (each of which references the BLE t_pb_type).
 *
 *      The output of clustering is 400 t_pb of type BLE which represent the clustered user netlist.
 *      Each of the 400 t_pb will reference one of the 4 BLE-type t_pb_graph_nodes.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <algorithm>
#include <fstream>

#include <thread>

#include "vtr_assert.h"
#include "vtr_log.h"
#include "vtr_math.h"
#include "vtr_memory.h"

#include "vpr_types.h"
#include "vpr_error.h"

#include "globals.h"
#include "atom_netlist.h"
#include "pack_types.h"
#include "cluster.h"
#include "cluster_util.h"
#include "output_clustering.h"
#include "SetupGrid.h"
#include "read_xml_arch_file.h"
#include "vpr_utils.h"
#include "cluster_placement.h"
#include "echo_files.h"
#include "cluster_router.h"
#include "lb_type_rr_graph.h"

#include "timing_info.h"
#include "timing_reports.h"
#include "PreClusterDelayCalculator.h"
#include "PreClusterTimingGraphResolver.h"
#include "tatum/echo_writer.hpp"
#include "tatum/report/graphviz_dot_writer.hpp"
#include "tatum/TimingReporter.hpp"

#include "re_cluster_util.h"
#include "constraints_report.h"
#include "nl_Par.h"

#include <dlfcn.h>

/*
 * When attraction groups are created, the purpose is to pack more densely by adding more molecules
 * from the cluster's attraction group to the cluster. In a normal flow, (when attraction groups are
 * not on), the cluster keeps being packed until the get_molecule routines return either a repeated
 * molecule or a nullptr. When attraction groups are on, we want to keep exploring molecules for the
 * cluster until a nullptr is returned. So, the number of repeated molecules is changed from 1 to 500,
 * effectively making the clusterer pack a cluster until a nullptr is returned.
 */
static constexpr int ATTRACTION_GROUPS_MAX_REPEATED_MOLECULES = 500;

static bool s_partitioning_status_NOP = false;
bool partitioning_status_NOP() { return s_partitioning_status_NOP; }

// function pointer types:
using MolPtr_to_uint_F = uint (*) (t_pack_molecule*);

std::map<t_logical_block_type_ptr, size_t> do_clustering(const t_packer_opts& packer_opts,
                                                         const t_analysis_opts& analysis_opts,
                                                         const t_arch* arch,
                                                         t_pack_molecule* molecule_head,
                                                         const std::unordered_set<AtomNetId>& is_clock,
                                                         const std::unordered_set<AtomNetId>& is_global,
                                                         const std::unordered_map<AtomBlockId, t_pb_graph_node*>& expected_lowest_cost_pb_gnode,
                                                         bool allow_unrelated_clustering,
                                                         bool balance_block_type_utilization,
                                                         std::vector<t_lb_type_rr_node>* lb_type_rr_graphs,
                                                         AttractionInfo& attraction_groups,
                                                         bool& floorplan_regions_overfull,
                                                         t_clustering_data& clustering_data) {
    /* Does the actual work of clustering multiple netlist blocks *
     * into clusters.                                                  */

    /* Algorithm employed
     * 1.  Find type that can legally hold block and create cluster with pb info
     * 2.  Populate started cluster
     * 3.  Repeat 1 until no more blocks need to be clustered
     *
     */

    /* This routine returns a map that details the number of used block type instances.
     * The bool floorplan_regions_overfull also acts as a return value - it is set to
     * true when one or more floorplan regions have more blocks assigned to them than
     * they can fit.
     */

    /****************************************************************
     * Initialization
     *****************************************************************/
    VTR_ASSERT(packer_opts.packer_algorithm == PACK_GREEDY);

    t_cluster_progress_stats cluster_stats;

    //int num_molecules, num_molecules_processed, mols_since_last_print, blocks_since_last_analysis,
    int num_blocks_hill_added = 0, max_pb_depth = 0, detailed_routing_stage = 0;
    int seedindex = 0, savedseedindex = 0; // index of next most timing critical block

    const int verbosity = packer_opts.pack_verbosity;

    int unclustered_list_head_size = 0;
    std::unordered_map<AtomNetId, int> net_output_feeds_driving_block_input;

    cluster_stats.num_molecules_processed = 0;
    cluster_stats.mols_since_last_print = 0;

    std::map<t_logical_block_type_ptr, size_t> num_used_type_instances;

    bool isClusterLegal = false;
    enum e_block_pack_status block_pack_status;

    t_cluster_placement_stats* cur_cluster_placement_stats_ptr = nullptr;
    t_lb_router_data* router_data = nullptr;
    t_pack_molecule *istart = 0, *next_molecule = 0, *prev_molecule = 0;

    auto& atom_ctx = g_vpr_ctx.atom();
    auto& device_ctx = g_vpr_ctx.mutable_device();
    auto& cluster_ctx = g_vpr_ctx.mutable_clustering();
    auto& helper_ctx = g_vpr_ctx.mutable_cl_helper();

    helper_ctx.enable_pin_feasibility_filter = packer_opts.enable_pin_feasibility_filter;
    helper_ctx.feasible_block_array_size = packer_opts.feasible_block_array_size;

    std::shared_ptr<PreClusterDelayCalculator> clustering_delay_calc;
    std::shared_ptr<SetupTimingInfo> timing_info;

    // this data structure tracks the number of Logic Elements (LEs) used. It is
    // populated only for architectures which has LEs. The architecture is assumed
    // to have LEs only iff it has a logic block that contains LUT primitives and is
    // the first pb_block to have more than one instance from the top of the hierarchy
    // (All parent pb_block have one instance only and one mode only). Index 0 holds
    // the number of LEs that are used for both logic (LUTs/adders) and registers.
    // Index 1 holds the number of LEs that are used for logic (LUTs/adders) only.
    // Index 2 holds the number of LEs that are used for registers only.
    std::vector<int> le_count(3, 0);

    helper_ctx.total_clb_num = 0;

    /* TODO: This is memory inefficient, fix if causes problems */
    /* Store stats on nets used by packed block, useful for determining transitively connected blocks
     * (eg. [A1, A2, ..]->[B1, B2, ..]->C implies cluster [A1, A2, ...] and C have a weak link) */
    vtr::vector<ClusterBlockId, std::vector<AtomNetId>> clb_inter_blk_nets(atom_ctx.nlist.blocks().size());

    istart = nullptr;

    /* determine bound on cluster size and primitive input size */
    helper_ctx.max_cluster_size = 0;
    max_pb_depth = 0;

    const t_molecule_stats max_molecule_stats = calc_max_molecules_stats(molecule_head);

    mark_all_molecules_valid(molecule_head);

    cluster_stats.num_molecules = count_molecules(molecule_head);

    get_max_cluster_size_and_pb_depth(helper_ctx.max_cluster_size, max_pb_depth);

    if (packer_opts.hill_climbing_flag) {
        clustering_data.hill_climbing_inputs_avail = new int[helper_ctx.max_cluster_size + 1];
        for (int i = 0; i < helper_ctx.max_cluster_size + 1; i++)
            clustering_data.hill_climbing_inputs_avail[i] = 0;
    } else {
        clustering_data.hill_climbing_inputs_avail = nullptr; /* if used, die hard */
    }

#if 0
	check_for_duplicate_inputs ();
#endif
    alloc_and_init_clustering(max_molecule_stats,
                              &(helper_ctx.cluster_placement_stats), &(helper_ctx.primitives_list), molecule_head,
                              clustering_data, net_output_feeds_driving_block_input,
                              unclustered_list_head_size, cluster_stats.num_molecules);

    auto primitive_candidate_block_types = identify_primitive_candidate_block_types();
    // find the cluster type that has lut primitives
    auto logic_block_type = identify_logic_block_type(primitive_candidate_block_types);
    // find a LE pb_type within the found logic_block_type
    auto le_pb_type = identify_le_block_type(logic_block_type);

    cluster_stats.blocks_since_last_analysis = 0;
    num_blocks_hill_added = 0;

    VTR_ASSERT(helper_ctx.max_cluster_size < MAX_SHORT);
    /* Limit maximum number of elements for each cluster */

    //Default criticalities set to zero (e.g. if not timing driven)
    vtr::vector<AtomBlockId, float> atom_criticality(atom_ctx.nlist.blocks().size(), 0.);

    if (packer_opts.timing_driven) {
        calc_init_packing_timing(packer_opts, analysis_opts, expected_lowest_cost_pb_gnode,
                                 clustering_delay_calc, timing_info, atom_criticality);
    }

    // Assign gain scores to atoms and sort them based on the scores.
    auto seed_atoms = initialize_seed_atoms(packer_opts.cluster_seed_type, max_molecule_stats, atom_criticality);

    /* index of next most timing critical block */
    int seed_index = 0;
    istart = get_highest_gain_seed_molecule(seed_index, seed_atoms);

    print_pack_status_header();

    /****************************************************************
     * Clustering
     *****************************************************************/

    // Used to check if the "for loop" will find a legal solution ("failed_for_loop" == true)
    // (T.Besson)
    //
    bool failed_for_loop = false;
    int max_nb_molecule = 0;
    int nb_packed_molecules = 0;

    bool have_partitioning = false;
    nlp::Par par;

    if (packer_opts.use_partitioning_in_pack) {
        int npart = packer_opts.number_of_molecules_in_partition;
        // auto seed_atoms = initialize_seed_atoms(packer_opts.cluster_seed_type, max_molecule_stats, atom_criticality);

        int mol_count = 0;
        MolPtr_to_uint_F countMol_func = nullptr;

        if (::getenv("PLN_TEST_libPlanner")) {
          VTR_LOG("\n\t  PLN_TEST_libPlanner\n");
          const char* libPlanner_DLL_fn = "/home/serge/bes/DLL/Backend/build/stars/libPlanner_dll.so";
          const char* ers = nullptr;
          void* planner_dll = ::dlopen(libPlanner_DLL_fn, RTLD_NOW );
          if (planner_dll) {
            VTR_LOG("dlopen() OK\n");
            void* symp = ::dlsym(planner_dll, "nlp_count_molecules");
            if (symp) {
              VTR_LOG("  OK sym nlp_count_molecules\n");
              countMol_func = reinterpret_cast<MolPtr_to_uint_F>(symp);
            }
            else {
              VTR_LOG("  !sym nlp_count_molecules\n");
              ers = ::dlerror();
              if (ers)
                VTR_LOG("  dlerror: %s\n", ers);
            }
          }
          else {
            VTR_LOG("dlopen() FAILED\n");
            ers = ::dlerror();
            if (ers)
              VTR_LOG("  dlerror: %s\n", ers);
          }
        }

        if (countMol_func) {
          mol_count = countMol_func(molecule_head);
          VTR_LOG("countMol_func returned %i\n", mol_count);
        } else {
          mol_count = nlp::Par::countMolecules(molecule_head);
        }

        VTR_LOG("(in packer_opts.use_partitioning_in_pack)  mol_count= %i  #molecules_in_partition= %i\n",
                       mol_count, npart);

        if (mol_count < npart) {
            VTR_LOG("Partitioning NOP.  mol_count= %i  #molecules_in_partition= %i\n",
                       mol_count, npart);
            s_partitioning_status_NOP = true;
        } else {
            par.init(molecule_head);
            par.recursive_partitioning(npart);
            uint psz = par.partitions_.size();
            if (psz < 2) {
              VTR_LOG("Partitioning FAILED.  #partitions= %u\n", psz);
            } else {
              VTR_LOG("Partitioning succeeded.  #partitions= %u\n", psz);
              have_partitioning = true;
            }
        }
    }

    if (have_partitioning) {

        // auto seed_atoms = initialize_seed_atoms(packer_opts.cluster_seed_type, max_molecule_stats, atom_criticality);
        uint lastIdx = par.partitions_.size() - 1;
        uint size = par.partitions_[lastIdx] + 100;
        std::vector<int>* clusterMoleculeOrder = new std::vector<int>[size];

        for (uint mid = 0; mid < par.numMolecules_; mid++) {
            clusterMoleculeOrder[par.partition_array_[mid]].push_back(mid);
        }

        for (uint partId : par.partitions_) {
            uint currentIndexOfBestMolecule = 0;

            for (uint mid = 0; mid < par.numMolecules_; mid++) {
                if (par.partition_array_[mid] == partId) {
                    par.molecules_[mid]->valid = true;
                    //istart = molecules[mid];
                } else {
                    par.molecules_[mid]->valid = false;
                }
            }

            if (currentIndexOfBestMolecule >= clusterMoleculeOrder[partId].size()) {
                istart = nullptr;
            } else {
                istart = par.molecules_[clusterMoleculeOrder[partId][currentIndexOfBestMolecule]];
                currentIndexOfBestMolecule++;
                while (!istart->valid && currentIndexOfBestMolecule < clusterMoleculeOrder[partId].size()) {
                    istart = par.molecules_[clusterMoleculeOrder[partId][currentIndexOfBestMolecule]];
                    currentIndexOfBestMolecule++;
                }
                if (!istart->valid) {
                    istart = nullptr;
                }
            }

            while (istart != nullptr) {

                isClusterLegal = false;
                savedseedindex = seedindex;

                // If in the previous "for (detailed_routing_stage ..." call we failed all the 
                // time (e.g 'failed_for_loop' is false) then when we re-enter it but we reduce the 
                // calls to "try_fill_cluster" by half in order to expect a feasible solution. 
                // 
                // This case can happen with unit test case "mult_seq" where we stuck on a mode4/mode5 
                // conflict because of the calls to "try_fill_cluster" that always fail. In this case the 
                // only feasible solution is to stick to the original "start_new_cluster" solution and
                // avoid any call to "try_fill_cluster". To do this, "nb_max_molecule" needs to reduce 
                // to 0 to avoid calling "try_fill_cluster". That's why we decrease "max_nb_molecule" in
                // the iteration process to get down to 0. We decrease it by half each time but it could
                // have been another scheme to decrease it. This one looks to give good QoR results.
                // (T.Besson, Rapid Silicon)
                //
                if (failed_for_loop) {
                max_nb_molecule = max_nb_molecule / 1.4; // decrease by 1.4 looks to be a good strategy. 
                                                        // The packer is hyper sensitive to this number.
                                                        // A change by 0.1 currently may generate big
                                                        // difference on some designs like "axil_crossbar".
                                                        // More investigation to understand why ? (T.Besson)
                } else {
                max_nb_molecule = 512; // important starting number. The packer is again sensitive
                                        // to that number. This number needs to be high to guarantee some
                                        // stable behavior. (T.Besson).
                }

                // Expect that we will not find a legal solution in the below "for loop"
                // (T.Besson)
                //
                failed_for_loop = true;

                for (detailed_routing_stage = (int)E_DETAILED_ROUTE_AT_END_ONLY; 
                    !isClusterLegal && detailed_routing_stage != (int)E_DETAILED_ROUTE_INVALID; 
                    detailed_routing_stage++) {

                    ClusterBlockId clb_index(helper_ctx.total_clb_num);

                    VTR_LOGV(verbosity > 2, "Complex block %d:\n", helper_ctx.total_clb_num);

                    /*Used to store cluster's PartitionRegion as primitives are added to it.
                    * Since some of the primitives might fail legality, this structure temporarily
                    * stores PartitionRegion information while the cluster is packed*/
                    PartitionRegion temp_cluster_pr;
                    NocGroupId temp_cluster_noc_grp_id = NocGroupId::INVALID();

                    start_new_cluster(helper_ctx.cluster_placement_stats, helper_ctx.primitives_list,
                                    clb_index, istart,
                                    num_used_type_instances,
                                    packer_opts.target_device_utilization,
                                    helper_ctx.num_models, helper_ctx.max_cluster_size,
                                    arch, packer_opts.device_layout,
                                    lb_type_rr_graphs, &router_data,
                                    detailed_routing_stage, &cluster_ctx.clb_nlist,
                                    primitive_candidate_block_types,
                                    verbosity,
                                    packer_opts.enable_pin_feasibility_filter,
                                    balance_block_type_utilization,
                                    packer_opts.feasible_block_array_size,
                                    temp_cluster_pr, temp_cluster_noc_grp_id);

                    //initial molecule in cluster has been processed
                    cluster_stats.num_molecules_processed++;
                    cluster_stats.mols_since_last_print++;
                    print_pack_status(helper_ctx.total_clb_num,
                                    cluster_stats.num_molecules,
                                    cluster_stats.num_molecules_processed,
                                    cluster_stats.mols_since_last_print,
                                    device_ctx.grid.width(),
                                    device_ctx.grid.height(),
                                    attraction_groups);

                    VTR_LOGV(verbosity > 2,
                            "Complex block %d: '%s' (%s) ", helper_ctx.total_clb_num,
                            cluster_ctx.clb_nlist.block_name(clb_index).c_str(),
                            cluster_ctx.clb_nlist.block_type(clb_index)->name);
                    VTR_LOGV(verbosity > 2, ".");
                    //Progress dot for seed-block
                    fflush(stdout);

                    t_ext_pin_util target_ext_pin_util = helper_ctx.target_external_pin_util.get_pin_util(cluster_ctx.clb_nlist.block_type(clb_index)->name);
                    int high_fanout_threshold = helper_ctx.high_fanout_thresholds.get_threshold(cluster_ctx.clb_nlist.block_type(clb_index)->name);
                    update_cluster_stats(istart, clb_index,
                                        is_clock, //Set of clock nets
                                        is_global, //Set of global nets (currently all clocks)
                                        packer_opts.global_clocks,
                                        packer_opts.alpha, packer_opts.beta,
                                        packer_opts.timing_driven, packer_opts.connection_driven,
                                        high_fanout_threshold,
                                        *timing_info,
                                        attraction_groups,
                                        net_output_feeds_driving_block_input);
                    helper_ctx.total_clb_num++;

                    if (packer_opts.timing_driven) {
                        cluster_stats.blocks_since_last_analysis++;
                        /*it doesn't make sense to do a timing analysis here since there*
                        *is only one atom block clustered it would not change anything      */
                    }
                    cur_cluster_placement_stats_ptr = &(helper_ctx.cluster_placement_stats[cluster_ctx.clb_nlist.block_type(clb_index)->index]);
                    cluster_stats.num_unrelated_clustering_attempts = 0;
                    next_molecule = get_molecule_for_cluster(cluster_ctx.clb_nlist.block_pb(clb_index),
                                                            attraction_groups,
                                                            allow_unrelated_clustering,
                                                            packer_opts.prioritize_transitive_connectivity,
                                                            packer_opts.transitive_fanout_threshold,
                                                            packer_opts.feasible_block_array_size,
                                                            &cluster_stats.num_unrelated_clustering_attempts,
                                                            cur_cluster_placement_stats_ptr,
                                                            clb_inter_blk_nets,
                                                            clb_index,
                                                            verbosity,
                                                            clustering_data.unclustered_list_head,
                                                            unclustered_list_head_size,
                                                            primitive_candidate_block_types);
                    prev_molecule = istart;

                    /*
                    * When attraction groups are created, the purpose is to pack more densely by adding more molecules
                    * from the cluster's attraction group to the cluster. In a normal flow, (when attraction groups are
                    * not on), the cluster keeps being packed until the get_molecule routines return either a repeated
                    * molecule or a nullptr. When attraction groups are on, we want to keep exploring molecules for the
                    * cluster until a nullptr is returned. So, the number of repeated molecules allowed is increased to a
                    * large value.
                    */
                    int max_num_repeated_molecules = 0;
                    if (attraction_groups.num_attraction_groups() > 0) {
                        max_num_repeated_molecules = ATTRACTION_GROUPS_MAX_REPEATED_MOLECULES;
                    } else {
                        max_num_repeated_molecules = 1;
                    }
                    int num_repeated_molecules = 0;

                    int i = 0;

                    while (next_molecule != nullptr && num_repeated_molecules < max_num_repeated_molecules) {
                        prev_molecule = next_molecule;

                        if (i == max_nb_molecule) {
                        break;
                        }

                        try_fill_cluster(packer_opts,
                                        cur_cluster_placement_stats_ptr,
                                        prev_molecule,
                                        next_molecule,
                                        num_repeated_molecules,
                                        helper_ctx.primitives_list,
                                        cluster_stats,
                                        helper_ctx.total_clb_num,
                                        helper_ctx.num_models,
                                        helper_ctx.max_cluster_size,
                                        clb_index,
                                        detailed_routing_stage,
                                        attraction_groups,
                                        clb_inter_blk_nets,
                                        allow_unrelated_clustering,
                                        high_fanout_threshold,
                                        is_clock,
                                        is_global,
                                        timing_info,
                                        router_data,
                                        target_ext_pin_util,
                                        temp_cluster_pr,
                                        temp_cluster_noc_grp_id,
                                        block_pack_status,
                                        clustering_data.unclustered_list_head,
                                        unclustered_list_head_size,
                                        net_output_feeds_driving_block_input,
                                        primitive_candidate_block_types);

                        i++;

                    }

                    max_nb_molecule = i;

                    isClusterLegal = check_cluster_legality(verbosity, detailed_routing_stage, router_data);

                    if (isClusterLegal) {

                        // Calls the extra check for "clb" only (may need to revisit this check) (T.Besson)
                        //
                        if (!strcmp(cluster_ctx.clb_nlist.block_type(clb_index)->name, "clb")) {

                        // Temporary fix : make sure that the solution has no mode confict. This check is
                        // performed by initiating a first xml kind of output work. There may be a tricky
                        // conflict with some lb routing so we need to store temporary the lb nets in the
                        // cluster data structure to make the check inside "check_if_xml_mode_conflict".
                        // (T.Besson, Rapid Silicon)
                        //
                        (clustering_data.intra_lb_routing).push_back(router_data->saved_lb_nets);

                        // Call the check as if we would output the final packing ... and see if there is any
                        // mode conflict. (T.Besson)
                        // 'isClusterLegal' turns to false if there is a mode conflict.
                        //
                        isClusterLegal = rsbe::check_if_xml_mode_conflict(packer_opts, arch,
                                                                        clustering_data.intra_lb_routing);

                        // Remove the previous pushed "intra_lb_routing_solution" to clean up
                        // the place. (T.Besson)
                        //
                        (clustering_data.intra_lb_routing).pop_back();

                        if (!isClusterLegal &&
                            (detailed_routing_stage == (int)E_DETAILED_ROUTE_FOR_EACH_ATOM)) {

                            VTR_LOGV(verbosity > 0, "Info: rejected cluster packing solution with modes conflict [%d]\n",
                                    max_nb_molecule);
                        }
                        }

                        if (isClusterLegal) {

                            istart = save_cluster_routing_and_pick_new_seed(packer_opts, helper_ctx.total_clb_num,
                                                        seed_atoms, num_blocks_hill_added, clustering_data.intra_lb_routing,
                                                        seedindex, cluster_stats, router_data);

                            store_cluster_info_and_free(packer_opts, clb_index, logic_block_type, le_pb_type,
                                                        le_count, clb_inter_blk_nets);

                            nb_packed_molecules += max_nb_molecule;

                            VTR_LOGV(verbosity > 0, "Successfully packed Logic Block [%d]\n", max_nb_molecule);

                            failed_for_loop = false; // tell the outer loop that we succeeded within this loop

                        } else {
                        free_data_and_requeue_used_mols_if_illegal(clb_index, savedseedindex,
                                                    num_used_type_instances, helper_ctx.total_clb_num, seedindex);
                        }


                    } else {
                        free_data_and_requeue_used_mols_if_illegal(clb_index, savedseedindex,
                                                    num_used_type_instances, helper_ctx.total_clb_num, seedindex);
                    }

                    for (uint index = 0; index < clusterMoleculeOrder[partId].size(); index++) {
                        istart = par.molecules_[clusterMoleculeOrder[partId][index]];
                        if (istart->valid) {
                            break;
                        }
                    }
                    if (!istart->valid) {
                        istart = nullptr;
                    }

                    free_router_data(router_data);
                    router_data = nullptr;
                }

            }
        } //// for(int partId=0;partId < numberOfClusters;partId++)
    } else {

        while (istart != nullptr) {
            isClusterLegal = false;
            int saved_seed_index = seed_index;
            for (detailed_routing_stage = (int)E_DETAILED_ROUTE_AT_END_ONLY; !isClusterLegal && detailed_routing_stage != (int)E_DETAILED_ROUTE_INVALID; detailed_routing_stage++) {
                // Use the total number created clusters so far as the ID for the new cluster
                ClusterBlockId clb_index(helper_ctx.total_clb_num);

                VTR_LOGV(verbosity > 2, "Complex block %d:\n", helper_ctx.total_clb_num);

                /*Used to store cluster's PartitionRegion as primitives are added to it.
                 * Since some of the primitives might fail legality, this structure temporarily
                 * stores PartitionRegion information while the cluster is packed*/
                PartitionRegion temp_cluster_pr;
                /*
                 * Stores the cluster's NoC group ID as more primitives are added to it.
                 * This is used to check if a candidate primitive is in the same NoC group
                 * as the atom blocks that have already been added to the primitive.
                 */
                NocGroupId temp_cluster_noc_grp_id = NocGroupId::INVALID();

                start_new_cluster(helper_ctx.cluster_placement_stats, helper_ctx.primitives_list,
                                  clb_index, istart,
                                  num_used_type_instances,
                                  packer_opts.target_device_utilization,
                                  helper_ctx.num_models, helper_ctx.max_cluster_size,
                                  arch, packer_opts.device_layout,
                                  lb_type_rr_graphs, &router_data,
                                  detailed_routing_stage, &cluster_ctx.clb_nlist,
                                  primitive_candidate_block_types,
                                  verbosity,
                                  packer_opts.enable_pin_feasibility_filter,
                                  balance_block_type_utilization,
                                  packer_opts.feasible_block_array_size,
                                  temp_cluster_pr,
                                  temp_cluster_noc_grp_id);

                //initial molecule in cluster has been processed
                cluster_stats.num_molecules_processed++;
                cluster_stats.mols_since_last_print++;
                print_pack_status(helper_ctx.total_clb_num,
                                  cluster_stats.num_molecules,
                                  cluster_stats.num_molecules_processed,
                                  cluster_stats.mols_since_last_print,
                                  device_ctx.grid.width(),
                                  device_ctx.grid.height(),
                                  attraction_groups);

                VTR_LOGV(verbosity > 2,
                         "Complex block %d: '%s' (%s) ", helper_ctx.total_clb_num,
                         cluster_ctx.clb_nlist.block_name(clb_index).c_str(),
                         cluster_ctx.clb_nlist.block_type(clb_index)->name);
                VTR_LOGV(verbosity > 2, ".");
                //Progress dot for seed-block
                fflush(stdout);

                t_ext_pin_util target_ext_pin_util = helper_ctx.target_external_pin_util.get_pin_util(cluster_ctx.clb_nlist.block_type(clb_index)->name);
                int high_fanout_threshold = helper_ctx.high_fanout_thresholds.get_threshold(cluster_ctx.clb_nlist.block_type(clb_index)->name);
                update_cluster_stats(istart, clb_index,
                                     is_clock,  //Set of clock nets
                                     is_global, //Set of global nets (currently all clocks)
                                     packer_opts.global_clocks,
                                     packer_opts.alpha, packer_opts.beta,
                                     packer_opts.timing_driven, packer_opts.connection_driven,
                                     high_fanout_threshold,
                                     *timing_info,
                                     attraction_groups,
                                     net_output_feeds_driving_block_input);
                helper_ctx.total_clb_num++;

                if (packer_opts.timing_driven) {
                    cluster_stats.blocks_since_last_analysis++;
                    /*it doesn't make sense to do a timing analysis here since there*
                     *is only one atom block clustered it would not change anything      */
                }
                cur_cluster_placement_stats_ptr = &(helper_ctx.cluster_placement_stats[cluster_ctx.clb_nlist.block_type(clb_index)->index]);
                cluster_stats.num_unrelated_clustering_attempts = 0;
                next_molecule = get_molecule_for_cluster(cluster_ctx.clb_nlist.block_pb(clb_index),
                                                         attraction_groups,
                                                         allow_unrelated_clustering,
                                                         packer_opts.prioritize_transitive_connectivity,
                                                         packer_opts.transitive_fanout_threshold,
                                                         packer_opts.feasible_block_array_size,
                                                         &cluster_stats.num_unrelated_clustering_attempts,
                                                         cur_cluster_placement_stats_ptr,
                                                         clb_inter_blk_nets,
                                                         clb_index,
                                                         verbosity,
                                                         clustering_data.unclustered_list_head,
                                                         unclustered_list_head_size,
                                                         primitive_candidate_block_types);
                prev_molecule = istart;

                /*
                 * When attraction groups are created, the purpose is to pack more densely by adding more molecules
                 * from the cluster's attraction group to the cluster. In a normal flow, (when attraction groups are
                 * not on), the cluster keeps being packed until the get_molecule routines return either a repeated
                 * molecule or a nullptr. When attraction groups are on, we want to keep exploring molecules for the
                 * cluster until a nullptr is returned. So, the number of repeated molecules allowed is increased to a
                 * large value.
                 */
                int max_num_repeated_molecules = 0;
                if (attraction_groups.num_attraction_groups() > 0) {
                    max_num_repeated_molecules = ATTRACTION_GROUPS_MAX_REPEATED_MOLECULES;
                } else {
                    max_num_repeated_molecules = 1;
                }
                int num_repeated_molecules = 0;

                while (next_molecule != nullptr && num_repeated_molecules < max_num_repeated_molecules) {
                    prev_molecule = next_molecule;

                    try_fill_cluster(packer_opts,
                                     cur_cluster_placement_stats_ptr,
                                     prev_molecule,
                                     next_molecule,
                                     num_repeated_molecules,
                                     helper_ctx.primitives_list,
                                     cluster_stats,
                                     helper_ctx.total_clb_num,
                                     helper_ctx.num_models,
                                     helper_ctx.max_cluster_size,
                                     clb_index,
                                     detailed_routing_stage,
                                     attraction_groups,
                                     clb_inter_blk_nets,
                                     allow_unrelated_clustering,
                                     high_fanout_threshold,
                                     is_clock,
                                     is_global,
                                     timing_info,
                                     router_data,
                                     target_ext_pin_util,
                                     temp_cluster_pr,
                                     temp_cluster_noc_grp_id,
                                     block_pack_status,
                                     clustering_data.unclustered_list_head,
                                     unclustered_list_head_size,
                                     net_output_feeds_driving_block_input,
                                     primitive_candidate_block_types);
                }

                isClusterLegal = check_cluster_legality(verbosity, detailed_routing_stage, router_data);

                if (isClusterLegal) {
                    istart = save_cluster_routing_and_pick_new_seed(packer_opts, helper_ctx.total_clb_num, seed_atoms, num_blocks_hill_added, clustering_data.intra_lb_routing, seed_index, cluster_stats, router_data);
                    store_cluster_info_and_free(packer_opts, clb_index, logic_block_type, le_pb_type, le_count, clb_inter_blk_nets);
                } else {
                    free_data_and_requeue_used_mols_if_illegal(clb_index, saved_seed_index, num_used_type_instances, helper_ctx.total_clb_num, seed_index);
                }
                free_router_data(router_data);
                router_data = nullptr;
            }
        }
    }

    // if this architecture has LE physical block, report its usage
    if (le_pb_type) {
        print_le_count(le_count, le_pb_type);
    }

    //check_floorplan_regions(floorplan_regions_overfull);
    floorplan_regions_overfull = floorplan_constraints_regions_overfull();

    return num_used_type_instances;
}

/**
 * Print the total number of used physical blocks for each pb type in the architecture
 */
void print_pb_type_count(const ClusteredNetlist& clb_nlist) {
    auto& device_ctx = g_vpr_ctx.device();

    std::map<t_pb_type*, int> pb_type_count;

    size_t max_depth = 0;
    for (ClusterBlockId blk : clb_nlist.blocks()) {
        size_t pb_max_depth = update_pb_type_count(clb_nlist.block_pb(blk), pb_type_count, 0);

        max_depth = std::max(max_depth, pb_max_depth);
    }

    size_t max_pb_type_name_chars = 0;
    for (auto& pb_type : pb_type_count) {
        max_pb_type_name_chars = std::max(max_pb_type_name_chars, strlen(pb_type.first->name));
    }

    VTR_LOG("\nPb types usage...\n");
    for (const auto& logical_block_type : device_ctx.logical_block_types) {
        if (!logical_block_type.pb_type) continue;

        print_pb_type_count_recurr(logical_block_type.pb_type, max_pb_type_name_chars + max_depth, 0, pb_type_count);
    }
    VTR_LOG("\n");
}
