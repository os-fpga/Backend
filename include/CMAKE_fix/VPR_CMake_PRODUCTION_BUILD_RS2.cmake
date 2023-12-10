#
#  2nd Rapid Silicon addition to VPR CMake
#

if (PRODUCTION_BUILD)
    target_include_directories(libvpr PUBLIC
                                ${FLEX_LM_SRC_DIR}
                                ${FLEX_LM_SRC_DIR}/machind)
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
                            ${read_verilog_lib}
                            ${veri_prune_lib}
                            libreadedif
                            rs_licenseManager
                            ${OPENSSL_LIBRARIES}
                            ${VERIFIC_LIBS})
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
                            ${read_verilog_lib}
                            ${veri_prune_lib}
                            libreadedif
                            ${OPENSSL_LIBRARIES}
                            ${VERIFIC_LIBS})
endif(PRODUCTION_BUILD)

