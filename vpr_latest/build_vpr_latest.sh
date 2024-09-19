#!/bin/bash

cur_dir=$(pwd)
printf "\n   building vpr_latest in $cur_dir\n"

hw_num_cores=`grep -F "model name" /proc/cpuinfo | wc -l`

export CPU_CORES=${CPU_CORES:="$hw_num_cores"}

printf "   CPU_CORES= $CPU_CORES   hw_num_cores= $hw_num_cores\n"

vpr_latest_bin_dir="$cur_dir/vtr/build/vpr"
printf "\n   vpr_latest_bin_dir= $vpr_latest_bin_dir\n\n"


cd vtr && make -j $CPU_CORES


vtr_make_status=$?

printf "\n   vtr_make_status= $vtr_make_status\n"
printf "   vpr_latest_bin_dir= $vpr_latest_bin_dir\n"

printf "\n   "
ls -1 "$vpr_latest_bin_dir/vpr"
file_status=$?
if (( $file_status == 0 )); then
    printf "\ncopying to vpr_latest..\n"
    cp -v "$vpr_latest_bin_dir/vpr" "$cur_dir/vpr_latest"
fi

printf "\n   done building vpr_latest in $cur_dir\n\n"

