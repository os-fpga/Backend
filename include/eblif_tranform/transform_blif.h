/**
 * @file transforme_blif.h
 * @author Manadher Kharroubi (manadher@gmail.com)
 * @brief
 * @version 0.1
 * @date 2023-09-13
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "boost/multiprecision/cpp_int.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream> // std::cout
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace boost::multiprecision;

using namespace std;

class Eblif_Transformer {
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

    std::string get_MODE_BITS() {
      return parameters["COEFF_0"] + parameters["COEFF_1"] +
             parameters["COEFF_2"] + parameters["COEFF_3"];
    }

    std::string get_block_key() {
      return parameters["accumulator"] + parameters["adder"] +
             parameters["output_reg"] + parameters["input_reg"];
    }

    std::unordered_map<std::string, std::string> port_connections;
  };
  std::vector<dsp38_instance> dsp38_instances;

public:
  std::unordered_map<std::string, std::string>
  blifToCPlusPlusMap(const std::vector<std::string> &tokens) {
    std::unordered_map<std::string, std::string> res;
    for (const std::string &token : tokens) {
      // Split each token using the '=' character
      size_t equalSignPos = token.find('=');
      if (equalSignPos != std::string::npos) {
        std::string key = token.substr(0, equalSignPos);
        std::string value = token.substr(equalSignPos + 1);

        // Add key-value pair to the result string
        res[key] = value;
      }
    }
    return res;
  }

  /**
   * @brief some useful functions
   *
   * transform(str.begin(), str.end(), str.begin(), ::toupper);
   *
   *
   */

  bool is_binary_param_(const std::string &param) {
    /* Must be non-empty */
    if (param.empty()) {
      return false;
    }

    /* The string must contain only '0' and '1' */
    for (size_t i = 0; i < param.length(); ++i) {
      if (param[i] != '0' && param[i] != '1') {
        return false;
      }
    }

    /* This is a binary word param */
    return true;
  }

  bool is_real_param_(const std::string &param) {
    /* Must be non-empty */
    if (param.empty()) {
      return false;
    }

    /* The string must match the regular expression */
    const std::regex real_number_expr(
        "[+-]?([0-9]*\\.[0-9]+)|([0-9]+\\.[0-9]*)");
    if (!std::regex_match(param, real_number_expr)) {
      return false;
    }

    /* This is a real number param */
    return true;
  }
  unsigned long long veriValue(const string &exp) {
    if (!isdigit(exp[0]))
      throw(std::invalid_argument(
          "Not a valid expression (i.e 4'h0A9 ) at line " +
          std::to_string(__LINE__) + " " + exp));
    stringstream ss(exp);
    string dd;
    char *str, *stops;
    unsigned int aa;
    int bb = 2;
    ss >> aa >> dd;
    if (aa < 1 || aa > 64)
      throw(std::invalid_argument(
          "Not supporting more than 64 bit values or less than one bit " +
          exp));
    if (dd[1] == 'b' || dd[1] == 'B')
      bb = 2;
    else if (dd[1] == 'h' || dd[1] == 'H')
      bb = 16;
    else if (dd[1] == 'd' || dd[1] == 'D')
      bb = 10;
    else
      throw(std::invalid_argument("Invalid base indicator " + string(1, dd[1]) +
                                  " in " + exp +
                                  ", should be in {d,D,b,B,h,H}"));
    str = &dd[2];
    return strtoull(str, &stops, bb);
  }

  string bitsOfHexaDecimal(string &s) {
    if (!s.size()) {
      throw(std::invalid_argument(
          "Can't generate an hexadecimal number out of an empty string"));
    }
    for (auto &d : s) {
      if (d == 'z' || d == 'Z')
        d = 'x';
      if (!isxdigit(d) && d != 'x' && d != 'X')
        throw(std::invalid_argument("Non hexadigit in hexadecimal string" + s));
    }
    string res = "";
    res.reserve(4 * s.size());
    for (auto d : s) {
      for (auto c : hexDecoder[d])
        res.push_back(c);
    }
    return res;
  }

  int1024_t bigDecimalInteger(string &s) {
    if (!s.size()) {
      throw(std::invalid_argument(
          "Can't generate a decimal number out of an empty string"));
    }
    for (auto d : s) {
      if (!isdigit(d))
        throw(std::invalid_argument("Non digit in adecimal string" + s));
    }
    if (s.size() > mx.size() || (s.size() == mx.size() && s > mx)) {
      throw(std::invalid_argument(
          "Can't generate a decimal number bigger than " + mx));
    }
    int1024_t res = 0;
    for (auto d : s) {
      res = res * 10;
      res = res + (d - '0');
    }
    return res;
  }
  string bitsOfBigDecimal(string &s) {

    string res = "";
    int1024_t v = bigDecimalInteger(s);
    int1024_t r;
    while (v) {
      r = v % 2;
      res.push_back(r ? '1' : '0');
      v = v / 2;
    }
    reverse(begin(res), end(res));
    return res;
  }
  void bits(const string &exp, std::vector<std::string> &vec_, string &strRes) {
    std::vector<std::string> vec;
    if (exp.size() < 4)
      throw(std::invalid_argument(
          "Not a valid expression, should be of size more than 3 " + exp));
    if (!isdigit(exp[0]))
      throw(std::invalid_argument(
          "Not a valid expression (i.e 4'h0A9 ) at line " +
          std::to_string(__LINE__) + " " + exp));
    stringstream ss(exp);
    string rad_and_value;

    unsigned int bit_size;
    ss >> bit_size >> rad_and_value;
    string value = rad_and_value.substr(2);
    string bit_value;

    if (rad_and_value[1] == 'b' || rad_and_value[1] == 'B') {
      bit_value = value;
      for (auto &d : bit_value) {
        if (d == 'z' || d == 'Z')
          d = 'x';
        if ('1' != d && '0' != d && 'x' != d)
          throw(std::invalid_argument("Not valid bit value " + string(1, d) +
                                      " in " + exp));
      }
    } else if (rad_and_value[1] == 'h' || rad_and_value[1] == 'H') {
      bit_value = bitsOfHexaDecimal(value);
    } else if (rad_and_value[1] == 'd' || rad_and_value[1] == 'D') {
      bit_value = bitsOfBigDecimal(value);
    } else
      throw(std::invalid_argument("Invalid base indicator " +
                                  string(1, rad_and_value[1]) + " in " + exp +
                                  ", should be in {d,D,b,B,h,H}"));
    strRes = string(bit_size, '0');
    vec = vector<string>(bit_size, "0");

    for (unsigned idx = 0; idx < bit_size && idx < bit_value.size(); ++idx) {
      strRes[bit_size - 1 - idx] = bit_value[bit_value.size() - 1 - idx];
      if ('x' == bit_value[bit_value.size() - 1 - idx])
        vec[bit_size - 1 - idx] = "$undef";
      else
        vec[bit_size - 1 - idx] =
            std::string(1, bit_value[bit_value.size() - 1 - idx]);
    }
    // expand leading x s
    unsigned pos = 0;
    while (pos < vec.size() && "0" == vec[pos])
      ++pos;
    if (pos < vec.size() && "$undef" == vec[pos]) {
      for (unsigned idx = 0; idx < pos; ++idx) {
        vec[idx] = "$undef";
        strRes[idx] = 'x';
      }
    }
    for (unsigned vIdx = 0; vIdx < vec.size(); vIdx++) {
      vec_.push_back(vec[vIdx]);
    }
  }

  std::vector<unsigned> entryTruth(unsigned long long e, unsigned long long w) {
    std::vector<unsigned> res(w, 0);
    unsigned p = 1;
    for (unsigned int i = 0; i < w; ++i) {
      int k = e & p;
      if (k)
        res[w - 1 - i] = 1;
      p *= 2;
    }
    return res;
  }

  void simpleTruthTable(std::string tr, std::string w,
                        std::vector<std::vector<unsigned>> &vec) {
    if (is_binary_param_(w))
      w = "32'b" + w;
    if (is_binary_param_(tr))
      tr = "512'b" + tr;
    unsigned long long width = veriValue(w);
    string stringRes;
    vector<string> v;
    bits(tr, v, stringRes);

    for (unsigned i = 0; i < stringRes.size(); ++i) {
      if ('1' == stringRes[stringRes.size() - 1 - i]) {
        auto ent = entryTruth(i, width);
        ent.push_back(1);
        vec.push_back(ent);
      }
    }
  }

  void vector_print(std::ostream &f, vector<unsigned> &vec) {
    for (auto n : vec) {
      f << " " << n;
    }
    f << std::endl;
  }

  struct names_str {
    std::string Y = "";
    std::vector<std::string> A;
    std::string INIT_VALUE;
    std::string W;
    std::vector<std::vector<unsigned>> truth_table;
    void print_names(std::ostream &f, Eblif_Transformer *t) {
      t->simpleTruthTable(INIT_VALUE, "32'd" + W, truth_table);
      f << ".names"
        << " ";
      for (int i = A.size() - 1; i > -1; --i) {
        f << A[i] << " ";
      }
      f << Y << std::endl;
      for (auto &v : truth_table) {
        t->vector_print(f, v);
      }
    }
  };

  std::vector<std::string> split_on_space(const std::string &line) {
    std::vector<std::string> tokens;
    std::stringstream ss(line);
    std::string token;
    while (ss >> token) {
      tokens.push_back(token);
    }
    return tokens;
  }

  void rs_transform_eblif(std::istream &ifs, std::ostream &ofs) {

    std::string line;
    while (std::getline(ifs, line)) {
      std::string ln(line);
      while ('\\' == ln.back() && std::getline(ifs, line)) {
        ln.pop_back();
        ln += " " + line;
      }
      auto tokens = split_on_space(ln);
      if (!tokens.size()) {
        ofs << line << "\n";
        continue;
      }
      if (tokens.size() < 4) {
        if (tokens.size() == 3 && tokens[0] == ".param") {
          std::string par_name = tokens[1];
          if (par_name.back() == '"')
            par_name.pop_back();
          if (par_name.size() && par_name[0] == '"') {
            for (size_t idx = 0; idx < par_name.size() - 1; ++idx)
              par_name[idx] = par_name[idx + 1];
            par_name.pop_back();
          }
          std::string par_value = tokens[2];
          if (par_value.back() == '"')
            par_value.pop_back();
          if (par_value.size() && par_value[0] == '"') {
            for (size_t idx = 0; idx < par_value.size() - 1; ++idx)
              par_value[idx] = par_value[idx + 1];
            par_value.pop_back();
          }
          if (dsp38_instances.size() &&
              dsp38_instances.back().set_param(par_name, par_value)) {
          } else {
            ofs << ln << "\n";
          }
        } else if (tokens[0] == ".end") {
          for (auto &ds : dsp38_instances) {
            std::string rs_prim = RS_DSP_Primitives.at(ds.get_block_key());
            ofs << ".subckt " << rs_prim << " ";
            for (auto &cn : ds.port_connections) {
              if (prim_io_maps_dsp_to_rs_prim[rs_prim].find(cn.first) !=
                  end(prim_io_maps_dsp_to_rs_prim[rs_prim])) {
                std::string at_prim =
                    prim_io_maps_dsp_to_rs_prim[rs_prim][cn.first];
                ofs << at_prim << "=" << cn.second << " ";
              }
            }
            ofs << std::endl;
            ofs << ".param MODE_BITS " << ds.get_MODE_BITS() << std::endl;
          }

          ofs << ln << "\n";
        } else {
          ofs << ln << "\n";
        }
        continue;
      }
      if (tokens[0] == ".subckt") {
        std::string name = tokens[1];
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.find("dff") != std::string::npos ||
            name == std::string("adder_carry") ||
            name == std::string("carry")) {
          if (name == std::string("carry")) {
            name = std::string("adder_carry");
            for(int i =2; i < tokens.size(); ++i){
              auto itr = tokens[i].find('=');
              if(std::string::npos == itr){
                continue;
              }
              std::transform(tokens[i].begin(),tokens[i].begin() + itr, tokens[i].begin(), ::tolower);
              if(tokens[i].size() > 2 && tokens[i][0] == 'o' && tokens[i][1] == '='){
                tokens[i] = std::string("sumout=") + tokens[i].substr(2);
              }
            }
          }
          tokens[1] = name;
          for (auto &t : tokens) {
            ofs << t << " ";
          }
          ofs << "\n";
        } else if (name.find("lut") == 0) {
          bool supported = true;
          names_str names;
          names.W = name.substr(3);
          bool wholeA = false;
          // test if we have the supported format
          // .subckt LUTx A[0]=i_name0 A[1]=i_name1 ...  A[x-1]=i_name<x-1>
          // Y=o_name
          if (names.W.size() != 1 || !isdigit(names.W[0])) {
            supported = false;
          } else {
            for (uint idx = 2; idx < tokens.size() - 1; ++idx) {
              std::string s = tokens[idx];
              s[2] = '-';
              if (s.find("A[-]=") == std::string::npos &&
                  s.find("A=") == std::string::npos) {
                supported = false;
                break;
              }
              if (s.find("A=") != std::string::npos) {
                wholeA = true;
              }
            }
            if (tokens.back().find("Y=") != 0) {
              supported = false;
              break;
            }
          }
          if (!supported) {
            for (auto &t : tokens) {
              ofs << t << " ";
            }
            ofs << "\n";
            continue;
          }
          // Populate names and call print_names()

          names.Y = tokens.back().substr(2);
          names.A = std::vector<std::string>(tokens.size() - 3, "$undef");
          if (wholeA) {
            names.A[0] = tokens[2].substr(2);
          } else {
            for (int idx = 2; idx < tokens.size() - 1; ++idx) {
              int pos = tokens[idx][2] - '0';
              names.A[pos] = tokens[idx].substr(5);
            }
          }
          std::string init_line;
          std::getline(ifs, init_line);
          auto init_tokens = split_on_space(init_line);
          while (!init_tokens.size()) {
            std::getline(ifs, init_line);
            init_tokens = split_on_space(init_line);
          }
          if (init_tokens.size() < 3 || init_tokens[0] != ".param" ||
              init_tokens[1] != "INIT_VALUE" ||
              !is_binary_param_(init_tokens[2])) {
            //
            for (auto &t : tokens) {
              ofs << t << " ";
            }
            ofs << "\n";
            for (auto &t : init_tokens) {
              ofs << t << " ";
            }
            ofs << "\n";
            continue;
          }
          names.INIT_VALUE = init_tokens[2];
          names.print_names(ofs, this);
        } else if (name.find("latch") == 0) {
          try {
            // Implement a latch using a lut (if possible : well written from
            // rs yosys)
            if (end(latch_lut_WIDTH_strs) ==
                latch_lut_WIDTH_strs.find(tokens[1])) {
              ofs << ln << "\n";
              continue;
            }
            names_str names;
            names.W = latch_lut_WIDTH_strs[tokens[1]];
            uint sz = names.W[0] - '0';
            names.A = std::vector<std::string>(sz, "$undef");
            for (uint i = 2; i < tokens.size(); ++i) {
              std::string s = std::string(1, tokens[i][0]);
              std::string port_conn = std::string(tokens[i]).substr(2);
              if (s == "Q")
                names.Y = port_conn; // output of the latch
              uint idx = latch_lut_port_conversion.at(tokens[1]).at(s)[2] - '0';
              int w_pos = -1;
              w_pos = idx;

              names.A.at(w_pos) = port_conn;
            }
            names.INIT_VALUE = latch_lut_LUT_strs.at(tokens[1]);
            names.print_names(ofs, this);
          } catch (...) {
            for (auto &t : tokens) {
              ofs << t << " ";
            }
            ofs << "\n";
          }
        } else if (name.find("dsp38") == 0) {
          dsp38_instances.push_back(dsp38_instance());
          dsp38_instances.back().port_connections = blifToCPlusPlusMap(tokens);
        } else {
          for (auto &t : tokens) {
            ofs << t << " ";
          }
          ofs << "\n";
        }
      } else {
        ofs << line << "\n";
      }
    }
  }

  void transform_files(std::string in_file, std::string out_file) {

    std::ifstream ifs(in_file);
    std::ofstream ofs(out_file);

    rs_transform_eblif(ifs, ofs);

    ifs.close();
    ofs.close();
  }

  void printFileContents(FILE *pf, FILE *pfout = nullptr, bool close = false) {
    if (!pf) {
      std::cerr << "Invalid file pointer!" << std::endl;
      return;
    }

    char buffer[1024];
    size_t bytesRead = 0;
    if (pfout) {
      while ((bytesRead = fread(buffer, 1, sizeof(buffer), pf)) > 0) {
        fwrite(buffer, 1, bytesRead, pfout);
      }
    } else {
      while ((bytesRead = fread(buffer, 1, sizeof(buffer), pf)) > 0) {
        fwrite(buffer, 1, bytesRead, stdout);
      }
    }

    if (ferror(pf)) {
      std::cerr << "Error reading the file!" << std::endl;
    }
    if (close)
      fclose(pf); // Close the memory stream.
  }

  inline FILE *open_and_transform_eblif(const char *filename) {
    // FILE* infile = std::fopen(filename, "r");
    // int fd = fileno(infile);
    std::ifstream fstr;
    fstr.open(filename);
    std::stringstream ss;
    ss << "";
    // Call transform function
    rs_transform_eblif(fstr, ss);
    // std::cout << ss.str() << std::endl << std::endl;
    std::string data = ss.str();
    // std::cout << data << std::endl << std::endl;
    FILE *infile = fmemopen((void *)data.c_str(), data.size(), "r");
    printFileContents(infile);
    return infile;
  }

