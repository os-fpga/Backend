#pragma once
/*
 * @file reconstruct_ram36k.h
 * @author Manadher Kharroubi (manadher@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-01-
 *
 * @copyright Copyright (c) 2024
 */

#include "reconstruct_utils.h"

struct TDP_RAM36K_instance {
  std::string str_1 = "00000000000000000000000000000001";
  std::string str_2 = "00000000000000000000000000000010";
  std::string str_4 = "00000000000000000000000000000100";
  std::string str_9 = "00000000000000000000000000001001";
  std::string str_18 = "00000000000000000000000000010010";
  std::string str_36 = "00000000000000000000000000100100";
  std::string str_26_zeroes = "00000000000000000000000000";
  std::string read_mode_A;
  std::string read_mode_B;
  std::string write_mode_A;
  std::string write_mode_B;
  std::set<std::string> TDP_RAM36K_internal_signals = {
      "DATA_OUT_A1", "DATA_OUT_A2", "DATA_OUT_B1", "DATA_OUT_B2",
      "WDATA_A1",    "WDATA_A2",    "WDATA_B1",    "WDATA_B2"};
  std::string per_instance_sig_name(std::string sn, int number) {
    std::stringstream ss;
    std::string num;
    ss << number;
    ss >> num;
    for (auto &sg : TDP_RAM36K_internal_signals) {
      if (sn.find(sg) == 0) {
        return sn + std::string("_") + num;
      }
    }
    return sn;
  }
  std::unordered_map<std::string, int> parameterSize = {
      {"INIT", 32768},       {"INIT_PARITY", 4096}, {"READ_WIDTH_A", 32},
      {"WRITE_WIDTH_A", 32}, {"READ_WIDTH_B", 32},  {"WRITE_WIDTH_B", 32}};
  std::unordered_map<std::string, std::string> parameters = {
      {"INIT", ""},
      {"INIT_PARITY", ""},
      {"READ_WIDTH_A", "00000000000000000000000000100100"},
      {"WRITE_WIDTH_A", "00000000000000000000000000100100"},
      {"READ_WIDTH_B", "00000000000000000000000000100100"},
      {"WRITE_WIDTH_B", "00000000000000000000000000100100"}};
  bool set_param(const std::string &par_name, std::string value) {
    if (parameters.find(par_name) != end(parameters)) {
      // Implementing expansion and truncation of parameters to their specified
      // lengths
      if (value.size() < parameterSize[par_name]) {
        value =
            std::string(parameterSize[par_name] - value.size(), '0') + value;
      } else if (value.size() > parameterSize[par_name]) {
        value = value.substr(parameterSize[par_name] - value.size());
      }
      parameters[par_name] = value;
      return true;
    }
    return false;
  }
  // The Keys are RS_TDP36K ports
  std::unordered_map<std::string, std::string>
      TDP_RAM36K_to_RS_TDP36K_port_map = {{"WEN_A1", "WEN_A"},
                                          {"WEN_B1", "WEN_B"},
                                          {"REN_A1", "REN_A"},
                                          {"REN_B1", "REN_B"},
                                          {"CLK_A1", "CLK_A"},
                                          {"CLK_B1", "CLK_B"},
                                          {"BE_A1[0]", "BE_A[0]"},
                                          {"BE_A1[1]", "BE_A[1]"},
                                          {"BE_B1[0]", "BE_B[0]"},
                                          {"BE_B1[1]", "BE_B[1]"},
                                          {"ADDR_A1[0]", "ADDR_A[0]"},
                                          {"ADDR_A1[1]", "ADDR_A[1]"},
                                          {"ADDR_A1[2]", "ADDR_A[2]"},
                                          {"ADDR_A1[3]", "ADDR_A[3]"},
                                          {"ADDR_A1[4]", "ADDR_A[4]"},
                                          {"ADDR_A1[5]", "ADDR_A[5]"},
                                          {"ADDR_A1[6]", "ADDR_A[6]"},
                                          {"ADDR_A1[7]", "ADDR_A[7]"},
                                          {"ADDR_A1[8]", "ADDR_A[8]"},
                                          {"ADDR_A1[9]", "ADDR_A[9]"},
                                          {"ADDR_A1[10]", "ADDR_A[10]"},
                                          {"ADDR_A1[11]", "ADDR_A[11]"},
                                          {"ADDR_A1[12]", "ADDR_A[12]"},
                                          {"ADDR_A1[13]", "ADDR_A[13]"},
                                          {"ADDR_A1[14]", "ADDR_A[14]"},
                                          {"ADDR_B1[0]", "ADDR_B[0]"},
                                          {"ADDR_B1[1]", "ADDR_B[1]"},
                                          {"ADDR_B1[2]", "ADDR_B[2]"},
                                          {"ADDR_B1[3]", "ADDR_B[3]"},
                                          {"ADDR_B1[4]", "ADDR_B[4]"},
                                          {"ADDR_B1[5]", "ADDR_B[5]"},
                                          {"ADDR_B1[6]", "ADDR_B[6]"},
                                          {"ADDR_B1[7]", "ADDR_B[7]"},
                                          {"ADDR_B1[8]", "ADDR_B[8]"},
                                          {"ADDR_B1[9]", "ADDR_B[9]"},
                                          {"ADDR_B1[10]", "ADDR_B[10]"},
                                          {"ADDR_B1[11]", "ADDR_B[11]"},
                                          {"ADDR_B1[12]", "ADDR_B[12]"},
                                          {"ADDR_B1[13]", "ADDR_B[13]"},
                                          {"ADDR_B1[14]", "ADDR_B[14]"},
                                          {"WDATA_A1[0]", "WDATA_A1[0]"},
                                          {"WDATA_A1[1]", "WDATA_A1[1]"},
                                          {"WDATA_A1[2]", "WDATA_A1[2]"},
                                          {"WDATA_A1[3]", "WDATA_A1[3]"},
                                          {"WDATA_A1[4]", "WDATA_A1[4]"},
                                          {"WDATA_A1[5]", "WDATA_A1[5]"},
                                          {"WDATA_A1[6]", "WDATA_A1[6]"},
                                          {"WDATA_A1[7]", "WDATA_A1[7]"},
                                          {"WDATA_A1[8]", "WDATA_A1[8]"},
                                          {"WDATA_A1[9]", "WDATA_A1[9]"},
                                          {"WDATA_A1[10]", "WDATA_A1[10]"},
                                          {"WDATA_A1[11]", "WDATA_A1[11]"},
                                          {"WDATA_A1[12]", "WDATA_A1[12]"},
                                          {"WDATA_A1[13]", "WDATA_A1[13]"},
                                          {"WDATA_A1[14]", "WDATA_A1[14]"},
                                          {"WDATA_A1[15]", "WDATA_A1[15]"},
                                          {"WDATA_A1[16]", "WDATA_A1[16]"},
                                          {"WDATA_A1[17]", "WDATA_A1[17]"},
                                          {"WDATA_B1[0]", "WDATA_B1[0]"},
                                          {"WDATA_B1[1]", "WDATA_B1[1]"},
                                          {"WDATA_B1[2]", "WDATA_B1[2]"},
                                          {"WDATA_B1[3]", "WDATA_B1[3]"},
                                          {"WDATA_B1[4]", "WDATA_B1[4]"},
                                          {"WDATA_B1[5]", "WDATA_B1[5]"},
                                          {"WDATA_B1[6]", "WDATA_B1[6]"},
                                          {"WDATA_B1[7]", "WDATA_B1[7]"},
                                          {"WDATA_B1[8]", "WDATA_B1[8]"},
                                          {"WDATA_B1[9]", "WDATA_B1[9]"},
                                          {"WDATA_B1[10]", "WDATA_B1[10]"},
                                          {"WDATA_B1[11]", "WDATA_B1[11]"},
                                          {"WDATA_B1[12]", "WDATA_B1[12]"},
                                          {"WDATA_B1[13]", "WDATA_B1[13]"},
                                          {"WDATA_B1[14]", "WDATA_B1[14]"},
                                          {"WDATA_B1[15]", "WDATA_B1[15]"},
                                          {"WDATA_B1[16]", "WDATA_B1[16]"},
                                          {"WDATA_B1[17]", "WDATA_B1[17]"},
                                          {"RDATA_A1[0]", "DATA_OUT_A1[0]"},
                                          {"RDATA_A1[1]", "DATA_OUT_A1[1]"},
                                          {"RDATA_A1[2]", "DATA_OUT_A1[2]"},
                                          {"RDATA_A1[3]", "DATA_OUT_A1[3]"},
                                          {"RDATA_A1[4]", "DATA_OUT_A1[4]"},
                                          {"RDATA_A1[5]", "DATA_OUT_A1[5]"},
                                          {"RDATA_A1[6]", "DATA_OUT_A1[6]"},
                                          {"RDATA_A1[7]", "DATA_OUT_A1[7]"},
                                          {"RDATA_A1[8]", "DATA_OUT_A1[8]"},
                                          {"RDATA_A1[9]", "DATA_OUT_A1[9]"},
                                          {"RDATA_A1[10]", "DATA_OUT_A1[10]"},
                                          {"RDATA_A1[11]", "DATA_OUT_A1[11]"},
                                          {"RDATA_A1[12]", "DATA_OUT_A1[12]"},
                                          {"RDATA_A1[13]", "DATA_OUT_A1[13]"},
                                          {"RDATA_A1[14]", "DATA_OUT_A1[14]"},
                                          {"RDATA_A1[15]", "DATA_OUT_A1[15]"},
                                          {"RDATA_A1[16]", "DATA_OUT_A1[16]"},
                                          {"RDATA_A1[17]", "DATA_OUT_A1[17]"},
                                          {"RDATA_B1[0]", "DATA_OUT_B1[0]"},
                                          {"RDATA_B1[1]", "DATA_OUT_B1[1]"},
                                          {"RDATA_B1[2]", "DATA_OUT_B1[2]"},
                                          {"RDATA_B1[3]", "DATA_OUT_B1[3]"},
                                          {"RDATA_B1[4]", "DATA_OUT_B1[4]"},
                                          {"RDATA_B1[5]", "DATA_OUT_B1[5]"},
                                          {"RDATA_B1[6]", "DATA_OUT_B1[6]"},
                                          {"RDATA_B1[7]", "DATA_OUT_B1[7]"},
                                          {"RDATA_B1[8]", "DATA_OUT_B1[8]"},
                                          {"RDATA_B1[9]", "DATA_OUT_B1[9]"},
                                          {"RDATA_B1[10]", "DATA_OUT_B1[10]"},
                                          {"RDATA_B1[11]", "DATA_OUT_B1[11]"},
                                          {"RDATA_B1[12]", "DATA_OUT_B1[12]"},
                                          {"RDATA_B1[13]", "DATA_OUT_B1[13]"},
                                          {"RDATA_B1[14]", "DATA_OUT_B1[14]"},
                                          {"RDATA_B1[15]", "DATA_OUT_B1[15]"},
                                          {"RDATA_B1[16]", "DATA_OUT_B1[16]"},
                                          {"RDATA_B1[17]", "DATA_OUT_B1[17]"},
                                          {"FLUSH1", "$false"},
                                          {"WEN_A2", "WEN_A"},
                                          {"WEN_B2", "WEN_B"},
                                          {"REN_A2", "REN_A"},
                                          {"REN_B2", "REN_B"},
                                          {"CLK_A2", "CLK_A"},
                                          {"CLK_B2", "CLK_B"},
                                          {"BE_A2[0]", "BE_A[2]"},
                                          {"BE_A2[1]", "BE_A[3]"},
                                          {"BE_B2[0]", "BE_B[2]"},
                                          {"BE_B2[1]", "BE_B[3]"},
                                          {"ADDR_A2[0]", "ADDR_A[0]"},
                                          {"ADDR_A2[1]", "ADDR_A[1]"},
                                          {"ADDR_A2[2]", "ADDR_A[2]"},
                                          {"ADDR_A2[3]", "ADDR_A[3]"},
                                          {"ADDR_A2[4]", "ADDR_A[4]"},
                                          {"ADDR_A2[5]", "ADDR_A[5]"},
                                          {"ADDR_A2[6]", "ADDR_A[6]"},
                                          {"ADDR_A2[7]", "ADDR_A[7]"},
                                          {"ADDR_A2[8]", "ADDR_A[8]"},
                                          {"ADDR_A2[9]", "ADDR_A[9]"},
                                          {"ADDR_A2[10]", "ADDR_A[10]"},
                                          {"ADDR_A2[11]", "ADDR_A[11]"},
                                          {"ADDR_A2[12]", "ADDR_A[12]"},
                                          {"ADDR_A2[13]", "ADDR_A[13]"},
                                          {"ADDR_A2[14]", "ADDR_A[14]"},
                                          {"ADDR_B2[0]", "ADDR_B[0]"},
                                          {"ADDR_B2[1]", "ADDR_B[1]"},
                                          {"ADDR_B2[2]", "ADDR_B[2]"},
                                          {"ADDR_B2[3]", "ADDR_B[3]"},
                                          {"ADDR_B2[4]", "ADDR_B[4]"},
                                          {"ADDR_B2[5]", "ADDR_B[5]"},
                                          {"ADDR_B2[6]", "ADDR_B[6]"},
                                          {"ADDR_B2[7]", "ADDR_B[7]"},
                                          {"ADDR_B2[8]", "ADDR_B[8]"},
                                          {"ADDR_B2[9]", "ADDR_B[9]"},
                                          {"ADDR_B2[10]", "ADDR_B[10]"},
                                          {"ADDR_B2[11]", "ADDR_B[11]"},
                                          {"ADDR_B2[12]", "ADDR_B[12]"},
                                          {"ADDR_B2[13]", "ADDR_B[13]"},
                                          {"ADDR_B2[14]", "ADDR_B[14]"},
                                          {"WDATA_A2[0]", "WDATA_A2[0]"},
                                          {"WDATA_A2[1]", "WDATA_A2[1]"},
                                          {"WDATA_A2[2]", "WDATA_A2[2]"},
                                          {"WDATA_A2[3]", "WDATA_A2[3]"},
                                          {"WDATA_A2[4]", "WDATA_A2[4]"},
                                          {"WDATA_A2[5]", "WDATA_A2[5]"},
                                          {"WDATA_A2[6]", "WDATA_A2[6]"},
                                          {"WDATA_A2[7]", "WDATA_A2[7]"},
                                          {"WDATA_A2[8]", "WDATA_A2[8]"},
                                          {"WDATA_A2[9]", "WDATA_A2[9]"},
                                          {"WDATA_A2[10]", "WDATA_A2[10]"},
                                          {"WDATA_A2[11]", "WDATA_A2[11]"},
                                          {"WDATA_A2[12]", "WDATA_A2[12]"},
                                          {"WDATA_A2[13]", "WDATA_A2[13]"},
                                          {"WDATA_A2[14]", "WDATA_A2[14]"},
                                          {"WDATA_A2[15]", "WDATA_A2[15]"},
                                          {"WDATA_A2[16]", "WDATA_A2[16]"},
                                          {"WDATA_A2[17]", "WDATA_A2[17]"},
                                          {"WDATA_B2[0]", "WDATA_B2[0]"},
                                          {"WDATA_B2[1]", "WDATA_B2[1]"},
                                          {"WDATA_B2[2]", "WDATA_B2[2]"},
                                          {"WDATA_B2[3]", "WDATA_B2[3]"},
                                          {"WDATA_B2[4]", "WDATA_B2[4]"},
                                          {"WDATA_B2[5]", "WDATA_B2[5]"},
                                          {"WDATA_B2[6]", "WDATA_B2[6]"},
                                          {"WDATA_B2[7]", "WDATA_B2[7]"},
                                          {"WDATA_B2[8]", "WDATA_B2[8]"},
                                          {"WDATA_B2[9]", "WDATA_B2[9]"},
                                          {"WDATA_B2[10]", "WDATA_B2[10]"},
                                          {"WDATA_B2[11]", "WDATA_B2[11]"},
                                          {"WDATA_B2[12]", "WDATA_B2[12]"},
                                          {"WDATA_B2[13]", "WDATA_B2[13]"},
                                          {"WDATA_B2[14]", "WDATA_B2[14]"},
                                          {"WDATA_B2[15]", "WDATA_B2[15]"},
                                          {"WDATA_B2[16]", "WDATA_B2[16]"},
                                          {"WDATA_B2[17]", "WDATA_B2[17]"},
                                          {"RDATA_A2[0]", "DATA_OUT_A2[0]"},
                                          {"RDATA_A2[1]", "DATA_OUT_A2[1]"},
                                          {"RDATA_A2[2]", "DATA_OUT_A2[2]"},
                                          {"RDATA_A2[3]", "DATA_OUT_A2[3]"},
                                          {"RDATA_A2[4]", "DATA_OUT_A2[4]"},
                                          {"RDATA_A2[5]", "DATA_OUT_A2[5]"},
                                          {"RDATA_A2[6]", "DATA_OUT_A2[6]"},
                                          {"RDATA_A2[7]", "DATA_OUT_A2[7]"},
                                          {"RDATA_A2[8]", "DATA_OUT_A2[8]"},
                                          {"RDATA_A2[9]", "DATA_OUT_A2[9]"},
                                          {"RDATA_A2[10]", "DATA_OUT_A2[10]"},
                                          {"RDATA_A2[11]", "DATA_OUT_A2[11]"},
                                          {"RDATA_A2[12]", "DATA_OUT_A2[12]"},
                                          {"RDATA_A2[13]", "DATA_OUT_A2[13]"},
                                          {"RDATA_A2[14]", "DATA_OUT_A2[14]"},
                                          {"RDATA_A2[15]", "DATA_OUT_A2[15]"},
                                          {"RDATA_A2[16]", "DATA_OUT_A2[16]"},
                                          {"RDATA_A2[17]", "DATA_OUT_A2[17]"},
                                          {"RDATA_B2[0]", "DATA_OUT_B2[0]"},
                                          {"RDATA_B2[1]", "DATA_OUT_B2[1]"},
                                          {"RDATA_B2[2]", "DATA_OUT_B2[2]"},
                                          {"RDATA_B2[3]", "DATA_OUT_B2[3]"},
                                          {"RDATA_B2[4]", "DATA_OUT_B2[4]"},
                                          {"RDATA_B2[5]", "DATA_OUT_B2[5]"},
                                          {"RDATA_B2[6]", "DATA_OUT_B2[6]"},
                                          {"RDATA_B2[7]", "DATA_OUT_B2[7]"},
                                          {"RDATA_B2[8]", "DATA_OUT_B2[8]"},
                                          {"RDATA_B2[9]", "DATA_OUT_B2[9]"},
                                          {"RDATA_B2[10]", "DATA_OUT_B2[10]"},
                                          {"RDATA_B2[11]", "DATA_OUT_B2[11]"},
                                          {"RDATA_B2[12]", "DATA_OUT_B2[12]"},
                                          {"RDATA_B2[13]", "DATA_OUT_B2[13]"},
                                          {"RDATA_B2[14]", "DATA_OUT_B2[14]"},
                                          {"RDATA_B2[15]", "DATA_OUT_B2[15]"},
                                          {"RDATA_B2[16]", "DATA_OUT_B2[16]"},
                                          {"RDATA_B2[17]", "DATA_OUT_B2[17]"},
                                          {"FLUSH2", "$false"}};
  std::string conf_code(std::string W) {
    return W == str_36   ? "110"
           : W == str_18 ? "010"
           : W == str_9  ? "100"
           : W == str_4  ? "001"
           : W == str_2  ? "011"
                         : "101";
  }
  std::vector<std::string> TDP_RAM36K_Assigns_params_WRITE_WIDTH_A_9_4_2_1 = {
      ".names WPARITY_A[0] WDATA_A1[17]", ".names $false WDATA_A1[16]",
      ".names $false WDATA_A1[15]", ".names $false WDATA_A1[14]",
      ".names $false WDATA_A1[13]", ".names $false WDATA_A1[12]",
      ".names $false WDATA_A1[11]", ".names $false WDATA_A1[10]",
      ".names $false WDATA_A1[9]", ".names WDATA_A[7] WDATA_A1[8]",
      ".names WDATA_A[6] WDATA_A1[7]", ".names WDATA_A[5] WDATA_A1[6]",
      ".names WDATA_A[4] WDATA_A1[5]", ".names WDATA_A[3] WDATA_A1[4]",
      ".names WDATA_A[2] WDATA_A1[3]", ".names WDATA_A[1] WDATA_A1[2]",
      // ".names $undef WDATA_A2[17]",
      // ".names $undef WDATA_A2[16]",
      // ".names $undef WDATA_A2[15]",
      // ".names $undef WDATA_A2[14]",
      // ".names $undef WDATA_A2[13]",
      // ".names $undef WDATA_A2[12]",
      // ".names $undef WDATA_A2[11]",
      // ".names $undef WDATA_A2[10]",
      // ".names $undef WDATA_A2[9]",
      // ".names $undef WDATA_A2[8]",
      // ".names $undef WDATA_A2[7]",
      // ".names $undef WDATA_A2[6]",
      // ".names $undef WDATA_A2[5]",
      // ".names $undef WDATA_A2[4]",
      // ".names $undef WDATA_A2[3]",
      // ".names $undef WDATA_A2[2]",
      // ".names $undef WDATA_A2[1]",
      // ".names $undef WDATA_A2[0]",
      ".names WDATA_A[0] WDATA_A1[1]"};
  std::vector<std::string> TDP_RAM36K_Assigns_params_WRITE_WIDTH_B_9_4_2_1 = {
      ".names WPARITY_B[0] WDATA_B1[17]", ".names $false WDATA_B1[16]",
      ".names $false WDATA_B1[15]", ".names $false WDATA_B1[14]",
      ".names $false WDATA_B1[13]", ".names $false WDATA_B1[12]",
      ".names $false WDATA_B1[11]", ".names $false WDATA_B1[10]",
      ".names $false WDATA_B1[9]", ".names WDATA_B[7] WDATA_B1[8]",
      ".names WDATA_B[6] WDATA_B1[7]", ".names WDATA_B[5] WDATA_B1[6]",
      ".names WDATA_B[4] WDATA_B1[5]", ".names WDATA_B[3] WDATA_B1[4]",
      ".names WDATA_B[2] WDATA_B1[3]", ".names WDATA_B[1] WDATA_B1[2]",
      // ".names $undef WDATA_B2[17]",
      // ".names $undef WDATA_B2[16]",
      // ".names $undef WDATA_B2[15]",
      // ".names $undef WDATA_B2[14]",
      // ".names $undef WDATA_B2[13]",
      // ".names $undef WDATA_B2[12]",
      // ".names $undef WDATA_B2[11]",
      // ".names $undef WDATA_B2[10]",
      // ".names $undef WDATA_B2[9]",
      // ".names $undef WDATA_B2[8]",
      // ".names $undef WDATA_B2[7]",
      // ".names $undef WDATA_B2[6]",
      // ".names $undef WDATA_B2[5]",
      // ".names $undef WDATA_B2[4]",
      // ".names $undef WDATA_B2[3]",
      // ".names $undef WDATA_B2[2]",
      // ".names $undef WDATA_B2[1]",
      // ".names $undef WDATA_B2[0]",
      ".names WDATA_B[0] WDATA_B1[1]"};
  std::vector<std::string> TDP_RAM36K_Assigns_params_READ_WIDTH_A_9_4_2_1 = {
      ".names DATA_OUT_A1[7] RDATA_A[31]",  ".names DATA_OUT_A1[6] RDATA_A[30]",
      ".names DATA_OUT_A1[5] RDATA_A[29]",  ".names DATA_OUT_A1[4] RDATA_A[28]",
      ".names DATA_OUT_A1[3] RDATA_A[27]",  ".names DATA_OUT_A1[2] RDATA_A[26]",
      ".names DATA_OUT_A1[1] RDATA_A[25]",  ".names DATA_OUT_A1[0] RDATA_A[24]",
      ".names DATA_OUT_A1[16] RPARITY_A[3]"};
  std::vector<std::string> TDP_RAM36K_Assigns_params_READ_WIDTH_B_9_4_2_1 = {
      ".names DATA_OUT_B1[7] RDATA_B[31]",  ".names DATA_OUT_B1[6] RDATA_B[30]",
      ".names DATA_OUT_B1[5] RDATA_B[29]",  ".names DATA_OUT_B1[4] RDATA_B[28]",
      ".names DATA_OUT_B1[3] RDATA_B[27]",  ".names DATA_OUT_B1[2] RDATA_B[26]",
      ".names DATA_OUT_B1[1] RDATA_B[25]",  ".names DATA_OUT_B1[0] RDATA_B[24]",
      ".names DATA_OUT_B1[16] RPARITY_B[3]"};
  std::vector<std::string> TDP_RAM36K_Assigns_params_WRITE_WIDTH_A_36_18 = {
      ".names WPARITY_A[1] WDATA_A1[17]", ".names WPARITY_A[0] WDATA_A1[16]",
      ".names WDATA_A[15] WDATA_A1[15]",  ".names WDATA_A[14] WDATA_A1[14]",
      ".names WDATA_A[13] WDATA_A1[13]",  ".names WDATA_A[12] WDATA_A1[12]",
      ".names WDATA_A[11] WDATA_A1[11]",  ".names WDATA_A[10] WDATA_A1[10]",
      ".names WDATA_A[9] WDATA_A1[9]",    ".names WDATA_A[8] WDATA_A1[8]",
      ".names WDATA_A[7] WDATA_A1[7]",    ".names WDATA_A[6] WDATA_A1[6]",
      ".names WDATA_A[5] WDATA_A1[5]",    ".names WDATA_A[4] WDATA_A1[4]",
      ".names WDATA_A[3] WDATA_A1[3]",    ".names WDATA_A[2] WDATA_A1[2]",
      ".names WDATA_A[1] WDATA_A1[1]",    ".names WDATA_A[0] WDATA_A1[0]",
      ".names WPARITY_A[3] WDATA_A2[17]", ".names WPARITY_A[2] WDATA_A2[16]",
      ".names WDATA_A[31] WDATA_A2[15]",  ".names WDATA_A[30] WDATA_A2[14]",
      ".names WDATA_A[29] WDATA_A2[13]",  ".names WDATA_A[28] WDATA_A2[12]",
      ".names WDATA_A[27] WDATA_A2[11]",  ".names WDATA_A[26] WDATA_A2[10]",
      ".names WDATA_A[25] WDATA_A2[9]",   ".names WDATA_A[24] WDATA_A2[8]",
      ".names WDATA_A[23] WDATA_A2[7]",   ".names WDATA_A[22] WDATA_A2[6]",
      ".names WDATA_A[21] WDATA_A2[5]",   ".names WDATA_A[20] WDATA_A2[4]",
      ".names WDATA_A[19] WDATA_A2[3]",   ".names WDATA_A[18] WDATA_A2[2]",
      ".names WDATA_A[17] WDATA_A2[1]",   ".names WDATA_A[16] WDATA_A2[0]"};
  std::vector<std::string> TDP_RAM36K_Assigns_params_WRITE_WIDTH_B_36_18 = {
      ".names WPARITY_B[1] WDATA_B1[17]", ".names WPARITY_B[0] WDATA_B1[16]",
      ".names WDATA_B[15] WDATA_B1[15]",  ".names WDATA_B[14] WDATA_B1[14]",
      ".names WDATA_B[13] WDATA_B1[13]",  ".names WDATA_B[12] WDATA_B1[12]",
      ".names WDATA_B[11] WDATA_B1[11]",  ".names WDATA_B[10] WDATA_B1[10]",
      ".names WDATA_B[9] WDATA_B1[9]",    ".names WDATA_B[8] WDATA_B1[8]",
      ".names WDATA_B[7] WDATA_B1[7]",    ".names WDATA_B[6] WDATA_B1[6]",
      ".names WDATA_B[5] WDATA_B1[5]",    ".names WDATA_B[4] WDATA_B1[4]",
      ".names WDATA_B[3] WDATA_B1[3]",    ".names WDATA_B[2] WDATA_B1[2]",
      ".names WDATA_B[1] WDATA_B1[1]",    ".names WDATA_B[0] WDATA_B1[0]",
      ".names WPARITY_B[3] WDATA_B2[17]", ".names WPARITY_B[2] WDATA_B2[16]",
      ".names WDATA_B[31] WDATA_B2[15]",  ".names WDATA_B[30] WDATA_B2[14]",
      ".names WDATA_B[29] WDATA_B2[13]",  ".names WDATA_B[28] WDATA_B2[12]",
      ".names WDATA_B[27] WDATA_B2[11]",  ".names WDATA_B[26] WDATA_B2[10]",
      ".names WDATA_B[25] WDATA_B2[9]",   ".names WDATA_B[24] WDATA_B2[8]",
      ".names WDATA_B[23] WDATA_B2[7]",   ".names WDATA_B[22] WDATA_B2[6]",
      ".names WDATA_B[21] WDATA_B2[5]",   ".names WDATA_B[20] WDATA_B2[4]",
      ".names WDATA_B[19] WDATA_B2[3]",   ".names WDATA_B[18] WDATA_B2[2]",
      ".names WDATA_B[17] WDATA_B2[1]",   ".names WDATA_B[16] WDATA_B2[0]"};
  std::vector<std::string> TDP_RAM36K_Assigns_params_READ_WIDTH_A_36_18 = {
      ".names DATA_OUT_A2[15] RDATA_A[31]",
      ".names DATA_OUT_A2[14] RDATA_A[30]",
      ".names DATA_OUT_A2[13] RDATA_A[29]",
      ".names DATA_OUT_A2[12] RDATA_A[28]",
      ".names DATA_OUT_A2[11] RDATA_A[27]",
      ".names DATA_OUT_A2[10] RDATA_A[26]",
      ".names DATA_OUT_A2[9] RDATA_A[25]",
      ".names DATA_OUT_A2[8] RDATA_A[24]",
      ".names DATA_OUT_A2[7] RDATA_A[23]",
      ".names DATA_OUT_A2[6] RDATA_A[22]",
      ".names DATA_OUT_A2[5] RDATA_A[21]",
      ".names DATA_OUT_A2[4] RDATA_A[20]",
      ".names DATA_OUT_A2[3] RDATA_A[19]",
      ".names DATA_OUT_A2[2] RDATA_A[18]",
      ".names DATA_OUT_A2[1] RDATA_A[17]",
      ".names DATA_OUT_A2[0] RDATA_A[16]",
      ".names DATA_OUT_A1[15] RDATA_A[15]",
      ".names DATA_OUT_A1[14] RDATA_A[14]",
      ".names DATA_OUT_A1[13] RDATA_A[13]",
      ".names DATA_OUT_A1[12] RDATA_A[12]",
      ".names DATA_OUT_A1[11] RDATA_A[11]",
      ".names DATA_OUT_A1[10] RDATA_A[10]",
      ".names DATA_OUT_A1[9] RDATA_A[9]",
      ".names DATA_OUT_A1[8] RDATA_A[8]",
      ".names DATA_OUT_A1[7] RDATA_A[7]",
      ".names DATA_OUT_A1[6] RDATA_A[6]",
      ".names DATA_OUT_A1[5] RDATA_A[5]",
      ".names DATA_OUT_A1[4] RDATA_A[4]",
      ".names DATA_OUT_A1[3] RDATA_A[3]",
      ".names DATA_OUT_A1[2] RDATA_A[2]",
      ".names DATA_OUT_A1[1] RDATA_A[1]",
      ".names DATA_OUT_A1[0] RDATA_A[0]",
      ".names DATA_OUT_A2[17] RPARITY_A[3]",
      ".names DATA_OUT_A2[16] RPARITY_A[2]",
      ".names DATA_OUT_A1[17] RPARITY_A[1]",
      ".names DATA_OUT_A1[16] RPARITY_A[0]"};
  std::vector<std::string> TDP_RAM36K_Assigns_params_READ_WIDTH_B_36_18 = {
      ".names DATA_OUT_B2[15] RDATA_B[31]",
      ".names DATA_OUT_B2[14] RDATA_B[30]",
      ".names DATA_OUT_B2[13] RDATA_B[29]",
      ".names DATA_OUT_B2[12] RDATA_B[28]",
      ".names DATA_OUT_B2[11] RDATA_B[27]",
      ".names DATA_OUT_B2[10] RDATA_B[26]",
      ".names DATA_OUT_B2[9] RDATA_B[25]",
      ".names DATA_OUT_B2[8] RDATA_B[24]",
      ".names DATA_OUT_B2[7] RDATA_B[23]",
      ".names DATA_OUT_B2[6] RDATA_B[22]",
      ".names DATA_OUT_B2[5] RDATA_B[21]",
      ".names DATA_OUT_B2[4] RDATA_B[20]",
      ".names DATA_OUT_B2[3] RDATA_B[19]",
      ".names DATA_OUT_B2[2] RDATA_B[18]",
      ".names DATA_OUT_B2[1] RDATA_B[17]",
      ".names DATA_OUT_B2[0] RDATA_B[16]",
      ".names DATA_OUT_B1[15] RDATA_B[15]",
      ".names DATA_OUT_B1[14] RDATA_B[14]",
      ".names DATA_OUT_B1[13] RDATA_B[13]",
      ".names DATA_OUT_B1[12] RDATA_B[12]",
      ".names DATA_OUT_B1[11] RDATA_B[11]",
      ".names DATA_OUT_B1[10] RDATA_B[10]",
      ".names DATA_OUT_B1[9] RDATA_B[9]",
      ".names DATA_OUT_B1[8] RDATA_B[8]",
      ".names DATA_OUT_B1[7] RDATA_B[7]",
      ".names DATA_OUT_B1[6] RDATA_B[6]",
      ".names DATA_OUT_B1[5] RDATA_B[5]",
      ".names DATA_OUT_B1[4] RDATA_B[4]",
      ".names DATA_OUT_B1[3] RDATA_B[3]",
      ".names DATA_OUT_B1[2] RDATA_B[2]",
      ".names DATA_OUT_B1[1] RDATA_B[1]",
      ".names DATA_OUT_B1[0] RDATA_B[0]",
      ".names DATA_OUT_B2[17] RPARITY_B[3]",
      ".names DATA_OUT_B2[16] RPARITY_B[2]",
      ".names DATA_OUT_B1[17] RPARITY_B[1]",
      ".names DATA_OUT_B1[16] RPARITY_B[0]"};
  std::unordered_map<std::string, std::vector<std::string>> assign_map = {
      {"WRITE_WIDTH_A_9_4_2_1",
       TDP_RAM36K_Assigns_params_WRITE_WIDTH_A_9_4_2_1},
      {"WRITE_WIDTH_B_9_4_2_1",
       TDP_RAM36K_Assigns_params_WRITE_WIDTH_B_9_4_2_1},
      {"READ_WIDTH_A_9_4_2_1", TDP_RAM36K_Assigns_params_READ_WIDTH_A_9_4_2_1},
      {"READ_WIDTH_B_9_4_2_1", TDP_RAM36K_Assigns_params_READ_WIDTH_B_9_4_2_1},
      {"WRITE_WIDTH_A_36_18", TDP_RAM36K_Assigns_params_WRITE_WIDTH_A_36_18},
      {"WRITE_WIDTH_B_36_18", TDP_RAM36K_Assigns_params_WRITE_WIDTH_B_36_18},
      {"READ_WIDTH_A_36_18", TDP_RAM36K_Assigns_params_READ_WIDTH_A_36_18},
      {"READ_WIDTH_B_36_18", TDP_RAM36K_Assigns_params_READ_WIDTH_B_36_18},
      {"DEFAULT", std::vector<std::string>()}};
  std::vector<std::string> &get_param_value_assigns(std::string p) {
    if (parameters.find(p) == end(parameters))
      throw std::invalid_argument("Invalid TDP_RAM36K parameter name " + p);
    std::unordered_set<std::string> _9_4_2_1 = {str_1, str_2, str_4, str_9};
    std::unordered_set<std::string> _36_18 = {str_18, str_36};
    if (_9_4_2_1.find(parameters[p]) != end(_9_4_2_1)) {
      return assign_map[p + "_9_4_2_1"];
    };
    if (_36_18.find(parameters[p]) != end(_36_18)) {
      return assign_map[p + "_36_18"];
    };
    return assign_map["DEFAULT"];
  }
  std::string get_MODE_BITS() {
    read_mode_A = conf_code(parameters["READ_WIDTH_A"]);
    read_mode_B = conf_code(parameters["WRITE_WIDTH_B"]);
    write_mode_A = conf_code(parameters["WRITE_WIDTH_A"]);
    write_mode_B = conf_code(parameters["WRITE_WIDTH_B"]);
    std::string MODE_BITS = std::string("0") + read_mode_A + read_mode_B +
                            write_mode_A + write_mode_B + std::string(29, '0') +
                            read_mode_A + read_mode_B + write_mode_A +
                            write_mode_B + std::string(27, '0');
    return MODE_BITS;
  };

