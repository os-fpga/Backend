set STD_CELL_PATH	$::env(STD_CELL_PATH)
set TARGET_LIB		$::env(TARGET_LIB)
set BRAM_MEM_PATH	$::env(BRAM_MEM_PATH)
set BRAM_MEM		$::env(BRAM_MEM)

set search_path [concat $search_path ${STD_CELL_PATH} ]
set search_path [concat $search_path ${BRAM_MEM_PATH} ]
set link_library   [concat * \
   ${BRAM_MEM}.db \
   ${TARGET_LIB}.db \
]

# slow/slow, -40C (temperature inversion WC)
set target_library   [concat [list] \
   ${BRAM_MEM}.db \
   ${TARGET_LIB}.db \
]

#set_dont_use [get_lib_cell */*D0*]
