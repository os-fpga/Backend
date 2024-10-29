#include "RS/rsCheck.h"
#include "file_io/pln_blif_file.h"
#include "file_io/pln_csv_reader.h"
#include <filesystem>

namespace pln {

using std::vector;
using std::string;
using std::endl;

// in 'cleanup' mode checker may modify the blif
// if checker wrote blif, 'outFn' is the file name
bool do_check_blif(CStr cfn,
                   vector<string>& badInputs,
                   vector<string>& badOutputs,
                   vector<uspair>& corrected,
                   string& outFn,
                   bool cleanup) {
  assert(cfn);
  uint16_t tr = ltrace();
  auto& ls = lout();
  badInputs.clear();
  badOutputs.clear();
  corrected.clear();
  outFn.clear();
  flush_out(false);

  BLIF_file bfile(string{cfn});

  if (tr >= 3)
    bfile.setTrace(tr);

  bool exi = false;
  exi = bfile.fileExists();
  if (not exi) {
    err_puts(); flush_out(true);
    lprintf2("[Error] file '%s' does not exist\n", cfn); 
    flush_out(true); err_puts();
    return false;
  }

  exi = bfile.fileAccessible();
  if (not exi) {
    err_puts(); flush_out(true);
    lprintf2("[Error] file '%s' is not accessible\n", cfn); 
    flush_out(true); err_puts();
    return false;
  }

  bool rd_ok = bfile.readBlif();
  if (not rd_ok) {
    err_puts(); flush_out(true);
    lprintf2("[Error] failed reading file '%s'\n", cfn); 
    flush_out(true); err_puts();
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

  lprintf("=====  passed: %s\n", chk_ok ? "YES" : "NO");

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
  uint nISERD = 0, nDSP38 = 0, nDSP19X = 0,
       nTDP_RAM36K = 0, nTDP_RAM18K = 0, nCARRY = 0;
  bfile.countMOGs(nISERD, nDSP38, nDSP19X, nTDP_RAM36K, nTDP_RAM18K, nCARRY);
  ls << "-----     #I_SERDES= " << nISERD << endl;
  ls << "-----       #DSP19X= " << nDSP19X << endl;
  ls << "-----        #DSP38= " << nDSP38 << endl;
  ls << "-----   #TDP_RAM36K= " << nTDP_RAM36K << endl;
  ls << "----- #TDP_RAM18KX2= " << nTDP_RAM18K << endl;
  ls << "-----        #CARRY= " << nCARRY << endl;
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
    if (0&& (numWarn or ::getenv("pln_always_write_blif"))) {
      std::filesystem::path full_path{bfile.fnm_};
      std::filesystem::path base_path = full_path.filename();
      std::string base = base_path.string();
      //lputs9();
      if (cleanup) {
        flush_out(true);
        lprintf("[PLANNER BLIF-CLEANER] : replacing file '%s' ...\n", cfn);
        flush_out(true);
        // cannot write to the currently mem-mapped file,
        // write to a temporary and rename later.
        outFn = str::concat(full_path.lexically_normal().string(),
                    "+BLIF_CLEANER.tmp_", std::to_string(get_PID()));
      } else {
        outFn = str::concat("PLN_WARN", std::to_string(numWarn), "_", base);
      }

      string wr_ok = bfile.writeBlif(outFn, bool(numWarn), corrected);

      if (wr_ok.empty()) {
        lprintf("---!!  FAILED writeBlif to '%s'\n", outFn.c_str());
        if (cleanup)
          lprintf("[PLANNER BLIF-CLEANER] : FAILED\n");
        outFn.clear();
      } else {
        lprintf("+++++  WRITTEN '%s'\n", wr_ok.c_str());
        if (cleanup) {
          lprintf("[PLANNER BLIF-CLEANER] : replaced file '%s'\n", cfn);
        }
      }
    }
  }
  lprintf("=====  passed: %s\n", chk_ok ? "YES" : "NO");

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

// 'corrected' : (lnum, removed_net)
bool do_cleanup_blif(CStr cfn, vector<uspair>& corrected) {
  using namespace fio;
  using namespace std;
  assert(cfn);
  corrected.clear();
  uint16_t tr = ltrace();

  vector<string> badInp, badOut;
  string outFn;

  bool status = do_check_blif(cfn, badInp, badOut, corrected, outFn, true);

  size_t cor_sz = corrected.size();
  if (status and cor_sz) {

    flush_out(true);
    lprintf("[PLANNER BLIF-CLEANER] : corrected netlist '%s'\n", cfn);
    lprintf("-- removed dangling nets (%zu):\n", cor_sz);
    if (tr >= 3) {
      for (size_t i = 0; i < cor_sz; i++) {
        const uspair& cor = corrected[i];
        lprintf("    line %u  net %s\n", cor.first, cor.second.c_str());
        if (tr < 4 and i > 40 and cor_sz > 50) {
          lputs("    ...");
          break;
        }
      }
      lprintf("-- removed dangling nets (%zu).\n", cor_sz);
    }
    flush_out(true);

    bool outFn_exists = Fio::nonEmptyFileExists(outFn);
    bool cfn_exists = Fio::nonEmptyFileExists(cfn);
    //assert(cfn_exists);
    if (outFn_exists and cfn_exists) {

      bool error1 = false, error2 = false;

      // -- 1. add prefix 'orig_' to the original BLIF
      {
        // string newName = str::concat("orig_", cfn);
        filesystem::path oldPath{cfn};
        filesystem::path newPath = oldPath.lexically_normal();
        assert(newPath.has_filename());
        string path_fn = newPath.filename().string();
        string new_fn = str::concat("orig_", path_fn);
        newPath.replace_filename(new_fn);
        error_code ec;
        filesystem::rename(oldPath, newPath, ec);
        if (ec) {
          error1 = true;
          lout() << endl
            << "BLIF-CLEANER: [Error] renaming original BLIF to "
            << newPath << endl
            << "       ERROR: " << ec.message() << '\n' << endl;
        }
        else if (tr >= 3) {
          string newName = newPath.string();
          lprintf("[PLANNER BLIF-CLEANER] :   original BLIF saved as '%s'\n",
                  newName.c_str());
        }
      }

      // -- 2. rename 'outFn' to 'cfn'
      if (not error1) {
        filesystem::path oldPath{outFn};
        filesystem::path newPath{cfn};
        error_code ec;
        filesystem::rename(oldPath, newPath, ec);
        if (ec) {
          error2 = true;
          lout() << endl
            << "BLIF-CLEANER: [Error] renaming temporary BLIF to "
            << newPath << endl
            << "       ERROR: " << ec.message() << '\n' << endl;
        }
        else if (tr >= 3) {
          string newName = newPath.lexically_normal().string();
          lprintf("[PLANNER BLIF-CLEANER] :  corrected BLIF written : %s\n",
                  newName.c_str());
        }
      }
  
      if (error1 or error2) {
        status = false;
        lputs("[PLANNER BLIF-CLEANER] : FAILED due to filesystem errors\n");
      }
    }
  }

  return status;
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
    vector<uspair> corrected;
    string outFn;
    status = do_check_blif(cfn, badInp, badOut, corrected, outFn, false);
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

