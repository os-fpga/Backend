cmake_minimum_required(VERSION 3.15)

project("Backend_RS" C CXX)

#variable_watch(VPR_DEST_DIR)

if (NOT RAPTOR)
  set(ADD_READ_VERILOG_TO_VPR 1)
endif()
set(VPR_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/read_verilog/OpenFPGA/vpr)
set(PACKER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/packer_fix)
set(PLACER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/placer_fix)
# Enable Analytic Placer in VTR CMake
set(VTR_CMAKE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/VTR_CMAKE_fix)
set(OPENFPGA_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/openfpga)
set(TILEABLE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/tileable_fix)
set(MEM_FIX_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/mem_fix)

set(PUGI_SRC ${CMAKE_CURRENT_SOURCE_DIR}/include/xml_enc/OpenFPGA/in_pugi_xml)
set(ARCH_FPGA_SRC ${CMAKE_CURRENT_SOURCE_DIR}/include/xml_enc/OpenFPGA/arch_fpga_src)
set(VPR_DEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/vpr)
set(OPENFPGA_DEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/)
set(ARCH_FPGA_DEST ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/libarchfpga)
set(PUGI_DEST ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/libpugiutil)
set(LIB_RRG_DEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/librrgraph)
set(MEM_FIX_DEST_DIR1 ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/libvtrutil/src)
set(MEM_FIX_DEST_DIR2 ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/librrgraph/src/base)

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
set(WITH_ABC OFF CACHE BOOL "Disable building ABC in Verilog-to-Routing" FORCE)
set(OPENFPGA_WITH_SWIG OFF CACHE BOOL "Disable OpenFPGA swig" FORCE)

if (PRODUCTION_BUILD)
  set(OPENFPGA_WITH_VERSION ON CACHE BOOL "Disable OpenFPGA version check" FORCE)
else()
  set(OPENFPGA_WITH_VERSION OFF CACHE BOOL "Disable OpenFPGA version check" FORCE)
  set(OPENFPGA_IPO_BUILD "off" CACHE STRING "Disable VTR/OpenFGPA IPO build - long linktime if on" FORCE)
  set(VTR_IPO_BUILD "off" CACHE STRING "Should VTR be compiled with interprocedural compiler optimizations? NO!")
endif()

FILE(COPY ${OPENFPGA_SRC_DIR}/src/fpga_verilog/verilog_top_testbench.cpp
          DESTINATION
          ${OPENFPGA_DEST_DIR}/openfpga/src/fpga_verilog/)

FILE(COPY ${MEM_FIX_SRC_DIR}/libvtrutil/vtr_ragged_matrix.h
          DESTINATION
          ${MEM_FIX_DEST_DIR1})
FILE(COPY ${MEM_FIX_SRC_DIR}/librrgraph/rr_graph_view.h
          DESTINATION
          ${MEM_FIX_DEST_DIR2})

FILE(COPY ${PACKER_SRC_DIR}/cluster.cpp
          ${PACKER_SRC_DIR}/cluster_util.cpp
          ${PACKER_SRC_DIR}/cluster_util.h
          ${PACKER_SRC_DIR}/output_clustering.cpp
          ${PACKER_SRC_DIR}/output_clustering.h
          ${PACKER_SRC_DIR}/cluster_router.cpp
          ${PACKER_SRC_DIR}/post_routing_pb_pin_fixup.cpp
          DESTINATION
          ${VPR_DEST_DIR}/src/pack)

FILE(COPY ${PACKER_SRC_DIR}/vpr_utils.cpp
          DESTINATION
          ${VPR_DEST_DIR}/src/util/)

if (ADD_READ_VERILOG_TO_VPR)
  file(COPY ${VPR_SRC_DIR}/src/base/read_blif.cpp
    ${VPR_SRC_DIR}/src/base/read_blif.h
    ${VPR_SRC_DIR}/src/base/read_circuit.cpp
    ${VPR_SRC_DIR}/src/base/read_circuit.h
    ${VPR_SRC_DIR}/src/base/read_options.cpp
    ${VPR_SRC_DIR}/src/base/netlist_writer.cpp
    ${VPR_SRC_DIR}/src/base/vcs_enc_support.cpp
    ${VPR_SRC_DIR}/src/base/vcs_enc_support.h
    ${VPR_SRC_DIR}/src/base/SetupVPR.cpp
    ${VPR_SRC_DIR}/src/base/read_options.h
     ${VPR_SRC_DIR}/src/base/vpr_types.h
    ${PACKER_SRC_DIR}/cluster_placement.cpp
    DESTINATION
    ${VPR_DEST_DIR}/src/base/)

file(COPY ${VPR_SRC_DIR}/CMakeLists.txt
  DESTINATION
  ${VPR_DEST_DIR}/
)
endif()

# FILE(COPY ${PLACER_SRC_DIR}/SetupVPR.cpp
#           ${PLACER_SRC_DIR}/read_options.cpp
#           ${PLACER_SRC_DIR}/read_options.h
#           ${PLACER_SRC_DIR}/vpr_types.h
#           DESTINATION
#           ${VPR_DEST_DIR}/src/base/)

# Update VTR CMake to enable analytical placer
FILE(COPY ${VTR_CMAKE_SRC_DIR}/CMakeLists.txt
     DESTINATION
     ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/
)

FILE(COPY ${PLACER_SRC_DIR}/move_generator.h
          ${PLACER_SRC_DIR}/place.cpp
          ${PLACER_SRC_DIR}/place_util.cpp
          ${PLACER_SRC_DIR}/place_util.h
          # Analytical Placer
          ${PLACER_SRC_DIR}/analytic_placer.h
          ${PLACER_SRC_DIR}/analytic_placer.cpp
          DESTINATION
          ${VPR_DEST_DIR}/src/place/)

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

FILE(COPY ${TILEABLE_SRC_DIR}/rr_graph_builder.cpp
          ${TILEABLE_SRC_DIR}/rr_graph_builder.h
          DESTINATION
          ${LIB_RRG_DEST_DIR}/src/base)

FILE(COPY ${TILEABLE_SRC_DIR}/rr_graph_uxsdcxx_serializer.h
          DESTINATION
          ${LIB_RRG_DEST_DIR}/src/io)

#add_subdirectory(Raptor_Tools/graph_partition)
set(MtKaHyPar_BINARY_FILE_PATH "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/MtKaHyPar" CACHE INTERNAL "")
configure_file(config.h.in "${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/vpr/src/pack/config.h")
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
