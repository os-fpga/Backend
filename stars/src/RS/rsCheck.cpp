#include "RS/rsCheck.h"
#include "file_readers/pln_blif_file.h"

namespace pln {

using std::string;
using std::endl;

bool do_check(const rsOpts& opts) {
  if (!opts.input_)
    return false;
  uint16_t tr = ltrace();
  auto& ls = lout();

  string fn1 = opts.input_;
  CStr cfn = fn1.c_str();
  ls << "  checking BLIF file: " << fn1 << endl;

  BLIF_file bfile(fn1);

  if (tr >= 4)
    bfile.setTrace(3);

  bool exi = false;
  exi = bfile.fileExists();
  if (tr >= 7)
    ls << int(exi) << endl;
  if (not exi) {
    lprintf2("[Error] file '%s' does not exist\n", cfn); 
    return false;
  }

  exi = bfile.fileAccessible();
  if (tr >= 7)
    ls << int(exi) << endl;
  if (not exi) {
    lprintf2("[Error] file '%s' is not accessible\n", cfn); 
    return false;
  }

  bool rd_ok = bfile.readBlif();
  if (tr >= 7)
    ls << int(rd_ok) << endl;
  if (not rd_ok) {
    lprintf2("[Error] failed reading file '%s'\n", cfn); 
    return false;
  }

  lprintf("  (blif_file)   #inputs= %u  #outputs= %u\n",
          bfile.numInputs(), bfile.numOutputs());

  if (tr >= 4) {
    lputs();
    bfile.printInputs(ls);
    lputs();
    bfile.printOutputs(ls);
  }

  lputs();
  ls << ">>>>> checking BLIF " << fn1 << "  ..." << endl;

  bool chk_ok = bfile.checkBlif();

  ls << ">>>>> chk_ok: " << int(chk_ok) << endl;
  if (chk_ok) {
    ls << " === BLIF is OK." << endl;
    return true;
  }

  ls << " !!! BLIF is not OK:  " << endl;
  ls << " !!! " << bfile.err_msg_ << endl;

  return false;
}

}

