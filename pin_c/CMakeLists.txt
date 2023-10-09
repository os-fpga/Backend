cmake_minimum_required(VERSION 3.9)

project("pin_c")

#
# We require c++17 support minimum
#
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#
# Build Configuration
#
include(CheckCXXSymbolExists)

if (PRODUCTION_BUILD)
    if(NOT RAPTOR)
        get_filename_component(FLEX_LM_SRC_DIR "../Raptor_Tools/Flex_LM"
        REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    message("Production Build type set to ON")
    set (PRODUCTION_BUILD_FLAG "-DPRODUCTION_BUILD=1")
    add_definitions(-DPRODUCTION_BUILD)
    set(CXX_OPTS "-fPIC;-O2;-pipe;-mavx")
    message("FLEX: "  ${FLEX_LM_SRC_DIR})
endif(PRODUCTION_BUILD)

if (NOT RAPTOR)
    if(ENABLE_VERIFIC)
        get_filename_component(READ_VERILOG_SRC_DIR "../Raptor_Tools/gatelevel_readers/read_verilog"
        REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
        include_directories(PUBLIC ${READ_VERILOG_SRC_DIR})
        set(read_verilog_lib libreadver)
    endif()
    get_filename_component(READ_EDIF_SRC_DIR "../Raptor_Tools/gatelevel_readers/read_edif"
    REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
endif()

include_directories(PUBLIC ${READ_EDIF_SRC_DIR})
include_directories(PUBLIC ${FLEX_LM_SRC_DIR}
                        ${FLEX_LM_SRC_DIR}/machind)

if(OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    message("SSL not found")
    # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
endif()

#Collect the source files
file(GLOB_RECURSE EXEC_SOURCES src/main.cpp)
file(GLOB_RECURSE LIB_SOURCES src/*/*.cpp)
file(GLOB_RECURSE LIB_HEADERS src/*/*.h)
#files_to_dirs(LIB_HEADERS LIB_INCLUDE_DIRS)
set(LIB_INCLUDE_DIRS src)

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

set_target_properties(pin_c libpinconst PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

if(PRODUCTION_BUILD)
    target_link_libraries(pin_c PUBLIC libpinconst
                                       blif_parse_static
                                       vprst_static
                                       libvtrutil
                                       libpugixml
                                       libreadedif
                                       rs_licenseManager
                                       ${OPENSSL_LIBRARIES})
else()
    target_link_libraries(pin_c PUBLIC libpinconst
                                       blif_parse_static
                                       vprst_static
                                       libvtrutil
                                       libpugixml
                                       libreadedif
                                       ${OPENSSL_LIBRARIES})
endif()

##Supress IPO link warnings if IPO is enabled
#get_target_property(PIN_C_USES_IPO vpr INTERPROCEDURAL_OPTIMIZATION)
#if (PIN_C_USES_IPO)
#    set_target_properties(vpr PROPERTIES LINK_FLAGS ${IPO_LINK_WARN_SUPRESS_FLAGS})
#endif()

#
# PIN_C compiler options
#
target_compile_options(pin_c PRIVATE -Werror -Wall -Wextra -O2 -pipe -mavx)


add_dependencies(pin_c vprst_static)
add_dependencies(pin_c blif_parse_static)

set_target_properties(pin_c libpinconst PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

  install(TARGETS pin_c DESTINATION bin)

