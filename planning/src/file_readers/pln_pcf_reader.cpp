#include "file_readers/pln_pcf_reader.h"
#include "file_readers/pln_Fio.h"

namespace pln {

using namespace std;
using fio::Fio;

bool PcfReader::read_pcf(const string& f) {
  uint16_t tr = ltrace();
  auto& ls = lout();
  if (tr >= 2) ls << "PcfReader::read_pcf( " << f << " )" << endl;

  assert(!f.empty());

  std::ifstream infile(f);
  if (!infile.is_open()) {
    flush_out(true);
    err_puts();
    lprintf2("[Error] PCF Reader: could not open the file %s\n", f.c_str());
    flush_out(true);
    return false;
  }

  string line;
  bool has_error = false;
  string set_io_cmd, user_pin, bump_or_ball_name, dash_mode, mode_name;
  string optional_dash_internal_pin, optional_internal_pin;

  vector<string> dat;
  string row_str;
  int row_num = -1;

  while (std::getline(infile, line)) {
    if (line.length() < 2) continue;
    const char* cline = str::trimFront(line.c_str());
    if (!cline || !*cline) continue;
    if (::strlen(cline) < 2) continue;
    if (cline[0] == '#') continue;

    dat.clear();
    row_str.clear();
    row_num = -1;
    bool split_ok = Fio::split_spa(line.c_str(), dat);
    if (split_ok && dat.size() > 2) {
      for (uint j = 1; j < dat.size() - 1; j++) {
        if (dat[j] == "-pt_row") {
          row_str = dat[j + 1];
          break;
        }
      }
      if (!row_str.empty()) {
        row_num = ::atoi(row_str.c_str());
      }
    }

    std::istringstream iss(line);
    // set_io USER_PPIN BUMP_PIN_NAME -mode MODE_NAME
    set_io_cmd.clear();
    user_pin.clear();
    bump_or_ball_name.clear();
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
      if (!(iss >> set_io_cmd >> user_pin >> bump_or_ball_name >> dash_mode >>
            mode_name >> optional_dash_internal_pin >> optional_internal_pin)) {
        has_error = true;
        break;  // error
      }
    } else {
      if (!(iss >> set_io_cmd >> user_pin >> bump_or_ball_name >> dash_mode >>
            mode_name)) {
        has_error = true;
        break;  // error
      }
    }

    if (tr >= 4) {
      ls << "    user_pin:" << user_pin << "  bump_or_ball_name:" << bump_or_ball_name
         << "  mode_name:" << mode_name << endl;
    }

    commands_.emplace_back();
    Cmd& last = commands_.back();
    vector<string>& cur_command = last.cmd_;

    cur_command.push_back(set_io_cmd);
    cur_command.push_back(user_pin);
    cur_command.push_back(bump_or_ball_name);
    cur_command.push_back(dash_mode);
    cur_command.push_back(mode_name);

    if (has_internal_pin) {
      cur_command.push_back(optional_dash_internal_pin);
      cur_command.push_back(optional_internal_pin);
    }

    if (row_num > 0) {
      cur_command.emplace_back("-pt_row");
      cur_command.emplace_back(std::to_string(row_num));
    }
  }

  if (tr >= 4) {
    lprintf("done  PcfReader::read_pcf().  commands_.size()= %zu  has_error:%i\n",
            commands_.size(), has_error);
  }
  if (tr >= 1 && has_error) {
    flush_out(true);
    err_puts();
    lprintf2("[Error] in PcfReader::read_pcf()\n");
    flush_out(true);
  }
  flush_out(false);

  return true;
}

} // namespace pln

