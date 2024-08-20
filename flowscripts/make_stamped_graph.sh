#!/bin/bash

scr=make_stamped_graph

# stderr to file and screen
# stdout to file
exec 2> >(tee $scr.err >&2)
exec  >       $scr.out

# generate a backup
bakroot=${scr}.backup
gen=$(compgen -G ${bakroot}.'*' | wc -l)
bakdir=${bakroot}.$gen
mkdir -p $bakdir
mv stamp.{collide,jumpmap,pyx,pinmap,cco,unfound,ccu,badchan,json,stamped.xml,log,looped.xml} $bakdir

# run the stamper
./stamp.py -c stamp.xml

exit $?
