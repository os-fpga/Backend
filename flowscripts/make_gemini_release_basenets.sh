#!/bin/bash

basenets=/nfs_project/castor/dhow/workarea/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task/scripts/basenets.py

cd /nfs_project/castor/castor_release/v1.6.274/k6n8_TSMC16nm_7.5T/FPGA10x8_gemini_compact_latch_pnr/
$basenets --define DTI --read_design netlist_integrated.f > basenets.pin ; /bin/rm -f basenets.pin.gz ; gzip basenets.pin

cd ../FPGA62x44_gemini_compact_latch_pnr/
$basenets --define DTI --read_design netlist_integrated.f > basenets.pin ; /bin/rm -f basenets.pin.gz ; gzip basenets.pin

# ../FPGA104x68_gemini_compact_latch_pnr/ : Verilog is incomplete

