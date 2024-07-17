#include "file_readers/blif_reader.h"
#include "file_readers/pln_blif_file.h"

namespace pln {

using std::vector;
using std::string;
using std::endl;

bool BlifReader::read_blif(const string& blif_fn) {
  using namespace fio;
  flush_out(true);
  inputs_.clear();
  outputs_.clear();

  CStr cfn = blif_fn.c_str();
  uint16_t tr = ltrace();
  auto& ls = lout();

  BLIF_file bfile(blif_fn);

  if (tr >= 4)
    bfile.setTrace(3);

  bool exi = false;
  exi = bfile.fileExists();
  if (tr >= 8)
    ls << int(exi) << endl;
  if (not exi) {
    flush_out(true); err_puts();
    lprintf2("[Error] BLIF file '%s' does not exist\n", cfn); 
    err_puts(); flush_out(true);
    return false;
  }

  exi = bfile.fileAccessible();
  if (tr >= 8)
    ls << int(exi) << endl;
  if (not exi) {
    flush_out(true); err_puts();
    lprintf2("[Error] BLIF file '%s' is not accessible\n", cfn); 
    err_puts(); flush_out(true);
    return false;
  }

  bool rd_ok = bfile.readBlif();
  if (tr >= 8)
    ls << int(rd_ok) << endl;
  if (not rd_ok) {
    flush_out(true); err_puts();
    lprintf2("[Error] failed reading BLIF file '%s'\n", cfn); 
    err_puts(); flush_out(true);
    return false;
  }

  lprintf("  (blif_file)   #inputs= %u  #outputs= %u  topModel= %s\n",
          bfile.numInputs(), bfile.numOutputs(), bfile.topModel_.c_str());

  if (tr >= 5) {
    flush_out(true);
    bfile.printInputs(ls);
    flush_out(true);
    bfile.printOutputs(ls);
  }

  std::swap(inputs_, bfile.inputs_);
  std::swap(outputs_, bfile.outputs_);
  if (tr >= 3) {
    flush_out(true);
    lprintf("pin_c: finished read_blif().  #inputs= %zu  #outputs= %zu\n",
             inputs_.size(), outputs_.size());
  }

  flush_out(true);

  return true;
}

}

