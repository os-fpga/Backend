cmake_minimum_required(VERSION 3.0)

project("FPGA" C CXX)

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
# hide OpenFPGA for now to avoid conflict in raptor
# will enable once OpenFPGA is ready to be merged in
#add_subdirectory(OpenFPGA)
