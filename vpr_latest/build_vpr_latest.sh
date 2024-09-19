#!/bin/bash

printf "\n   building vpr_latest in $(pwd)\n"

hw_num_cores=`grep -F "model name" /proc/cpuinfo | wc -l`

export CPU_CORES=${CPU_CORES:="$hw_num_cores"}

printf "   CPU_CORES= $CPU_CORES   hw_num_cores= $hw_num_cores\n\n"

cd vtr && make -j $CPU_CORES

vtr_make_status=$?

printf "\n   vtr_make_status= $vtr_make_status\n"
printf "\n   done building vpr_latest in $(pwd)\n\n"

