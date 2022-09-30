#include "pcf_reader.h"

bool PcfReader::read_pcf(const std::string &f) {
  std::ifstream infile(f);
  if (!infile.is_open()) {
    std::cerr << "ERROR: cound not open the file " << f << endl;
    return false;
  }
  std::string line;
  while (std::getline(infile, line)) {
    std::istringstream iss(line);
    // set_io USER_PPIN BUMP_PIN_NAME -mode MODE_NAME
    string set_io_command, user_pin, bump_pin, dash_mode, mode_name;
    if (!(iss >> set_io_command >> user_pin >> bump_pin >> dash_mode >>
          mode_name)) {
      break;
    } // error
    commands.push_back(vector<string>());
    commands.back().push_back(set_io_command);
    commands.back().push_back(user_pin);
    commands.back().push_back(bump_pin);
    commands.back().push_back(dash_mode);
    commands.back().push_back(mode_name);
    // pcf_pin_map.insert(std::pair<std::string, string>(b, c));
  }
  return true;
}

bool PcfReader::read_os_pcf(const std::string &f) {
  std::ifstream infile(f);
  if (!infile.is_open()) {
    std::cerr << "ERROR: cound not open the file " << f << endl;
    return false;
  }
  std::string line;
  while (std::getline(infile, line)) {
    std::istringstream iss(line);
    string a, b, c;
    if (!(iss >> a >> b >> c)) {
      break;
    } // error
    commands.push_back(vector<string>());
    commands.back().push_back(a);
    commands.back().push_back(b);
    commands.back().push_back(c);
    // pcf_pin_map.insert(std::pair<std::string, string>(b, c));
  }
  return true;
}
