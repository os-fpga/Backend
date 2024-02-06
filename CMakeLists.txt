cmake_minimum_required(VERSION 3.15)

project("Backend_RS" C CXX)

set(BACKEND_RS on)

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

find_package(Git QUIET)

#variable_watch(VPR_DEST_DIR)

set(PATCH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
message(STATUS "NOTE: PATCH_DIR= ${PATCH_DIR}")

set(VPR_SRC_DIR ${PATCH_DIR}/read_verilog/OpenFPGA/vpr)
set(PACKER_SRC_DIR ${PATCH_DIR}/packer_fix)
#set(PLACER_SRC_DIR ${PATCH_DIR}/placer_fix)

# Enable Analytic Placer in VTR CMake
set(VPR_ANALYTIC_PLACE ON CACHE BOOL "Enable Analytic Placer in VTR" FORCE)

set(OPENFPGA_SRC_DIR ${PATCH_DIR}/openfpga)
set(TILEABLE_SRC_DIR ${PATCH_DIR}/tileable_fix)

# Logical Levels
set(ANALYSIS_SRC_DIR ${PATCH_DIR}/analysis_fix)
set(BASE_SRC_DIR ${PATCH_DIR}/base_fix)
set(UTIL_SRC_DIR ${PATCH_DIR}/util_fix)
set(TATUM_SRC_DIR ${PATCH_DIR}/tatum_fix)

set(XML_DEC_KEY_SRC ${CMAKE_CURRENT_SOURCE_DIR}/)
set(VPR_DEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/vpr)
set(OPENFPGA_DEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/)
set(XML_DEC_KEY_DEST ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/libdecrypt/)
set(LIB_RRG_DEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/librrgraph)

set(EBLIF_TRANSFORM_DEST ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/libs/EXTERNAL/libblifparse/src)
if(RAPTOR)
       file(GLOB EBLIF_TRANSFORM_SRC_H ${CMAKE_CURRENT_SOURCE_DIR}/../Raptor_Tools/gatelevel_readers/reconstruct_verilog/src/*.h)
       file(GLOB EBLIF_TRANSFORM_SRC_CPP ${CMAKE_CURRENT_SOURCE_DIR}/include/eblif_tranform/*.cpp)
else()
       file(GLOB EBLIF_TRANSFORM_SRC_H ${CMAKE_CURRENT_SOURCE_DIR}/Raptor_Tools/gatelevel_readers/reconstruct_verilog/src/*.h)
       file(GLOB EBLIF_TRANSFORM_SRC_CPP ${CMAKE_CURRENT_SOURCE_DIR}/include/eblif_tranform/*.cpp)
endif()

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

message(STATUS "NOTE: VPR-root= ${VPR_DEST_DIR}")
message(STATUS "NOTE: BEGIN PATCHING")


# apply_patch() takes a diff and applies it in WORK_DIR by 'GNU patch'.
function(apply_patch DIFF_F WORK_DIR TITLE)
  message(STATUS "(apply_patch) ${TITLE} : ${DIFF_F}")
  message(STATUS "(apply_patch) WORK_DIR= ${WORK_DIR}")
  set(PATCH_APPLY_STATUS 111)
  # check if already patched:
  execute_process(
    COMMAND patch --silent -f -u --ignore-whitespace --reverse --dry-run -i ${DIFF_F}
    WORKING_DIRECTORY ${WORK_DIR}
    TIMEOUT 30
    RESULT_VARIABLE PATCH_APPLY_STATUS
    )
  message(STATUS "(apply_patch) REVERSE_CHECK_STATUS= ${PATCH_APPLY_STATUS}")
  if (PATCH_APPLY_STATUS EQUAL "0")
    message(STATUS "ALREADY PATCHED, skipping ${DIFF_F}")
  else()
    # actual patch:
    execute_process(
      COMMAND patch --verbose -f -u --ignore-whitespace -i ${DIFF_F}
      WORKING_DIRECTORY ${WORK_DIR}
      TIMEOUT 30
      RESULT_VARIABLE PATCH_APPLY_STATUS
      )
    message(STATUS "(apply_patch) ACTUAL_APPLY_STATUS= ${PATCH_APPLY_STATUS}")
  endif()
  message(STATUS "(apply_patch) FINAL_APPLY_STATUS= ${PATCH_APPLY_STATUS}\n")
  if (NOT PATCH_APPLY_STATUS EQUAL "0")
    message(FATAL_ERROR "PATCHING ${TITLE} by GNU-patch FAILED")
  endif()
endfunction(apply_patch)


FILE(COPY ${EBLIF_TRANSFORM_SRC_H}
          ${EBLIF_TRANSFORM_SRC_CPP}
           DESTINATION
           ${EBLIF_TRANSFORM_DEST})

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

# add to VPR/pack
FILE(COPY ${PACKER_SRC_DIR}/nl_Par.h
          ${PACKER_SRC_DIR}/nl_Par.cpp
          ${PACKER_SRC_DIR}/pinc_log.h
          ${PACKER_SRC_DIR}/pinc_log.cpp
          DESTINATION
          ${VPR_DEST_DIR}/src/pack)
message(STATUS "NOTE: ADDED to VPR src/pack: nl_Par.cpp,h pinc_log.cpp,h")

# add to VPR/base
file(COPY
  ${VPR_SRC_DIR}/src/base/vcs_enc_support.cpp
  ${VPR_SRC_DIR}/src/base/vcs_enc_support.h
  ${PATCH_DIR}/base_fix/READ/read_blif_RS.cpp
  ${PATCH_DIR}/base_fix/READ/read_blif_RS.h
  ${PATCH_DIR}/base_fix/READ/read_blif.cpp
  ${PATCH_DIR}/base_fix/READ/BlifAllocCallback.cpp
  ${PATCH_DIR}/base_fix/READ/BlifAllocCallback.h
  DESTINATION
  ${VPR_DEST_DIR}/src/base/)
message(STATUS "NOTE: ADDED to VPR src/base: vcs_enc_support.cpp,h read_blif_RS.cpp,h read_blif.cpp BlifAllocCallback.cpp,h")

message(STATUS "NOTE: PATCHING VPR: src/base\n")
set(TARGET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/vpr/src/base)
file(COPY ${VPR_SRC_DIR}/src/base/netlist_writer.cpp
  DESTINATION ${TARGET_DIR})
message(STATUS "NOTE: COPIED ${VPR_SRC_DIR}/src/base/netlist_writer.cpp\n       to ${TARGET_DIR}/\n")

message(STATUS "NOTE: PATCHING VPR src/base/vpr_api.cpp by GNU-patch..")
set(DIFF_FILE ${PATCH_DIR}/base_fix/TYPES/vpr_api_cpp__ignore_space.diff)
set(TARGET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/vpr/src/base)
apply_patch(${DIFF_FILE}  ${TARGET_DIR}  "VPR src/base/vpr_api.cpp")

message(STATUS "NOTE: PATCHING VPR src/base/vpr_context.h by GNU-patch..")
set(DIFF_FILE ${PATCH_DIR}/base_fix/TYPES/vpr_context_h__ignore_space.diff)
apply_patch(${DIFF_FILE}  ${TARGET_DIR}  "VPR src/base/vpr_context.h")

message(STATUS "NOTE: PATCHING VPR src/base/vpr_types.h by GNU-patch..")
set(DIFF_FILE ${PATCH_DIR}/base_fix/TYPES/vpr_types_h__ignore_space.diff)
apply_patch(${DIFF_FILE}  ${TARGET_DIR}  "VPR src/base/vpr_types.h")

message(STATUS "NOTE: PATCHING VPR src/base READ by GNU-patch..")
set(DIFF_FILE ${PATCH_DIR}/base_fix/READ/VPR_src_base_READ.diff)
set(TARGET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/vpr/src/base)
apply_patch(${DIFF_FILE}  ${TARGET_DIR}  "VPR src/base READ")

message(STATUS "NOTE: PATCHING VPR src/base/SetupVPR.cpp by GNU-patch..")
set(DIFF_FILE ${PATCH_DIR}/base_fix/base_SetupVPR_cpp.diff)
set(TARGET_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/vpr/src/base)
apply_patch(${DIFF_FILE}  ${TARGET_DIR}  "VPR src/base/src/base/SetupVPR.cpp")

# copy RS additions and patch CMakefiles
message(STATUS "NOTE: PATCHING OpenFPGA CMakefile by GNU-patch..")
set(DIFF_FILE ${PATCH_DIR}/CMAKE_fix/OpenFPGA_CMake.diff)
apply_patch(${DIFF_FILE}  ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA  "OpenFPGA CMakefile")

file(COPY
  ${PATCH_DIR}/CMAKE_fix/VPR_CMake_RS1.cmake
  ${PATCH_DIR}/CMAKE_fix/VPR_CMake_PRODUCTION_BUILD_RS2.cmake
  DESTINATION
  ${VPR_DEST_DIR})
message(STATUS "NOTE: ADDED to VPR-root: VPR_CMake_RS1.cmake")
message(STATUS "NOTE: VPR-root= ${VPR_DEST_DIR}\n")

message(STATUS "NOTE: PATCHING VPR CMakefile by GNU-patch..")
set(DIFF_FILE ${PATCH_DIR}/CMAKE_fix/VPR_CMake.diff)
apply_patch(${DIFF_FILE}  ${CMAKE_CURRENT_SOURCE_DIR}/OpenFPGA/vtr-verilog-to-routing/vpr  "VPR CMakefile")

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

message(STATUS "NOTE: PATCHING rr_graph_builder")
FILE(COPY ${TILEABLE_SRC_DIR}/rr_graph_builder.cpp
          ${TILEABLE_SRC_DIR}/rr_graph_builder.h
          DESTINATION
          ${LIB_RRG_DEST_DIR}/src/base)

FILE(COPY ${TILEABLE_SRC_DIR}/rr_graph_uxsdcxx_serializer.h
          DESTINATION
          ${LIB_RRG_DEST_DIR}/src/io)

# Logical Levels
message(STATUS "NOTE: PATCHING TimingReporter")
FILE(COPY ${TATUM_SRC_DIR}/TimingReporter.hpp
          ${TATUM_SRC_DIR}/TimingReporter.cpp
          DESTINATION
          ${OPENFPGA_DEST_DIR}/vtr-verilog-to-routing/libs/EXTERNAL/libtatum/libtatum/tatum
)

# Logical Levels
FILE(COPY ${UTIL_SRC_DIR}/rsbe_utils.cpp
          ${UTIL_SRC_DIR}/rsbe_utils.h
          DESTINATION
          ${VPR_DEST_DIR}/src/util/
)
message(STATUS "NOTE: ADDED to VPR src/util: rsbe_utils.cpp,h")

# Logical Levels
FILE(COPY ${ANALYSIS_SRC_DIR}/timing_reports.cpp
          DESTINATION
          ${VPR_DEST_DIR}/src/analysis
)

message(STATUS "NOTE: DONE PATCHING\n")

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
