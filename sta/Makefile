# Makefile

ifeq ($(origin CMAKE_COMMAND),undefined)
CMAKE_COMMAND := cmake
else
CMAKE_COMMAND := ${CMAKE_COMMAND}
endif

.PHONY: all checkout compile

all: checkout
	mkdir -p build && cd build && $(CMAKE_COMMAND) ${CMAKE_FLAGS} ..
	cd build && $(MAKE)

checkout: 
	git submodule init
	git submodule update --init --recursive
	cd yosys-plugins
	git checkout fe8fe891c6c5be01c7ef69606a79b1ec2c723fe7
	cd ..


compile:
	mkdir -p build && cd build && $(CMAKE_COMMAND) ${CMAKE_FLAGS} ..
	cd build && $(MAKE)

clean:
	rm -rf OpenSTA/app/sta

build/Makefile:
	make checkout

.PHONY: Makefile

%: build/Makefile
	cd build && $(MAKE) $@
