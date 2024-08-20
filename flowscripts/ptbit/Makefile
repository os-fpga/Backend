# Local Make Variables
TSMC	?= false
# Variables Used by Synthesis
ifeq ($(TSMC), true)
export VENDOR_ID		:=tsmc
export TARGET_LIB		:=tcbn16ffcllbwp7d5t16p96cpdssgnp0p72vm40c_ccs
export STD_CELL_PATH	:=/cadlib/gemini/TSMC16NMFFC/library/std_cells/tsmc/7.5t/tcbn16ffcllbwp7d5t16p96cpd_100i/tcbn16ffcllbwp7d5t16p96cpd_100d_ccs/TSMCHOME/digital/Front_End/timing_power_noise/CCS/tcbn16ffcllbwp7d5t16p96cpd_100d
else
PROCESS			:=tm16ffc_90c
TRACK			:=7p5t
TECH			:=stdcells
CORNER			:=ssgnp
VOLTAGE			:=0p72v
TEMP			:=125c
REL_ID			:=rev1p0p0
SDC_FILE		:=
export VENDOR_ID		:=dti
export TARGET_LIB		:=${VENDOR_ID}_${PROCESS}_${TRACK}_${TECH}_${CORNER}_${VOLTAGE}_${TEMP}_${REL_ID}
export STD_CELL_PATH	:=/cadlib/gemini/TSMC16NMFFC/library/std_cells/dti/7p5t/rev_181022/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid_fe_views_svt/221018_dti_tm16ffc_90c_7p5t_stdcells_rev1p0p8_rapid/db
endif
export BRAM_MEM_PATH	:=/cadlib/gemini/TSMC16NMFFC/library/memory/dti/memories/rev_111022/101122_tsmc16ffc_DP_RAPIDSILICON_GEMINI_rev1p0p3_BE/dti_dp_tm16ffcll_1024x18_t8bw2x_m_hc
export BRAM_MEM			:=dti_dp_tm16ffcll_1024x18_t8bw2x_m_hc_ssgn40c

# Targets
default:
	echo "make {synth,timing}"

dirs:
	mkdir -p WORK
	mkdir -p report

load_fabric: dirs
	script --flush --command 'dc_shell-t -f load_fabric.tcl' report/$(shell date +"%Y_%m_%d_%I_%M_%p").log

bram: dirs
	script --flush --command 'dc_shell-t -f synth_bram.tcl' report/$(shell date +"%Y_%m_%d_%I_%M_%p").log

dsp: dirs
	script --flush --command 'dc_shell-t -f synth_dsp.tcl' report/$(shell date +"%Y_%m_%d_%I_%M_%p").log

clean:
	rm -rf WORK/ report/ command.log  default.svf
