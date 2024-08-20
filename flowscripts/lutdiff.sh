#!/bin/bash

echo GOLDEN
sed -e '/^1.*clb_8__3_.*fle_7.*frac_lut6.*RS_LATCH/!D' -e 's/.*\[//' -e 's/.$//' golden.csv | sort -n

echo TOP
sed -e '/^1.*clb_8__3_.*fle_7.*frac_lut6.*RS_LATCH/!D' -e 's/.*\[//' -e 's/.$//'    top.csv | sort -n
