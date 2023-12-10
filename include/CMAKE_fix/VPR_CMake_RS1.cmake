#
#  1st Rapid Silicon addition to VPR CMake
#

if (PRODUCTION_BUILD)
    get_filename_component(FLEX_LM_SRC_DIR "../../../Raptor_Tools/Flex_LM"
    REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    add_subdirectory(${FLEX_LM_SRC_DIR} flex_lm)
    message("Production Build type set to ON")
    set (PRODUCTION_BUILD_FLAG "-DPRODUCTION_BUILD=1")
    add_definitions(-DPRODUCTION_BUILD)
    message("FLEX: "  ${FLEX_LM_SRC_DIR})
endif(PRODUCTION_BUILD)

option(ENABLE_VERIFIC "Enable Verific front end" OFF)

get_filename_component(READ_VERILOG_SRC_DIR "../../../Raptor_Tools/gatelevel_readers/read_verilog"
REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
add_subdirectory(${READ_VERILOG_SRC_DIR} read_verilog)
get_filename_component(VERI_PRUNE_SRC_DIR "../../../Raptor_Tools/gatelevel_readers/veri_prune"
REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
add_subdirectory(${VERI_PRUNE_SRC_DIR} veri_prune)

if(ENABLE_VERIFIC)
    get_filename_component(VERIFIC_HOME "../../../Raptor_Tools/verific_rs/"
    REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
    add_subdirectory(${VERIFIC_HOME} verific_rs)
endif()

get_filename_component(READ_EDIF_SRC_DIR "../../../Raptor_Tools/gatelevel_readers/read_edif"
REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
add_subdirectory(${READ_EDIF_SRC_DIR} read_edif)

find_package(PkgConfig REQUIRED)
pkg_search_module(OPENSSL REQUIRED openssl)

if( OPENSSL_FOUND )
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    message("SSL not found")
    # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
endif()

if(ENABLE_VERIFIC)
    add_compile_definitions(ENABLE_VERIFIC)
endif()

