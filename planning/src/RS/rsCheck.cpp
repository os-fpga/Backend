#include "RS/rsCheck.h"
#include "file_readers/pln_blif_file.h"
#include "file_readers/pln_csv_reader.h"
#include "util/nw/Nw.h"

namespace pln {

using std::string;
using std::endl;

static void nw_test_01();

static bool do_check_blif(CStr cfn) {
  assert(cfn);
  uint16_t tr = ltrace();
  auto& ls = lout();

  if (::getenv("pln_nw_test_01")) {
    nw_test_01();
  }

  BLIF_file bfile(string{cfn});

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
    flush_out(true);
    bfile.printInputs(ls);
    flush_out(true);
    bfile.printOutputs(ls);
  }

  flush_out(true);
  ls << ">>>>> checking BLIF " << cfn << "  ..." << endl;

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

  flush_out(true);
  lprintf2("ERROR: BLIF verification failed at %s:%zu\n",
            bfile.fnm_.c_str(), bfile.err_lnum_);

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
  lprintf("  checking %s file: %s\n", fileType, cfn);

  bool status;
  if (blif_vs_csv)
    status = do_check_blif(cfn);
  else
    status = do_check_csv(cfn);

  flush_out(true);

  if (tr >= 6) {
    lprintf("  do_check() status: %i\n", status);
  }
  return status;
}

// DEBUG new NW code:
static void nw_test_01() {
  flush_out(true);
  pln::NW g;
  uint64_t key_cnt = 100;
  uint nid = g.insP(XY{0, 0}, key_cnt++);
  assert(nid > 0);
  assert(g.hasNode(nid));

  g.addRoot(nid);
  g.insP(XY{200, 0}, key_cnt++);
  g.insP(XY{200, 200}, key_cnt++);
  g.insP(XY{0, 200}, key_cnt++);

  g.beComplete();

  g.dump("\t  g is a complete graph");
  lprintf("\t  g. numN()= %u   numE()= %u\n", g.numN(), g.numE());
  assert(g.numE() == 6);

  pln::vecu topo;
  g.getTopo(topo);

  flush_out(true);
  lputs("==== **** DOT ****");

  g.dumpDot("g");
  std::ofstream fos("g.dot");
  if (fos.is_open()) {
    g.printDot(fos, nullptr, false);
    lputs(" written g.dot");
    fos.close();
  } else {
    flush_out(true);
    lprintf2("[Error] NOT fos.is_open()\n");
    flush_out(true);
  }

  flush_out(true);
  lputs("==== **** METIS ****");

  g.dumpMetis(true);

  flush_out(true);

  bool wr_ok = g.writeMetis("g.met", true);
  lprintf("wr_ok:%i\n", wr_ok);
  if (wr_ok)
    lputs(" written g.met");
}

}

