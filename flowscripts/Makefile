# destination directories
# .../openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task/arch/rr_graph_builder
CURRDIR=$(PWD)
# .../openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task/arch
ARCHDIR=$(abspath $(dir $(CURRDIR)))
# .../openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task/arch/mako
MAKODIR=$(ARCHDIR)/mako
# .../openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles/task
TASKDIR=$(abspath $(dir $(ARCHDIR)))
# .../openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T/CommonFiles
CMMNDIR=$(abspath $(dir $(TASKDIR)))
# .../openfpga-pd-castor-rs/k6n8_TSMC16nm_7.5T
PROCDIR=$(abspath $(dir $(CMMNDIR)))

# first one matches if d empty (unspecified); d=106 gets 106x77
DEV=$(firstword $(or $(filter $d%,10x8_dp_castor) $(filter $d%,106x77_final_castor) $(filter $d%,4x4_dp_castor) $(filter $d%,3x3_benchmark) d_unrecog))
FLOWDIR=$(PROCDIR)/FPGA$(DEV)_pnr/FPGA$(DEV)_task/flow_inputs

# make stampable SB/CB pattern
clb.mux other.mux: genb.py
	./genb.py

# make local interconnect
other.json clb.json: genl.py
	./genl.py

# run stamper
.PHONY: stamp
stamp:
	cd $(FLOWDIR); $(CURRDIR)/stamp.py rr_graph

MUXES=clb other right left top bottom trcorner tlcorner brcorner blcorner

# compare current files against installed
.PHONY: cmp
cmp:
	@echo ""
	@$(foreach f,other clb,md5sum {.,$(MAKODIR)}/$f.json ; ) @$(foreach f,$(MUXES),md5sum {.,$(FLOWDIR)}/$f.mux ; )
	@echo ""

# install where they will be used
.PHONY: install
install:
	cp *.json $(MAKODIR)
	cp *.mux  $(FLOWDIR)

# clean out local versions
.PHONY: clean
clean:
	/bin/rm -f other.json clb.json $(foreach f,$(MUXES),$f.mux)

# save a snapshot locally
FILES=genl.py other.json clb.json genb.py $(foreach f,$(MUXES),$f.mux) uf.py stamp.py Makefile
.PHONY: save
save:
	@if [ "x$(dir)" = "x" ] ; then echo You must specify dir=directory ; exit 1 ; fi
	mkdir $(dir)
	cp -d $(FILES) $(dir)


