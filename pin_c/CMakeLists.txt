cmake_minimum_required(VERSION 3.9)

project("pin_c")


#
# Build Configuration
#
include(CheckCXXSymbolExists)

#Collect the source files
file(GLOB_RECURSE EXEC_SOURCES src/main.cpp)
file(GLOB_RECURSE LIB_SOURCES src/*/*.cpp)
file(GLOB_RECURSE LIB_HEADERS src/*/*.h)
files_to_dirs(LIB_HEADERS LIB_INCLUDE_DIRS)

file(GLOB_RECURSE BLIF_READ_HEADERS ../OpenFPGA/vtr-verilog-to-routing/vpr/src/base/*.h)

files_to_dirs(BLIF_READ_HEADERS BLIF_READ_INCLUDE_DIRS)

#Create the static library
add_library(libpinconst STATIC
             ${LIB_HEADERS}
             ${LIB_SOURCES}
)

target_include_directories(libpinconst PUBLIC ${LIB_INCLUDE_DIRS} ${BLIF_READ_INCLUDE_DIRS})
set_target_properties(libpinconst PROPERTIES PREFIX "") #Avoid extra 'lib' prefix

include_directories(
    ${PROJECT_SOURCE_DIR}/../OpenFPGA/vtr-verilog-to-routing/vpr/src/base/
    ${PROJECT_SOURCE_DIR}/../OpenFPGA/vtr-verilog-to-routing/vpr/src/route/
    ${PROJECT_SOURCE_DIR}/../OpenFPGA/vtr-verilog-to-routing/libs/libarchfpga/src/
    ${PROJECT_SOURCE_DIR}/../OpenFPGA/vtr-verilog-to-routing/libs/librrgraph/src/base/
    ${PROJECT_SOURCE_DIR}/../OpenFPGA/vtr-verilog-to-routing/libs/EXTERNAL/libblifparse/src/
  )

set(VPRST_STATIC_LIB libvpr.a)
add_library(vprst_static STATIC IMPORTED )

set_target_properties(vprst_static PROPERTIES
  IMPORTED_LOCATION  ${CMAKE_CURRENT_BINARY_DIR}/../OpenFPGA/vtr-verilog-to-routing/vpr/${VPRST_STATIC_LIB})      

set(BLIF_PARSE_STATIC_LIB libblifparse.a)
add_library(blif_parse_static STATIC IMPORTED )

set_target_properties(blif_parse_static PROPERTIES
  IMPORTED_LOCATION  ${CMAKE_CURRENT_BINARY_DIR}/../OpenFPGA/vtr-verilog-to-routing/libs/EXTERNAL/libblifparse/${BLIF_PARSE_STATIC_LIB})

target_link_libraries(libpinconst PUBLIC blif_parse_static vprst_static libvtrutil libpugixml)

add_executable(pin_c ${EXEC_SOURCES})
target_link_libraries(pin_c PUBLIC libpinconst blif_parse_static vprst_static libvtrutil libpugixml)

#Supress IPO link warnings if IPO is enabled
get_target_property(PIN_C_USES_IPO vpr INTERPROCEDURAL_OPTIMIZATION)
if (PIN_C_USES_IPO)
    set_target_properties(vpr PROPERTIES LINK_FLAGS ${IPO_LINK_WARN_SUPRESS_FLAGS})
endif()

#
# PIN_C compiler options
#
target_compile_options(pin_c PRIVATE -Werror -Wall)

#
# Profile Guilded Optimization Configuration
#
set(PROF_GEN_FLAGS_TO_CHECK
    #GCC-like
    "-fprofile-generate" #Build will generate profiling information
    )
set(PROF_USE_FLAGS_TO_CHECK
    #GCC-like
    "-fprofile-use"     #Build will use previously generated profiling information to guide code optimization
    "-Wmissing-profile" #Warn if the used profiling information doesn't match the source code or is missing
    )


#
# Unit Tests
#
file(GLOB_RECURSE TEST_SOURCES test/*.cpp)
add_executable(test_pin_c ${TEST_SOURCES})
target_link_libraries(test_pin_c
                        libpinconst)

add_dependencies(test_pin_c pin_c)
add_dependencies(pin_c vprst_static)
add_dependencies(pin_c blif_parse_static)


#Supress IPO link warnings if IPO is enabled
get_target_property(TEST_PIN_C_USES_IPO pin_c INTERPROCEDURAL_OPTIMIZATION)
if (TEST_PIN_C_USES_IPO)
    set_target_properties(test_vpr PROPERTIES LINK_FLAGS ${IPO_LINK_WARN_SUPRESS_FLAGS})
endif()

add_test(NAME test_vpr
    COMMAND test_vpr --use-colour=yes
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test
    )

  install(TARGETS pin_c DESTINATION bin)
