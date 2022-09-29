#ifndef WRITE_XML_OPENFPGA_ARCH_H
#define WRITE_XML_OPENFPGA_ARCH_H

/********************************************************************
 * Include header files that are required by function declaration
 *******************************************************************/
#include <string>
#include "openfpga_arch.h"

/********************************************************************
 * Function declaration
 *******************************************************************/
void write_xml_openfpga_arch(const char* xml_fname, 
                             const openfpga::Arch& openfpga_arch);

void write_xml_openfpga_simulation_settings(const char* xml_fname, 
                                            const openfpga::SimulationSetting& openfpga_sim_setting);

void write_xml_openfpga_bitstream_settings(const char* fname, 
                                           const openfpga::BitstreamSetting& openfpga_bitstream_setting);


#endif
