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
public:
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
  void flatten_latch(std::string &latch, std::istream &ifs, std::ostream &ofs) {
    // --
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
      if (tokens.size() < 4) {
        ofs << ln << "\n";
        continue;
      }
      if (tokens[0] == ".subckt") {
        std::string name = tokens[1];
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.find("dff") != std::string::npos ||
            name == std::string("adder_carry")) {
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
            // Implement a latch using a lut (if possible : well written from rs
            // yosys)
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
              // bool flipped = false;
              int w_pos = -1;
              // const char *value =
              //     std::getenv("FLIP_TRANSFORMED_EBLIF_INPUT_ORDER");
              // if (value) {
              //   std::cout << "FLIP_TRANSFORMED_EBLIF_INPUT_ORDER is set"
              //             << std::endl;
              //   w_pos = sz - 1 - idx;
              // } else {
              w_pos = idx;
              // }
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