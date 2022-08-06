
project("STA" C CXX)

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
# add OpenSTA
add_subdirectory(OpenSTA)

add_executable(test_sta ${PROJECT_SOURCE_DIR}/test/main.cpp)
add_dependencies(test_sta sta)

# move executable sta to build/bin
add_custom_command(TARGET test_sta POST_BUILD
  COMMAND echo "Copying sta to ${PROJECT_BINARY_DIR}/bin"
  COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_BINARY_DIR}/bin
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${PROJECT_SOURCE_DIR}/OpenSTA/app/sta ${PROJECT_BINARY_DIR}/bin/sta ${PROJECT_BINARY_DIR}/bin/.
  )

