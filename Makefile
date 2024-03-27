# 
# Top Makefile
# ============
# 
# This is the top-level makefile of the project
# 
SUBMODULE = 
PYTHON_EXEC ?= python3

ifeq ($(origin CMAKE_COMMAND),undefined)
CMAKE_COMMAND := cmake
else
CMAKE_COMMAND := ${CMAKE_COMMAND}
endif

ifeq ($(CPU_CORES),)
	CPU_CORES := $(shell nproc)
	ifeq ($(CPU_CORES),)
		CPU_CORES := $(shell sysctl -n hw.physicalcpu)
	endif
	ifeq ($(CPU_CORES),)
		CPU_CORES := 2  # Good minimum assumption
	endif
endif

.SILENT:

# Put it first so that "make" without argument is like "make help".
export COMMENT_EXTRACT

# Put it first so that "make" without argument is like "make help".
help:
	@${PYTHON_EXEC} -c "$$COMMENT_EXTRACT"

.PHONY: all checkout compile

release: run-cmake-release
	cmake --build build -j $(CPU_CORES)

release_no_tcmalloc: run-cmake-release_no_tcmalloc
	cmake --build build -j $(CPU_CORES)

debug: run-cmake-debug
	cmake --build dbuild -j $(CPU_CORES)

run-cmake-release:
	cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_RULE_MESSAGES=$(RULE_MESSAGES) -DPRODUCTION_BUILD=$(PRODUCTION_BUILD) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B build

run-cmake-release_no_tcmalloc:
	cmake -DNO_TCMALLOC=On -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_RULE_MESSAGES=$(RULE_MESSAGES) $(ADDITIONAL_CMAKE_OPTIONS) -S . -B build

run-cmake-debug:
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_RULE_MESSAGES=$(RULE_MESSAGES) -DPRODUCTION_BUILD=$(PRODUCTION_BUILD) -DNO_TCMALLOC=On  $(ADDITIONAL_CMAKE_OPTIONS) -S . -B dbuild

run-cmake-coverage:
	cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_RULE_MESSAGES=$(RULE_MESSAGES) -DMY_CXX_WARNING_FLAGS="--coverage"  $(ADDITIONAL_CMAKE_OPTIONS) -S . -B coverage-build

clean:
	$(RM) -r build dbuild coverage-build dist tests/TestInstall/build

checkout: 
# This command will checkout all the submodules when no other options are provided
# Available variables
# 
# - SUBMODULE: Specify the submodule to checkout. For example, SUBMODULE=OpenFPGA
	git submodule update --init Raptor_Tools OpenFPGA
	cd OpenFPGA && git -c submodule.yosys.update=none  -c  submodule.yosys-plugins.update=none  submodule update --init
	cd OpenFPGA/vtr-verilog-to-routing && git submodule update --init 
	cd Raptor_Tools && git submodule update --init graph_partition

compile:
# This command will compile the codebase
	mkdir -p build && cd build && $(CMAKE_COMMAND) ${CMAKE_FLAGS} ..
	cd build && $(MAKE) 

all: checkout compile
# This is a shortcut command to checkout all the submodules and compile the codebase

# Functions to extract comments from Makefiles
define COMMENT_EXTRACT
import re
with open ('Makefile', 'r' ) as f:
    matches = re.finditer('^([a-zA-Z-_]*):.*\n#(.*)', f.read(), flags=re.M)
    for _, match in enumerate(matches, start=1):
        header, content = match[1], match[2]
        print(f"  {header:10} {content}")
endef
