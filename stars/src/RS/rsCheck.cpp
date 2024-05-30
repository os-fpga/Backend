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

  lprintf("  (blif_file)   #inputs= %u  #outputs= %u  topModel= %s\n",
          bfile.numInputs(), bfile.numOutputs(), bfile.topModel_.c_str());

  if (tr >= 4) {
    lputs();
    bfile.printInputs(ls);
    lputs();
    bfile.printOutputs(ls);
  }

  lputs();
  ls << ">>>>> checking BLIF " << fn1 << "  ..." << endl;

  bool chk_ok = bfile.checkBlif();
  assert(chk_ok == bfile.chk_ok_);

  lprintf(">>>>>     passed: %s\n", chk_ok ? "YES" : "NO");

  ls << ">>>>>   topModel: " << bfile.topModel_ << endl;
  ls << ">>>>>       file: " << bfile.fnm_ << endl;
  if (bfile.num_MOGs_) {
    ls << ">>>>> [NOTE] num_MOGs_ =  " << bfile.num_MOGs_ << endl;
  }

  flush_out(true);
  if (chk_ok) {
    ls << " === BLIF is OK." << endl;
    return true;
  }

  ls << "[Error] !!! BLIF is not OK !!!" << endl;
  ls << "[Error] !!! " << bfile.err_msg_ << endl;

  return false;
}

}

