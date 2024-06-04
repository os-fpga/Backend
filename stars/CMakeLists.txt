cmake_minimum_required(VERSION 3.9)

project("stars")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
message(STATUS "Building with IPO: off")

set(OPENFPGA_IPO_BUILD "off" CACHE STRING "Should be compiled with LTO?")
set_property(CACHE OPENFPGA_IPO_BUILD PROPERTY STRINGS auto off off)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC -O1 -mavx2 -march=haswell")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC -O1 -mavx2 -march=haswell")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -fPIC -O1 -mavx2 -march=haswell")

file(GLOB_RECURSE EXEC_SOURCE src/main.cpp)
file(GLOB_RECURSE LIB_SOURCES src/*/*.cpp)
file(GLOB_RECURSE LIB_HEADERS src/*/*.h)

set(LIB_INCLUDE_DIRS src)

#Remove test executable from library
list(REMOVE_ITEM LIB_SOURCES ${EXEC_SOURCE})

#Create .a library
add_library(libPlanner STATIC
            ${LIB_HEADERS}
            ${LIB_SOURCES})
target_include_directories(libPlanner PUBLIC ${LIB_INCLUDE_DIRS})
set_target_properties(libPlanner PROPERTIES PREFIX "") #Avoid extra 'lib' prefix

#Create .so library
#add_library(libPlanner_dll SHARED
#            ${LIB_HEADERS}
#            ${LIB_SOURCES})
#target_include_directories(libPlanner_dll PUBLIC ${LIB_INCLUDE_DIRS})
#set_target_properties(libPlanner_dll PROPERTIES PREFIX "") #Avoid extra 'lib' prefix

#Specify link-time dependancies
target_link_libraries(libPlanner
                      libvpr)

#target_link_libraries(libPlanner_dll
#                      libvpr)

#Create the executable
add_executable(stars ${EXEC_SOURCE})
target_link_libraries(stars libPlanner libvpr)

set_target_properties(stars libPlanner PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)

install(TARGETS stars DESTINATION bin)

