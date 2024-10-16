yosys -import
read_liberty -ignore_miss_func /cadlib/virgo/TSMC16NMFFC/library/std_cells/dti/7p5t/rev_181022/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid_fe_views_svt/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid/lib/dti_tm16ffc_90c_7p5t_stdcells_ssgnp_0p72v_125c_rev1p0p0.lib
read_verilog tmp_muxes.v
hierarchy -check -top $::env(MUX_TOP)
stat -liberty /cadlib/virgo/TSMC16NMFFC/library/std_cells/dti/7p5t/rev_181022/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid_fe_views_svt/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid/lib/dti_tm16ffc_90c_7p5t_stdcells_ssgnp_0p72v_125c_rev1p0p0.lib
procs
flatten
muxcover -mux4=110
abc -liberty /cadlib/virgo/TSMC16NMFFC/library/std_cells/dti/7p5t/rev_181022/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid_fe_views_svt/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid/lib/dti_tm16ffc_90c_7p5t_stdcells_ssgnp_0p72v_125c_rev1p0p0.lib
stat -liberty /cadlib/virgo/TSMC16NMFFC/library/std_cells/dti/7p5t/rev_181022/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid_fe_views_svt/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid/lib/dti_tm16ffc_90c_7p5t_stdcells_ssgnp_0p72v_125c_rev1p0p0.lib
show
write_verilog $::env(TMP_MUX_OUT)
