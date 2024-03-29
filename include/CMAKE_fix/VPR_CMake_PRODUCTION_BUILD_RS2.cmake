#
#  2nd Rapid Silicon addition to VPR CMake
#

if (PRODUCTION_BUILD)
#    target_include_directories(libvpr PUBLIC
#                                ${FLEX_LM_SRC_DIR}
#                                ${FLEX_LM_SRC_DIR}/machind)
    #Specify link-time dependancies
    target_link_libraries(libvpr
                            libvtrutil
                            libarchfpga
                            libsdcparse
                            libblifparse
                            libtatum
                            libargparse
                            libpugixml
                            librrgraph
                            libreadedif
                            ${OPENSSL_LIBRARIES}
                            ${VERIFIC_LIBS} dl)
else()
    #Specify link-time dependancies
    target_link_libraries(libvpr
                            libvtrutil
                            libarchfpga
                            libsdcparse
                            libblifparse
                            libtatum
                            libargparse
                            libpugixml
                            librrgraph
                            libreadedif
                            ${OPENSSL_LIBRARIES}
                            ${VERIFIC_LIBS} dl)
endif(PRODUCTION_BUILD)

