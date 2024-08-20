#!/bin/bash

sed	-e 's/[0-9][0-9]*/#/g' \
	-e 's/grid_io_[a-z]*_/grid_io_SIDE_/' \
	-e 's/\.cb[xy]_#__#_\.mem_[a-z]*_ipin_/.cbXY_#__#_.mem_SIDE_ipin_/' \
	-e 's/mem_[a-z]*_track_/mem_SIDE_track_/' \
	-e 's/grid_[a-z]*_#__#_\.logical_tile_[a-z}_mode_[a-z]*__#\.mem_[a-z]*_lr_#_\([a-zA-Z_0-9#]*\)_#\.mem_out/grid_BLOCK_#__#_\.logical_tile_BLOCK_mode_BLOCK__#.mem_BLOCK_lr_#_PIN_#.mem_out/' \
	$* | LC_ALL=C sort | LC_ALL=C uniq -c | awk '{s+=$1; printf "%2d %s\n", NR, $0}END{printf "   %7d\n",s}'
