#include "globals.h"
#include "read_options.h"
#include "sta_file_writer.h"
#include "vpr_main.h"

#include <cstring>

int main(int argc, const char *argv[]) {

  // run status
  bool status = true;

  std::cout << "STARS: Preparing design data ... " << std::endl;

  // 1. add --analysis option if missing:
  char** vprArgv = (char**) calloc(argc + 4, sizeof(char*));
  int vprArgc = argc;
  bool found_analysis = false;
  std::string analysis = "--analysis";
  for (int i = 0; i < argc; i++) {
    const char* a = argv[i];
    if (analysis == a) found_analysis = true;
    vprArgv[i] = strdup(a);
  }
  if (not found_analysis) {
    std::cout << "STARS: added --analysis to options" << std::endl;
    vprArgv[argc] = strdup("--analysis");
    vprArgc++;
  }

  // 2. call vpr to build design context
  vpr::vpr(vprArgc, vprArgv);

  // 3. write files for opensta
  std::cout << "STARS: Creating sta files ... " << std::endl;
  if (!stars::create_sta_files(argc, argv)) {
    std::cerr << "STARS: Creating sta files failed." << std::endl;
    status = false;
  } else {
    std::cout << "STARS: Creating sta files succeeded." << std::endl;
    status = true;
  }

  return status ? 0 : 1;
}
