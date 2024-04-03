#include "util/cmd_line.h"

namespace pln {

using namespace std;

cmd_line::cmd_line(int argc, const char** argv) {
  assert(argc > 0 && argv);

  bool needVal = false;
  string key, s;

  for (int i = 1; i < argc; ++i) {
    assert(argv[i]);
    s = argv[i];
    if (s.size() < 2) {
      cout << "Warning: Not a valid flag  \"" << s << "\" discarding" << endl;
      continue;
    }
    if ('-' == s[0]) {
      if ('-' == s[1]) {  // param key
        if (needVal)
          cout << "Warning: Key " << key << " did not get a value" << endl;
        needVal = true;
        key = s;
      } else {  // flag
        flags_.insert(s);
        if (needVal)
          cout << "Warning: Key " << key << " did not get a value" << endl;
        needVal = false;
      }
    } else {  // param value
      if (needVal) {
        params_[key] = s;
        needVal = false;
      } else {
        cout << "Warning: No key for value " << s << endl;
      }
    }
  }
}

void cmd_line::set_flag(const string& fl) { flags_.insert(fl); }

void cmd_line::set_param_value(string& key, string& val) { params_[key] = val; }

void cmd_line::print_options() const {
  cout << "Flags :\n";
  for (const auto& f : flags_) cout << "\t" << f << endl;
  cout << "Params :" << endl;

  // sort by name
  vector<pair<string, string>> V;
  V.reserve(params_.size());
  for (const auto& p : params_) V.emplace_back(p.first, p.second);

  std::sort(V.begin(), V.end());

  for (const auto& p : V) cout << '\t' << p.first << '\t' << p.second << endl;
}

}  // namespace pln
