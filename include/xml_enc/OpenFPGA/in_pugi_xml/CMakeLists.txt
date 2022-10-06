cmake_minimum_required(VERSION 3.9)

project("libpugiutil")

#
# Source files and library
#
file(GLOB_RECURSE LIB_SOURCES src/*.cpp)
file(GLOB_RECURSE LIB_HEADERS src/*.hpp src/*.h)
files_to_dirs(LIB_HEADERS LIB_INCLUDE_DIRS)

find_package(PkgConfig REQUIRED)
pkg_search_module(OPENSSL REQUIRED openssl)

if( OPENSSL_FOUND )
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
else()
    message("SSL not found")
    # Error; with REQUIRED, pkg_search_module() will throw an error by it's own
endif()

#Create the library
add_library(libpugiutil STATIC
             ${LIB_HEADERS}
             ${LIB_SOURCES})
target_include_directories(libpugiutil PUBLIC ${LIB_INCLUDE_DIRS})
set_target_properties(libpugiutil PROPERTIES PREFIX "") #Avoid extra 'lib' prefix

target_link_libraries(libpugiutil 
                        libpugixml
                        ${OPENSSL_LIBRARIES})

install(TARGETS libpugiutil DESTINATION bin)
