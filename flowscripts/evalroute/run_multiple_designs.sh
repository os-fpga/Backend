#!/bin/bash

#set -x	# expand variables and print "+ command" before executing command
#set -v	# prints before expanding

###############################################################################################
# Description:
# Specify the name of the pnr directory and design(s) and run collect_stats for them
# For required and optional arguments of collect_stats, read its description.
#
# To generate node-based visualized statistics for all testbenches in one chart run
# plot_results.py in the top directory where this script exist, using "all" argument
#                       working_dir=$(pwd)
#                       python3 plot_results.py "$working_dir" all
# To generate only for a one testbench, go the testbench's node-based directory and
# run plot_results.py there:
#                       testbench_dir=/[testbench directory]
#                       python3 plot_results.py "$testbench_dir"     
#       
###############################################################################################
ARCHBENCH=`git rev-parse --show-toplevel`
ALL_TESTCASES=$ARCHBENCH/Testcases
WORKING_DIR=$(pwd)
OPENFPGA_PD_CASTOR_RS=$ARCHBENCH/openfpga-pd-castor-rs
EVALROUTE=$OPENFPGA_PD_CASTOR_RS/k6n8_TSMC16nm_7.5T/CommonFiles/task/automation/evalroute

# Set the output and error log file paths
log_file="$ALL_TESTCASES/run_multiple_designs.log"

# Redirect standard outputs and standard errors to the log file
exec > >(tee "$log_file") 2>&1

# use vpr rr_graph
run_vpr=true

# use stamp rr_graph with g_safe = true
run_stamp_gsafe_true=false

# use stamp rr_graph with g_safe = false
run_stamp_gsafe_false=false

# designs for small device
run_small=false

# designs for large device
run_big=true

# draw a figure for all test cases
visualize_all=false

# run designs in parallel, sequentially, or just echo them
parallel=echo			# echo runs in sequence
parallel=true			# issue runs in parallel
parallel=false			# issue runs one at a time

# what stats
statmode=""						                            # no stat
statmode="--gen_nd_tx --gen_nd_vs --gen_eb_no --gen_eb_wo"	# all
statmode="--gen_nd_tx"

# NOTE: These variables are mostly used unquoted, so must not be null

# Congestion-aware packing/place example:
congestion_aware="--target_ext_pin_util 0.8 --use_partitioning_in_pack on --number_of_molecules_in_partition 200 --enable_cascade_placer on --place_algorithm congestion_aware"
congestion_aware="--target_ext_pin_util 0.8"

# read/write rr_graph example:
read_rr_graph="--read_rr_graph /nfs_project/castor/dhow/workarea/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/FPGA104x68_gemini_compact_stamp_pnr/fabric_task/flow_inputs/rr_graph.stamped.xml"
write_rr_graph="--write_rr_graph /nfs_project/castor/mabbaszadeh/workarea/test/rr_graph.xml"

# packing options:
packing_options="--packing_options -clb_packing timing_driven --packing_options_end"

# pnr_options:
pnr_options="--pnr_options $congestion_aware $read_rr_graph $write_rr_graph --seed 2 --pnr_options_end"
pnr_options="--pnr_options $congestion_aware $read_rr_graph --timing_report_npaths 1000 --pnr_options_end"

# use --cp_run DIR to copy the run directory as well as routing stats for current run to DIR. Example:
cp_run="--cp_run /nfs_project/castor/mabbaszadeh/workarea/exp5"

# use --gen_place_const to generate correct placement constraint (use valid IO slots). Example:
gen_place_const="--gen_place_const"


ArchBench_10x8_designs="add_1bit and2 and8 or_1bit design3_5_5_top dffre_inst lut_ff_mux MultiplierLUT mux_8_1 mux_8_1_a shift_register up5bit_counter"
ArchBench_104x68_designs="AES_DECRYPT b19 bch_configurable canny_edge_detection cordic"
MCNC_designs="alu4 apex2 apex4 bigkey clma des diffeq dsip elliptic ex1010 ex5p frisc misex3 pdc s298 s38417 s38584 seq spla tseng"
# (LU32PEEng, mcml excluded)
VTR_designs="bgm blob_merge boundtop ch_intrinsics diffeq1 diffeq2 LU8PEEng mkDelayWorker32B mkPktMerge mkSMAdapter4B or1200 raygentop sha stereovision0 stereovision1 stereovision2 stereovision3"

