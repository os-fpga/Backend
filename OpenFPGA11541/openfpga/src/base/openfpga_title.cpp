/********************************************************************
 * This file includes the functions to create the title page
 * which introduces generality of OpenFPGA framework
 *******************************************************************/
#include "vtr_log.h"

#include "openfpga_title.h"
#include "openfpga_version.h"

/********************************************************************
 * Generate a string of openfpga title introduction
 * This is mainly used when launching OpenFPGA shell 
 *******************************************************************/
std::string create_openfpga_title() {
  std::string title;

  title += std::string("\n");
  title += std::string("            ___                   _____ ____   ____    _     \n"); 
  title += std::string("           / _ \\ _ __   ___ _ __ |  ___|  _ \\ / ___|  / \\    \n"); 
  title += std::string("          | | | | '_ \\ / _ \\ '_ \\| |_  | |_) | |  _  / _ \\   \n"); 
  title += std::string("          | |_| | |_) |  __/ | | |  _| |  __/| |_| |/ ___ \\  \n"); 
  title += std::string("           \\___/| .__/ \\___|_| |_|_|   |_|    \\____/_/   \\_\\ \n"); 
  title += std::string("                |_|                                          \n"); 
  title += std::string("\n");
  title += std::string("               OpenFPGA: An Open-source FPGA IP Generator\n");
  title += std::string("                     Versatile Place and Route (VPR)\n");
  title += std::string("                           FPGA-Verilog\n");
  title += std::string("                           FPGA-SPICE\n");
  title += std::string("                           FPGA-SDC\n");
  title += std::string("                           FPGA-Bitstream\n");
  title += std::string("\n");
  title += std::string("             This is a free software under the MIT License\n");
  title += std::string("\n");
  title += std::string("             Copyright (c) 2018 LNIS - The University of Utah\n");
  title += std::string("\n");
  title += std::string("Permission is hereby granted, free of charge, to any person obtaining a copy\n");
  title += std::string("of this software and associated documentation files (the \"Software\"), to deal\n");
  title += std::string("in the Software without restriction, including without limitation the rights\n");
  title += std::string("to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n");
  title += std::string("copies of the Software, and to permit persons to whom the Software is\n");
  title += std::string("furnished to do so, subject to the following conditions:\n");
  title += std::string("\n");
  title += std::string("The above copyright notice and this permission notice shall be included in\n");
  title += std::string("all copies or substantial portions of the Software.\n");
  title += std::string("\n");
  title += std::string("THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n");
  title += std::string("IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n");
  title += std::string("FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n");
  title += std::string("AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n");
  title += std::string("LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n");
  title += std::string("OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN\n");
  title += std::string("THE SOFTWARE.\n");
  title += std::string("\n");

  return title;
}

/********************************************************************
 * Generate a string of openfpga title introduction
 * This is mainly used when launching OpenFPGA shell 
 *******************************************************************/
void print_openfpga_version_info() {
  /* Display version */
  VTR_LOG("Version: %s\n", openfpga::VERSION);
  VTR_LOG("Revision: %s\n", openfpga::VCS_REVISION);
  VTR_LOG("Compiled: %s\n", openfpga::BUILD_TIMESTAMP);
  VTR_LOG("Compiler: %s\n", openfpga::COMPILER);
  VTR_LOG("Build Info: %s\n", openfpga::BUILD_INFO);
  VTR_LOG("\n");
}