  void print(std::ostream &ofs) {
    static unsigned cnt = 0;
    cnt++;
    std::string rs_prim = "RS_TDP36K";
    ofs << ".subckt " << rs_prim << " ";
    for (auto &cn : TDP_RAM36K_to_RS_TDP36K_port_map) {
      ofs << " " << cn.first;
      if (port_connections.find(cn.second) != port_connections.end()) {

        ofs << "=" << port_connections[cn.second];
      } else {
        ofs << "=" << per_instance_sig_name(cn.second, cnt);
      }
    }
    ofs << std::endl;
    ofs << ".param MODE_BITS " << get_MODE_BITS() << std::endl;
    ofs << ".param INIT_i "
        << get_init_i1(parameters["INIT"], parameters["INIT_PARITY"])
        << std::endl;
    for (auto &p : parameters) {
      if (p.first.find("READ") == 0 || p.first.find("WRITE") == 0) {
        for (auto &assign : get_param_value_assigns(p.first)) {
          bool connected = true;
          std::vector<std::string> tokens = split_on_space(assign);
          std::string f_i_name = per_instance_sig_name(tokens[1], cnt);
          std::string s_i_name = per_instance_sig_name(tokens[2], cnt);
          if (f_i_name == tokens[1]) {
            if (port_connections.find(f_i_name) != port_connections.end()) {
              f_i_name = port_connections[f_i_name];
            } else {
              connected = false;
            }
          }
          if (s_i_name == tokens[2]) {
            if (port_connections.find(s_i_name) != port_connections.end()) {
              s_i_name = port_connections[s_i_name];
            } else {
              connected = false;
            }
          }
          if (connected) {
            ofs << tokens[0] << " " << f_i_name << " " << s_i_name << "\n1 1\n";
          }
        }
      }
    }
  }
  std::string extract_sram1(const std::string &init,
                            const std::string &init_parity) {
    if (init.size() < parameterSize["INIT"] ||
        init_parity.size() < parameterSize["INIT_PARITY"]) {
      return ""; // Not enough init size
    }
    std::string sram1(18432, '0');

    for (int i = 0; i < 2048; i += 2) {
      sram1.replace((i * 9), 16, init.substr(i * 16, 16));
      sram1.replace(((i + 2) * 9) - 2, 2, init_parity.substr(i * 2, 2));
    }

    return sram1;
  }

  std::string extract_sram2(const std::string &init,
                            const std::string &init_parity) {
    std::string sram2(18432, '0');

    for (int i = 1; i < 2048; i += 2) {
      sram2.replace((i - 1) * 9, 16, init.substr(i * 16, 16));
      sram2.replace(((i + 1) * 9) - 2, 2, init_parity.substr(i * 2, 2));
    }

    return sram2;
  }

  std::string get_init_i1(const std::string &init,
                          const std::string &init_parity) {
    return extract_sram2(init, init_parity) + extract_sram1(init, init_parity);
  }
  std::unordered_map<std::string, std::string> port_connections;
};
