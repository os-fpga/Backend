#ifndef sta_file_writer_H
#define sta_file_writer_H
#include "globals.h"
#include "netlist_walker.h"
#include "vpr_context.h"
#include "vpr_types.h"

namespace stars {
// API
bool create_sta_files();

class sta_file_writer {
private:
  // design info
  std::string design_name_;

public:
  sta_file_writer() { design_name_ = g_vpr_ctx.atom().nlist.netlist_name(); }
  bool write_sta_files();
};

} // end namespace stars
#endif