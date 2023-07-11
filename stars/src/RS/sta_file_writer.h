#pragma once
#ifndef __rsbe__sta_file_writer_H_h_
#define __rsbe__sta_file_writer_H_h_

#include "globals.h"
#include "netlist_walker.h"
#include "vpr_context.h"
#include "vpr_types.h"

namespace rsbe {

// API
bool create_sta_files(int argc, const char** argv);

class sta_file_writer
{
private:
  // design info
  std::string design_name_;

  // utilities

public:
  sta_file_writer() { design_name_ = g_vpr_ctx.atom().nlist.netlist_name(); }

  bool write_sta_files(int argc, const char** argv) const;
};

}

#endif