###################################################### VPR RR GRAPH ######################################################
if [ "$run_vpr" == true ]; then

    # # Run testbenches using vpr rr_graph

    # --gen_arch --gen_vpr_rrg add them if required
    first_run="--gen_arch --gen_vpr_rrg" # use it only once
    first_run=""
    device="FPGA10x8_gemini_compact_latch_pnr"
    
    designs=$ArchBench_10x8_designs

    for d in $designs ; do
        if [ "$run_small" != true ]; then continue ; fi
        
        echo EXECUTING non-SR collect_stats $d

        # custom working directory
        working_dir="/nfs_project/castor/mabbaszadeh/workarea/exp2/$d"

        # normal run
        VTR_command="./collect_stats.sh $device $d --clean $first_run --gen_bs --gen_vpr_route $statmode $packing_options $pnr_options $gen_place_const"

        # This only copies the entire run directory of vpr run
        CP_command="./collect_stats.sh $device $d --gen_vpr_route --cp_run $working_dir"
        
        #restart routing from working directory
        restart_command="./collect_stats.sh $device $d --clean --gen_vpr_route --restart_route --work_dir $working_dir"

        if [ $parallel == true ] ; then
            ( $VTR_command > $d.biglog 2>&1 & )
        elif [ $parallel == false ] ; then
            ( $VTR_command )
        else
            echo $VTR_command
        fi
            first_run=""
    done

    ###################################

    # --gen_arch --gen_vpr_rrg add them if required
    first_run="--gen_arch --gen_vpr_rrg" # use it only once
    first_run=""
    device="FPGA104x68_gemini_compact_latch_pnr"
    
    designs=$ArchBench_104x68_designs
    designs=$MCNC_designs
    designs=$VTR_designs
    designs="alu4"
    
    for d in $designs ; do
        if [ "$run_big" != true ]; then continue ; fi
        echo EXECUTING non-SR collect_stats $d

        # custom working directory
        working_dir="/nfs_project/castor/mabbaszadeh/workarea/exp2/$d"

        # normal run
        congestion_aware="--target_ext_pin_util 0.8 --place_algorithm congestion_aware"
        congestion_aware="--target_ext_pin_util 0.8"
        packing_options="--packing_options -clb_packing timing_driven --packing_options_end"
        statmode="--gen_nd_tx"
        
        if [ "$d" == "bgm" ] || [ "$d" == "stereovision2" ]; then
            congestion_aware="--target_ext_pin_util 0.8 --use_partitioning_in_pack on --number_of_molecules_in_partition 250 --alpha_clustering 0.1"
        fi

        pnr_options="--pnr_options $congestion_aware --timing_report_npaths 1000 --pnr_options_end"
        
        VTR_command="./collect_stats.sh $device $d --clean $first_run --gen_bs --gen_vpr_route $statmode $packing_options $pnr_options $gen_place_const"

        # This only copies the entire run directory of vpr run
        CP_command="./collect_stats.sh $device $d --gen_vpr_route --cp_run $working_dir"
        
        #restart routing from working directory
        restart_command="./collect_stats.sh $device $d --clean --gen_vpr_route --restart_route --work_dir $working_dir"

        # only collect stats
        stats_command="./collect_stats.sh $device $d --clean $first_run --gen_vpr_route $statmode"

        if [ $parallel == true ] ; then
            ( $VTR_command > $d\_VPR.biglog 2>&1 & )
        elif [ $parallel == false ] ; then
            ( $VTR_command )
            # ( $stats_command )
        else
            echo $VTR_command
        fi
            first_run=""
    done

fi

