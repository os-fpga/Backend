#pragma once
/**
 * @file reconstruct_ram18kx2.h
 * @author Manadher Kharroubi (manadher@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-01-
 *
 * @copyright Copyright (c) 2024
 *
 */
#include "reconstruct_utils.h"

struct TDP_RAM18KX2_instance {
  std::set<std::string> TDP_RAM18KX2_internal_signals = {
      "DATA_OUT_A1", "DATA_OUT_A2", "DATA_OUT_B1", "DATA_OUT_B2"};
  std::vector<std::string> TDP_RAM18KX2_Assigns = {
      ".names DATA_OUT_A1[15] RDATA_A1[15]",
      ".names DATA_OUT_A1[14] RDATA_A1[14]",
      ".names DATA_OUT_A1[13] RDATA_A1[13]",
      ".names DATA_OUT_A1[12] RDATA_A1[12]",
      ".names DATA_OUT_A1[11] RDATA_A1[11]",
      ".names DATA_OUT_A1[10] RDATA_A1[10]",
      ".names DATA_OUT_A1[9] RDATA_A1[9]",
      ".names DATA_OUT_A1[8] RDATA_A1[8]",
      ".names DATA_OUT_A1[7] RDATA_A1[7]",
      ".names DATA_OUT_A1[6] RDATA_A1[6]",
      ".names DATA_OUT_A1[5] RDATA_A1[5]",
      ".names DATA_OUT_A1[4] RDATA_A1[4]",
      ".names DATA_OUT_A1[3] RDATA_A1[3]",
      ".names DATA_OUT_A1[2] RDATA_A1[2]",
      ".names DATA_OUT_A1[1] RDATA_A1[1]",
      ".names DATA_OUT_A1[0] RDATA_A1[0]",
      ".names DATA_OUT_B1[15] RDATA_B1[15]",
      ".names DATA_OUT_B1[14] RDATA_B1[14]",
      ".names DATA_OUT_B1[13] RDATA_B1[13]",
      ".names DATA_OUT_B1[12] RDATA_B1[12]",
      ".names DATA_OUT_B1[11] RDATA_B1[11]",
      ".names DATA_OUT_B1[10] RDATA_B1[10]",
      ".names DATA_OUT_B1[9] RDATA_B1[9]",
      ".names DATA_OUT_B1[8] RDATA_B1[8]",
      ".names DATA_OUT_B1[7] RDATA_B1[7]",
      ".names DATA_OUT_B1[6] RDATA_B1[6]",
      ".names DATA_OUT_B1[5] RDATA_B1[5]",
      ".names DATA_OUT_B1[4] RDATA_B1[4]",
      ".names DATA_OUT_B1[3] RDATA_B1[3]",
      ".names DATA_OUT_B1[2] RDATA_B1[2]",
      ".names DATA_OUT_B1[1] RDATA_B1[1]",
      ".names DATA_OUT_B1[0] RDATA_B1[0]",
      ".names DATA_OUT_A1[17] RPARITY_A1[1]",
      ".names DATA_OUT_A1[16] RPARITY_A1[0]",
      ".names DATA_OUT_B1[17] RPARITY_B1[1]",
      ".names DATA_OUT_B1[16] RPARITY_B1[0]",
      ".names DATA_OUT_A2[15] RDATA_A2[15]",
      ".names DATA_OUT_A2[14] RDATA_A2[14]",
      ".names DATA_OUT_A2[13] RDATA_A2[13]",
      ".names DATA_OUT_A2[12] RDATA_A2[12]",
      ".names DATA_OUT_A2[11] RDATA_A2[11]",
      ".names DATA_OUT_A2[10] RDATA_A2[10]",
      ".names DATA_OUT_A2[9] RDATA_A2[9]",
      ".names DATA_OUT_A2[8] RDATA_A2[8]",
      ".names DATA_OUT_A2[7] RDATA_A2[7]",
      ".names DATA_OUT_A2[6] RDATA_A2[6]",
      ".names DATA_OUT_A2[5] RDATA_A2[5]",
      ".names DATA_OUT_A2[4] RDATA_A2[4]",
      ".names DATA_OUT_A2[3] RDATA_A2[3]",
      ".names DATA_OUT_A2[2] RDATA_A2[2]",
      ".names DATA_OUT_A2[1] RDATA_A2[1]",
      ".names DATA_OUT_A2[0] RDATA_A2[0]",
      ".names DATA_OUT_B2[15] RDATA_B2[15]",
      ".names DATA_OUT_B2[14] RDATA_B2[14]",
      ".names DATA_OUT_B2[13] RDATA_B2[13]",
      ".names DATA_OUT_B2[12] RDATA_B2[12]",
      ".names DATA_OUT_B2[11] RDATA_B2[11]",
      ".names DATA_OUT_B2[10] RDATA_B2[10]",
      ".names DATA_OUT_B2[9] RDATA_B2[9]",
      ".names DATA_OUT_B2[8] RDATA_B2[8]",
      ".names DATA_OUT_B2[7] RDATA_B2[7]",
      ".names DATA_OUT_B2[6] RDATA_B2[6]",
      ".names DATA_OUT_B2[5] RDATA_B2[5]",
      ".names DATA_OUT_B2[4] RDATA_B2[4]",
      ".names DATA_OUT_B2[3] RDATA_B2[3]",
      ".names DATA_OUT_B2[2] RDATA_B2[2]",
      ".names DATA_OUT_B2[1] RDATA_B2[1]",
      ".names DATA_OUT_B2[0] RDATA_B2[0]",
      ".names DATA_OUT_A2[17] RPARITY_A2[1]",
      ".names DATA_OUT_A2[16] RPARITY_A2[0]",
      ".names DATA_OUT_B2[17] RPARITY_B2[1]",
      ".names DATA_OUT_B2[16] RPARITY_B2[0]"};
  std::unordered_map<std::string, std::string>
      TDP_RAM18KX2_to_RS_TDP36K_port_map = {{"WEN_A1", "WEN_A1"},
                                            {"WEN_B1", "WEN_B1"},
                                            {"REN_A1", "REN_A1"},
                                            {"REN_B1", "REN_B1"},
                                            {"CLK_A1", "CLK_A1"},
                                            {"CLK_B1", "CLK_B1"},
                                            {"BE_A1[0]", "BE_A1[0]"},
                                            {"BE_A1[1]", "BE_A1[1]"},
                                            {"BE_B1[0]", "BE_B1[0]"},
                                            {"BE_B1[1]", "BE_B1[1]"},
                                            {"ADDR_A1[0]", "ADDR_A1[0]"},
                                            {"ADDR_A1[1]", "ADDR_A1[1]"},
                                            {"ADDR_A1[2]", "ADDR_A1[2]"},
                                            {"ADDR_A1[3]", "ADDR_A1[3]"},
                                            {"ADDR_A1[4]", "ADDR_A1[4]"},
                                            {"ADDR_A1[5]", "ADDR_A1[5]"},
                                            {"ADDR_A1[6]", "ADDR_A1[6]"},
                                            {"ADDR_A1[7]", "ADDR_A1[7]"},
                                            {"ADDR_A1[8]", "ADDR_A1[8]"},
                                            {"ADDR_A1[9]", "ADDR_A1[9]"},
                                            {"ADDR_A1[10]", "ADDR_A1[10]"},
                                            {"ADDR_A1[11]", "ADDR_A1[11]"},
                                            {"ADDR_A1[12]", "ADDR_A1[12]"},
                                            {"ADDR_A1[13]", "ADDR_A1[13]"},
                                            {"ADDR_B1[0]", "ADDR_B1[0]"},
                                            {"ADDR_B1[1]", "ADDR_B1[1]"},
                                            {"ADDR_B1[2]", "ADDR_B1[2]"},
                                            {"ADDR_B1[3]", "ADDR_B1[3]"},
                                            {"ADDR_B1[4]", "ADDR_B1[4]"},
                                            {"ADDR_B1[5]", "ADDR_B1[5]"},
                                            {"ADDR_B1[6]", "ADDR_B1[6]"},
                                            {"ADDR_B1[7]", "ADDR_B1[7]"},
                                            {"ADDR_B1[8]", "ADDR_B1[8]"},
                                            {"ADDR_B1[9]", "ADDR_B1[9]"},
                                            {"ADDR_B1[10]", "ADDR_B1[10]"},
                                            {"ADDR_B1[11]", "ADDR_B1[11]"},
                                            {"ADDR_B1[12]", "ADDR_B1[12]"},
                                            {"ADDR_B1[13]", "ADDR_B1[13]"},
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
                                            {"WDATA_A1[16]", "WPARITY_A1[0]"},
                                            {"WDATA_A1[17]", "WPARITY_A1[1]"},
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
                                            {"WDATA_B1[16]", "WPARITY_B1[0]"},
                                            {"WDATA_B1[17]", "WPARITY_B1[1]"},
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
                                            {"WEN_A2", "WEN_A2"},
                                            {"WEN_B2", "WEN_B2"},
                                            {"REN_A2", "REN_A2"},
                                            {"REN_B2", "REN_B2"},
                                            {"CLK_A2", "CLK_A2"},
                                            {"CLK_B2", "CLK_B2"},
                                            {"BE_A2[0]", "BE_A2[0]"},
                                            {"BE_A2[1]", "BE_A2[1]"},
                                            {"BE_B2[0]", "BE_B2[0]"},
                                            {"BE_B2[1]", "BE_B2[1]"},
                                            {"ADDR_A2[0]", "ADDR_A2[0]"},
                                            {"ADDR_A2[1]", "ADDR_A2[1]"},
                                            {"ADDR_A2[2]", "ADDR_A2[2]"},
                                            {"ADDR_A2[3]", "ADDR_A2[3]"},
                                            {"ADDR_A2[4]", "ADDR_A2[4]"},
                                            {"ADDR_A2[5]", "ADDR_A2[5]"},
                                            {"ADDR_A2[6]", "ADDR_A2[6]"},
                                            {"ADDR_A2[7]", "ADDR_A2[7]"},
                                            {"ADDR_A2[8]", "ADDR_A2[8]"},
                                            {"ADDR_A2[9]", "ADDR_A2[9]"},
                                            {"ADDR_A2[10]", "ADDR_A2[10]"},
                                            {"ADDR_A2[11]", "ADDR_A2[11]"},
                                            {"ADDR_A2[12]", "ADDR_A2[12]"},
                                            {"ADDR_A2[13]", "ADDR_A2[13]"},
                                            {"ADDR_B2[0]", "ADDR_B2[0]"},
                                            {"ADDR_B2[1]", "ADDR_B2[1]"},
                                            {"ADDR_B2[2]", "ADDR_B2[2]"},
                                            {"ADDR_B2[3]", "ADDR_B2[3]"},
                                            {"ADDR_B2[4]", "ADDR_B2[4]"},
                                            {"ADDR_B2[5]", "ADDR_B2[5]"},
                                            {"ADDR_B2[6]", "ADDR_B2[6]"},
                                            {"ADDR_B2[7]", "ADDR_B2[7]"},
                                            {"ADDR_B2[8]", "ADDR_B2[8]"},
                                            {"ADDR_B2[9]", "ADDR_B2[9]"},
                                            {"ADDR_B2[10]", "ADDR_B2[10]"},
                                            {"ADDR_B2[11]", "ADDR_B2[11]"},
                                            {"ADDR_B2[12]", "ADDR_B2[12]"},
                                            {"ADDR_B2[13]", "ADDR_B2[13]"},
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
                                            {"WDATA_A2[16]", "WPARITY_A2[0]"},
                                            {"WDATA_A2[17]", "WPARITY_A2[1]"},
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
                                            {"WDATA_B2[16]", "WPARITY_B2[0]"},
                                            {"WDATA_B2[17]", "WPARITY_B2[1]"},
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
  std::string per_instance_sig_name(std::string sn, int number) {
    std::stringstream ss;
    std::string num;
    ss << number;
    ss >> num;
    for (auto &sg : TDP_RAM18KX2_internal_signals) {
      if (sn.find(sg) == 0) {
        return sn + std::string("_") + num;
      }
    }
    return sn;
  }
  std::unordered_map<std::string, std::string> parameters = {
      {"INIT1", ""},          {"INIT1_PARITY", ""},   {"INIT2", ""},
      {"INIT2_PARITY", ""},   {"READ_WIDTH_A1", ""},  {"READ_WIDTH_A2", ""},
      {"READ_WIDTH_B1", ""},  {"READ_WIDTH_B2", ""},  {"WRITE_WIDTH_A1", ""},
      {"WRITE_WIDTH_A2", ""}, {"WRITE_WIDTH_B1", ""}, {"WRITE_WIDTH_B2", ""}};

  std::unordered_map<std::string, int> parameterSize = {
      {"INIT1", 16384},       {"INIT1_PARITY", 2048}, {"INIT2", 16384},
      {"INIT2_PARITY", 2048}, {"READ_WIDTH_A1", 32},  {"READ_WIDTH_A2", 32},
      {"READ_WIDTH_B1", 32},  {"READ_WIDTH_B2", 32},  {"WRITE_WIDTH_A1", 32},
      {"WRITE_WIDTH_A2", 32}, {"WRITE_WIDTH_B1", 32}, {"WRITE_WIDTH_B2", 32}};

  std::string str_2 = "00000000000000000000000000000010";
  std::string str_4 = "00000000000000000000000000000100";
  std::string str_9 = "00000000000000000000000000001001";
  std::string str_18 = "00000000000000000000000000010010";
  std::string str_36 = "00000000000000000000000000100100";
  std::string read_mode_A1;
  std::string read_mode_B1;
  std::string write_mode_A1;
  std::string write_mode_B1;
  std::string read_mode_A2;
  std::string read_mode_B2;
  std::string write_mode_A2;
  std::string write_mode_B2;
  std::string conf_code(std::string W) {
    return W == str_36   ? "110"
           : W == str_18 ? "010"
           : W == str_9  ? "100"
           : W == str_4  ? "001"
           : W == str_2  ? "011"
                         : "101";
  }
  std::string get_MODE_BITS() {
    read_mode_A1 = conf_code(parameters["READ_WIDTH_A1"]);
    read_mode_B1 = conf_code(parameters["WRITE_WIDTH_B1"]);
    write_mode_A1 = conf_code(parameters["WRITE_WIDTH_A1"]);
    write_mode_B1 = conf_code(parameters["WRITE_WIDTH_B1"]);
    read_mode_A2 = conf_code(parameters["READ_WIDTH_A2"]);
    read_mode_B2 = conf_code(parameters["READ_WIDTH_B2"]);
    write_mode_A2 = conf_code(parameters["WRITE_WIDTH_A2"]);
    write_mode_B2 = conf_code(parameters["WRITE_WIDTH_B2"]);
    std::string MODE_BITS =
        std::string("0") + read_mode_A1 + read_mode_B1 + write_mode_A1 +
        write_mode_B1 + std::string(29, '0') + read_mode_A2 + read_mode_B2 +
        write_mode_A2 + write_mode_B2 + std::string(26, '0') + std::string("1");
    return MODE_BITS;
  };
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

  std::string extract_sram1(const std::string &init,
                            const std::string &parity) {
    std::string sram(18432, '0');

    for (int i = 0; i < 1024; i++) {
      sram.replace(i * 18, 16, init.substr(i * 16, 16));
      sram.replace((i + 1) * 16 + (2 * i), 2, parity.substr(i * 2, 2));
    }

    return sram;
  }

  std::string extract_sram2(const std::string &init,
                            const std::string &parity) {
    std::string sram(18432, '0');

    for (int i = 0; i < 1024; i++) {
      sram.replace(i * 18, 16, init.substr(i * 16, 16));
      sram.replace((i + 1) * 16 + (2 * i), 2, parity.substr(i * 2, 2));
    }

    return sram;
  }

  std::string get_init_i1(const std::string &init1, const std::string &parity1,
                          const std::string &init2,
                          const std::string &parity2) {

    return extract_sram2(init2, parity2) + extract_sram1(init1, parity1);
  }
  void print(std::ostream &ofs) {
    static unsigned cnt = 0;
    cnt++;
    std::string rs_prim = "RS_TDP36K";
    ofs << ".subckt " << rs_prim << " ";
    for (auto &cn : TDP_RAM18KX2_to_RS_TDP36K_port_map) {
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
        << get_init_i1(parameters["INIT1"], parameters["INIT1_PARITY"],
                       parameters["INIT2"], parameters["INIT2_PARITY"])
        << std::endl;
    for (auto &assign : TDP_RAM18KX2_Assigns) {
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
  std::unordered_map<std::string, std::string> port_connections;
};
