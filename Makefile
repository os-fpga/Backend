# Makefile
SUBMODULE = 

ifeq ($(origin CMAKE_COMMAND),undefined)
CMAKE_COMMAND := cmake
else
CMAKE_COMMAND := ${CMAKE_COMMAND}
endif

.PHONY: all checkout compile

checkout: 
# This command will checkout all the submodules when no other options are provided
# Available variables
# 
# - SUBMODULE: Specify the submodule to checkout. For example, SUBMODULE=OpenFPGA
	git submodule init ${SUBMODULE}
	git submodule update --init --recursive ${SUBMODULE}

compile:
# This command will compile the codebase
	mkdir -p build && cd build && $(CMAKE_COMMAND) ${CMAKE_FLAGS} ..
	cd build && $(MAKE)

all: checkout compile
# This is a shortcut command to checkout all the submodules and compile the codebase
