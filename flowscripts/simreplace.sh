#!/bin/bash

scb_dir=/nfs_home/dhow/workarea/restart/ArchBench/Testcases/up5bit_counter/up5bit_counter_golden/SRC/n
top_src=/nfs_project/castor/dhow/workarea/restart/ArchBench/Testcases/up5bit_counter/up5bit_counter_golden/fpga_out.v
rrg_src=/nfs_project/castor/dhow/workarea/restart/ArchBench/Testcases/up5bit_counter/up5bit_counter_golden/stamp.stamped.xml

# Step 0: back up files we are aboutt to change
if [ ! -e BACKUP ] ; then
	echo 0/5 Creating back up
	mkdir BACKUP
	tar cf - routing fpga_top.v stamp.stamped.xml stamp.stamped.pyx | ( cd BACKUP ; tar xf - )
fi

# put new mux and mem modules here
muxmems=cbx_1__1_.v
# put new routing subtiles here
subtiles=cby_1__1_.v

# Step 1: make all current routing modules empty
echo 1/5 Removing old routing block definitions
for f in routing/*.v ; do
	echo "// moved into $subtiles" > $f
done

# Step 2: put definitions of ALL routing modules into cby_1__1_.v
echo 2/5 Collecting new routing subtile definitions
cat $scb_dir/*.v > routing/$subtiles

# Step 3: put definitions of ALL missing muxes/mems into cbx_1__1_.v
echo 3/5 Generating missing mux/mem module definitions
./genmuxmem routing/$subtiles sub_module/muxes.v sub_module/memories.v > routing/$muxmems

# Step 4: update fpga_top.v
echo 4/5 Updating top array
cp $top_src fpga_top.v

# Step 5: bring over the stamped routing graph
echo 5/5 Copying stamped routing graph
cp $rrg_src .
xmlstarlet pyx < $(basename $rrg_src) > $(basename $rrg_src .xml).pyx

echo DONE

