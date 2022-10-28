#include "globals.h"
#include "read_options.h"
#include "sta_file_writer.h"
#include "vpr_main.h"

int main(int argc, const char *argv[]) {

  // run status
  bool status = true;

  // 0. call vpr to build design context
  std::cout << "STARS: Preparing design data ... " << std::endl;
  vpr::vpr(argc, (char **)argv);

  // 1. write files for opensta
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
