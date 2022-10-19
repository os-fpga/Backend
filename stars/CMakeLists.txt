cmake_minimum_required(VERSION 3.9)

project("stars")

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto")

file(GLOB_RECURSE EXEC_SOURCE src/main.cpp)
file(GLOB_RECURSE LIB_SOURCES src/*/*.cpp)
file(GLOB_RECURSE LIB_HEADERS src/*/*.h)
files_to_dirs(LIB_HEADERS LIB_INCLUDE_DIRS)

#Remove test executable from library
list(REMOVE_ITEM LIB_SOURCES ${EXEC_SOURCE})

#Create the library
add_library(libstars STATIC
            ${LIB_HEADERS}
            ${LIB_SOURCES})
target_include_directories(libstars PUBLIC ${LIB_INCLUDE_DIRS})
set_target_properties(libstars PROPERTIES PREFIX "") #Avoid extra 'lib' prefix

#Specify link-time dependancies
target_link_libraries(libstars
                      libvpr)

#Create the test executable
add_executable(stars ${EXEC_SOURCE})
target_link_libraries(stars libstars)

install(TARGETS stars DESTINATION bin)
