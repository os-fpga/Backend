#
#  1st Rapid Silicon addition to VPR CMake
#

option(ENABLE_VERIFIC "Enable Verific front end" OFF)
if (NOT RAPTOR)
    if(ENABLE_VERIFIC)
        get_filename_component(VERIFIC_HOME "../../../Raptor_Tools/verific_rs/"
        REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
        add_subdirectory(${VERIFIC_HOME} verific_rs)
    endif()
    get_filename_component(READ_EDIF_SRC_DIR "../../../Raptor_Tools/gatelevel_readers/read_edif"
    REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" CACHE)
    add_subdirectory(${READ_EDIF_SRC_DIR} read_edif)
endif()

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

