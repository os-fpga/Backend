cmake_minimum_required(VERSION 3.15)

project("Backend_RS" C CXX)

set(VPR_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/read_verilog/OpenFPGA/vpr)
set(PUGI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/include/xml_enc/OpenFPGA/in_pugi_xml)
set(ARCH_FPGA_SRC ${CMAKE_CURRENT_SOURCE_DIR}/include/xml_enc/OpenFPGA/arch_fpga_src)
set(VPR_DEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/vpr)
set(ARCH_FPGA_DEST ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/libarchfpga)
set(PUGI_DEST ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/libpugiutil)

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
set(OPENFPGA_WITH_VERSION ON CACHE BOOL "Disable OpenFPGA version check" FORCE)
set(OPENFPGA_WITH_SWIG OFF CACHE BOOL "Disable OpenFPGA swig" FORCE)
set(WITH_ABC OFF CACHE BOOL "Enable building ABC in Verilog-to-Routing" FORCE)

if (ADD_READ_VERILOG_TO_VPR)
  file(COPY ${VPR_SRC_DIR}/src/base/read_blif.cpp
    ${VPR_SRC_DIR}/src/base/read_blif.h
    ${VPR_SRC_DIR}/src/base/read_circuit.cpp
    ${VPR_SRC_DIR}/src/base/read_circuit.h
    ${VPR_SRC_DIR}/src/base/read_options.cpp
    DESTINATION
    ${VPR_DEST_DIR}/src/base/)

file(COPY ${VPR_SRC_DIR}/CMakeLists.txt
  DESTINATION
  ${VPR_DEST_DIR}/
)
endif()

file(COPY ${PUGI_SRC}/pugixml_loc.cpp
  ${PUGI_SRC}/pugixml_loc.hpp
  ${PUGI_SRC}/pugixml_util.cpp
  ${PUGI_SRC}/aes.h
  ${PUGI_SRC}/function.h
  ${PUGI_SRC}/obfuscate.h
  ${PUGI_SRC}/openssl_rsa.cpp
  ${PUGI_SRC}/openssl_rsa.h
  DESTINATION
  ${PUGI_DEST}/src
 )

file(COPY ${ARCH_FPGA_SRC}/read_xml_arch_file.cpp
  DESTINATION
  ${ARCH_FPGA_DEST}/src 
 )

file(COPY ${PUGI_SRC}/CMakeLists.txt
  DESTINATION
  ${PUGI_DEST}/
 )

add_subdirectory(OpenFPGA)

set(IPO_LINK_WARN_SUPRESS_FLAGS " ")

# Sub-project pin_c
add_subdirectory(pin_c)
add_dependencies(pin_c openfpga)
set_target_properties(libpinconst pin_c
  PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/pin_c"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/pin_c"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/pin_c")

# Sub-project stars (Static Timing Analysis Rapid Silicon)
add_subdirectory(stars)
add_dependencies(stars vpr)
