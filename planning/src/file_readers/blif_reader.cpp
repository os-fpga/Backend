#include "file_readers/blif_reader.h"
#include "file_readers/pln_blif_file.h"

namespace pln {

using std::vector;
using std::string;
using std::cerr;
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

// ========================= OLD ONE:
#ifdef _PLN_OLD_BLIF_READER_MODE

using namespace blifparse;

struct PlnBlifParserCB : public blifparse::Callback {
  int lineno_ = -1;
  e_circuit_format circuit_format = e_circuit_format::BLIF;

public:
  PlnBlifParserCB(e_circuit_format circuit_format_) : circuit_format(circuit_format_) {}
  void start_parse() override {}

  void filename(string /*fname*/) override {}
  void lineno(int /*line_num*/) override {}

  void begin_model(string /*model_name*/) override {}
  void inputs(vector<string> input_ports) override {
    for (auto input_port : input_ports) {
      inputs_.push_back(input_port);
    }
  }
  void outputs(vector<string> output_ports) override {
    for (auto output_port : output_ports) {
      outputs_.push_back(output_port);
    }
  }

  void names(vector<string> /*nets*/, vector<vector<LogicValue>> /*so_cover*/) override {}
  void latch(string /*input*/,
             string /*output*/,
             LatchType /*        type*/,
             string /*control*/,
             LogicValue /*init*/) override {}
  void subckt(string /*model*/,
              vector<string> /*ports*/,
              vector<string> /*nets*/) override {}
  void blackbox() override {}

  //BLIF Extensions
  void conn(string src, string dst) override {
    if (circuit_format != e_circuit_format::EBLIF) {
      parse_error(lineno_, ".conn", "Supported only in extended BLIF format");
    }
  }

  void cname(string cell_name) override {
    if (circuit_format != e_circuit_format::EBLIF) {
      parse_error(lineno_, ".cname", "Supported only in extended BLIF format");
    }
  }

  void attr(string name, string value) override {
    if (circuit_format != e_circuit_format::EBLIF) {
      parse_error(lineno_, ".attr", "Supported only in extended BLIF format");
    }
  }

  void param(string name, string value) override {
    if (circuit_format != e_circuit_format::EBLIF) {
      parse_error(lineno_, ".param", "Supported only in extended BLIF format");
    }
  }

  void end_model() override {}

  void finish_parse() override {}

  void parse_error(const int curr_lineno, const string& near_text, const string& msg) override {
    fprintf(stderr, "pin_c read_blif: Error at line %d near '%s': %s\n", curr_lineno,
            near_text.c_str(), msg.c_str());
    lprintf("pin_c read_blif: Error at line %d near '%s': %s\n", curr_lineno,
            near_text.c_str(), msg.c_str());
    had_error_ = true;
  }

public:
  bool had_error_ = false;
  vector<string> inputs_;
  vector<string> outputs_;
};

// read port info from blif file
bool BlifReader::read_blif(const string& blif_fn) {
  using namespace fio;
  e_circuit_format circuit_format;
  auto name_ext = vtr::split_ext(blif_fn);
  if (name_ext[1] == ".blif") {
    circuit_format = e_circuit_format::BLIF;
  } else if (name_ext[1] == ".eblif") {
    circuit_format = e_circuit_format::EBLIF;
  } else {
    lputs("\n[Error] pin_c read_blif: blif-file must have either .blif or .eblif extension");
    lprintf("    pin_c read_blif file_name= %s\n", blif_fn.c_str());
    cerr << "[Error] pin_c read_blif: blif-file must have either .blif or .eblif extension" << endl;
    return false;
  }

  const char* fn = blif_fn.c_str();
  if (not Fio::regularFileExists(fn)) {
    lprintf("\n[Error] specified BLIF file %s does not exist\n", fn);
    cerr << "[Error] specified BLIF file does not exist: " << fn << endl;
    return false;
  }
  if (not Fio::nonEmptyFileExists(fn)) {
    lprintf("\n[Error] specified BLIF file %s is empty\n", fn);
    cerr << "[Error] specified BLIF file is empty: " << fn << endl;
    return false;
  }

  PlnBlifParserCB callback(circuit_format);
  blif_parse_filename(blif_fn, callback);
  if (callback.had_error_) {
    return false;
  }

  std::swap(inputs_, callback.inputs_);
  std::swap(outputs_, callback.outputs_);
  if (ltrace() >= 4) {
    lprintf("pin_c: finished read_blif().  #inputs= %zu  #outputs= %zu\n",
             inputs_.size(), outputs_.size());
  }
  return true;
}

#endif // _PLN_OLD_BLIF_READER_MODE

}

