#include "vtr_path.h"
#include "read_blif.h"
#include "blifparse.hpp"
#include "blif_reader.h"

#include "pinc_log.h"

using namespace blifparse;
using namespace std;

 // blif parser callback
 struct BlifParserCallback : public blifparse::Callback
 {
        void start_parse() override {}

        void filename(string /*fname*/) override {}
        void lineno(int /*line_num*/) override {}

        void begin_model(string /*model_name*/) override {}

        void inputs(vector<string> input_ports) override {
            // join inputs_ and input_ports
            inputs_.insert(inputs_.end(), input_ports.cbegin(), input_ports.cend());
        }

        void outputs(vector<string> output_ports) override {
            // join outputs_ and output_ports
            outputs_.insert(outputs_.end(), output_ports.cbegin(), output_ports.cend());
        }

        void names(vector<string> /*nets*/, vector<vector<LogicValue>> /*so_cover*/) override {}

        void latch(string /*input*/, string /*output*/,
            LatchType /*   type*/, string /*control*/,   LogicValue /*init*/) override {}

        void subckt(string /*model*/, vector<string> /*ports*/, vector<string> /*nets*/)  override {}

        void blackbox() override {}

        void end_model() override {}

        void finish_parse() override {}

        void parse_error(const int curr_lineno, const string& near_text, const   string& msg) override {
             fprintf(stderr, "Custom Error at line %d near '%s': %s\n", curr_lineno,  near_text.c_str(), msg.c_str());
             had_error_ = true;
        }

        bool had_error() const { return had_error_; }
        vector<string> get_inputs() { return inputs_;}
        vector<string> get_outputs() { return outputs_;}

     //DATA:
         bool had_error_ = false;
         vector<string> inputs_;
         vector<string> outputs_;
 };

namespace pinc {

// read port info from blif file
bool BlifReader::read_blif(const string& blif_file_name)
{
    uint16_t tr = ltrace();
    auto& ls = lout();
    if (tr >= 2)
        ls << "BlifReader::read_blif( " << blif_file_name << " ) ..." << endl;

    e_circuit_format circuit_format;

    auto name_ext = vtr::split_ext(blif_file_name);
    const string& ext = name_ext[1];
    if (tr >= 2)
        ls << "\t ext= " << ext << endl;

    if (ext == ".blif") {
        circuit_format = e_circuit_format::BLIF;
    } else if (ext == ".eblif") {
        circuit_format = e_circuit_format::EBLIF;
    } else {
        ls << "  (EE) expected file extension .blif or .eblif" << endl;
        return false;
    }

    if (tr >= 2)
        ls << "\t circuit_format= " << int(circuit_format) << endl;

    BlifParserCallback callback;
    blif_parse_filename(blif_file_name, callback);

    if (callback.had_error()) {
        ls << "  (EE) callback.had_error()" << endl;
        return false;
    }

    inputs_ = std::move(callback.inputs_);
    outputs_ = std::move(callback.outputs_);
    return true;
}

} // namespace pinc