###################################################### STAMP RR GRAPH G_SAFE = TRUE ######################################################
if [ "$run_stamp_gsafe_true" == true ]; then

    # --gen_arch --gen_vpr_rrg --gen_stamp_rrg add them if required
    first_run="--gen_arch --gen_stamp_rrg" # use it only once
    first_run=""
    device=FPGA10x8_gemini_compact_stamp_pnr

    # ArchBench 10x8 Designs
    designs=$ArchBench_10x8_designs

    for d in $designs ; do
        if [ "$run_small" != true ]; then continue ; fi
        
        echo EXECUTING SR collect_stats $d

        # custom working directory
        working_dir="/nfs_project/castor/mabbaszadeh/workarea/exp2/$d"

        # normal run
        SR_command="./collect_stats.sh $device $d --clean $first_run --g_safe --gen_bs_stamp --gen_stamp_route $statmode $gen_place_const $pnr_options $packing_options"

        # only copy the entire run directory of SR run
        CP_command="./collect_stats.sh $device $d --gen_stamp_route --g_safe --cp_run $working_dir"

        # copy VPR run directory as SR directory and create SR raptor.tcl
        CP_VPR_command="./collect_stats.sh $device $d --clean --gen_stamp_route --g_safe --gen_bs_stamp --cp_run $working_dir --vpr_based_stamp_run  $gen_place_const $pnr_options $packing_options"
        
        #restart routing from working directory
        restart_command="./collect_stats.sh $device $d --clean --gen_stamp_route --g_safe --restart_route --work_dir $working_dir --vpr_based_stamp_run"

        if [ $parallel == true ] ; then
            ( $SR_command > $d.biglog 2>&1 & )
        elif [ $parallel == false ] ; then
            ( $SR_command )
        else
            echo $SR_command
        fi
            first_run=""
    done

    ###################################

    # --gen_arch --gen_vpr_rrg --gen_stamp_rrg add them if required
    first_run="--gen_arch --gen_vpr_rrg --gen_stamp_rrg" # use it only once
    first_run=""
    device=FPGA104x68_gemini_compact_stamp_pnr

    designs=$ArchBench_104x68_designs
    designs=$MCNC_designs
    designs=$VTR_designs

    for d in $designs ; do
        if [ "$run_big" != true ]; then continue ; fi
        
        echo EXECUTING SR collect_stats $d

        # custom working directory
        working_dir="/nfs_project/castor/mabbaszadeh/workarea/exp2/$d"

        # normal run
        SR_command="./collect_stats.sh $device $d --clean $first_run --g_safe --gen_bs_stamp --gen_stamp_route $statmode $gen_place_const $pnr_options $packing_options"

        # only copy the entire run directory of SR run
        CP_command="./collect_stats.sh $device $d --gen_stamp_route --g_safe --cp_run $working_dir"

        # copy VPR run directory as SR directory and create SR raptor.tcl
        CP_VPR_command="./collect_stats.sh $device $d --clean --gen_stamp_route --g_safe --gen_bs_stamp --cp_run $working_dir --vpr_based_stamp_run  $gen_place_const $pnr_options $packing_options"
        
        #restart routing from working directory
        restart_command="./collect_stats.sh $device $d --clean --gen_stamp_route --g_safe --restart_route --work_dir $working_dir --vpr_based_stamp_run"

        if [ $parallel == true ] ; then
            ( $SR_command > $d.biglog 2>&1 & )
        elif [ $parallel == false ] ; then
            ( $SR_command )
        else
            echo $SR_command
        fi
            first_run=""
    done
fi

