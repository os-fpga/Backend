#pragma once
/**
 * @file reconstruct_utils.h
 * @author Manadher Kharroubi (manadher@gmail.com)
 * @brief
 * @version 0.1
 * @date 2024-01-
 *
 * @copyright Copyright (c) 2024
*/
#include "boost/multiprecision/cpp_int.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream> 
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <set>

using namespace boost::multiprecision;
using namespace std;
std::vector<std::string> split_on_space(const std::string &line) {
  std::vector<std::string> tokens;
  std::stringstream ss(line);
  std::string token;
  while (ss >> token) {
    tokens.push_back(token);
  }
  return tokens;
}
