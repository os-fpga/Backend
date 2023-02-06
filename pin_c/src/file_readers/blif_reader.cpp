#include "vtr_path.h"
#include "read_blif.h"
#include "blifparse.hpp"
#include "blif_reader.h"

// blif parser callback
 using namespace blifparse;
 class BlifParserCallback : public blifparse::Callback {
    int lineno_ = -1;
    e_circuit_format circuit_format = e_circuit_format::BLIF;
     public:
         BlifParserCallback(e_circuit_format circuit_format_) : circuit_format(circuit_format_) {}
         void start_parse() override {}

         void filename(std::string /*fname*/) override {}
         void lineno(int /*line_num*/) override {}

         void begin_model(std::string /*model_name*/) override {}
         void inputs(std::vector<std::string> input_ports) override {
             for (auto input_port : input_ports) {
                 inputs_.push_back(input_port);
             }
         }
         void outputs(std::vector<std::string> output_ports) override {
             for (auto output_port : output_ports) {
                 outputs_.push_back(output_port);
             }
         }

         void names(std::vector<std::string> /*nets*/, std::vector<std::               vector<LogicValue>> /*so_cover*/) override {}
         void latch(std::string /*input*/, std::string /*output*/, LatchType /*        type*/, std::string /*control*/,            LogicValue /*init*/) override {}
         void subckt(std::string /*model*/, std::vector<std::string> /*ports*/, std::  vector<std::string> /*nets*/)         override {}
         void blackbox() override {}

         //BLIF Extensions
    void conn(std::string src, std::string dst) override {
        if (circuit_format != e_circuit_format::EBLIF) {
            parse_error(lineno_, ".conn", "Supported only in extended BLIF format");
        }
    }

    void cname(std::string cell_name) override {
        if (circuit_format != e_circuit_format::EBLIF) {
            parse_error(lineno_, ".cname", "Supported only in extended BLIF format");
        }
    }

    void attr(std::string name, std::string value) override {
        if (circuit_format != e_circuit_format::EBLIF) {
            parse_error(lineno_, ".attr", "Supported only in extended BLIF format");
        }
    }

    void param(std::string name, std::string value) override {
        if (circuit_format != e_circuit_format::EBLIF) {
            parse_error(lineno_, ".param", "Supported only in extended BLIF format");
        }

        // Validate the parameter value
        bool is_valid = is_string_param(value) || is_binary_param(value) || is_real_param(value);

        if (!is_valid) {
            parse_error(lineno_, ".param", "Incorrect parameter value specification");
        }
    }

         void end_model() override {}

         void finish_parse() override {}

         void parse_error(const int curr_lineno, const std::string& near_text, const   std::string& msg) override {
              fprintf(stderr, "Custom Error at line %d near '%s': %s\n", curr_lineno,  near_text.c_str(), msg.c_str());
              had_error_ = true;
         }

         bool had_error() { return had_error_ == true; }
         std::vector<std::string> get_inputs() { return inputs_;}
         std::vector<std::string> get_outputs() { return outputs_;}
     private:
         bool had_error_ = false;
         std::vector<std::string> inputs_;
         std::vector<std::string> outputs_;
};

// read port info from blif file
bool BlifReader::read_blif(const std::string &blif_file_name)
{
    e_circuit_format circuit_format;
    auto name_ext = vtr::split_ext(blif_file_name);
    if (name_ext[1] == ".blif") {
        circuit_format = e_circuit_format::BLIF;
    } else if (name_ext[1] == ".eblif") {
        circuit_format = e_circuit_format::EBLIF;
    } else {
        return false;
    }
    BlifParserCallback callback(circuit_format);
    blif_parse_filename(blif_file_name, callback);
    if (callback.had_error()) {
        return false;
    }
    inputs = callback.get_inputs();
    outputs = callback.get_outputs();
    return true;
}
