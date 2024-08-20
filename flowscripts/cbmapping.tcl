#!/nfs_cadtools/softwares/starvision/starvision-7.2.1/bin/starsh

if {[llength $argv] > 0} {
	set sffile [lindex $argv 0]
} else {
	set sffile netlist_integrated.f
}
if {[llength $argv] > 1} {
	set scsv [lindex $argv 1]
} else {
	set scsv "cbmapping.csv"
}

set szdb "$scsv.zdb"
regsub {^(..*).csv$} $scsv {\1.zdb} szdb
set slog "$scsv.log"
regsub {^(..*).csv$} $scsv {\1.log} slog

puts "2/6 Running rtl2zdb $sffile --> $szdb $slog"
exec -ignorestderr -- rtl2zdb -wait_for_license 600 -preserveAssign off -f rigel_behav_model.f -f $sffile -top fpga_top +define+DTI +define+DTI -o $szdb -logfile $slog -pedantic off

puts "3/6 Opening $szdb"
set db [zdb open $szdb]
set fo [open $scsv w]

set otopmod [list "module" "fpga_top" "fpga_top"]

set dsnet2lsbits [dict create]

puts "4/6 Visiting nets"
$db flat foreach signal $otopmod osignal {

	# top only
	if { [llength $osignal] > 3 } { continue}

	# BL/WL only
	set snet [lindex $osignal 2]
	if { [string first bl__ $snet] == -1 && [string first wl__ $snet] == -1 } { continue }
	#set slead [string range $snet 0 2]

	#puts "A $osignal"
	#puts "B $snet"
	#puts "C $slead"
	$db flat foreach pin -addHier -stopHier $osignal opin {

		set stype [$db oid type $opin]
		set lspath [$db oid path $opin]
		#set cpath [llength $lspath]

		if { $stype != "port" } { continue }

		set omodport [$db oid createModBased $opin]

		if { [lindex $omodport 1] != "RS_LATCH" } { continue }

		# convert a b RS_LATCH_\d+_ to fpga_top.a.b.mem_out[\d+]
		if { ![regsub {^RS_LATCH_([0-9][0-9]*)_$} [lindex $lspath end] {mem_out[\1]} sresult] } {
			error "could not understand $lspath"
		}
		set lspath2 [linsert $lspath 0 "fpga_top"]
		lset lspath2 end $sresult
		set spath [join $lspath2 "."]

		#puts "1 stype $stype"
		#puts "2 lspath $lspath"
		#puts "3 cpath $cpath"
		#puts "4 omodport $omodport"
		#puts "5 spath $spath"

		dict lappend dsnet2lsbits $snet $spath

	}
	#puts ""
}

puts "5/6 Translating nets to BL/WL"

set lsnet [lsort -dictionary [dict keys $dsnet2lsbits]]

set all [llength $lsnet]
set nbl [lsearch -regexp $lsnet {^wl_}]
set nwl [expr {$all - $nbl}]

set dsbit2cbl [dict create]
for {set cbl 0} {$cbl < $nbl} {incr cbl} {
	foreach sbit [dict get $dsnet2lsbits [lindex $lsnet $cbl]] {
		dict set dsbit2cbl $sbit $cbl
	}
}

set dsbit2cwl [dict create]
for {         } {$cbl < $all} {incr cbl} {
	set cwl [expr {$cbl - $nbl}]
	foreach sbit [dict get $dsnet2lsbits [lindex $lsnet $cbl]] {
		dict set dsbit2cwl $sbit $cwl
	}
}

set nb [llength [dict keys $dsbit2cbl]]
if { $nb != [llength [dict keys $dsbit2cwl]] } {
	error "BL and WL mappings are unequal in size" 
}

# this doesn't need to be sorted
puts "6/6 Writing BL/WL bit map --> $scsv"
dict for {sbit cbl} $dsbit2cbl {
	set cwl [dict get $dsbit2cwl $sbit]
	puts $fo "$cbl,$cwl,$sbit"
}
#puts "nbl=$nbl nwl=$nwl"
set nv [expr {round(100 * (1 - ($nb / double($nbl * $nwl))))}]
puts "Wrote $nb config bit mappings for a $nbl BL * $nwl WL array ($nv% vacant)"

close $fo
