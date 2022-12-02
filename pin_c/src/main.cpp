#include <iostream>
#include <string>
#include "csv_reader.h"
#include "pcf_reader.h"
#include "xml_reader.h"
#include "cmd_line.h"
// #include "pin_location.h"
#include "pin_constrain_loc.h"
#include "pinc_log.h"

// Convert a PCF file into a VPR io.place file.
// This requires : XML file where we can get (x, y, z) of internal port 
//                 CSV file where we have the maching list (external, internal) ports
//                 PCF file: constraint file. a design pin can be assigned to an external port
//                 BLIF file: user design. We need to check the input and output. Special handling for outputs
// The output is a file constraint in VPR format.
//
// Usage options: --xml PINMAP_XML --pcf PCF 
//                --blif BLIF or --port_info port_info.json
//                --output OUTPUT --xml PINMAP_XML --csv CSV_FILE

int main(int argc, const char* argv[])
{
  const char* trace = getenv("pinc_trace");
  if (trace)
    pinc::set_ltrace(atoi(trace));

  pinc::cmd_line cmd(argc, argv);

  if (ltrace() >= 3)
    cmd.print_options();

  return pin_constrain_location(cmd);
}
