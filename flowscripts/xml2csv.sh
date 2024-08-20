#!/bin/bash

des=and_openfpga_sample_bitstream
xml=$des.xml.orig
csv=$des.csv

sed -e '/path=/!D' -e 's/.*value="//' -e 's/" path="/,/' -e 's/">$//' $xml | LC_ALL=C sort -t , -k 2 > $csv
