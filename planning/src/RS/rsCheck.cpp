#include "RS/rsCheck.h"
#include "file_io/pln_blif_file.h"
#include "file_io/pln_csv_reader.h"

namespace pln {

using std::vector;
using std::string;
using std::endl;

bool do_check_blif(CStr cfn,
                   vector<string>& badInputs,
                   vector<string>& badOutputs) {
  assert(cfn);
  uint16_t tr = ltrace();
  auto& ls = lout();
  badInputs.clear();
  badOutputs.clear();

  BLIF_file bfile(string{cfn});

  if (tr >= 4)
    bfile.setTrace(tr);

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

  uint numInp = bfile.numInputs();
  uint numOut = bfile.numOutputs();

  lprintf("  (blif_file)   #inputs= %u  #outputs= %u  topModel= %s\n",
          numInp, numOut, bfile.topModel_.c_str());

  if (tr >= 4) {
    flush_out(true);
    bfile.printInputs(ls);
    flush_out(true);
    bfile.printOutputs(ls);
  }

  flush_out(true);
  ls << ">>>>> checking BLIF " << cfn << "  ..." << endl;

  bool chk_ok = bfile.checkBlif(badInputs, badOutputs);
  assert(chk_ok == bfile.chk_ok_);

  lprintf("=====   passed: %s\n", chk_ok ? "YES" : "NO");

  ls << "-----    topModel: " << bfile.topModel_ << endl;
  ls << "-----        file: " << bfile.fnm_ << endl;
  ls << "-----     #inputs= " << numInp << endl;
  ls << "-----    #outputs= " << numOut << endl;
  ls << "-----\n";
  ls << "-----   #ALL_LUTs= " << bfile.countLUTs() << endl;
  ls << "-----       #LUT1= " << bfile.typeHist(prim::LUT1) << endl;
  ls << "-----       #LUT2= " << bfile.typeHist(prim::LUT2) << endl;
  ls << "-----       #LUT3= " << bfile.typeHist(prim::LUT3) << endl;
  ls << "-----       #LUT4= " << bfile.typeHist(prim::LUT4) << endl;
  ls << "-----       #LUT5= " << bfile.typeHist(prim::LUT5) << endl;
  ls << "-----       #LUT6= " << bfile.typeHist(prim::LUT6) << endl;
  ls << "-----    #FFs= " << bfile.countFFs() << endl;

  ls << "-----\n";
  {
  uint nIBUF = 0, nOBUF = 0, nCBUF = 0;
  bfile.countBUFs(nIBUF, nOBUF, nCBUF);
  ls << "-----     #I_BUFs= " << nIBUF
     << "   #I_FABs= " << bfile.typeHist(prim::I_FAB) << endl;

  ls << "-----     #O_BUFs= " << nOBUF
     << "   #O_FABs= " << bfile.typeHist(prim::O_FAB) << endl;

  ls << "-----   #CLK_BUFs= " << nCBUF << endl;
  }

  ls << "-----\n";
  {
  uint nISERD = 0, nDSP38 = 0, nDSP19X = 0, nTDP_RAM36K = 0, nTDP_RAM18K = 0;
  bfile.countMOGs(nISERD, nDSP38, nDSP19X, nTDP_RAM36K, nTDP_RAM18K);
  ls << "-----     #I_SERDES= " << nISERD << endl;
  ls << "-----       #DSP19X= " << nDSP19X << endl;
  ls << "-----        #DSP38= " << nDSP38 << endl;
  ls << "-----   #TDP_RAM36K= " << nTDP_RAM36K << endl;
  ls << "----- #TDP_RAM18KX2= " << nTDP_RAM18K << endl;
  }
  ls << "-----\n";
  ls << "-----    PinGraph: " << bfile.pinGraphFile_ << endl;
  if (bfile.num_MOGs_ and tr >= 6) {
    ls << ">>>>> [NOTE] num_MOGs_ =  " << bfile.num_MOGs_ << endl;
  }
  if (chk_ok) {
    uint numWarn = bfile.numWarnings();
    lprintf("=====    has warnings: %s", numWarn ? "YES" : "NO");
    if (numWarn)
      lprintf("  # WARNINGS= %u", numWarn);
    lputs();
  }
  lprintf("=====    passed: %s\n", chk_ok ? "YES" : "NO");

  flush_out(true);
  if (chk_ok) {
    ls << "=====  BLIF is OK." << endl;
    if (tr >= 4 or bfile.trace_ >= 4) {
      flush_out(true);
      lprintf("     ltrace()= %u  pln_blif_trace= %u\n", tr, bfile.trace_);
    }
    return true;
  }

  ls << "[Error]  ERROR  BLIF is not OK  ERROR" << endl;
  ls << "[Error]  ERROR  " << bfile.err_msg_ << endl;

  flush_out(true);
  uint errLnum = std::max(bfile.err_lnum_, bfile.err_lnum2_);
  lprintf2("ERROR: BLIF verification failed at %s:%u\n",
            bfile.fnm_.c_str(), errLnum);

  return false;
}

static bool do_check_csv(CStr cfn) {
  assert(cfn);
  uint16_t tr = ltrace();
  auto& ls = lout();

  {
    fio::MMapReader fileTester(string{cfn});

    if (tr >= 4)
      fileTester.setTrace(3);

    bool exi = false;
    exi = fileTester.fileExists();
    if (tr >= 7)
      ls << int(exi) << endl;
    if (not exi) {
      lprintf2("[Error] file '%s' does not exist\n", cfn); 
      return false;
    }

    exi = fileTester.fileAccessible();
    if (tr >= 7)
      ls << int(exi) << endl;
    if (not exi) {
      lprintf2("[Error] file '%s' is not accessible\n", cfn); 
      return false;
    }
  }

  flush_out(true);

  // run CSV reader
  PcCsvReader csv_rd;
  bool chk_ok = csv_rd.read_csv(string{cfn}, 1000);

  flush_out(true);
  if (chk_ok) {
    ls << " === CSV is OK." << endl;
    return true;
  }

  flush_out(true);
  lprintf2("ERROR: CSV verification failed at %s:%u\n",
            cfn, 0);

  return false;
}

bool do_check(const rsOpts& opts, bool blif_vs_csv) {
  if (!opts.input_ and !opts.csvFile_)
    return false;
  uint16_t tr = ltrace();

  if (blif_vs_csv and !opts.input_)
    return false;
  if (!blif_vs_csv and !opts.csvFile_)
    return false;

  CStr fileType = blif_vs_csv ? "BLIF" : "CSV";
  CStr cfn = blif_vs_csv ? opts.input_ : opts.csvFile_;
  assert(cfn);
  flush_out(true);
  lprintf("  checking %s file: %s\n", fileType, cfn);

  bool status;
  if (blif_vs_csv) {
    vector<string> badInp, badOut;
    status = do_check_blif(cfn, badInp, badOut);
  } else {
    status = do_check_csv(cfn);
  }

  flush_out(true);

  if (tr >= 6) {
    lprintf("  do_check() status: %i\n", status);
  }
  return status;
}

}

