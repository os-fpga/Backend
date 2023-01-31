#include "cmd_line.h"

namespace pinc
{

  using namespace std;

  cmd_line::cmd_line(int argc, const char **argv)
  {
    assert(argc > 0 && argv);

    string s;
    bool clock_args = false;

    for (int i = 1; i < argc; ++i)
    {
      assert(argv[i]);
      s = argv[i];
      if (s == "set_clock_pin")
      {
        clock_args = true;
        continue;
      }
      if (s == "--")
      {
        clock_args = false;
        clock_args_ = clock_args_vec;
        clock_args_vec.clear();
      }
      if (clock_args)
        clock_args_vec.push_back(s);
      else if (s.size() < 2)
      {
        cout << "Warning: Not a valid flag  \"" << s << "\" discarding" << endl;
        continue;
      }
      else if ('-' == s[0])
      {
        if ('-' == s[1])
        { // param key
          string key = s;
          if (i + 1 < argc)
          {
            params_[key] = argv[i + 1];
            ++i;
          }
          else
            cout << "Warning: Key " << key << " did not get a value" << endl;
        }
        else
        { // flag
          flags_.insert(s);
        }
      }
      else
      { // param value
        cout << "Warning: No key for value " << s << endl;
      }
    }
  }

  void cmd_line::set_flag(const string &fl) { flags_.insert(fl); }

  void cmd_line::set_param_value(string &key, string &val) { params_[key] = val; }

  void cmd_line::print_options() const
  {
    cout << "Flags :\n";
    for (const auto &f : flags_)
      cout << "\t" << f << endl;
    cout << "Params :" << endl;

    // sort by name
    vector<pair<string, string>> V;
    V.reserve(params_.size());
    for (const auto &p : params_)
      V.emplace_back(p.first, p.second);

    std::sort(V.begin(), V.end());

    for (const auto &p : V)
      cout << '\t' << p.first << '\t' << p.second << endl;
  }
} // namespace pinc
