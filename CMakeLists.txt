cmake_minimum_required(VERSION 3.15)

project("Backend_RS" C CXX)

# Version number
file (STRINGS "VERSION.md" VERSION_NUMBER)
string (REPLACE "." ";" VERSION_LIST ${VERSION_NUMBER})
list(GET VERSION_LIST 0 CMAKE_PROJECT_VERSION_MAJOR)
list(GET VERSION_LIST 1 CMAKE_PROJECT_VERSION_MINOR)
list(GET VERSION_LIST 2 CMAKE_PROJECT_VERSION_PATCH)
set(CMAKE_PROJECT_VERSION ${VERSION_NUMBER})

# 
# Sub-projects
#
set(OPENFPGA_WITH_YOSYS OFF CACHE BOOL "Disable building Yosys in OpenFPGA" FORCE)
set(OPENFPGA_WITH_TEST OFF CACHE BOOL "Disable OpenFPGA tests" FORCE)
set(OPENFPGA_WITH_VERSION OFF CACHE BOOL "Disable OpenFPGA version check" FORCE)
set(WITH_ABC OFF CACHE BOOL "Enable building ABC in Verilog-to-Routing" FORCE)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/include/read_verilog/read_blif.cpp
${CMAKE_CURRENT_SOURCE_DIR}/include/read_verilog/read_blif.h
${CMAKE_CURRENT_SOURCE_DIR}/include/read_verilog/read_circuit.cpp
${CMAKE_CURRENT_SOURCE_DIR}/include/read_verilog/read_circuit.h
${CMAKE_CURRENT_SOURCE_DIR}/include/read_verilog/read_options.cpp
${CMAKE_CURRENT_SOURCE_DIR}/include/read_verilog/read_options.h
DESTINATION
${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/vpr/src/base/
)

if (OPENFPGA11541)
  set(VPR_ENABLE_INTERCHANGE OFF CACHE BOOL "Interchange" FORCE)
  add_subdirectory(OpenFPGA11541)
else()
  add_subdirectory(OpenFPGA)
endif()


set(IPO_LINK_WARN_SUPRESS_FLAGS " ")

# Sub-project pin_c
add_subdirectory(pin_c)
add_dependencies(pin_c openfpga)
set_target_properties(libpinconst pin_c
  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/pin_c"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/pin_c"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/pin_c")