###################################################### STAMP RR GRAPH G_SAFE = FALSE ######################################################
if [ "$run_stamp_gsafe_false" == true ]; then

    # --gen_arch --gen_vpr_rrg --gen_stamp_rrg add them if required
    first_run="--gen_arch --gen_stamp_rrg" # use it only once
    first_run=""
    device=FPGA10x8_gemini_compact_stamp_pnr

    # ArchBench 10x8 Designs
    designs=$ArchBench_10x8_designs

    for d in $designs ; do
        if [ "$run_small" != true ]; then continue ; fi
        
        echo EXECUTING SR collect_stats $d

        # custom working directory
        working_dir="/nfs_project/castor/mabbaszadeh/workarea/exp2/$d"

        # normal run
        SR_command="./collect_stats.sh $device $d --clean $first_run --gen_bs_stamp --gen_stamp_route $statmode $gen_place_const $pnr_options $packing_options"

        # only copy the entire run directory of SR run
        CP_command="./collect_stats.sh $device $d --gen_stamp_route --cp_run $working_dir"

        # copy VPR run directory as SR directory and create SR raptor.tcl
        CP_VPR_command="./collect_stats.sh $device $d --clean --gen_stamp_route --gen_bs_stamp --cp_run $working_dir --vpr_based_stamp_run  $gen_place_const $pnr_options $packing_options"
        
        #restart routing from working directory
        restart_command="./collect_stats.sh $device $d --clean --gen_stamp_route --restart_route --work_dir $working_dir --vpr_based_stamp_run"

        if [ $parallel == true ] ; then
            ( $SR_command > $d.biglog 2>&1 & )
        elif [ $parallel == false ] ; then
            ( $SR_command )
        else
            echo $SR_command
        fi
            first_run=""
    done

    ###################################

    # --gen_arch --gen_vpr_rrg --gen_stamp_rrg add them if required
    first_run="--gen_arch --gen_vpr_rrg --gen_stamp_rrg" # use it only once
    first_run=""
    device=FPGA104x68_gemini_compact_stamp_pnr

    designs=$ArchBench_104x68_designs
    designs=$MCNC_designs
    designs=$VTR_designs
    designs="stereovision2"

    for d in $designs ; do
        if [ "$run_big" != true ]; then continue ; fi
        
        echo EXECUTING SR collect_stats $d

        # custom working directory
        working_dir="/nfs_project/castor/mabbaszadeh/workarea/exp2/$d"

        # normal run
        SR_command="./collect_stats.sh $device $d --clean $first_run --gen_bs_stamp --gen_stamp_route $statmode $gen_place_const $pnr_options $packing_options"

        # only copy the entire run directory of SR run
        CP_command="./collect_stats.sh $device $d --gen_stamp_route --cp_run $working_dir"

        # copy VPR run directory as SR directory and create SR raptor.tcl
        congestion_aware="--target_ext_pin_util 0.8"
        packing_options="--packing_options -clb_packing timing_driven --packing_options_end"
        statmode="--gen_nd_tx"

        if [ "$d" == "bgm" ] || [ "$d" == "stereovision2" ]; then
            congestion_aware="--target_ext_pin_util 0.8 --use_partitioning_in_pack on --number_of_molecules_in_partition 250 --alpha_clustering 0.1"
        fi

        pnr_options="--pnr_options $congestion_aware $read_rr_graph --timing_report_npaths 1000 --pnr_options_end"
        
        CP_VPR_command="./collect_stats.sh $device $d --clean --gen_stamp_route --gen_bs_stamp --cp_run $working_dir --vpr_based_stamp_run  $gen_place_const $pnr_options $packing_options"
        
        #restart routing from working directory
        restart_command="./collect_stats.sh $device $d --clean --gen_stamp_route --restart_route --work_dir $working_dir --vpr_based_stamp_run $statmode"

        # only collect stats
        stats_command="./collect_stats.sh $device $d --clean $first_run --gen_stamp_route $statmode --work_dir $working_dir"

        if [ $parallel == true ] ; then
            ( $CP_VPR_command > $d\_SR_CP.biglog 2>&1 & )
            ( $restart_command > $d\_SR_RS.biglog 2>&1 & )
        elif [ $parallel == false ] ; then
            ( $CP_VPR_command )
            ( $restart_command )
            # ( $stats_command )
        else
            echo $CP_VPR_command
            echo $restart_command
        fi
            first_run=""
    done
fi

###################################################### NODE_BASE STATS VISUAL FOR ALL ######################################################
if [ "$visualize_all" == true ]; then
    # node-based statistics visualization for all testbenches
    cd $ALL_TESTCASES
    python3 $EVALROUTE/plot_results.py $ALL_TESTCASES all 2> plot_all.log > plot_all.out
    cd ../
fi