private:
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
  std::unordered_map<std::string, std::string> RS_DSP_MULT_io_map = {
      {"a[0]", "A[0]"},
      {"a[1]", "A[1]"},
      {"a[2]", "A[2]"},
      {"a[3]", "A[3]"},
      {"a[4]", "A[4]"},
      {"a[5]", "A[5]"},
      {"a[6]", "A[6]"},
      {"a[7]", "A[7]"},
      {"a[8]", "A[8]"},
      {"a[9]", "A[9]"},
      {"a[10]", "A[10]"},
      {"a[11]", "A[11]"},
      {"a[12]", "A[12]"},
      {"a[13]", "A[13]"},
      {"a[14]", "A[14]"},
      {"a[15]", "A[15]"},
      {"a[16]", "A[16]"},
      {"a[17]", "A[17]"},
      {"a[18]", "A[18]"},
      {"a[19]", "A[19]"},
      {"b[0]", "B[0]"},
      {"b[1]", "B[1]"},
      {"b[2]", "B[2]"},
      {"b[3]", "B[3]"},
      {"b[4]", "B[4]"},
      {"b[5]", "B[5]"},
      {"b[6]", "B[6]"},
      {"b[7]", "B[7]"},
      {"b[8]", "B[8]"},
      {"b[9]", "B[9]"},
      {"b[10]", "B[10]"},
      {"b[11]", "B[11]"},
      {"b[12]", "B[12]"},
      {"b[13]", "B[13]"},
      {"b[14]", "B[14]"},
      {"b[15]", "B[15]"},
      {"b[16]", "B[16]"},
      {"b[17]", "B[17]"},
      {"z[0]", "Z[0]"},
      {"z[1]", "Z[1]"},
      {"z[2]", "Z[2]"},
      {"z[3]", "Z[3]"},
      {"z[4]", "Z[4]"},
      {"z[5]", "Z[5]"},
      {"z[6]", "Z[6]"},
      {"z[7]", "Z[7]"},
      {"z[8]", "Z[8]"},
      {"z[9]", "Z[9]"},
      {"z[10]", "Z[10]"},
      {"z[11]", "Z[11]"},
      {"z[12]", "Z[12]"},
      {"z[13]", "Z[13]"},
      {"z[14]", "Z[14]"},
      {"z[15]", "Z[15]"},
      {"z[16]", "Z[16]"},
      {"z[17]", "Z[17]"},
      {"z[18]", "Z[18]"},
      {"z[19]", "Z[19]"},
      {"z[20]", "Z[20]"},
      {"z[21]", "Z[21]"},
      {"z[22]", "Z[22]"},
      {"z[23]", "Z[23]"},
      {"z[24]", "Z[24]"},
      {"z[25]", "Z[25]"},
      {"z[26]", "Z[26]"},
      {"z[27]", "Z[27]"},
      {"z[28]", "Z[28]"},
      {"z[29]", "Z[29]"},
      {"z[30]", "Z[30]"},
      {"z[31]", "Z[31]"},
      {"z[32]", "Z[32]"},
      {"z[33]", "Z[33]"},
      {"z[34]", "Z[34]"},
      {"z[35]", "Z[35]"},
      {"z[36]", "Z[36]"},
      {"z[37]", "Z[37]"},
      {"feedback[0]", "FEEDBACK[0]"},
      {"feedback[1]", "FEEDBACK[1]"},
      {"feedback[2]", "FEEDBACK[2]"},
      {"unsigned_a", "UNSIGNED_A"},
      {"unsigned_b", "UNSIGNED_B"}};
  std::unordered_map<std::string, std::string>
      RS_DSP_MULTACC_REGIN_REGOUT_io_map = {
          {"a[0]", "A[0]"},
          {"a[1]", "A[1]"},
          {"a[2]", "A[2]"},
          {"a[3]", "A[3]"},
          {"a[4]", "A[4]"},
          {"a[5]", "A[5]"},
          {"a[6]", "A[6]"},
          {"a[7]", "A[7]"},
          {"a[8]", "A[8]"},
          {"a[9]", "A[9]"},
          {"a[10]", "A[10]"},
          {"a[11]", "A[11]"},
          {"a[12]", "A[12]"},
          {"a[13]", "A[13]"},
          {"a[14]", "A[14]"},
          {"a[15]", "A[15]"},
          {"a[16]", "A[16]"},
          {"a[17]", "A[17]"},
          {"a[18]", "A[18]"},
          {"a[19]", "A[19]"},
          {"b[0]", "B[0]"},
          {"b[1]", "B[1]"},
          {"b[2]", "B[2]"},
          {"b[3]", "B[3]"},
          {"b[4]", "B[4]"},
          {"b[5]", "B[5]"},
          {"b[6]", "B[6]"},
          {"b[7]", "B[7]"},
          {"b[8]", "B[8]"},
          {"b[9]", "B[9]"},
          {"b[10]", "B[10]"},
          {"b[11]", "B[11]"},
          {"b[12]", "B[12]"},
          {"b[13]", "B[13]"},
          {"b[14]", "B[14]"},
          {"b[15]", "B[15]"},
          {"b[16]", "B[16]"},
          {"b[17]", "B[17]"},
          {"z[0]", "Z[0]"},
          {"z[1]", "Z[1]"},
          {"z[2]", "Z[2]"},
          {"z[3]", "Z[3]"},
          {"z[4]", "Z[4]"},
          {"z[5]", "Z[5]"},
          {"z[6]", "Z[6]"},
          {"z[7]", "Z[7]"},
          {"z[8]", "Z[8]"},
          {"z[9]", "Z[9]"},
          {"z[10]", "Z[10]"},
          {"z[11]", "Z[11]"},
          {"z[12]", "Z[12]"},
          {"z[13]", "Z[13]"},
          {"z[14]", "Z[14]"},
          {"z[15]", "Z[15]"},
          {"z[16]", "Z[16]"},
          {"z[17]", "Z[17]"},
          {"z[18]", "Z[18]"},
          {"z[19]", "Z[19]"},
          {"z[20]", "Z[20]"},
          {"z[21]", "Z[21]"},
          {"z[22]", "Z[22]"},
          {"z[23]", "Z[23]"},
          {"z[24]", "Z[24]"},
          {"z[25]", "Z[25]"},
          {"z[26]", "Z[26]"},
          {"z[27]", "Z[27]"},
          {"z[28]", "Z[28]"},
          {"z[29]", "Z[29]"},
          {"z[30]", "Z[30]"},
          {"z[31]", "Z[31]"},
          {"z[32]", "Z[32]"},
          {"z[33]", "Z[33]"},
          {"z[34]", "Z[34]"},
          {"z[35]", "Z[35]"},
          {"z[36]", "Z[36]"},
          {"z[37]", "Z[37]"},
          {"feedback[0]", "FEEDBACK[0]"},
          {"feedback[1]", "FEEDBACK[1]"},
          {"feedback[2]", "FEEDBACK[2]"},
          {"unsigned_a", "UNSIGNED_A"},
          {"unsigned_b", "UNSIGNED_B"},
          {"clk", "CLK"},
          {"lreset", "RESET"},
          {"load_acc", "LOAD_ACC"},
          {"saturate_enable", "SATURATE"},
          {"shift_right[0]", "SHIFT_RIGHT[0]"},
          {"shift_right[1]", "SHIFT_RIGHT[1]"},
          {"shift_right[2]", "SHIFT_RIGHT[2]"},
          {"shift_right[3]", "SHIFT_RIGHT[3]"},
          {"shift_right[4]", "SHIFT_RIGHT[4]"},
          {"shift_right[5]", "SHIFT_RIGHT[5]"},
          {"round", "ROUND"},
          {"subtract", "SUBTRACT"}};
  std::unordered_map<std::string, std::string> RS_DSP_MULTACC_REGOUT_io_map = {
      {"a[0]", "A[0]"},
      {"a[1]", "A[1]"},
      {"a[2]", "A[2]"},
      {"a[3]", "A[3]"},
      {"a[4]", "A[4]"},
      {"a[5]", "A[5]"},
      {"a[6]", "A[6]"},
      {"a[7]", "A[7]"},
      {"a[8]", "A[8]"},
      {"a[9]", "A[9]"},
      {"a[10]", "A[10]"},
      {"a[11]", "A[11]"},
      {"a[12]", "A[12]"},
      {"a[13]", "A[13]"},
      {"a[14]", "A[14]"},
      {"a[15]", "A[15]"},
      {"a[16]", "A[16]"},
      {"a[17]", "A[17]"},
      {"a[18]", "A[18]"},
      {"a[19]", "A[19]"},
      {"b[0]", "B[0]"},
      {"b[1]", "B[1]"},
      {"b[2]", "B[2]"},
      {"b[3]", "B[3]"},
      {"b[4]", "B[4]"},
      {"b[5]", "B[5]"},
      {"b[6]", "B[6]"},
      {"b[7]", "B[7]"},
      {"b[8]", "B[8]"},
      {"b[9]", "B[9]"},
      {"b[10]", "B[10]"},
      {"b[11]", "B[11]"},
      {"b[12]", "B[12]"},
      {"b[13]", "B[13]"},
      {"b[14]", "B[14]"},
      {"b[15]", "B[15]"},
      {"b[16]", "B[16]"},
      {"b[17]", "B[17]"},
      {"z[0]", "Z[0]"},
      {"z[1]", "Z[1]"},
      {"z[2]", "Z[2]"},
      {"z[3]", "Z[3]"},
      {"z[4]", "Z[4]"},
      {"z[5]", "Z[5]"},
      {"z[6]", "Z[6]"},
      {"z[7]", "Z[7]"},
      {"z[8]", "Z[8]"},
      {"z[9]", "Z[9]"},
      {"z[10]", "Z[10]"},
      {"z[11]", "Z[11]"},
      {"z[12]", "Z[12]"},
      {"z[13]", "Z[13]"},
      {"z[14]", "Z[14]"},
      {"z[15]", "Z[15]"},
      {"z[16]", "Z[16]"},
      {"z[17]", "Z[17]"},
      {"z[18]", "Z[18]"},
      {"z[19]", "Z[19]"},
      {"z[20]", "Z[20]"},
      {"z[21]", "Z[21]"},
      {"z[22]", "Z[22]"},
      {"z[23]", "Z[23]"},
      {"z[24]", "Z[24]"},
      {"z[25]", "Z[25]"},
      {"z[26]", "Z[26]"},
      {"z[27]", "Z[27]"},
      {"z[28]", "Z[28]"},
      {"z[29]", "Z[29]"},
      {"z[30]", "Z[30]"},
      {"z[31]", "Z[31]"},
      {"z[32]", "Z[32]"},
      {"z[33]", "Z[33]"},
      {"z[34]", "Z[34]"},
      {"z[35]", "Z[35]"},
      {"z[36]", "Z[36]"},
      {"z[37]", "Z[37]"},
      {"feedback[0]", "FEEDBACK[0]"},
      {"feedback[1]", "FEEDBACK[1]"},
      {"feedback[2]", "FEEDBACK[2]"},
      {"unsigned_a", "UNSIGNED_A"},
      {"unsigned_b", "UNSIGNED_B"},
      {"clk", "CLK"},
      {"lreset", "RESET"},
      {"load_acc", "LOAD_ACC"},
      {"saturate_enable", "SATURATE"},
      {"shift_right[0]", "SHIFT_RIGHT[0]"},
      {"shift_right[1]", "SHIFT_RIGHT[1]"},
      {"shift_right[2]", "SHIFT_RIGHT[2]"},
      {"shift_right[3]", "SHIFT_RIGHT[3]"},
      {"shift_right[4]", "SHIFT_RIGHT[4]"},
      {"shift_right[5]", "SHIFT_RIGHT[5]"},
      {"round", "ROUND"},
      {"subtract", "SUBTRACT"}};
  std::unordered_map<std::string, std::string> RS_DSP_MULTACC_REGIN_io_map = {
      {"a[0]", "A[0]"},
      {"a[1]", "A[1]"},
      {"a[2]", "A[2]"},
      {"a[3]", "A[3]"},
      {"a[4]", "A[4]"},
      {"a[5]", "A[5]"},
      {"a[6]", "A[6]"},
      {"a[7]", "A[7]"},
      {"a[8]", "A[8]"},
      {"a[9]", "A[9]"},
      {"a[10]", "A[10]"},
      {"a[11]", "A[11]"},
      {"a[12]", "A[12]"},
      {"a[13]", "A[13]"},
      {"a[14]", "A[14]"},
      {"a[15]", "A[15]"},
      {"a[16]", "A[16]"},
      {"a[17]", "A[17]"},
      {"a[18]", "A[18]"},
      {"a[19]", "A[19]"},
      {"b[0]", "B[0]"},
      {"b[1]", "B[1]"},
      {"b[2]", "B[2]"},
      {"b[3]", "B[3]"},
      {"b[4]", "B[4]"},
      {"b[5]", "B[5]"},
      {"b[6]", "B[6]"},
      {"b[7]", "B[7]"},
      {"b[8]", "B[8]"},
      {"b[9]", "B[9]"},
      {"b[10]", "B[10]"},
      {"b[11]", "B[11]"},
      {"b[12]", "B[12]"},
      {"b[13]", "B[13]"},
      {"b[14]", "B[14]"},
      {"b[15]", "B[15]"},
      {"b[16]", "B[16]"},
      {"b[17]", "B[17]"},
      {"z[0]", "Z[0]"},
      {"z[1]", "Z[1]"},
      {"z[2]", "Z[2]"},
      {"z[3]", "Z[3]"},
      {"z[4]", "Z[4]"},
      {"z[5]", "Z[5]"},
      {"z[6]", "Z[6]"},
      {"z[7]", "Z[7]"},
      {"z[8]", "Z[8]"},
      {"z[9]", "Z[9]"},
      {"z[10]", "Z[10]"},
      {"z[11]", "Z[11]"},
      {"z[12]", "Z[12]"},
      {"z[13]", "Z[13]"},
      {"z[14]", "Z[14]"},
      {"z[15]", "Z[15]"},
      {"z[16]", "Z[16]"},
      {"z[17]", "Z[17]"},
      {"z[18]", "Z[18]"},
      {"z[19]", "Z[19]"},
      {"z[20]", "Z[20]"},
      {"z[21]", "Z[21]"},
      {"z[22]", "Z[22]"},
      {"z[23]", "Z[23]"},
      {"z[24]", "Z[24]"},
      {"z[25]", "Z[25]"},
      {"z[26]", "Z[26]"},
      {"z[27]", "Z[27]"},
      {"z[28]", "Z[28]"},
      {"z[29]", "Z[29]"},
      {"z[30]", "Z[30]"},
      {"z[31]", "Z[31]"},
      {"z[32]", "Z[32]"},
      {"z[33]", "Z[33]"},
      {"z[34]", "Z[34]"},
      {"z[35]", "Z[35]"},
      {"z[36]", "Z[36]"},
      {"z[37]", "Z[37]"},
      {"feedback[0]", "FEEDBACK[0]"},
      {"feedback[1]", "FEEDBACK[1]"},
      {"feedback[2]", "FEEDBACK[2]"},
      {"unsigned_a", "UNSIGNED_A"},
      {"unsigned_b", "UNSIGNED_B"},
      {"clk", "CLK"},
      {"lreset", "RESET"},
      {"load_acc", "LOAD_ACC"},
      {"saturate_enable", "SATURATE"},
      {"shift_right[0]", "SHIFT_RIGHT[0]"},
      {"shift_right[1]", "SHIFT_RIGHT[1]"},
      {"shift_right[2]", "SHIFT_RIGHT[2]"},
      {"shift_right[3]", "SHIFT_RIGHT[3]"},
      {"shift_right[4]", "SHIFT_RIGHT[4]"},
      {"shift_right[5]", "SHIFT_RIGHT[5]"},
      {"round", "ROUND"},
      {"subtract", "SUBTRACT"}};
  std::unordered_map<std::string, std::string> RS_DSP_MULTACC_io_map = {
      {"a[0]", "A[0]"},
      {"a[1]", "A[1]"},
      {"a[2]", "A[2]"},
      {"a[3]", "A[3]"},
      {"a[4]", "A[4]"},
      {"a[5]", "A[5]"},
      {"a[6]", "A[6]"},
      {"a[7]", "A[7]"},
      {"a[8]", "A[8]"},
      {"a[9]", "A[9]"},
      {"a[10]", "A[10]"},
      {"a[11]", "A[11]"},
      {"a[12]", "A[12]"},
      {"a[13]", "A[13]"},
      {"a[14]", "A[14]"},
      {"a[15]", "A[15]"},
      {"a[16]", "A[16]"},
      {"a[17]", "A[17]"},
      {"a[18]", "A[18]"},
      {"a[19]", "A[19]"},
      {"b[0]", "B[0]"},
      {"b[1]", "B[1]"},
      {"b[2]", "B[2]"},
      {"b[3]", "B[3]"},
      {"b[4]", "B[4]"},
      {"b[5]", "B[5]"},
      {"b[6]", "B[6]"},
      {"b[7]", "B[7]"},
      {"b[8]", "B[8]"},
      {"b[9]", "B[9]"},
      {"b[10]", "B[10]"},
      {"b[11]", "B[11]"},
      {"b[12]", "B[12]"},
      {"b[13]", "B[13]"},
      {"b[14]", "B[14]"},
      {"b[15]", "B[15]"},
      {"b[16]", "B[16]"},
      {"b[17]", "B[17]"},
      {"z[0]", "Z[0]"},
      {"z[1]", "Z[1]"},
      {"z[2]", "Z[2]"},
      {"z[3]", "Z[3]"},
      {"z[4]", "Z[4]"},
      {"z[5]", "Z[5]"},
      {"z[6]", "Z[6]"},
      {"z[7]", "Z[7]"},
      {"z[8]", "Z[8]"},
      {"z[9]", "Z[9]"},
      {"z[10]", "Z[10]"},
      {"z[11]", "Z[11]"},
      {"z[12]", "Z[12]"},
      {"z[13]", "Z[13]"},
      {"z[14]", "Z[14]"},
      {"z[15]", "Z[15]"},
      {"z[16]", "Z[16]"},
      {"z[17]", "Z[17]"},
      {"z[18]", "Z[18]"},
      {"z[19]", "Z[19]"},
      {"z[20]", "Z[20]"},
      {"z[21]", "Z[21]"},
      {"z[22]", "Z[22]"},
      {"z[23]", "Z[23]"},
      {"z[24]", "Z[24]"},
      {"z[25]", "Z[25]"},
      {"z[26]", "Z[26]"},
      {"z[27]", "Z[27]"},
      {"z[28]", "Z[28]"},
      {"z[29]", "Z[29]"},
      {"z[30]", "Z[30]"},
      {"z[31]", "Z[31]"},
      {"z[32]", "Z[32]"},
      {"z[33]", "Z[33]"},
      {"z[34]", "Z[34]"},
      {"z[35]", "Z[35]"},
      {"z[36]", "Z[36]"},
      {"z[37]", "Z[37]"},
      {"feedback[0]", "FEEDBACK[0]"},
      {"feedback[1]", "FEEDBACK[1]"},
      {"feedback[2]", "FEEDBACK[2]"},
      {"unsigned_a", "UNSIGNED_A"},
      {"unsigned_b", "UNSIGNED_B"},
      {"clk", "CLK"},
      {"lreset", "RESET"},
      {"load_acc", "LOAD_ACC"},
      {"saturate_enable", "SATURATE"},
      {"shift_right[0]", "SHIFT_RIGHT[0]"},
      {"shift_right[1]", "SHIFT_RIGHT[1]"},
      {"shift_right[2]", "SHIFT_RIGHT[2]"},
      {"shift_right[3]", "SHIFT_RIGHT[3]"},
      {"shift_right[4]", "SHIFT_RIGHT[4]"},
      {"shift_right[5]", "SHIFT_RIGHT[5]"},
      {"round", "ROUND"},
      {"subtract", "SUBTRACT"}};
  std::unordered_map<std::string, std::string>
      RS_DSP_MULTADD_REGIN_REGOUT_io_map = {
          {"a[0]", "A[0]"},
          {"a[1]", "A[1]"},
          {"a[2]", "A[2]"},
          {"a[3]", "A[3]"},
          {"a[4]", "A[4]"},
          {"a[5]", "A[5]"},
          {"a[6]", "A[6]"},
          {"a[7]", "A[7]"},
          {"a[8]", "A[8]"},
          {"a[9]", "A[9]"},
          {"a[10]", "A[10]"},
          {"a[11]", "A[11]"},
          {"a[12]", "A[12]"},
          {"a[13]", "A[13]"},
          {"a[14]", "A[14]"},
          {"a[15]", "A[15]"},
          {"a[16]", "A[16]"},
          {"a[17]", "A[17]"},
          {"a[18]", "A[18]"},
          {"a[19]", "A[19]"},
          {"b[0]", "B[0]"},
          {"b[1]", "B[1]"},
          {"b[2]", "B[2]"},
          {"b[3]", "B[3]"},
          {"b[4]", "B[4]"},
          {"b[5]", "B[5]"},
          {"b[6]", "B[6]"},
          {"b[7]", "B[7]"},
          {"b[8]", "B[8]"},
          {"b[9]", "B[9]"},
          {"b[10]", "B[10]"},
          {"b[11]", "B[11]"},
          {"b[12]", "B[12]"},
          {"b[13]", "B[13]"},
          {"b[14]", "B[14]"},
          {"b[15]", "B[15]"},
          {"b[16]", "B[16]"},
          {"b[17]", "B[17]"},
          {"z[0]", "Z[0]"},
          {"z[1]", "Z[1]"},
          {"z[2]", "Z[2]"},
          {"z[3]", "Z[3]"},
          {"z[4]", "Z[4]"},
          {"z[5]", "Z[5]"},
          {"z[6]", "Z[6]"},
          {"z[7]", "Z[7]"},
          {"z[8]", "Z[8]"},
          {"z[9]", "Z[9]"},
          {"z[10]", "Z[10]"},
          {"z[11]", "Z[11]"},
          {"z[12]", "Z[12]"},
          {"z[13]", "Z[13]"},
          {"z[14]", "Z[14]"},
          {"z[15]", "Z[15]"},
          {"z[16]", "Z[16]"},
          {"z[17]", "Z[17]"},
          {"z[18]", "Z[18]"},
          {"z[19]", "Z[19]"},
          {"z[20]", "Z[20]"},
          {"z[21]", "Z[21]"},
          {"z[22]", "Z[22]"},
          {"z[23]", "Z[23]"},
          {"z[24]", "Z[24]"},
          {"z[25]", "Z[25]"},
          {"z[26]", "Z[26]"},
          {"z[27]", "Z[27]"},
          {"z[28]", "Z[28]"},
          {"z[29]", "Z[29]"},
          {"z[30]", "Z[30]"},
          {"z[31]", "Z[31]"},
          {"z[32]", "Z[32]"},
          {"z[33]", "Z[33]"},
          {"z[34]", "Z[34]"},
          {"z[35]", "Z[35]"},
          {"z[36]", "Z[36]"},
          {"z[37]", "Z[37]"},
          {"dly_b[0]", "DLY_B[0]"},
          {"dly_b[1]", "DLY_B[1]"},
          {"dly_b[2]", "DLY_B[2]"},
          {"dly_b[3]", "DLY_B[3]"},
          {"dly_b[4]", "DLY_B[4]"},
          {"dly_b[5]", "DLY_B[5]"},
          {"dly_b[6]", "DLY_B[6]"},
          {"dly_b[7]", "DLY_B[7]"},
          {"dly_b[8]", "DLY_B[8]"},
          {"dly_b[9]", "DLY_B[9]"},
          {"dly_b[10]", "DLY_B[10]"},
          {"dly_b[11]", "DLY_B[11]"},
          {"dly_b[12]", "DLY_B[12]"},
          {"dly_b[13]", "DLY_B[13]"},
          {"dly_b[14]", "DLY_B[14]"},
          {"dly_b[15]", "DLY_B[15]"},
          {"dly_b[16]", "DLY_B[16]"},
          {"dly_b[17]", "DLY_B[17]"},
          {"feedback[0]", "FEEDBACK[0]"},
          {"feedback[1]", "FEEDBACK[1]"},
          {"feedback[2]", "FEEDBACK[2]"},
          {"unsigned_a", "UNSIGNED_A"},
          {"unsigned_b", "UNSIGNED_B"},
          {"clk", "CLK"},
          {"lreset", "RESET"},
          {"acc_fir[0]", "ACC_FIR[0]"},
          {"acc_fir[1]", "ACC_FIR[1]"},
          {"acc_fir[2]", "ACC_FIR[2]"},
          {"acc_fir[3]", "ACC_FIR[3]"},
          {"acc_fir[4]", "ACC_FIR[4]"},
          {"acc_fir[5]", "ACC_FIR[5]"},
          {"load_acc", "LOAD_ACC"},
          {"saturate_enable", "SATURATE"},
          {"shift_right[0]", "SHIFT_RIGHT[0]"},
          {"shift_right[1]", "SHIFT_RIGHT[1]"},
          {"shift_right[2]", "SHIFT_RIGHT[2]"},
          {"shift_right[3]", "SHIFT_RIGHT[3]"},
          {"shift_right[4]", "SHIFT_RIGHT[4]"},
          {"shift_right[5]", "SHIFT_RIGHT[5]"},
          {"round", "ROUND"},
          {"subtract", "SUBTRACT"}};
  std::unordered_map<std::string, std::string> RS_DSP_MULTADD_REGOUT_io_map = {
      {"a[0]", "A[0]"},
      {"a[1]", "A[1]"},
      {"a[2]", "A[2]"},
      {"a[3]", "A[3]"},
      {"a[4]", "A[4]"},
      {"a[5]", "A[5]"},
      {"a[6]", "A[6]"},
      {"a[7]", "A[7]"},
      {"a[8]", "A[8]"},
      {"a[9]", "A[9]"},
      {"a[10]", "A[10]"},
      {"a[11]", "A[11]"},
      {"a[12]", "A[12]"},
      {"a[13]", "A[13]"},
      {"a[14]", "A[14]"},
      {"a[15]", "A[15]"},
      {"a[16]", "A[16]"},
      {"a[17]", "A[17]"},
      {"a[18]", "A[18]"},
      {"a[19]", "A[19]"},
      {"b[0]", "B[0]"},
      {"b[1]", "B[1]"},
      {"b[2]", "B[2]"},
      {"b[3]", "B[3]"},
      {"b[4]", "B[4]"},
      {"b[5]", "B[5]"},
      {"b[6]", "B[6]"},
      {"b[7]", "B[7]"},
      {"b[8]", "B[8]"},
      {"b[9]", "B[9]"},
      {"b[10]", "B[10]"},
      {"b[11]", "B[11]"},
      {"b[12]", "B[12]"},
      {"b[13]", "B[13]"},
      {"b[14]", "B[14]"},
      {"b[15]", "B[15]"},
      {"b[16]", "B[16]"},
      {"b[17]", "B[17]"},
      {"z[0]", "Z[0]"},
      {"z[1]", "Z[1]"},
      {"z[2]", "Z[2]"},
      {"z[3]", "Z[3]"},
      {"z[4]", "Z[4]"},
      {"z[5]", "Z[5]"},
      {"z[6]", "Z[6]"},
      {"z[7]", "Z[7]"},
      {"z[8]", "Z[8]"},
      {"z[9]", "Z[9]"},
      {"z[10]", "Z[10]"},
      {"z[11]", "Z[11]"},
      {"z[12]", "Z[12]"},
      {"z[13]", "Z[13]"},
      {"z[14]", "Z[14]"},
      {"z[15]", "Z[15]"},
      {"z[16]", "Z[16]"},
      {"z[17]", "Z[17]"},
      {"z[18]", "Z[18]"},
      {"z[19]", "Z[19]"},
      {"z[20]", "Z[20]"},
      {"z[21]", "Z[21]"},
      {"z[22]", "Z[22]"},
      {"z[23]", "Z[23]"},
      {"z[24]", "Z[24]"},
      {"z[25]", "Z[25]"},
      {"z[26]", "Z[26]"},
      {"z[27]", "Z[27]"},
      {"z[28]", "Z[28]"},
      {"z[29]", "Z[29]"},
      {"z[30]", "Z[30]"},
      {"z[31]", "Z[31]"},
      {"z[32]", "Z[32]"},
      {"z[33]", "Z[33]"},
      {"z[34]", "Z[34]"},
      {"z[35]", "Z[35]"},
      {"z[36]", "Z[36]"},
      {"z[37]", "Z[37]"},
      {"dly_b[0]", "DLY_B[0]"},
      {"dly_b[1]", "DLY_B[1]"},
      {"dly_b[2]", "DLY_B[2]"},
      {"dly_b[3]", "DLY_B[3]"},
      {"dly_b[4]", "DLY_B[4]"},
      {"dly_b[5]", "DLY_B[5]"},
      {"dly_b[6]", "DLY_B[6]"},
      {"dly_b[7]", "DLY_B[7]"},
      {"dly_b[8]", "DLY_B[8]"},
      {"dly_b[9]", "DLY_B[9]"},
      {"dly_b[10]", "DLY_B[10]"},
      {"dly_b[11]", "DLY_B[11]"},
      {"dly_b[12]", "DLY_B[12]"},
      {"dly_b[13]", "DLY_B[13]"},
      {"dly_b[14]", "DLY_B[14]"},
      {"dly_b[15]", "DLY_B[15]"},
      {"dly_b[16]", "DLY_B[16]"},
      {"dly_b[17]", "DLY_B[17]"},
      {"feedback[0]", "FEEDBACK[0]"},
      {"feedback[1]", "FEEDBACK[1]"},
      {"feedback[2]", "FEEDBACK[2]"},
      {"unsigned_a", "UNSIGNED_A"},
      {"unsigned_b", "UNSIGNED_B"},
      {"clk", "CLK"},
      {"lreset", "RESET"},
      {"acc_fir[0]", "ACC_FIR[0]"},
      {"acc_fir[1]", "ACC_FIR[1]"},
      {"acc_fir[2]", "ACC_FIR[2]"},
      {"acc_fir[3]", "ACC_FIR[3]"},
      {"acc_fir[4]", "ACC_FIR[4]"},
      {"acc_fir[5]", "ACC_FIR[5]"},
      {"load_acc", "LOAD_ACC"},
      {"saturate_enable", "SATURATE"},
      {"shift_right[0]", "SHIFT_RIGHT[0]"},
      {"shift_right[1]", "SHIFT_RIGHT[1]"},
      {"shift_right[2]", "SHIFT_RIGHT[2]"},
      {"shift_right[3]", "SHIFT_RIGHT[3]"},
      {"shift_right[4]", "SHIFT_RIGHT[4]"},
      {"shift_right[5]", "SHIFT_RIGHT[5]"},
      {"round", "ROUND"},
      {"subtract", "SUBTRACT"}};
  std::unordered_map<std::string, std::string> RS_DSP_MULTADD_REGIN_io_map = {
      {"a[0]", "A[0]"},
      {"a[1]", "A[1]"},
      {"a[2]", "A[2]"},
      {"a[3]", "A[3]"},
      {"a[4]", "A[4]"},
      {"a[5]", "A[5]"},
      {"a[6]", "A[6]"},
      {"a[7]", "A[7]"},
      {"a[8]", "A[8]"},
      {"a[9]", "A[9]"},
      {"a[10]", "A[10]"},
      {"a[11]", "A[11]"},
      {"a[12]", "A[12]"},
      {"a[13]", "A[13]"},
      {"a[14]", "A[14]"},
      {"a[15]", "A[15]"},
      {"a[16]", "A[16]"},
      {"a[17]", "A[17]"},
      {"a[18]", "A[18]"},
      {"a[19]", "A[19]"},
      {"b[0]", "B[0]"},
      {"b[1]", "B[1]"},
      {"b[2]", "B[2]"},
      {"b[3]", "B[3]"},
      {"b[4]", "B[4]"},
      {"b[5]", "B[5]"},
      {"b[6]", "B[6]"},
      {"b[7]", "B[7]"},
      {"b[8]", "B[8]"},
      {"b[9]", "B[9]"},
      {"b[10]", "B[10]"},
      {"b[11]", "B[11]"},
      {"b[12]", "B[12]"},
      {"b[13]", "B[13]"},
      {"b[14]", "B[14]"},
      {"b[15]", "B[15]"},
      {"b[16]", "B[16]"},
      {"b[17]", "B[17]"},
      {"z[0]", "Z[0]"},
      {"z[1]", "Z[1]"},
      {"z[2]", "Z[2]"},
      {"z[3]", "Z[3]"},
      {"z[4]", "Z[4]"},
      {"z[5]", "Z[5]"},
      {"z[6]", "Z[6]"},
      {"z[7]", "Z[7]"},
      {"z[8]", "Z[8]"},
      {"z[9]", "Z[9]"},
      {"z[10]", "Z[10]"},
      {"z[11]", "Z[11]"},
      {"z[12]", "Z[12]"},
      {"z[13]", "Z[13]"},
      {"z[14]", "Z[14]"},
      {"z[15]", "Z[15]"},
      {"z[16]", "Z[16]"},
      {"z[17]", "Z[17]"},
      {"z[18]", "Z[18]"},
      {"z[19]", "Z[19]"},
      {"z[20]", "Z[20]"},
      {"z[21]", "Z[21]"},
      {"z[22]", "Z[22]"},
      {"z[23]", "Z[23]"},
      {"z[24]", "Z[24]"},
      {"z[25]", "Z[25]"},
      {"z[26]", "Z[26]"},
      {"z[27]", "Z[27]"},
      {"z[28]", "Z[28]"},
      {"z[29]", "Z[29]"},
      {"z[30]", "Z[30]"},
      {"z[31]", "Z[31]"},
      {"z[32]", "Z[32]"},
      {"z[33]", "Z[33]"},
      {"z[34]", "Z[34]"},
      {"z[35]", "Z[35]"},
      {"z[36]", "Z[36]"},
      {"z[37]", "Z[37]"},
      {"dly_b[0]", "DLY_B[0]"},
      {"dly_b[1]", "DLY_B[1]"},
      {"dly_b[2]", "DLY_B[2]"},
      {"dly_b[3]", "DLY_B[3]"},
      {"dly_b[4]", "DLY_B[4]"},
      {"dly_b[5]", "DLY_B[5]"},
      {"dly_b[6]", "DLY_B[6]"},
      {"dly_b[7]", "DLY_B[7]"},
      {"dly_b[8]", "DLY_B[8]"},
      {"dly_b[9]", "DLY_B[9]"},
      {"dly_b[10]", "DLY_B[10]"},
      {"dly_b[11]", "DLY_B[11]"},
      {"dly_b[12]", "DLY_B[12]"},
      {"dly_b[13]", "DLY_B[13]"},
      {"dly_b[14]", "DLY_B[14]"},
      {"dly_b[15]", "DLY_B[15]"},
      {"dly_b[16]", "DLY_B[16]"},
      {"dly_b[17]", "DLY_B[17]"},
      {"feedback[0]", "FEEDBACK[0]"},
      {"feedback[1]", "FEEDBACK[1]"},
      {"feedback[2]", "FEEDBACK[2]"},
      {"unsigned_a", "UNSIGNED_A"},
      {"unsigned_b", "UNSIGNED_B"},
      {"clk", "CLK"},
      {"lreset", "RESET"},
      {"acc_fir[0]", "ACC_FIR[0]"},
      {"acc_fir[1]", "ACC_FIR[1]"},
      {"acc_fir[2]", "ACC_FIR[2]"},
      {"acc_fir[3]", "ACC_FIR[3]"},
      {"acc_fir[4]", "ACC_FIR[4]"},
      {"acc_fir[5]", "ACC_FIR[5]"},
      {"load_acc", "LOAD_ACC"},
      {"saturate_enable", "SATURATE"},
      {"shift_right[0]", "SHIFT_RIGHT[0]"},
      {"shift_right[1]", "SHIFT_RIGHT[1]"},
      {"shift_right[2]", "SHIFT_RIGHT[2]"},
      {"shift_right[3]", "SHIFT_RIGHT[3]"},
      {"shift_right[4]", "SHIFT_RIGHT[4]"},
      {"shift_right[5]", "SHIFT_RIGHT[5]"},
      {"round", "ROUND"},
      {"subtract", "SUBTRACT"}};
  std::unordered_map<std::string, std::string> RS_DSP_MULTADD_io_map = {
      {"a[0]", "A[0]"},
      {"a[1]", "A[1]"},
      {"a[2]", "A[2]"},
      {"a[3]", "A[3]"},
      {"a[4]", "A[4]"},
      {"a[5]", "A[5]"},
      {"a[6]", "A[6]"},
      {"a[7]", "A[7]"},
      {"a[8]", "A[8]"},
      {"a[9]", "A[9]"},
      {"a[10]", "A[10]"},
      {"a[11]", "A[11]"},
      {"a[12]", "A[12]"},
      {"a[13]", "A[13]"},
      {"a[14]", "A[14]"},
      {"a[15]", "A[15]"},
      {"a[16]", "A[16]"},
      {"a[17]", "A[17]"},
      {"a[18]", "A[18]"},
      {"a[19]", "A[19]"},
      {"b[0]", "B[0]"},
      {"b[1]", "B[1]"},
      {"b[2]", "B[2]"},
      {"b[3]", "B[3]"},
      {"b[4]", "B[4]"},
      {"b[5]", "B[5]"},
      {"b[6]", "B[6]"},
      {"b[7]", "B[7]"},
      {"b[8]", "B[8]"},
      {"b[9]", "B[9]"},
      {"b[10]", "B[10]"},
      {"b[11]", "B[11]"},
      {"b[12]", "B[12]"},
      {"b[13]", "B[13]"},
      {"b[14]", "B[14]"},
      {"b[15]", "B[15]"},
      {"b[16]", "B[16]"},
      {"b[17]", "B[17]"},
      {"z[0]", "Z[0]"},
      {"z[1]", "Z[1]"},
      {"z[2]", "Z[2]"},
      {"z[3]", "Z[3]"},
      {"z[4]", "Z[4]"},
      {"z[5]", "Z[5]"},
      {"z[6]", "Z[6]"},
      {"z[7]", "Z[7]"},
      {"z[8]", "Z[8]"},
      {"z[9]", "Z[9]"},
      {"z[10]", "Z[10]"},
      {"z[11]", "Z[11]"},
      {"z[12]", "Z[12]"},
      {"z[13]", "Z[13]"},
      {"z[14]", "Z[14]"},
      {"z[15]", "Z[15]"},
      {"z[16]", "Z[16]"},
      {"z[17]", "Z[17]"},
      {"z[18]", "Z[18]"},
      {"z[19]", "Z[19]"},
      {"z[20]", "Z[20]"},
      {"z[21]", "Z[21]"},
      {"z[22]", "Z[22]"},
      {"z[23]", "Z[23]"},
      {"z[24]", "Z[24]"},
      {"z[25]", "Z[25]"},
      {"z[26]", "Z[26]"},
      {"z[27]", "Z[27]"},
      {"z[28]", "Z[28]"},
      {"z[29]", "Z[29]"},
      {"z[30]", "Z[30]"},
      {"z[31]", "Z[31]"},
      {"z[32]", "Z[32]"},
      {"z[33]", "Z[33]"},
      {"z[34]", "Z[34]"},
      {"z[35]", "Z[35]"},
      {"z[36]", "Z[36]"},
      {"z[37]", "Z[37]"},
      {"dly_b[0]", "DLY_B[0]"},
      {"dly_b[1]", "DLY_B[1]"},
      {"dly_b[2]", "DLY_B[2]"},
      {"dly_b[3]", "DLY_B[3]"},
      {"dly_b[4]", "DLY_B[4]"},
      {"dly_b[5]", "DLY_B[5]"},
      {"dly_b[6]", "DLY_B[6]"},
      {"dly_b[7]", "DLY_B[7]"},
      {"dly_b[8]", "DLY_B[8]"},
      {"dly_b[9]", "DLY_B[9]"},
      {"dly_b[10]", "DLY_B[10]"},
      {"dly_b[11]", "DLY_B[11]"},
      {"dly_b[12]", "DLY_B[12]"},
      {"dly_b[13]", "DLY_B[13]"},
      {"dly_b[14]", "DLY_B[14]"},
      {"dly_b[15]", "DLY_B[15]"},
      {"dly_b[16]", "DLY_B[16]"},
      {"dly_b[17]", "DLY_B[17]"},
      {"feedback[0]", "FEEDBACK[0]"},
      {"feedback[1]", "FEEDBACK[1]"},
      {"feedback[2]", "FEEDBACK[2]"},
      {"unsigned_a", "UNSIGNED_A"},
      {"unsigned_b", "UNSIGNED_B"},
      {"clk", "CLK"},
      {"lreset", "RESET"},
      {"acc_fir[0]", "ACC_FIR[0]"},
      {"acc_fir[1]", "ACC_FIR[1]"},
      {"acc_fir[2]", "ACC_FIR[2]"},
      {"acc_fir[3]", "ACC_FIR[3]"},
      {"acc_fir[4]", "ACC_FIR[4]"},
      {"acc_fir[5]", "ACC_FIR[5]"},
      {"load_acc", "LOAD_ACC"},
      {"saturate_enable", "SATURATE"},
      {"shift_right[0]", "SHIFT_RIGHT[0]"},
      {"shift_right[1]", "SHIFT_RIGHT[1]"},
      {"shift_right[2]", "SHIFT_RIGHT[2]"},
      {"shift_right[3]", "SHIFT_RIGHT[3]"},
      {"shift_right[4]", "SHIFT_RIGHT[4]"},
      {"shift_right[5]", "SHIFT_RIGHT[5]"},
      {"round", "ROUND"},
      {"subtract", "SUBTRACT"}};
  std::unordered_map<std::string, std::string> RS_DSP_MULT_REGIN_REGOUT_io_map =
      {{"a[0]", "A[0]"},
       {"a[1]", "A[1]"},
       {"a[2]", "A[2]"},
       {"a[3]", "A[3]"},
       {"a[4]", "A[4]"},
       {"a[5]", "A[5]"},
       {"a[6]", "A[6]"},
       {"a[7]", "A[7]"},
       {"a[8]", "A[8]"},
       {"a[9]", "A[9]"},
       {"a[10]", "A[10]"},
       {"a[11]", "A[11]"},
       {"a[12]", "A[12]"},
       {"a[13]", "A[13]"},
       {"a[14]", "A[14]"},
       {"a[15]", "A[15]"},
       {"a[16]", "A[16]"},
       {"a[17]", "A[17]"},
       {"a[18]", "A[18]"},
       {"a[19]", "A[19]"},
       {"b[0]", "B[0]"},
       {"b[1]", "B[1]"},
       {"b[2]", "B[2]"},
       {"b[3]", "B[3]"},
       {"b[4]", "B[4]"},
       {"b[5]", "B[5]"},
       {"b[6]", "B[6]"},
       {"b[7]", "B[7]"},
       {"b[8]", "B[8]"},
       {"b[9]", "B[9]"},
       {"b[10]", "B[10]"},
       {"b[11]", "B[11]"},
       {"b[12]", "B[12]"},
       {"b[13]", "B[13]"},
       {"b[14]", "B[14]"},
       {"b[15]", "B[15]"},
       {"b[16]", "B[16]"},
       {"b[17]", "B[17]"},
       {"z[0]", "Z[0]"},
       {"z[1]", "Z[1]"},
       {"z[2]", "Z[2]"},
       {"z[3]", "Z[3]"},
       {"z[4]", "Z[4]"},
       {"z[5]", "Z[5]"},
       {"z[6]", "Z[6]"},
       {"z[7]", "Z[7]"},
       {"z[8]", "Z[8]"},
       {"z[9]", "Z[9]"},
       {"z[10]", "Z[10]"},
       {"z[11]", "Z[11]"},
       {"z[12]", "Z[12]"},
       {"z[13]", "Z[13]"},
       {"z[14]", "Z[14]"},
       {"z[15]", "Z[15]"},
       {"z[16]", "Z[16]"},
       {"z[17]", "Z[17]"},
       {"z[18]", "Z[18]"},
       {"z[19]", "Z[19]"},
       {"z[20]", "Z[20]"},
       {"z[21]", "Z[21]"},
       {"z[22]", "Z[22]"},
       {"z[23]", "Z[23]"},
       {"z[24]", "Z[24]"},
       {"z[25]", "Z[25]"},
       {"z[26]", "Z[26]"},
       {"z[27]", "Z[27]"},
       {"z[28]", "Z[28]"},
       {"z[29]", "Z[29]"},
       {"z[30]", "Z[30]"},
       {"z[31]", "Z[31]"},
       {"z[32]", "Z[32]"},
       {"z[33]", "Z[33]"},
       {"z[34]", "Z[34]"},
       {"z[35]", "Z[35]"},
       {"z[36]", "Z[36]"},
       {"z[37]", "Z[37]"},
       {"feedback[0]", "FEEDBACK[0]"},
       {"feedback[1]", "FEEDBACK[1]"},
       {"feedback[2]", "FEEDBACK[2]"},
       {"unsigned_a", "UNSIGNED_A"},
       {"unsigned_b", "UNSIGNED_B"},
       {"clk", "CLK"},
       {"lreset", "RESET"}};
  std::unordered_map<std::string, std::string> RS_DSP_MULT_REGOUT_io_map = {
      {"a[0]", "A[0]"},
      {"a[1]", "A[1]"},
      {"a[2]", "A[2]"},
      {"a[3]", "A[3]"},
      {"a[4]", "A[4]"},
      {"a[5]", "A[5]"},
      {"a[6]", "A[6]"},
      {"a[7]", "A[7]"},
      {"a[8]", "A[8]"},
      {"a[9]", "A[9]"},
      {"a[10]", "A[10]"},
      {"a[11]", "A[11]"},
      {"a[12]", "A[12]"},
      {"a[13]", "A[13]"},
      {"a[14]", "A[14]"},
      {"a[15]", "A[15]"},
      {"a[16]", "A[16]"},
      {"a[17]", "A[17]"},
      {"a[18]", "A[18]"},
      {"a[19]", "A[19]"},
      {"b[0]", "B[0]"},
      {"b[1]", "B[1]"},
      {"b[2]", "B[2]"},
      {"b[3]", "B[3]"},
      {"b[4]", "B[4]"},
      {"b[5]", "B[5]"},
      {"b[6]", "B[6]"},
      {"b[7]", "B[7]"},
      {"b[8]", "B[8]"},
      {"b[9]", "B[9]"},
      {"b[10]", "B[10]"},
      {"b[11]", "B[11]"},
      {"b[12]", "B[12]"},
      {"b[13]", "B[13]"},
      {"b[14]", "B[14]"},
      {"b[15]", "B[15]"},
      {"b[16]", "B[16]"},
      {"b[17]", "B[17]"},
      {"z[0]", "Z[0]"},
      {"z[1]", "Z[1]"},
      {"z[2]", "Z[2]"},
      {"z[3]", "Z[3]"},
      {"z[4]", "Z[4]"},
      {"z[5]", "Z[5]"},
      {"z[6]", "Z[6]"},
      {"z[7]", "Z[7]"},
      {"z[8]", "Z[8]"},
      {"z[9]", "Z[9]"},
      {"z[10]", "Z[10]"},
      {"z[11]", "Z[11]"},
      {"z[12]", "Z[12]"},
      {"z[13]", "Z[13]"},
      {"z[14]", "Z[14]"},
      {"z[15]", "Z[15]"},
      {"z[16]", "Z[16]"},
      {"z[17]", "Z[17]"},
      {"z[18]", "Z[18]"},
      {"z[19]", "Z[19]"},
      {"z[20]", "Z[20]"},
      {"z[21]", "Z[21]"},
      {"z[22]", "Z[22]"},
      {"z[23]", "Z[23]"},
      {"z[24]", "Z[24]"},
      {"z[25]", "Z[25]"},
      {"z[26]", "Z[26]"},
      {"z[27]", "Z[27]"},
      {"z[28]", "Z[28]"},
      {"z[29]", "Z[29]"},
      {"z[30]", "Z[30]"},
      {"z[31]", "Z[31]"},
      {"z[32]", "Z[32]"},
      {"z[33]", "Z[33]"},
      {"z[34]", "Z[34]"},
      {"z[35]", "Z[35]"},
      {"z[36]", "Z[36]"},
      {"z[37]", "Z[37]"},
      {"feedback[0]", "FEEDBACK[0]"},
      {"feedback[1]", "FEEDBACK[1]"},
      {"feedback[2]", "FEEDBACK[2]"},
      {"unsigned_a", "UNSIGNED_A"},
      {"unsigned_b", "UNSIGNED_B"},
      {"clk", "CLK"},
      {"lreset", "RESET"}};
  std::unordered_map<std::string, std::string> RS_DSP_MULT_REGIN_io_map = {
      {"a[0]", "A[0]"},
      {"a[1]", "A[1]"},
      {"a[2]", "A[2]"},
      {"a[3]", "A[3]"},
      {"a[4]", "A[4]"},
      {"a[5]", "A[5]"},
      {"a[6]", "A[6]"},
      {"a[7]", "A[7]"},
      {"a[8]", "A[8]"},
      {"a[9]", "A[9]"},
      {"a[10]", "A[10]"},
      {"a[11]", "A[11]"},
      {"a[12]", "A[12]"},
      {"a[13]", "A[13]"},
      {"a[14]", "A[14]"},
      {"a[15]", "A[15]"},
      {"a[16]", "A[16]"},
      {"a[17]", "A[17]"},
      {"a[18]", "A[18]"},
      {"a[19]", "A[19]"},
      {"b[0]", "B[0]"},
      {"b[1]", "B[1]"},
      {"b[2]", "B[2]"},
      {"b[3]", "B[3]"},
      {"b[4]", "B[4]"},
      {"b[5]", "B[5]"},
      {"b[6]", "B[6]"},
      {"b[7]", "B[7]"},
      {"b[8]", "B[8]"},
      {"b[9]", "B[9]"},
      {"b[10]", "B[10]"},
      {"b[11]", "B[11]"},
      {"b[12]", "B[12]"},
      {"b[13]", "B[13]"},
      {"b[14]", "B[14]"},
      {"b[15]", "B[15]"},
      {"b[16]", "B[16]"},
      {"b[17]", "B[17]"},
      {"z[0]", "Z[0]"},
      {"z[1]", "Z[1]"},
      {"z[2]", "Z[2]"},
      {"z[3]", "Z[3]"},
      {"z[4]", "Z[4]"},
      {"z[5]", "Z[5]"},
      {"z[6]", "Z[6]"},
      {"z[7]", "Z[7]"},
      {"z[8]", "Z[8]"},
      {"z[9]", "Z[9]"},
      {"z[10]", "Z[10]"},
      {"z[11]", "Z[11]"},
      {"z[12]", "Z[12]"},
      {"z[13]", "Z[13]"},
      {"z[14]", "Z[14]"},
      {"z[15]", "Z[15]"},
      {"z[16]", "Z[16]"},
      {"z[17]", "Z[17]"},
      {"z[18]", "Z[18]"},
      {"z[19]", "Z[19]"},
      {"z[20]", "Z[20]"},
      {"z[21]", "Z[21]"},
      {"z[22]", "Z[22]"},
      {"z[23]", "Z[23]"},
      {"z[24]", "Z[24]"},
      {"z[25]", "Z[25]"},
      {"z[26]", "Z[26]"},
      {"z[27]", "Z[27]"},
      {"z[28]", "Z[28]"},
      {"z[29]", "Z[29]"},
      {"z[30]", "Z[30]"},
      {"z[31]", "Z[31]"},
      {"z[32]", "Z[32]"},
      {"z[33]", "Z[33]"},
      {"z[34]", "Z[34]"},
      {"z[35]", "Z[35]"},
      {"z[36]", "Z[36]"},
      {"z[37]", "Z[37]"},
      {"feedback[0]", "FEEDBACK[0]"},
      {"feedback[1]", "FEEDBACK[1]"},
      {"feedback[2]", "FEEDBACK[2]"},
      {"unsigned_a", "UNSIGNED_A"},
      {"unsigned_b", "UNSIGNED_B"},
      {"clk", "CLK"},
      {"lreset", "RESET"}};

  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      prim_io_maps = {
          {"RS_DSP_MULT", RS_DSP_MULT_io_map},
          {"RS_DSP_MULTACC_REGIN_REGOUT", RS_DSP_MULTACC_REGIN_REGOUT_io_map},
          {"RS_DSP_MULTACC_REGOUT", RS_DSP_MULTACC_REGOUT_io_map},
          {"RS_DSP_MULTACC_REGIN", RS_DSP_MULTACC_REGIN_io_map},
          {"RS_DSP_MULTACC", RS_DSP_MULTACC_io_map},
          {"RS_DSP_MULTADD_REGIN_REGOUT", RS_DSP_MULTADD_REGIN_REGOUT_io_map},
          {"RS_DSP_MULTADD_REGOUT", RS_DSP_MULTADD_REGOUT_io_map},
          {"RS_DSP_MULTADD_REGIN", RS_DSP_MULTADD_REGIN_io_map},
          {"RS_DSP_MULTADD", RS_DSP_MULTADD_io_map},
          {"RS_DSP_MULT_REGIN_REGOUT", RS_DSP_MULT_REGIN_REGOUT_io_map},
          {"RS_DSP_MULT_REGOUT", RS_DSP_MULT_REGOUT_io_map},
          {"RS_DSP_MULT_REGIN", RS_DSP_MULT_REGIN_io_map}};

  std::unordered_map<std::string, std::string> RS_DSP_MULT_REGIN_DSP_to_RS = {
      {"RESET", "lreset"},
      {"CLK", "clk"},
      {"UNSIGNED_A", "unsigned_a"},
      {"FEEDBACK[1]", "feedback[1]"},
      {"FEEDBACK[0]", "feedback[0]"},
      {"Z[37]", "z[37]"},
      {"Z[36]", "z[36]"},
      {"Z[34]", "z[34]"},
      {"Z[33]", "z[33]"},
      {"FEEDBACK[2]", "feedback[2]"},
      {"Z[30]", "z[30]"},
      {"Z[28]", "z[28]"},
      {"Z[27]", "z[27]"},
      {"Z[25]", "z[25]"},
      {"UNSIGNED_B", "unsigned_b"},
      {"Z[24]", "z[24]"},
      {"Z[23]", "z[23]"},
      {"Z[22]", "z[22]"},
      {"Z[21]", "z[21]"},
      {"B[5]", "b[5]"},
      {"B[8]", "b[8]"},
      {"B[3]", "b[3]"},
      {"Z[10]", "z[10]"},
      {"Z[31]", "z[31]"},
      {"A[18]", "a[18]"},
      {"B[2]", "b[2]"},
      {"A[16]", "a[16]"},
      {"A[13]", "a[13]"},
      {"Z[2]", "z[2]"},
      {"B[10]", "b[10]"},
      {"B[0]", "b[0]"},
      {"B[14]", "b[14]"},
      {"A[0]", "a[0]"},
      {"A[1]", "a[1]"},
      {"A[14]", "a[14]"},
      {"A[2]", "a[2]"},
      {"A[9]", "a[9]"},
      {"A[17]", "a[17]"},
      {"Z[32]", "z[32]"},
      {"Z[9]", "z[9]"},
      {"Z[26]", "z[26]"},
      {"B[4]", "b[4]"},
      {"Z[12]", "z[12]"},
      {"A[5]", "a[5]"},
      {"B[1]", "b[1]"},
      {"Z[8]", "z[8]"},
      {"A[11]", "a[11]"},
      {"B[12]", "b[12]"},
      {"Z[0]", "z[0]"},
      {"B[6]", "b[6]"},
      {"Z[18]", "z[18]"},
      {"B[16]", "b[16]"},
      {"A[6]", "a[6]"},
      {"Z[7]", "z[7]"},
      {"A[7]", "a[7]"},
      {"A[8]", "a[8]"},
      {"B[7]", "b[7]"},
      {"B[9]", "b[9]"},
      {"Z[19]", "z[19]"},
      {"A[10]", "a[10]"},
      {"A[12]", "a[12]"},
      {"Z[4]", "z[4]"},
      {"Z[29]", "z[29]"},
      {"A[3]", "a[3]"},
      {"B[11]", "b[11]"},
      {"B[13]", "b[13]"},
      {"Z[35]", "z[35]"},
      {"B[15]", "b[15]"},
      {"Z[1]", "z[1]"},
      {"B[17]", "b[17]"},
      {"Z[3]", "z[3]"},
      {"A[4]", "a[4]"},
      {"Z[5]", "z[5]"},
      {"A[19]", "a[19]"},
      {"Z[6]", "z[6]"},
      {"Z[14]", "z[14]"},
      {"Z[13]", "z[13]"},
      {"Z[15]", "z[15]"},
      {"A[15]", "a[15]"},
      {"Z[16]", "z[16]"},
      {"Z[17]", "z[17]"},
      {"Z[11]", "z[11]"},
      {"Z[20]", "z[20]"}};

  std::unordered_map<std::string, std::string> RS_DSP_MULT_REGOUT_DSP_to_RS = {
      {"RESET", "lreset"},
      {"CLK", "clk"},
      {"UNSIGNED_A", "unsigned_a"},
      {"FEEDBACK[1]", "feedback[1]"},
      {"FEEDBACK[0]", "feedback[0]"},
      {"Z[37]", "z[37]"},
      {"Z[36]", "z[36]"},
      {"Z[34]", "z[34]"},
      {"Z[33]", "z[33]"},
      {"FEEDBACK[2]", "feedback[2]"},
      {"Z[30]", "z[30]"},
      {"Z[28]", "z[28]"},
      {"Z[27]", "z[27]"},
      {"Z[25]", "z[25]"},
      {"UNSIGNED_B", "unsigned_b"},
      {"Z[24]", "z[24]"},
      {"Z[23]", "z[23]"},
      {"Z[22]", "z[22]"},
      {"Z[21]", "z[21]"},
      {"B[5]", "b[5]"},
      {"B[8]", "b[8]"},
      {"B[3]", "b[3]"},
      {"Z[10]", "z[10]"},
      {"Z[31]", "z[31]"},
      {"A[18]", "a[18]"},
      {"B[2]", "b[2]"},
      {"A[16]", "a[16]"},
      {"A[13]", "a[13]"},
      {"Z[2]", "z[2]"},
      {"B[10]", "b[10]"},
      {"B[0]", "b[0]"},
      {"B[14]", "b[14]"},
      {"A[0]", "a[0]"},
      {"A[1]", "a[1]"},
      {"A[14]", "a[14]"},
      {"A[2]", "a[2]"},
      {"A[9]", "a[9]"},
      {"A[17]", "a[17]"},
      {"Z[32]", "z[32]"},
      {"Z[9]", "z[9]"},
      {"Z[26]", "z[26]"},
      {"B[4]", "b[4]"},
      {"Z[12]", "z[12]"},
      {"A[5]", "a[5]"},
      {"B[1]", "b[1]"},
      {"Z[8]", "z[8]"},
      {"A[11]", "a[11]"},
      {"B[12]", "b[12]"},
      {"Z[0]", "z[0]"},
      {"B[6]", "b[6]"},
      {"Z[18]", "z[18]"},
      {"B[16]", "b[16]"},
      {"A[6]", "a[6]"},
      {"Z[7]", "z[7]"},
      {"A[7]", "a[7]"},
      {"A[8]", "a[8]"},
      {"B[7]", "b[7]"},
      {"B[9]", "b[9]"},
      {"Z[19]", "z[19]"},
      {"A[10]", "a[10]"},
      {"A[12]", "a[12]"},
      {"Z[4]", "z[4]"},
      {"Z[29]", "z[29]"},
      {"A[3]", "a[3]"},
      {"B[11]", "b[11]"},
      {"B[13]", "b[13]"},
      {"Z[35]", "z[35]"},
      {"B[15]", "b[15]"},
      {"Z[1]", "z[1]"},
      {"B[17]", "b[17]"},
      {"Z[3]", "z[3]"},
      {"A[4]", "a[4]"},
      {"Z[5]", "z[5]"},
      {"A[19]", "a[19]"},
      {"Z[6]", "z[6]"},
      {"Z[14]", "z[14]"},
      {"Z[13]", "z[13]"},
      {"Z[15]", "z[15]"},
      {"A[15]", "a[15]"},
      {"Z[16]", "z[16]"},
      {"Z[17]", "z[17]"},
      {"Z[11]", "z[11]"},
      {"Z[20]", "z[20]"}};

  std::unordered_map<std::string, std::string>
      RS_DSP_MULT_REGIN_REGOUT_DSP_to_RS = {{"RESET", "lreset"},
                                            {"CLK", "clk"},
                                            {"UNSIGNED_A", "unsigned_a"},
                                            {"FEEDBACK[1]", "feedback[1]"},
                                            {"FEEDBACK[0]", "feedback[0]"},
                                            {"Z[37]", "z[37]"},
                                            {"Z[36]", "z[36]"},
                                            {"Z[34]", "z[34]"},
                                            {"Z[33]", "z[33]"},
                                            {"FEEDBACK[2]", "feedback[2]"},
                                            {"Z[30]", "z[30]"},
                                            {"Z[28]", "z[28]"},
                                            {"Z[27]", "z[27]"},
                                            {"Z[25]", "z[25]"},
                                            {"UNSIGNED_B", "unsigned_b"},
                                            {"Z[24]", "z[24]"},
                                            {"Z[23]", "z[23]"},
                                            {"Z[22]", "z[22]"},
                                            {"Z[21]", "z[21]"},
                                            {"B[5]", "b[5]"},
                                            {"B[8]", "b[8]"},
                                            {"B[3]", "b[3]"},
                                            {"Z[10]", "z[10]"},
                                            {"Z[31]", "z[31]"},
                                            {"A[18]", "a[18]"},
                                            {"B[2]", "b[2]"},
                                            {"A[16]", "a[16]"},
                                            {"A[13]", "a[13]"},
                                            {"Z[2]", "z[2]"},
                                            {"B[10]", "b[10]"},
                                            {"B[0]", "b[0]"},
                                            {"B[14]", "b[14]"},
                                            {"A[0]", "a[0]"},
                                            {"A[1]", "a[1]"},
                                            {"A[14]", "a[14]"},
                                            {"A[2]", "a[2]"},
                                            {"A[9]", "a[9]"},
                                            {"A[17]", "a[17]"},
                                            {"Z[32]", "z[32]"},
                                            {"Z[9]", "z[9]"},
                                            {"Z[26]", "z[26]"},
                                            {"B[4]", "b[4]"},
                                            {"Z[12]", "z[12]"},
                                            {"A[5]", "a[5]"},
                                            {"B[1]", "b[1]"},
                                            {"Z[8]", "z[8]"},
                                            {"A[11]", "a[11]"},
                                            {"B[12]", "b[12]"},
                                            {"Z[0]", "z[0]"},
                                            {"B[6]", "b[6]"},
                                            {"Z[18]", "z[18]"},
                                            {"B[16]", "b[16]"},
                                            {"A[6]", "a[6]"},
                                            {"Z[7]", "z[7]"},
                                            {"A[7]", "a[7]"},
                                            {"A[8]", "a[8]"},
                                            {"B[7]", "b[7]"},
                                            {"B[9]", "b[9]"},
                                            {"Z[19]", "z[19]"},
                                            {"A[10]", "a[10]"},
                                            {"A[12]", "a[12]"},
                                            {"Z[4]", "z[4]"},
                                            {"Z[29]", "z[29]"},
                                            {"A[3]", "a[3]"},
                                            {"B[11]", "b[11]"},
                                            {"B[13]", "b[13]"},
                                            {"Z[35]", "z[35]"},
                                            {"B[15]", "b[15]"},
                                            {"Z[1]", "z[1]"},
                                            {"B[17]", "b[17]"},
                                            {"Z[3]", "z[3]"},
                                            {"A[4]", "a[4]"},
                                            {"Z[5]", "z[5]"},
                                            {"A[19]", "a[19]"},
                                            {"Z[6]", "z[6]"},
                                            {"Z[14]", "z[14]"},
                                            {"Z[13]", "z[13]"},
                                            {"Z[15]", "z[15]"},
                                            {"A[15]", "a[15]"},
                                            {"Z[16]", "z[16]"},
                                            {"Z[17]", "z[17]"},
                                            {"Z[11]", "z[11]"},
                                            {"Z[20]", "z[20]"}};

  std::unordered_map<std::string, std::string> RS_DSP_MULTADD_DSP_to_RS = {
      {"SUBTRACT", "subtract"},
      {"SHIFT_RIGHT[4]", "shift_right[4]"},
      {"SHIFT_RIGHT[3]", "shift_right[3]"},
      {"SHIFT_RIGHT[2]", "shift_right[2]"},
      {"SATURATE", "saturate_enable"},
      {"ACC_FIR[5]", "acc_fir[5]"},
      {"ACC_FIR[2]", "acc_fir[2]"},
      {"RESET", "lreset"},
      {"CLK", "clk"},
      {"ACC_FIR[3]", "acc_fir[3]"},
      {"FEEDBACK[1]", "feedback[1]"},
      {"DLY_B[17]", "dly_b[17]"},
      {"DLY_B[16]", "dly_b[16]"},
      {"DLY_B[15]", "dly_b[15]"},
      {"DLY_B[14]", "dly_b[14]"},
      {"FEEDBACK[0]", "feedback[0]"},
      {"DLY_B[13]", "dly_b[13]"},
      {"UNSIGNED_A", "unsigned_a"},
      {"DLY_B[12]", "dly_b[12]"},
      {"DLY_B[10]", "dly_b[10]"},
      {"DLY_B[9]", "dly_b[9]"},
      {"DLY_B[8]", "dly_b[8]"},
      {"DLY_B[7]", "dly_b[7]"},
      {"DLY_B[6]", "dly_b[6]"},
      {"DLY_B[3]", "dly_b[3]"},
      {"DLY_B[2]", "dly_b[2]"},
      {"DLY_B[0]", "dly_b[0]"},
      {"Z[37]", "z[37]"},
      {"Z[36]", "z[36]"},
      {"SHIFT_RIGHT[0]", "shift_right[0]"},
      {"Z[34]", "z[34]"},
      {"Z[33]", "z[33]"},
      {"FEEDBACK[2]", "feedback[2]"},
      {"Z[30]", "z[30]"},
      {"DLY_B[1]", "dly_b[1]"},
      {"Z[28]", "z[28]"},
      {"Z[27]", "z[27]"},
      {"Z[25]", "z[25]"},
      {"UNSIGNED_B", "unsigned_b"},
      {"Z[24]", "z[24]"},
      {"SHIFT_RIGHT[5]", "shift_right[5]"},
      {"LOAD_ACC", "load_acc"},
      {"Z[23]", "z[23]"},
      {"Z[22]", "z[22]"},
      {"Z[21]", "z[21]"},
      {"B[5]", "b[5]"},
      {"B[8]", "b[8]"},
      {"B[3]", "b[3]"},
      {"Z[10]", "z[10]"},
      {"Z[31]", "z[31]"},
      {"A[18]", "a[18]"},
      {"B[2]", "b[2]"},
      {"DLY_B[5]", "dly_b[5]"},
      {"A[16]", "a[16]"},
      {"A[13]", "a[13]"},
      {"Z[2]", "z[2]"},
      {"B[10]", "b[10]"},
      {"B[0]", "b[0]"},
      {"B[14]", "b[14]"},
      {"A[0]", "a[0]"},
      {"A[1]", "a[1]"},
      {"ACC_FIR[1]", "acc_fir[1]"},
      {"A[14]", "a[14]"},
      {"ACC_FIR[4]", "acc_fir[4]"},
      {"A[2]", "a[2]"},
      {"A[9]", "a[9]"},
      {"A[17]", "a[17]"},
      {"Z[32]", "z[32]"},
      {"Z[9]", "z[9]"},
      {"Z[26]", "z[26]"},
      {"B[4]", "b[4]"},
      {"ACC_FIR[0]", "acc_fir[0]"},
      {"Z[12]", "z[12]"},
      {"A[5]", "a[5]"},
      {"B[1]", "b[1]"},
      {"Z[8]", "z[8]"},
      {"A[11]", "a[11]"},
      {"B[12]", "b[12]"},
      {"Z[0]", "z[0]"},
      {"B[6]", "b[6]"},
      {"Z[18]", "z[18]"},
      {"B[16]", "b[16]"},
      {"A[6]", "a[6]"},
      {"Z[7]", "z[7]"},
      {"DLY_B[4]", "dly_b[4]"},
      {"A[7]", "a[7]"},
      {"SHIFT_RIGHT[1]", "shift_right[1]"},
      {"A[8]", "a[8]"},
      {"B[7]", "b[7]"},
      {"B[9]", "b[9]"},
      {"Z[19]", "z[19]"},
      {"A[10]", "a[10]"},
      {"A[12]", "a[12]"},
      {"Z[4]", "z[4]"},
      {"DLY_B[11]", "dly_b[11]"},
      {"Z[29]", "z[29]"},
      {"A[3]", "a[3]"},
      {"B[11]", "b[11]"},
      {"B[13]", "b[13]"},
      {"Z[35]", "z[35]"},
      {"B[15]", "b[15]"},
      {"Z[1]", "z[1]"},
      {"B[17]", "b[17]"},
      {"Z[3]", "z[3]"},
      {"A[4]", "a[4]"},
      {"Z[5]", "z[5]"},
      {"ROUND", "round"},
      {"A[19]", "a[19]"},
      {"Z[6]", "z[6]"},
      {"Z[14]", "z[14]"},
      {"Z[13]", "z[13]"},
      {"Z[15]", "z[15]"},
      {"A[15]", "a[15]"},
      {"Z[16]", "z[16]"},
      {"Z[17]", "z[17]"},
      {"Z[11]", "z[11]"},
      {"Z[20]", "z[20]"}};

  std::unordered_map<std::string, std::string> RS_DSP_MULTADD_REGIN_DSP_to_RS =
      {{"SUBTRACT", "subtract"},
       {"SHIFT_RIGHT[4]", "shift_right[4]"},
       {"SHIFT_RIGHT[3]", "shift_right[3]"},
       {"SHIFT_RIGHT[2]", "shift_right[2]"},
       {"SATURATE", "saturate_enable"},
       {"ACC_FIR[5]", "acc_fir[5]"},
       {"ACC_FIR[2]", "acc_fir[2]"},
       {"RESET", "lreset"},
       {"CLK", "clk"},
       {"ACC_FIR[3]", "acc_fir[3]"},
       {"FEEDBACK[1]", "feedback[1]"},
       {"DLY_B[17]", "dly_b[17]"},
       {"DLY_B[16]", "dly_b[16]"},
       {"DLY_B[15]", "dly_b[15]"},
       {"DLY_B[14]", "dly_b[14]"},
       {"FEEDBACK[0]", "feedback[0]"},
       {"DLY_B[13]", "dly_b[13]"},
       {"UNSIGNED_A", "unsigned_a"},
       {"DLY_B[12]", "dly_b[12]"},
       {"DLY_B[10]", "dly_b[10]"},
       {"DLY_B[9]", "dly_b[9]"},
       {"DLY_B[8]", "dly_b[8]"},
       {"DLY_B[7]", "dly_b[7]"},
       {"DLY_B[6]", "dly_b[6]"},
       {"DLY_B[3]", "dly_b[3]"},
       {"DLY_B[2]", "dly_b[2]"},
       {"DLY_B[0]", "dly_b[0]"},
       {"Z[37]", "z[37]"},
       {"Z[36]", "z[36]"},
       {"SHIFT_RIGHT[0]", "shift_right[0]"},
       {"Z[34]", "z[34]"},
       {"Z[33]", "z[33]"},
       {"FEEDBACK[2]", "feedback[2]"},
       {"Z[30]", "z[30]"},
       {"DLY_B[1]", "dly_b[1]"},
       {"Z[28]", "z[28]"},
       {"Z[27]", "z[27]"},
       {"Z[25]", "z[25]"},
       {"UNSIGNED_B", "unsigned_b"},
       {"Z[24]", "z[24]"},
       {"SHIFT_RIGHT[5]", "shift_right[5]"},
       {"LOAD_ACC", "load_acc"},
       {"Z[23]", "z[23]"},
       {"Z[22]", "z[22]"},
       {"Z[21]", "z[21]"},
       {"B[5]", "b[5]"},
       {"B[8]", "b[8]"},
       {"B[3]", "b[3]"},
       {"Z[10]", "z[10]"},
       {"Z[31]", "z[31]"},
       {"A[18]", "a[18]"},
       {"B[2]", "b[2]"},
       {"DLY_B[5]", "dly_b[5]"},
       {"A[16]", "a[16]"},
       {"A[13]", "a[13]"},
       {"Z[2]", "z[2]"},
       {"B[10]", "b[10]"},
       {"B[0]", "b[0]"},
       {"B[14]", "b[14]"},
       {"A[0]", "a[0]"},
       {"A[1]", "a[1]"},
       {"ACC_FIR[1]", "acc_fir[1]"},
       {"A[14]", "a[14]"},
       {"ACC_FIR[4]", "acc_fir[4]"},
       {"A[2]", "a[2]"},
       {"A[9]", "a[9]"},
       {"A[17]", "a[17]"},
       {"Z[32]", "z[32]"},
       {"Z[9]", "z[9]"},
       {"Z[26]", "z[26]"},
       {"B[4]", "b[4]"},
       {"ACC_FIR[0]", "acc_fir[0]"},
       {"Z[12]", "z[12]"},
       {"A[5]", "a[5]"},
       {"B[1]", "b[1]"},
       {"Z[8]", "z[8]"},
       {"A[11]", "a[11]"},
       {"B[12]", "b[12]"},
       {"Z[0]", "z[0]"},
       {"B[6]", "b[6]"},
       {"Z[18]", "z[18]"},
       {"B[16]", "b[16]"},
       {"A[6]", "a[6]"},
       {"Z[7]", "z[7]"},
       {"DLY_B[4]", "dly_b[4]"},
       {"A[7]", "a[7]"},
       {"SHIFT_RIGHT[1]", "shift_right[1]"},
       {"A[8]", "a[8]"},
       {"B[7]", "b[7]"},
       {"B[9]", "b[9]"},
       {"Z[19]", "z[19]"},
       {"A[10]", "a[10]"},
       {"A[12]", "a[12]"},
       {"Z[4]", "z[4]"},
       {"DLY_B[11]", "dly_b[11]"},
       {"Z[29]", "z[29]"},
       {"A[3]", "a[3]"},
       {"B[11]", "b[11]"},
       {"B[13]", "b[13]"},
       {"Z[35]", "z[35]"},
       {"B[15]", "b[15]"},
       {"Z[1]", "z[1]"},
       {"B[17]", "b[17]"},
       {"Z[3]", "z[3]"},
       {"A[4]", "a[4]"},
       {"Z[5]", "z[5]"},
       {"ROUND", "round"},
       {"A[19]", "a[19]"},
       {"Z[6]", "z[6]"},
       {"Z[14]", "z[14]"},
       {"Z[13]", "z[13]"},
       {"Z[15]", "z[15]"},
       {"A[15]", "a[15]"},
       {"Z[16]", "z[16]"},
       {"Z[17]", "z[17]"},
       {"Z[11]", "z[11]"},
       {"Z[20]", "z[20]"}};

  std::unordered_map<std::string, std::string>
      RS_DSP_MULTADD_REGIN_REGOUT_DSP_to_RS = {
          {"SUBTRACT", "subtract"},
          {"SHIFT_RIGHT[4]", "shift_right[4]"},
          {"SHIFT_RIGHT[3]", "shift_right[3]"},
          {"SHIFT_RIGHT[2]", "shift_right[2]"},
          {"SATURATE", "saturate_enable"},
          {"ACC_FIR[5]", "acc_fir[5]"},
          {"ACC_FIR[2]", "acc_fir[2]"},
          {"RESET", "lreset"},
          {"CLK", "clk"},
          {"ACC_FIR[3]", "acc_fir[3]"},
          {"FEEDBACK[1]", "feedback[1]"},
          {"DLY_B[17]", "dly_b[17]"},
          {"DLY_B[16]", "dly_b[16]"},
          {"DLY_B[15]", "dly_b[15]"},
          {"DLY_B[14]", "dly_b[14]"},
          {"FEEDBACK[0]", "feedback[0]"},
          {"DLY_B[13]", "dly_b[13]"},
          {"UNSIGNED_A", "unsigned_a"},
          {"DLY_B[12]", "dly_b[12]"},
          {"DLY_B[10]", "dly_b[10]"},
          {"DLY_B[9]", "dly_b[9]"},
          {"DLY_B[8]", "dly_b[8]"},
          {"DLY_B[7]", "dly_b[7]"},
          {"DLY_B[6]", "dly_b[6]"},
          {"DLY_B[3]", "dly_b[3]"},
          {"DLY_B[2]", "dly_b[2]"},
          {"DLY_B[0]", "dly_b[0]"},
          {"Z[37]", "z[37]"},
          {"Z[36]", "z[36]"},
          {"SHIFT_RIGHT[0]", "shift_right[0]"},
          {"Z[34]", "z[34]"},
          {"Z[33]", "z[33]"},
          {"FEEDBACK[2]", "feedback[2]"},
          {"Z[30]", "z[30]"},
          {"DLY_B[1]", "dly_b[1]"},
          {"Z[28]", "z[28]"},
          {"Z[27]", "z[27]"},
          {"Z[25]", "z[25]"},
          {"UNSIGNED_B", "unsigned_b"},
          {"Z[24]", "z[24]"},
          {"SHIFT_RIGHT[5]", "shift_right[5]"},
          {"LOAD_ACC", "load_acc"},
          {"Z[23]", "z[23]"},
          {"Z[22]", "z[22]"},
          {"Z[21]", "z[21]"},
          {"B[5]", "b[5]"},
          {"B[8]", "b[8]"},
          {"B[3]", "b[3]"},
          {"Z[10]", "z[10]"},
          {"Z[31]", "z[31]"},
          {"A[18]", "a[18]"},
          {"B[2]", "b[2]"},
          {"DLY_B[5]", "dly_b[5]"},
          {"A[16]", "a[16]"},
          {"A[13]", "a[13]"},
          {"Z[2]", "z[2]"},
          {"B[10]", "b[10]"},
          {"B[0]", "b[0]"},
          {"B[14]", "b[14]"},
          {"A[0]", "a[0]"},
          {"A[1]", "a[1]"},
          {"ACC_FIR[1]", "acc_fir[1]"},
          {"A[14]", "a[14]"},
          {"ACC_FIR[4]", "acc_fir[4]"},
          {"A[2]", "a[2]"},
          {"A[9]", "a[9]"},
          {"A[17]", "a[17]"},
          {"Z[32]", "z[32]"},
          {"Z[9]", "z[9]"},
          {"Z[26]", "z[26]"},
          {"B[4]", "b[4]"},
          {"ACC_FIR[0]", "acc_fir[0]"},
          {"Z[12]", "z[12]"},
          {"A[5]", "a[5]"},
          {"B[1]", "b[1]"},
          {"Z[8]", "z[8]"},
          {"A[11]", "a[11]"},
          {"B[12]", "b[12]"},
          {"Z[0]", "z[0]"},
          {"B[6]", "b[6]"},
          {"Z[18]", "z[18]"},
          {"B[16]", "b[16]"},
          {"A[6]", "a[6]"},
          {"Z[7]", "z[7]"},
          {"DLY_B[4]", "dly_b[4]"},
          {"A[7]", "a[7]"},
          {"SHIFT_RIGHT[1]", "shift_right[1]"},
          {"A[8]", "a[8]"},
          {"B[7]", "b[7]"},
          {"B[9]", "b[9]"},
          {"Z[19]", "z[19]"},
          {"A[10]", "a[10]"},
          {"A[12]", "a[12]"},
          {"Z[4]", "z[4]"},
          {"DLY_B[11]", "dly_b[11]"},
          {"Z[29]", "z[29]"},
          {"A[3]", "a[3]"},
          {"B[11]", "b[11]"},
          {"B[13]", "b[13]"},
          {"Z[35]", "z[35]"},
          {"B[15]", "b[15]"},
          {"Z[1]", "z[1]"},
          {"B[17]", "b[17]"},
          {"Z[3]", "z[3]"},
          {"A[4]", "a[4]"},
          {"Z[5]", "z[5]"},
          {"ROUND", "round"},
          {"A[19]", "a[19]"},
          {"Z[6]", "z[6]"},
          {"Z[14]", "z[14]"},
          {"Z[13]", "z[13]"},
          {"Z[15]", "z[15]"},
          {"A[15]", "a[15]"},
          {"Z[16]", "z[16]"},
          {"Z[17]", "z[17]"},
          {"Z[11]", "z[11]"},
          {"Z[20]", "z[20]"}};

  std::unordered_map<std::string, std::string> RS_DSP_MULTACC_DSP_to_RS = {
      {"SUBTRACT", "subtract"},
      {"SHIFT_RIGHT[4]", "shift_right[4]"},
      {"SHIFT_RIGHT[3]", "shift_right[3]"},
      {"SHIFT_RIGHT[2]", "shift_right[2]"},
      {"SATURATE", "saturate_enable"},
      {"RESET", "lreset"},
      {"CLK", "clk"},
      {"UNSIGNED_A", "unsigned_a"},
      {"FEEDBACK[1]", "feedback[1]"},
      {"FEEDBACK[0]", "feedback[0]"},
      {"Z[37]", "z[37]"},
      {"Z[36]", "z[36]"},
      {"SHIFT_RIGHT[0]", "shift_right[0]"},
      {"Z[34]", "z[34]"},
      {"Z[33]", "z[33]"},
      {"FEEDBACK[2]", "feedback[2]"},
      {"Z[30]", "z[30]"},
      {"Z[28]", "z[28]"},
      {"Z[27]", "z[27]"},
      {"Z[25]", "z[25]"},
      {"UNSIGNED_B", "unsigned_b"},
      {"Z[24]", "z[24]"},
      {"SHIFT_RIGHT[5]", "shift_right[5]"},
      {"LOAD_ACC", "load_acc"},
      {"Z[23]", "z[23]"},
      {"Z[22]", "z[22]"},
      {"Z[21]", "z[21]"},
      {"B[5]", "b[5]"},
      {"B[8]", "b[8]"},
      {"B[3]", "b[3]"},
      {"Z[10]", "z[10]"},
      {"Z[31]", "z[31]"},
      {"A[18]", "a[18]"},
      {"B[2]", "b[2]"},
      {"A[16]", "a[16]"},
      {"A[13]", "a[13]"},
      {"Z[2]", "z[2]"},
      {"B[10]", "b[10]"},
      {"B[0]", "b[0]"},
      {"B[14]", "b[14]"},
      {"A[0]", "a[0]"},
      {"A[1]", "a[1]"},
      {"A[14]", "a[14]"},
      {"A[2]", "a[2]"},
      {"A[9]", "a[9]"},
      {"A[17]", "a[17]"},
      {"Z[32]", "z[32]"},
      {"Z[9]", "z[9]"},
      {"Z[26]", "z[26]"},
      {"B[4]", "b[4]"},
      {"Z[12]", "z[12]"},
      {"A[5]", "a[5]"},
      {"B[1]", "b[1]"},
      {"Z[8]", "z[8]"},
      {"A[11]", "a[11]"},
      {"B[12]", "b[12]"},
      {"Z[0]", "z[0]"},
      {"B[6]", "b[6]"},
      {"Z[18]", "z[18]"},
      {"B[16]", "b[16]"},
      {"A[6]", "a[6]"},
      {"Z[7]", "z[7]"},
      {"A[7]", "a[7]"},
      {"SHIFT_RIGHT[1]", "shift_right[1]"},
      {"A[8]", "a[8]"},
      {"B[7]", "b[7]"},
      {"B[9]", "b[9]"},
      {"Z[19]", "z[19]"},
      {"A[10]", "a[10]"},
      {"A[12]", "a[12]"},
      {"Z[4]", "z[4]"},
      {"Z[29]", "z[29]"},
      {"A[3]", "a[3]"},
      {"B[11]", "b[11]"},
      {"B[13]", "b[13]"},
      {"Z[35]", "z[35]"},
      {"B[15]", "b[15]"},
      {"Z[1]", "z[1]"},
      {"B[17]", "b[17]"},
      {"Z[3]", "z[3]"},
      {"A[4]", "a[4]"},
      {"Z[5]", "z[5]"},
      {"ROUND", "round"},
      {"A[19]", "a[19]"},
      {"Z[6]", "z[6]"},
      {"Z[14]", "z[14]"},
      {"Z[13]", "z[13]"},
      {"Z[15]", "z[15]"},
      {"A[15]", "a[15]"},
      {"Z[16]", "z[16]"},
      {"Z[17]", "z[17]"},
      {"Z[11]", "z[11]"},
      {"Z[20]", "z[20]"}};

  std::unordered_map<std::string, std::string> RS_DSP_MULTACC_REGIN_DSP_to_RS =
      {{"SUBTRACT", "subtract"},
       {"SHIFT_RIGHT[4]", "shift_right[4]"},
       {"SHIFT_RIGHT[3]", "shift_right[3]"},
       {"SHIFT_RIGHT[2]", "shift_right[2]"},
       {"SATURATE", "saturate_enable"},
       {"RESET", "lreset"},
       {"CLK", "clk"},
       {"UNSIGNED_A", "unsigned_a"},
       {"FEEDBACK[1]", "feedback[1]"},
       {"FEEDBACK[0]", "feedback[0]"},
       {"Z[37]", "z[37]"},
       {"Z[36]", "z[36]"},
       {"SHIFT_RIGHT[0]", "shift_right[0]"},
       {"Z[34]", "z[34]"},
       {"Z[33]", "z[33]"},
       {"FEEDBACK[2]", "feedback[2]"},
       {"Z[30]", "z[30]"},
       {"Z[28]", "z[28]"},
       {"Z[27]", "z[27]"},
       {"Z[25]", "z[25]"},
       {"UNSIGNED_B", "unsigned_b"},
       {"Z[24]", "z[24]"},
       {"SHIFT_RIGHT[5]", "shift_right[5]"},
       {"LOAD_ACC", "load_acc"},
       {"Z[23]", "z[23]"},
       {"Z[22]", "z[22]"},
       {"Z[21]", "z[21]"},
       {"B[5]", "b[5]"},
       {"B[8]", "b[8]"},
       {"B[3]", "b[3]"},
       {"Z[10]", "z[10]"},
       {"Z[31]", "z[31]"},
       {"A[18]", "a[18]"},
       {"B[2]", "b[2]"},
       {"A[16]", "a[16]"},
       {"A[13]", "a[13]"},
       {"Z[2]", "z[2]"},
       {"B[10]", "b[10]"},
       {"B[0]", "b[0]"},
       {"B[14]", "b[14]"},
       {"A[0]", "a[0]"},
       {"A[1]", "a[1]"},
       {"A[14]", "a[14]"},
       {"A[2]", "a[2]"},
       {"A[9]", "a[9]"},
       {"A[17]", "a[17]"},
       {"Z[32]", "z[32]"},
       {"Z[9]", "z[9]"},
       {"Z[26]", "z[26]"},
       {"B[4]", "b[4]"},
       {"Z[12]", "z[12]"},
       {"A[5]", "a[5]"},
       {"B[1]", "b[1]"},
       {"Z[8]", "z[8]"},
       {"A[11]", "a[11]"},
       {"B[12]", "b[12]"},
       {"Z[0]", "z[0]"},
       {"B[6]", "b[6]"},
       {"Z[18]", "z[18]"},
       {"B[16]", "b[16]"},
       {"A[6]", "a[6]"},
       {"Z[7]", "z[7]"},
       {"A[7]", "a[7]"},
       {"SHIFT_RIGHT[1]", "shift_right[1]"},
       {"A[8]", "a[8]"},
       {"B[7]", "b[7]"},
       {"B[9]", "b[9]"},
       {"Z[19]", "z[19]"},
       {"A[10]", "a[10]"},
       {"A[12]", "a[12]"},
       {"Z[4]", "z[4]"},
       {"Z[29]", "z[29]"},
       {"A[3]", "a[3]"},
       {"B[11]", "b[11]"},
       {"B[13]", "b[13]"},
       {"Z[35]", "z[35]"},
       {"B[15]", "b[15]"},
       {"Z[1]", "z[1]"},
       {"B[17]", "b[17]"},
       {"Z[3]", "z[3]"},
       {"A[4]", "a[4]"},
       {"Z[5]", "z[5]"},
       {"ROUND", "round"},
       {"A[19]", "a[19]"},
       {"Z[6]", "z[6]"},
       {"Z[14]", "z[14]"},
       {"Z[13]", "z[13]"},
       {"Z[15]", "z[15]"},
       {"A[15]", "a[15]"},
       {"Z[16]", "z[16]"},
       {"Z[17]", "z[17]"},
       {"Z[11]", "z[11]"},
       {"Z[20]", "z[20]"}};

  std::unordered_map<std::string, std::string> RS_DSP_MULTACC_REGOUT_DSP_to_RS =
      {{"SUBTRACT", "subtract"},
       {"SHIFT_RIGHT[4]", "shift_right[4]"},
       {"SHIFT_RIGHT[3]", "shift_right[3]"},
       {"SHIFT_RIGHT[2]", "shift_right[2]"},
       {"SATURATE", "saturate_enable"},
       {"RESET", "lreset"},
       {"CLK", "clk"},
       {"UNSIGNED_A", "unsigned_a"},
       {"FEEDBACK[1]", "feedback[1]"},
       {"FEEDBACK[0]", "feedback[0]"},
       {"Z[37]", "z[37]"},
       {"Z[36]", "z[36]"},
       {"SHIFT_RIGHT[0]", "shift_right[0]"},
       {"Z[34]", "z[34]"},
       {"Z[33]", "z[33]"},
       {"FEEDBACK[2]", "feedback[2]"},
       {"Z[30]", "z[30]"},
       {"Z[28]", "z[28]"},
       {"Z[27]", "z[27]"},
       {"Z[25]", "z[25]"},
       {"UNSIGNED_B", "unsigned_b"},
       {"Z[24]", "z[24]"},
       {"SHIFT_RIGHT[5]", "shift_right[5]"},
       {"LOAD_ACC", "load_acc"},
       {"Z[23]", "z[23]"},
       {"Z[22]", "z[22]"},
       {"Z[21]", "z[21]"},
       {"B[5]", "b[5]"},
       {"B[8]", "b[8]"},
       {"B[3]", "b[3]"},
       {"Z[10]", "z[10]"},
       {"Z[31]", "z[31]"},
       {"A[18]", "a[18]"},
       {"B[2]", "b[2]"},
       {"A[16]", "a[16]"},
       {"A[13]", "a[13]"},
       {"Z[2]", "z[2]"},
       {"B[10]", "b[10]"},
       {"B[0]", "b[0]"},
       {"B[14]", "b[14]"},
       {"A[0]", "a[0]"},
       {"A[1]", "a[1]"},
       {"A[14]", "a[14]"},
       {"A[2]", "a[2]"},
       {"A[9]", "a[9]"},
       {"A[17]", "a[17]"},
       {"Z[32]", "z[32]"},
       {"Z[9]", "z[9]"},
       {"Z[26]", "z[26]"},
       {"B[4]", "b[4]"},
       {"Z[12]", "z[12]"},
       {"A[5]", "a[5]"},
       {"B[1]", "b[1]"},
       {"Z[8]", "z[8]"},
       {"A[11]", "a[11]"},
       {"B[12]", "b[12]"},
       {"Z[0]", "z[0]"},
       {"B[6]", "b[6]"},
       {"Z[18]", "z[18]"},
       {"B[16]", "b[16]"},
       {"A[6]", "a[6]"},
       {"Z[7]", "z[7]"},
       {"A[7]", "a[7]"},
       {"SHIFT_RIGHT[1]", "shift_right[1]"},
       {"A[8]", "a[8]"},
       {"B[7]", "b[7]"},
       {"B[9]", "b[9]"},
       {"Z[19]", "z[19]"},
       {"A[10]", "a[10]"},
       {"A[12]", "a[12]"},
       {"Z[4]", "z[4]"},
       {"Z[29]", "z[29]"},
       {"A[3]", "a[3]"},
       {"B[11]", "b[11]"},
       {"B[13]", "b[13]"},
       {"Z[35]", "z[35]"},
       {"B[15]", "b[15]"},
       {"Z[1]", "z[1]"},
       {"B[17]", "b[17]"},
       {"Z[3]", "z[3]"},
       {"A[4]", "a[4]"},
       {"Z[5]", "z[5]"},
       {"ROUND", "round"},
       {"A[19]", "a[19]"},
       {"Z[6]", "z[6]"},
       {"Z[14]", "z[14]"},
       {"Z[13]", "z[13]"},
       {"Z[15]", "z[15]"},
       {"A[15]", "a[15]"},
       {"Z[16]", "z[16]"},
       {"Z[17]", "z[17]"},
       {"Z[11]", "z[11]"},
       {"Z[20]", "z[20]"}};

  std::unordered_map<std::string, std::string> RS_DSP_MULTADD_REGOUT_DSP_to_RS =
      {{"SUBTRACT", "subtract"},
       {"SHIFT_RIGHT[4]", "shift_right[4]"},
       {"SHIFT_RIGHT[3]", "shift_right[3]"},
       {"SHIFT_RIGHT[2]", "shift_right[2]"},
       {"SATURATE", "saturate_enable"},
       {"ACC_FIR[5]", "acc_fir[5]"},
       {"ACC_FIR[2]", "acc_fir[2]"},
       {"RESET", "lreset"},
       {"CLK", "clk"},
       {"ACC_FIR[3]", "acc_fir[3]"},
       {"FEEDBACK[1]", "feedback[1]"},
       {"DLY_B[17]", "dly_b[17]"},
       {"DLY_B[16]", "dly_b[16]"},
       {"DLY_B[15]", "dly_b[15]"},
       {"DLY_B[14]", "dly_b[14]"},
       {"FEEDBACK[0]", "feedback[0]"},
       {"DLY_B[13]", "dly_b[13]"},
       {"UNSIGNED_A", "unsigned_a"},
       {"DLY_B[12]", "dly_b[12]"},
       {"DLY_B[10]", "dly_b[10]"},
       {"DLY_B[9]", "dly_b[9]"},
       {"DLY_B[8]", "dly_b[8]"},
       {"DLY_B[7]", "dly_b[7]"},
       {"DLY_B[6]", "dly_b[6]"},
       {"DLY_B[3]", "dly_b[3]"},
       {"DLY_B[2]", "dly_b[2]"},
       {"DLY_B[0]", "dly_b[0]"},
       {"Z[37]", "z[37]"},
       {"Z[36]", "z[36]"},
       {"SHIFT_RIGHT[0]", "shift_right[0]"},
       {"Z[34]", "z[34]"},
       {"Z[33]", "z[33]"},
       {"FEEDBACK[2]", "feedback[2]"},
       {"Z[30]", "z[30]"},
       {"DLY_B[1]", "dly_b[1]"},
       {"Z[28]", "z[28]"},
       {"Z[27]", "z[27]"},
       {"Z[25]", "z[25]"},
       {"UNSIGNED_B", "unsigned_b"},
       {"Z[24]", "z[24]"},
       {"SHIFT_RIGHT[5]", "shift_right[5]"},
       {"LOAD_ACC", "load_acc"},
       {"Z[23]", "z[23]"},
       {"Z[22]", "z[22]"},
       {"Z[21]", "z[21]"},
       {"B[5]", "b[5]"},
       {"B[8]", "b[8]"},
       {"B[3]", "b[3]"},
       {"Z[10]", "z[10]"},
       {"Z[31]", "z[31]"},
       {"A[18]", "a[18]"},
       {"B[2]", "b[2]"},
       {"DLY_B[5]", "dly_b[5]"},
       {"A[16]", "a[16]"},
       {"A[13]", "a[13]"},
       {"Z[2]", "z[2]"},
       {"B[10]", "b[10]"},
       {"B[0]", "b[0]"},
       {"B[14]", "b[14]"},
       {"A[0]", "a[0]"},
       {"A[1]", "a[1]"},
       {"ACC_FIR[1]", "acc_fir[1]"},
       {"A[14]", "a[14]"},
       {"ACC_FIR[4]", "acc_fir[4]"},
       {"A[2]", "a[2]"},
       {"A[9]", "a[9]"},
       {"A[17]", "a[17]"},
       {"Z[32]", "z[32]"},
       {"Z[9]", "z[9]"},
       {"Z[26]", "z[26]"},
       {"B[4]", "b[4]"},
       {"ACC_FIR[0]", "acc_fir[0]"},
       {"Z[12]", "z[12]"},
       {"A[5]", "a[5]"},
       {"B[1]", "b[1]"},
       {"Z[8]", "z[8]"},
       {"A[11]", "a[11]"},
       {"B[12]", "b[12]"},
       {"Z[0]", "z[0]"},
       {"B[6]", "b[6]"},
       {"Z[18]", "z[18]"},
       {"B[16]", "b[16]"},
       {"A[6]", "a[6]"},
       {"Z[7]", "z[7]"},
       {"DLY_B[4]", "dly_b[4]"},
       {"A[7]", "a[7]"},
       {"SHIFT_RIGHT[1]", "shift_right[1]"},
       {"A[8]", "a[8]"},
       {"B[7]", "b[7]"},
       {"B[9]", "b[9]"},
       {"Z[19]", "z[19]"},
       {"A[10]", "a[10]"},
       {"A[12]", "a[12]"},
       {"Z[4]", "z[4]"},
       {"DLY_B[11]", "dly_b[11]"},
       {"Z[29]", "z[29]"},
       {"A[3]", "a[3]"},
       {"B[11]", "b[11]"},
       {"B[13]", "b[13]"},
       {"Z[35]", "z[35]"},
       {"B[15]", "b[15]"},
       {"Z[1]", "z[1]"},
       {"B[17]", "b[17]"},
       {"Z[3]", "z[3]"},
       {"A[4]", "a[4]"},
       {"Z[5]", "z[5]"},
       {"ROUND", "round"},
       {"A[19]", "a[19]"},
       {"Z[6]", "z[6]"},
       {"Z[14]", "z[14]"},
       {"Z[13]", "z[13]"},
       {"Z[15]", "z[15]"},
       {"A[15]", "a[15]"},
       {"Z[16]", "z[16]"},
       {"Z[17]", "z[17]"},
       {"Z[11]", "z[11]"},
       {"Z[20]", "z[20]"}};

  std::unordered_map<std::string, std::string>
      RS_DSP_MULTACC_REGIN_REGOUT_DSP_to_RS = {
          {"SUBTRACT", "subtract"},
          {"SHIFT_RIGHT[4]", "shift_right[4]"},
          {"SHIFT_RIGHT[3]", "shift_right[3]"},
          {"SHIFT_RIGHT[2]", "shift_right[2]"},
          {"SATURATE", "saturate_enable"},
          {"RESET", "lreset"},
          {"CLK", "clk"},
          {"UNSIGNED_A", "unsigned_a"},
          {"FEEDBACK[1]", "feedback[1]"},
          {"FEEDBACK[0]", "feedback[0]"},
          {"Z[37]", "z[37]"},
          {"Z[36]", "z[36]"},
          {"SHIFT_RIGHT[0]", "shift_right[0]"},
          {"Z[34]", "z[34]"},
          {"Z[33]", "z[33]"},
          {"FEEDBACK[2]", "feedback[2]"},
          {"Z[30]", "z[30]"},
          {"Z[28]", "z[28]"},
          {"Z[27]", "z[27]"},
          {"Z[25]", "z[25]"},
          {"UNSIGNED_B", "unsigned_b"},
          {"Z[24]", "z[24]"},
          {"SHIFT_RIGHT[5]", "shift_right[5]"},
          {"LOAD_ACC", "load_acc"},
          {"Z[23]", "z[23]"},
          {"Z[22]", "z[22]"},
          {"Z[21]", "z[21]"},
          {"B[5]", "b[5]"},
          {"B[8]", "b[8]"},
          {"B[3]", "b[3]"},
          {"Z[10]", "z[10]"},
          {"Z[31]", "z[31]"},
          {"A[18]", "a[18]"},
          {"B[2]", "b[2]"},
          {"A[16]", "a[16]"},
          {"A[13]", "a[13]"},
          {"Z[2]", "z[2]"},
          {"B[10]", "b[10]"},
          {"B[0]", "b[0]"},
          {"B[14]", "b[14]"},
          {"A[0]", "a[0]"},
          {"A[1]", "a[1]"},
          {"A[14]", "a[14]"},
          {"A[2]", "a[2]"},
          {"A[9]", "a[9]"},
          {"A[17]", "a[17]"},
          {"Z[32]", "z[32]"},
          {"Z[9]", "z[9]"},
          {"Z[26]", "z[26]"},
          {"B[4]", "b[4]"},
          {"Z[12]", "z[12]"},
          {"A[5]", "a[5]"},
          {"B[1]", "b[1]"},
          {"Z[8]", "z[8]"},
          {"A[11]", "a[11]"},
          {"B[12]", "b[12]"},
          {"Z[0]", "z[0]"},
          {"B[6]", "b[6]"},
          {"Z[18]", "z[18]"},
          {"B[16]", "b[16]"},
          {"A[6]", "a[6]"},
          {"Z[7]", "z[7]"},
          {"A[7]", "a[7]"},
          {"SHIFT_RIGHT[1]", "shift_right[1]"},
          {"A[8]", "a[8]"},
          {"B[7]", "b[7]"},
          {"B[9]", "b[9]"},
          {"Z[19]", "z[19]"},
          {"A[10]", "a[10]"},
          {"A[12]", "a[12]"},
          {"Z[4]", "z[4]"},
          {"Z[29]", "z[29]"},
          {"A[3]", "a[3]"},
          {"B[11]", "b[11]"},
          {"B[13]", "b[13]"},
          {"Z[35]", "z[35]"},
          {"B[15]", "b[15]"},
          {"Z[1]", "z[1]"},
          {"B[17]", "b[17]"},
          {"Z[3]", "z[3]"},
          {"A[4]", "a[4]"},
          {"Z[5]", "z[5]"},
          {"ROUND", "round"},
          {"A[19]", "a[19]"},
          {"Z[6]", "z[6]"},
          {"Z[14]", "z[14]"},
          {"Z[13]", "z[13]"},
          {"Z[15]", "z[15]"},
          {"A[15]", "a[15]"},
          {"Z[16]", "z[16]"},
          {"Z[17]", "z[17]"},
          {"Z[11]", "z[11]"},
          {"Z[20]", "z[20]"}};

  std::unordered_map<std::string, std::string> RS_DSP_MULT_DSP_to_RS = {
      {"UNSIGNED_A", "unsigned_a"},
      {"FEEDBACK[1]", "feedback[1]"},
      {"FEEDBACK[0]", "feedback[0]"},
      {"Z[37]", "z[37]"},
      {"Z[36]", "z[36]"},
      {"Z[34]", "z[34]"},
      {"Z[33]", "z[33]"},
      {"FEEDBACK[2]", "feedback[2]"},
      {"Z[30]", "z[30]"},
      {"Z[28]", "z[28]"},
      {"Z[27]", "z[27]"},
      {"Z[25]", "z[25]"},
      {"UNSIGNED_B", "unsigned_b"},
      {"Z[24]", "z[24]"},
      {"Z[23]", "z[23]"},
      {"Z[22]", "z[22]"},
      {"Z[21]", "z[21]"},
      {"B[5]", "b[5]"},
      {"B[8]", "b[8]"},
      {"B[3]", "b[3]"},
      {"Z[10]", "z[10]"},
      {"Z[31]", "z[31]"},
      {"A[18]", "a[18]"},
      {"B[2]", "b[2]"},
      {"A[16]", "a[16]"},
      {"A[13]", "a[13]"},
      {"Z[2]", "z[2]"},
      {"B[10]", "b[10]"},
      {"B[0]", "b[0]"},
      {"B[14]", "b[14]"},
      {"A[0]", "a[0]"},
      {"A[1]", "a[1]"},
      {"A[14]", "a[14]"},
      {"A[2]", "a[2]"},
      {"A[9]", "a[9]"},
      {"A[17]", "a[17]"},
      {"Z[32]", "z[32]"},
      {"Z[9]", "z[9]"},
      {"Z[26]", "z[26]"},
      {"B[4]", "b[4]"},
      {"Z[12]", "z[12]"},
      {"A[5]", "a[5]"},
      {"B[1]", "b[1]"},
      {"Z[8]", "z[8]"},
      {"A[11]", "a[11]"},
      {"B[12]", "b[12]"},
      {"Z[0]", "z[0]"},
      {"B[6]", "b[6]"},
      {"Z[18]", "z[18]"},
      {"B[16]", "b[16]"},
      {"A[6]", "a[6]"},
      {"Z[7]", "z[7]"},
      {"A[7]", "a[7]"},
      {"A[8]", "a[8]"},
      {"B[7]", "b[7]"},
      {"B[9]", "b[9]"},
      {"Z[19]", "z[19]"},
      {"A[10]", "a[10]"},
      {"A[12]", "a[12]"},
      {"Z[4]", "z[4]"},
      {"Z[29]", "z[29]"},
      {"A[3]", "a[3]"},
      {"B[11]", "b[11]"},
      {"B[13]", "b[13]"},
      {"Z[35]", "z[35]"},
      {"B[15]", "b[15]"},
      {"Z[1]", "z[1]"},
      {"B[17]", "b[17]"},
      {"Z[3]", "z[3]"},
      {"A[4]", "a[4]"},
      {"Z[5]", "z[5]"},
      {"A[19]", "a[19]"},
      {"Z[6]", "z[6]"},
      {"Z[14]", "z[14]"},
      {"Z[13]", "z[13]"},
      {"Z[15]", "z[15]"},
      {"A[15]", "a[15]"},
      {"Z[16]", "z[16]"},
      {"Z[17]", "z[17]"},
      {"Z[11]", "z[11]"},
      {"Z[20]", "z[20]"}};
  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      prim_io_maps_dsp_to_rs_prim = {
          {"RS_DSP_MULT", RS_DSP_MULT_DSP_to_RS},
          {"RS_DSP_MULTACC_REGIN_REGOUT",
           RS_DSP_MULTACC_REGIN_REGOUT_DSP_to_RS},
          {"RS_DSP_MULTADD_REGOUT", RS_DSP_MULTADD_REGOUT_DSP_to_RS},
          {"RS_DSP_MULTACC_REGOUT", RS_DSP_MULTACC_REGOUT_DSP_to_RS},
          {"RS_DSP_MULTACC_REGIN", RS_DSP_MULTACC_REGIN_DSP_to_RS},
          {"RS_DSP_MULTACC", RS_DSP_MULTACC_DSP_to_RS},
          {"RS_DSP_MULTADD_REGIN_REGOUT",
           RS_DSP_MULTADD_REGIN_REGOUT_DSP_to_RS},
          {"RS_DSP_MULTADD_REGIN", RS_DSP_MULTADD_REGIN_DSP_to_RS},
          {"RS_DSP_MULTADD", RS_DSP_MULTADD_DSP_to_RS},
          {"RS_DSP_MULT_REGIN_REGOUT", RS_DSP_MULT_REGIN_REGOUT_DSP_to_RS},
          {"RS_DSP_MULT_REGOUT", RS_DSP_MULT_REGOUT_DSP_to_RS},
          {"RS_DSP_MULT_REGIN", RS_DSP_MULT_REGIN_DSP_to_RS}};

  // Latch flattening
  std::unordered_map<std::string, std::string> latch_lut_LUT_strs = {
      {"LATCH", "10101100"},
      {"LATCHN", "10101100"},
      {"LATCHR", "0000101000001100"},
      {"LATCHS", "1111101011111100"},
      {"LATCHNR", "0000101000001100"},
      {"LATCHNS", "1111101011111100"},
      {"LATCHSRE",
       "1011101111110011111100111111001100000000000000000000000000000000"},
      {"LATCHNSRE",
       "1111001110111011111100111111001100000000000000000000000000000000"}};

  std::unordered_map<std::string, std::string> latch_lut_WIDTH_strs = {
      {"LATCH", "3"},    {"LATCHN", "3"},   {"LATCHR", "4"},
      {"LATCHS", "4"},   {"LATCHNR", "4"},  {"LATCHNS", "4"},
      {"LATCHSRE", "6"}, {"LATCHNSRE", "6"}};
  std::unordered_map<std::string, std::string> latch_ports = {
      {"LATCH", "DGQ"},       {"LATCHN", "DGQ"},      {"LATCHR", "DGRQ"},
      {"LATCHS", "DGRQ"},     {"LATCHNR", "DGRQ"},    {"LATCHNS", "DGRQ"},
      {"LATCHSRE", "QSRDGE"}, {"LATCHNSRE", "QSRDGE"}};

  std::unordered_map<std::string, std::string> lut_A_port_connections = {
      {"LATCH", "GQD"},       {"LATCHN", "GDQ"},      {"LATCHR", "GRQD"},
      {"LATCHS", "GRQD"},     {"LATCHNR", "GRDQ"},    {"LATCHNS", "GRDQ"},
      {"LATCHSRE", "RGEQSD"}, {"LATCHNSRE", "REGQSD"}};

  std::unordered_map<std::string, std::string> lut_port_map_LATCH{
      {"G", "A[2]"}, {"Q", "A[1]"}, {"D", "A[0]"}};
  std::unordered_map<std::string, std::string> lut_port_map_LATCHN{
      {"G", "A[2]"}, {"D", "A[1]"}, {"Q", "A[0]"}};
  std::unordered_map<std::string, std::string> lut_port_map_LATCHR{
      {"G", "A[3]"}, {"R", "A[2]"}, {"Q", "A[1]"}, {"D", "A[0]"}};
  std::unordered_map<std::string, std::string> lut_port_map_LATCHS{
      {"G", "A[3]"}, {"R", "A[2]"}, {"Q", "A[1]"}, {"D", "A[0]"}};
  std::unordered_map<std::string, std::string> lut_port_map_LATCHNR{
      {"G", "A[3]"}, {"R", "A[2]"}, {"D", "A[1]"}, {"Q", "A[0]"}};
  std::unordered_map<std::string, std::string> lut_port_map_LATCHNS{
      {"G", "A[3]"}, {"R", "A[2]"}, {"D", "A[1]"}, {"Q", "A[0]"}};
  std::unordered_map<std::string, std::string> lut_port_map_LATCHSRE{
      {"R", "A[5]"}, {"G", "A[4]"}, {"E", "A[3]"},
      {"Q", "A[2]"}, {"S", "A[1]"}, {"D", "A[0]"}};
  std::unordered_map<std::string, std::string> lut_port_map_LATCHNSRE{
      {"R", "A[5]"}, {"E", "A[4]"}, {"G", "A[3]"},
      {"Q", "A[2]"}, {"S", "A[1]"}, {"D", "A[0]"}};

  std::unordered_map<std::string, std::unordered_map<std::string, std::string>>
      latch_lut_port_conversion{{"LATCH", lut_port_map_LATCH},
                                {"LATCHN", lut_port_map_LATCHN},
                                {"LATCHR", lut_port_map_LATCHR},
                                {"LATCHS", lut_port_map_LATCHS},
                                {"LATCHNR", lut_port_map_LATCHNR},
                                {"LATCHNS", lut_port_map_LATCHNS},
                                {"LATCHSRE", lut_port_map_LATCHSRE},
                                {"LATCHNSRE", lut_port_map_LATCHNSRE}};
  string mx = "179769313486231590772930519078902473361797697894230657273"
              "43008115773267580"
              "550096313270847732240753602112011387987139335765878976881"
              "44166224928474306"
              "394741243777678934248654852763022196012460941194530829520"
              "85005768838150682"
              "342462881473913110540827237163350510684586298239947245938"
              "47971630483535632"
              "9624224137215";
  std::map<char, string> hexDecoder = {
      {'0', "0000"}, {'1', "0001"}, {'2', "0010"}, {'3', "0011"}, {'4', "0100"},
      {'5', "0101"}, {'6', "0110"}, {'7', "0111"}, {'8', "1000"}, {'9', "1001"},
      {'A', "1010"}, {'B', "1011"}, {'C', "1100"}, {'D', "1101"}, {'E', "1110"},
      {'F', "1111"}, {'X', "xxxx"}, {'a', "1010"}, {'b', "1011"}, {'c', "1100"},
      {'d', "1101"}, {'e', "1110"}, {'f', "1111"}, {'x', "xxxx"}};
};