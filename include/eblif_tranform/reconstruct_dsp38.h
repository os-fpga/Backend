#pragma once
/**
 * @file reconstruct_dsp38.h
 * @author Manadher Kharroubi (manadher@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-01-
 *
 * @copyright Copyright (c) 2024
*/
#include "reconstruct_utils.h"

struct dsp38_instance {
  std::unordered_map<std::string, std::string> parameters = {
      {"COEFF_0", "00000000000000000000"},
      {"COEFF_1", "00000000000000000000"},
      {"COEFF_2", "00000000000000000000"},
      {"COEFF_3", "00000000000000000000"},
      {"DSP_MODE", "MULTIPLY_ACCUMULATE"},
      {"OUTPUT_REG_EN", "FALSE"},
      {"INPUT_REG_EN", "FALSE"},
      {"accumulator", "0"},
      {"adder", "0"},
      {"output_reg", "0"},
      {"input_reg", "0"}};
  bool set_param(const std::string &par_name, std::string value) {
    if (parameters.find(par_name) != end(parameters)) {
      parameters[par_name] = value;
      if (par_name == "DSP_MODE") {
        if (value == "MULTIPLY_ACCUMULATE")
          parameters["accumulator"] = "1";
        if (value == "MULTIPLY_ADD_SUB")
          parameters["adder"] = "1";
      } else if (par_name == "OUTPUT_REG_EN") {
        if (value == "TRUE")
          parameters["output_reg"] = "1";
      } else if (par_name == "INPUT_REG_EN") {
        if (value == "TRUE")
          parameters["input_reg"] = "1";
      }
      return true;
    }
    return false;
  }
  // DSP38
  std::unordered_map<std::string, std::string> RS_DSP_Primitives{
      {"0000", "RS_DSP_MULT"},
      {"1111", "RS_DSP_MULT"},
      {"1110", "RS_DSP_MULT"},
      {"1101", "RS_DSP_MULT"},
      {"1100", "RS_DSP_MULT"},
      {"1011", "RS_DSP_MULTACC_REGIN_REGOUT"},
      {"1010", "RS_DSP_MULTACC_REGOUT"},
      {"1001", "RS_DSP_MULTACC_REGIN"},
      {"1000", "RS_DSP_MULTACC"},
      {"0111", "RS_DSP_MULTADD_REGIN_REGOUT"},
      {"0110", "RS_DSP_MULTADD_REGOUT"},
      {"0101", "RS_DSP_MULTADD_REGIN"},
      {"0100", "RS_DSP_MULTADD"},
      {"0011", "RS_DSP_MULT_REGIN_REGOUT"},
      {"0010", "RS_DSP_MULT_REGOUT"},
      {"0001", "RS_DSP_MULT_REGIN"}};
  std::string get_MODE_BITS() {
    return parameters["COEFF_0"] + parameters["COEFF_1"] +
           parameters["COEFF_2"] + parameters["COEFF_3"];
  }
  std::string get_block_key() {
    return parameters["accumulator"] + parameters["adder"] +
           parameters["output_reg"] + parameters["input_reg"];
  }
  void print(std::ostream &ofs) {
    std::string rs_prim = RS_DSP_Primitives.at(get_block_key());
    ofs << ".subckt " << rs_prim << " ";
    for (auto &cn : port_connections) {
      std::string low_prim = cn.first;
      std::transform(low_prim.begin(), low_prim.end(), low_prim.begin(),
                     ::tolower);
      if (low_prim == std::string("saturate")) {
        low_prim = "saturate_enable";
      }
      if (low_prim == std::string("reset")) {
        low_prim = "lreset";
      }
      ofs << low_prim << "=" << cn.second << " ";
    }
    ofs << std::endl;
    ofs << ".param MODE_BITS " << get_MODE_BITS() << std::endl;
  }
  std::unordered_map<std::string, std::string> port_connections;
};
