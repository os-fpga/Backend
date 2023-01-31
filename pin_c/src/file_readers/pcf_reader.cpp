#include "pcf_reader.h"

#include "pinc_log.h"

using namespace std;

namespace pinc {

bool PcfReader::read_pcf(const string& f) {
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2) ls << "PcfReader::read_pcf( " << f << " )" << endl;

  assert(!f.empty());

  std::ifstream infile(f);
  if (!infile.is_open()) {
    cerr << "ERROR: could not open the file " << f << endl;
    ls << "ERROR: could not open the file " << f << endl;
    return false;
  }

  string line;
  bool has_error = false;
  string set_io_cmd, user_pin, bump_pin, dash_mode, mode_name;
  string optional_dash_internal_pin, optional_internal_pin;

  while (std::getline(infile, line)) {
    std::istringstream iss(line);
    // set_io USER_PPIN BUMP_PIN_NAME -mode MODE_NAME
    set_io_cmd.clear();
    user_pin.clear();
    bump_pin.clear();
    dash_mode.clear();
    mode_name.clear();
    optional_dash_internal_pin.clear();
    optional_internal_pin.clear();

    bool has_internal_pin = false;
    if (line.find("-internal_pin") != string::npos) {
      has_internal_pin = true;
    }

    if (tr >= 4) ls << "(pcf line) " << line << endl;

    if (has_internal_pin) {
      if (!(iss >> set_io_cmd >> user_pin >> bump_pin >> dash_mode >>
            mode_name >> optional_dash_internal_pin >> optional_internal_pin)) {
        has_error = true;
        break;  // error
      }
    } else {
      if (!(iss >> set_io_cmd >> user_pin >> bump_pin >> dash_mode >>
            mode_name)) {
        has_error = true;
        break;  // error
      }
    }

    if (tr >= 4) {
      ls << "    user_pin:" << user_pin << "  bump_pin:" << bump_pin
         << "  mode_name:" << mode_name << endl;
    }

    commands_.emplace_back();
    vector<string>& cur_command = commands_.back();

    cur_command.push_back(set_io_cmd);
    cur_command.push_back(user_pin);
    cur_command.push_back(bump_pin);
    cur_command.push_back(dash_mode);
    cur_command.push_back(mode_name);

    if (has_internal_pin) {
      cur_command.push_back(optional_dash_internal_pin);
      cur_command.push_back(optional_internal_pin);
    }
  }

  if (tr >= 2) {
    ls << "done  PcfReader::read_pcf().  commands_.size()= " << commands_.size()
       << "  has_error= " << boolalpha << has_error << endl;
  }

  return true;
}

bool PcfReader::read_os_pcf(const string& f) {
  std::ifstream infile(f);
  if (!infile.is_open()) {
    cerr << "ERROR: could not open the file " << f << endl;
    return false;
  }
  string line;
  while (std::getline(infile, line)) {
    std::istringstream iss(line);
    string a, b, c;
    if (!(iss >> a >> b >> c)) {
      break;
    }  // error
    commands_.push_back(vector<string>());
    commands_.back().push_back(a);
    commands_.back().push_back(b);
    commands_.back().push_back(c);
    // pcf_pin_map.insert(std::pair<string, string>(b, c));
  }
  return true;
}

}  // namespace pinc
