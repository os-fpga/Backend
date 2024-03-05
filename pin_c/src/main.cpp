#include "util/cmd_line.h"
#include "util/pinc_log.h"

#include "pin_loc/pinc_main.h"

// Convert a PCF file into a VPR io.place file.
// This requires : XML file where we can get (x, y, z) of internal port
//                 CSV file where we have the maching list (external, internal)
//                 ports PCF file: constraint file. a design pin can be assigned
//                 to an external port BLIF file: user design. We need to check
//                 the input and output. Special handling for outputs
// The output is a file constraint in VPR format.
//
// Usage options: --csv CSV_FILE --pcf PCF
//                --blif BLIF or --port_info port_info.json
//                --output OUTPUT --xml PINMAP_XML
//                [--edits netlist_edits_log.json]

int main(int argc, const char* argv[]) {
  using namespace pinc;
  const char* trace = getenv("pinc_trace");
  if (trace)
    set_ltrace(atoi(trace));
  else
    set_ltrace(3);

  cmd_line cmd(argc, argv);

  if (ltrace() >= 3) {
    lputs("\n    pin_c");
    if (ltrace() >= 5)
      traceEnv(argc, argv);
    cmd.print_options();
  }

  return pinc_main(cmd);
}
