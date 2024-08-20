#!/bin/bash

xml2map=/nfs_project/castor/dhow/workarea/openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task/scripts/xml2map.py

cd /nfs_project/castor/castor_release/v1.6.274/k6n8_TSMC16nm_7.5T/FPGA10x8_gemini_compact_latch_pnr/
$xml2map _run_dir/and_openfpga_sample_bitstream.xml  | sort -n --field-separator=, -k 1,1 -k 2,2 | gzip > cbmapping.csv.gz

cd ../FPGA62x44_gemini_compact_latch_pnr/
$xml2map _run_dir/and_openfpga_sample_bitstream.xml  | sort -n --field-separator=, -k 1,1 -k 2,2 | gzip > cbmapping.csv.gz

