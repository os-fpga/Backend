#include "csv_reader.h"

#include <algorithm>

bool CsvReader::read_csv(const std::string &f) {
  std::ifstream infile(f);
  if (!infile.is_open()) {
    std::cerr << "ERROR: could not open the file " << f << endl;
    return false;
  }

  std::string line;
  while (std::getline(infile, line)) {
    if (!line.size()) continue;
    entries.push_back(vector<string>());
    entries.back().push_back(string(""));
    for (auto c : line) {
      if (isspace(c)) continue;
      if (c == ',') {
        entries.back().push_back(string(""));
      } else
        entries.back().back().push_back(c);
    }
  }
  std::vector<string> first_v = entries[0];
  auto result1 = std::find(first_v.begin(), first_v.end(), "port_name");
  vector<string>::iterator result2 =
      find(first_v.begin(), first_v.end(), "mapped_pin");
  int port_name_index = distance(first_v.begin(), result1);
  int mapped_pin_index = distance(first_v.begin(), result2);

  for (auto &v : entries) {
    string port_name = v[port_name_index];
    string mapped_pin = v[mapped_pin_index];
    port_map.insert(std::pair<std::string, string>(mapped_pin, port_name));
  }

  return true;
}
