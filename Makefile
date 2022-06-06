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

.SILENT:

# Put it first so that "make" without argument is like "make help".
export COMMENT_EXTRACT

# Put it first so that "make" without argument is like "make help".
help:
	@${PYTHON_EXEC} -c "$$COMMENT_EXTRACT"

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

# Functions to extract comments from Makefiles
define COMMENT_EXTRACT
import re
with open ('Makefile', 'r' ) as f:
    matches = re.finditer('^([a-zA-Z-_]*):.*\n#(.*)', f.read(), flags=re.M)
    for _, match in enumerate(matches, start=1):
        header, content = match[1], match[2]
        print(f"  {header:10} {content}")
endef
