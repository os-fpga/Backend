cmake_minimum_required(VERSION 3.15)

project("Backend_RS" C CXX)

if (NOT RAPTOR)
  set(BACKEND_RS on)
endif()

# Detect build type, fallback to release and throw a warning if use didn't
# specify any
if(NOT CMAKE_BUILD_TYPE)
  message(WARNING "Build type not set, falling back to Release mode.
 To specify build type use:
 -DCMAKE_BUILD_TYPE=<mode> where <mode> is Debug or Release.")
  set(CMAKE_BUILD_TYPE
      "Release"
      CACHE STRING "Choose the type of build, options are: Debug Release."
            FORCE)
endif(NOT CMAKE_BUILD_TYPE)

#variable_watch(VPR_DEST_DIR)

if (NOT RAPTOR)
  message(STATUS "NOTE: NOT RAPTOR")
  set(ADD_READ_VERILOG_TO_VPR 1)
endif()

set(VPR_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/read_verilog/OpenFPGA/vpr)
set(PACKER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/packer_fix)
#set(PLACER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/placer_fix)

# Enable Analytic Placer in VTR CMake
set(VPR_ANALYTIC_PLACE ON CACHE BOOL "Enable Analytic Placer in VTR" FORCE)

set(OPENFPGA_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/openfpga)
set(TILEABLE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/tileable_fix)

# Logical Levels
set(ANALYSIS_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/analysis_fix)
set(BASE_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/base_fix)
set(UTIL_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/util_fix)
set(TATUM_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include/tatum_fix)

set(XML_DEC_KEY_SRC ${CMAKE_CURRENT_SOURCE_DIR}/)
set(VPR_DEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/vpr)
set(OPENFPGA_DEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/)
set(XML_DEC_KEY_DEST ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/libdecrypt/)
set(LIB_RRG_DEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/librrgraph)

set(EBLIF_TRANSFORM_DEST ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/EXTERNAL/libblifparse/src)
set(EBLIF_TRANSFORM_SRC ${CMAKE_CURRENT_SOURCE_DIR}/include/eblif_tranform)

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

FILE(COPY ${EBLIF_TRANSFORM_SRC}/blifparse.cpp
          ${EBLIF_TRANSFORM_SRC}/transform_blif.h
          DESTINATION
          ${EBLIF_TRANSFORM_DEST})

FILE(COPY ${OPENFPGA_SRC_DIR}/src/fpga_verilog/verilog_top_testbench.cpp
          DESTINATION
          ${OPENFPGA_DEST_DIR}/openfpga/src/fpga_verilog/)

# patch VPR
FILE(COPY ${PACKER_SRC_DIR}/cluster.cpp
          ${PACKER_SRC_DIR}/cluster_util.cpp
          ${PACKER_SRC_DIR}/cluster_util.h
          ${PACKER_SRC_DIR}/output_clustering.cpp
          ${PACKER_SRC_DIR}/output_clustering.h
          ${PACKER_SRC_DIR}/cluster_router.cpp
          ${PACKER_SRC_DIR}/post_routing_pb_pin_fixup.cpp
          DESTINATION
          ${VPR_DEST_DIR}/src/pack)

# add to VPR
FILE(COPY ${PACKER_SRC_DIR}/nl_Par.h
          ${PACKER_SRC_DIR}/nl_Par.cpp
          ${PACKER_SRC_DIR}/pinc_log.h
          ${PACKER_SRC_DIR}/pinc_log.cpp
          DESTINATION
          ${VPR_DEST_DIR}/src/pack)
message(STATUS "NOTE: ADDED to VPR src/pack: nl_Par.cpp,h pinc_log.cpp,h")

if (ADD_READ_VERILOG_TO_VPR)
  message(STATUS "NOTE: ADDING READ_VERILOG_TO_VPR")
  file(COPY ${VPR_SRC_DIR}/src/base/SetupVPR.cpp
    ${VPR_SRC_DIR}/src/base/read_blif.h
    ${VPR_SRC_DIR}/src/base/read_circuit.cpp
    ${VPR_SRC_DIR}/src/base/read_circuit.h
    ${VPR_SRC_DIR}/src/base/read_options.cpp
    ${VPR_SRC_DIR}/src/base/read_options.h
    ${VPR_SRC_DIR}/src/base/netlist_writer.cpp
    ${VPR_SRC_DIR}/src/base/vcs_enc_support.cpp
    ${VPR_SRC_DIR}/src/base/vcs_enc_support.h
    ${VPR_SRC_DIR}/src/base/read_blif.cpp
    ${VPR_SRC_DIR}/src/base/vpr_types.h
    DESTINATION
    ${VPR_DEST_DIR}/src/base/)

  message(STATUS "NOTE: PATCHING VPR CMakefile")
  file(COPY ${VPR_SRC_DIR}/CMakeLists.txt
    DESTINATION
    ${VPR_DEST_DIR}/
  )
else()
  message(STATUS "NOTE: SKIPPING READ_VERILOG_TO_VPR")
  file(COPY ${VPR_SRC_DIR}/src/base/SetupVPR.cpp
    ${VPR_SRC_DIR}/src/base/read_options.cpp
    ${VPR_SRC_DIR}/src/base/read_options.h
    ${VPR_SRC_DIR}/src/base/vpr_types.h
    DESTINATION
    ${VPR_DEST_DIR}/src/base/)
endif()

#FILE(COPY ${PLACER_SRC_DIR}/move_generator.h
#          ${PLACER_SRC_DIR}/place.cpp
#          ${PLACER_SRC_DIR}/place_util.cpp
#          ${PLACER_SRC_DIR}/place_util.h
#          # Analytical Placer
#          ${PLACER_SRC_DIR}/analytic_placer.h
#          ${PLACER_SRC_DIR}/analytic_placer.cpp
#          DESTINATION
#          ${VPR_DEST_DIR}/src/place/)

file(COPY ${XML_DEC_KEY_SRC}/private_key.pem
  DESTINATION
  ${XML_DEC_KEY_DEST}/
 )

FILE(COPY ${TILEABLE_SRC_DIR}/rr_graph_builder.cpp
          ${TILEABLE_SRC_DIR}/rr_graph_builder.h
          DESTINATION
          ${LIB_RRG_DEST_DIR}/src/base)

FILE(COPY ${TILEABLE_SRC_DIR}/rr_graph_uxsdcxx_serializer.h
          DESTINATION
          ${LIB_RRG_DEST_DIR}/src/io)

# Logical Levels
FILE(COPY ${TATUM_SRC_DIR}/TimingReporter.hpp
          ${TATUM_SRC_DIR}/TimingReporter.cpp
          DESTINATION
          ${OPENFPGA_DEST_DIR}/vtr-verilog-to-routing/libs/EXTERNAL/libtatum/libtatum/tatum
)

# Logical Levels
FILE(COPY ${UTIL_SRC_DIR}/vpr_utils.cpp
          ${UTIL_SRC_DIR}/vpr_utils.h
          DESTINATION
          ${VPR_DEST_DIR}/src/util/
)

# Logical Levels
FILE(COPY ${ANALYSIS_SRC_DIR}/timing_reports.cpp
          DESTINATION
          ${VPR_DEST_DIR}/src/analysis
)

# Logical Levels
FILE(COPY ${BASE_SRC_DIR}/vpr_api.cpp
          ${BASE_SRC_DIR}/vpr_context.h
          DESTINATION
          ${VPR_DEST_DIR}/src/base/
)

if(NOT RAPTOR)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/Raptor_Tools/libs/external/boost)
  add_subdirectory("Raptor_Tools/graph_partition")
endif()
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
