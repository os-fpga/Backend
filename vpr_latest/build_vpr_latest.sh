#!/bin/bash

cur_dir=$(pwd)
printf "\n   building vpr_latest in $cur_dir\n"

hw_num_cores=`grep -F "model name" /proc/cpuinfo | wc -l`

export CPU_CORES=${CPU_CORES:="$hw_num_cores"}

printf "   CPU_CORES= $CPU_CORES   hw_num_cores= $hw_num_cores\n"

vpr_latest_bin_dir="$cur_dir/vtr/build/vpr"

mkdir -p vtr
pushd vtr
ls -1 Makefile || git submodule update --init --recursive
popd

# copy the patched CMakefile
# include/CMAKE_fix/PATCHED_vpr_latest/CMakeLists.txt
if (( $# > 0 )); then
  cp -v $1 vtr
fi

# do make in vtr
echo
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
    echo "build_vpr_latest SUCCEEDED"
else
  echo
  echo "build_vpr_latest: vpr binary not found: $vpr_latest_bin_dir/vpr"
  echo "build_vpr_latest FAILED"
  echo
fi

printf "\n   done building vpr_latest in $cur_dir\n\n"

