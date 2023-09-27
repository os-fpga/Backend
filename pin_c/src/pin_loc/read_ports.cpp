#include "pin_loc/pin_placer.h"
#include "file_readers/blif_reader.h"
#include "file_readers/Fio.h"
#include "file_readers/nlohmann3_11_2_json.h"
#include "util/cmd_line.h"
#include <filesystem>

namespace pinc {

using namespace std;
using fio::Fio;

#define CERROR std::cerr << "[Error] "
#define OUT_ERROR std::cout << "[Error] "

bool PinPlacer::read_design_ports() {
  uint16_t tr = ltrace();
  if (tr >= 2) {
    lputs("\nread_design_ports() __ getting port info .json");
  }

  string port_info_fn = cl_.get_param("--port_info");
  string str_path;
  ifstream json_ifs;

  if (port_info_fn.length() > 0) {
    str_path = port_info_fn;
    if (Fio::regularFileExists(str_path)) {
      json_ifs.open(port_info_fn);
      if (!json_ifs.is_open())
        lprintf("\nWARNING: could not open port info file %s => using blif\n",
                port_info_fn.c_str());
    } else {
      lprintf("\nWARNING: port info file %s does not exist => using blif\n",
              port_info_fn.c_str());
    }
  } else {
    if (tr >= 1) lprintf("port_info cmd option not specified => using blif\n");
  }

  if (json_ifs.is_open()) {
    if (tr >= 2) lprintf("... reading %s\n", port_info_fn.c_str());
    if (!read_port_info(json_ifs, user_design_inputs_, user_design_outputs_)) {
      CERROR    << " failed reading " << port_info_fn << endl;
      OUT_ERROR << " failed reading " << port_info_fn << endl;
      return false;
    }
    json_ifs.close();
  } else {
    string blif_fn = cl_.get_param("--blif");
    if (tr >= 2) lprintf("... reading %s\n", blif_fn.c_str());
    str_path = blif_fn;
    if (not Fio::regularFileExists(str_path)) {
      lprintf("\npin_c WARNING: blif file %s does not exist\n", str_path.c_str());
      std::filesystem::path fs_path{str_path}, eblif_ext{"eblif"};
      fs_path.replace_extension(eblif_ext);
      string eblif_fn = fs_path.string();
      if (Fio::regularFileExists(fs_path)) {
        blif_fn = eblif_fn;
        str_path = blif_fn;
        lprintf("\npin_c INFO: using eblif file %s\n", eblif_fn.c_str());
      } else {
        lprintf("pin_c WARNING: eblif file %s does not exist\n", eblif_fn.c_str());
      }
    }
    BlifReader rd_blif;
    if (!rd_blif.read_blif(blif_fn)) {
      CERROR << err_lookup("PORT_INFO_PARSE_ERROR") << endl;
      return false;
    }
    user_design_inputs_ = rd_blif.get_inputs();
    user_design_outputs_ = rd_blif.get_outputs();
    if (user_design_inputs_.empty() and user_design_outputs_.empty()) {
      if (tr >= 2)
        lputs("\nread_design_ports() FAILED : both user_design_inputs_ and user_design_outputs_ are empty");
      CERROR << err_lookup("PORT_INFO_PARSE_ERROR") << endl;
      return false;
    }
  }

  if (tr >= 2) {
    lprintf(
        "DONE read_design_ports()  user_design_inputs_.size()= %zu  "
        "user_design_outputs_.size()= %zu\n",
        user_design_inputs_.size(), user_design_outputs_.size());
  }

  return true;
}

static bool read_json_ports(const nlohmann::ordered_json& from,
                            vector<PinPlacer::PortInfo>& ports) {
  ports.clear();

  nlohmann::ordered_json portsObj = from["ports"];
  if (!portsObj.is_array()) return false;

  uint16_t tr = ltrace();
  auto& ls = lout();

  if (tr >= 3) ls << " .... portsObj.size() = " << portsObj.size() << endl;

  string s;
  for (auto I = portsObj.cbegin(); I != portsObj.cend(); ++I) {
    const nlohmann::ordered_json& obj = *I;
    if (obj.is_array()) {
      ls << " [Warning] unexpected json array..." << endl;
      continue;
    }

    // 1. check size and presence of 'name'
    if (tr >= 7) ls << " ...... obj.size() " << obj.size() << endl;
    if (obj.size() == 0) {
      ls << " [Warning] unexpected json object size..." << endl;
      continue;
    }
    if (not obj.contains("name")) {
      ls << " [Warning] expected obj.name string..." << endl;
      continue;
    }
    if (not obj["name"].is_string()) {
      ls << " [Warning] expected obj.name string..." << endl;
      continue;
    }

    ports.emplace_back();

    // 2. read 'name'
    PinPlacer::PortInfo& last = ports.back();
    last.name_ = obj["name"];
    if (tr >= 7) ls << " ........ last.name_ " << last.name_ << endl;

    // 3. read 'direction'
    if (obj.contains("direction")) {
      s = str::sToLower(obj["direction"]);
      if (s == "input")
        last.dir_ = PinPlacer::PortDir::Input;
      else if (s == "output")
        last.dir_ = PinPlacer::PortDir::Output;
    }

    // 4. read 'type'
    if (obj.contains("type")) {
      s = str::sToLower(obj["type"]);
      if (s == "wire")
        last.type_ = PinPlacer::PortType::Wire;
      else if (s == "reg")
        last.type_ = PinPlacer::PortType::Reg;
    }

    // 5. read 'range'
    if (obj.contains("range")) {
      const auto& rangeObj = obj["range"];
      if (rangeObj.size() == 2 && rangeObj.contains("lsb") &&
          rangeObj.contains("msb")) {
        if (tr >= 8) ls << " ........ has range..." << endl;
        const auto& lsb = rangeObj["lsb"];
        const auto& msb = rangeObj["msb"];
        if (lsb.is_number_integer() && msb.is_number_integer()) {
          int l = lsb.get<int>();
          int m = msb.get<int>();
          if (l >= 0 && m >= 0) {
            if (tr >= 7)
              ls << " ........ has valid range  l= " << l << "  m= " << m
                 << endl;
            last.range_.set(l, m);
          }
        }
      }
    }

    // 6. read 'define_order'
    if (obj.contains("define_order")) {
      s = str::sToLower(obj["define_order"]);
      if (s == "lsb_to_msb")
        last.order_ = PinPlacer::Order::LSB_to_MSB;
      else if (s == "msb_to_lsb")
        last.order_ = PinPlacer::Order::MSB_to_LSB;
      if (tr >= 7 && last.hasOrder())
        ls << " ........ has define_order: " << s << endl;
    }
  }

  return true;
}

// static
bool PinPlacer::read_port_info(std::ifstream& ifs,
                               vector<string>& inputs,
                               vector<string>& outputs) {
  inputs.clear();
  outputs.clear();
  if (!ifs.is_open()) return false;

  uint16_t tr = ltrace();
  auto& ls = lout();

  nlohmann::ordered_json rootObj;

  try {
    ifs >> rootObj;
  } catch (...) {
    ls << "[Error] pin_c: Failed json input stream reading" << endl;
    CERROR << "[Error] pin_c: Failed json input stream reading" << endl;
    return false;
  }

  size_t root_sz = rootObj.size();
  if (tr >= 2) {
    ls << "json: rootObj.size() = " << root_sz
       << "  rootObj.is_array() : " << std::boolalpha << rootObj.is_array()
       << endl;
  }

  if (root_sz == 0) {
    ls << "[Error] pin_c: json: rootObj.size() == 0" << endl;
    CERROR << "[Error] pin_c: json: rootObj.size() == 0" << endl;
    return false;
  }

  vector<PortInfo> ports;
  if (rootObj.is_array())
    read_json_ports(rootObj[0], ports);
  else
    read_json_ports(rootObj, ports);

  if (tr >= 2) ls << "json: got ports.size()= " << ports.size() << endl;

  if (ports.empty()) return false;

  uint num_inp = 0, num_out = 0;
  for (const PortInfo& p : ports) {
    if (p.isInput())
      num_inp++;
    else if (p.isOutput())
      num_out++;
  }
  inputs.reserve(num_inp);
  outputs.reserve(num_out);

  for (const PortInfo& p : ports) {
    if (p.isInput()) {
      if (p.name_.length())
        inputs.push_back(p.name_);
      else
        ls << "\nWARNING: got empty name in port_info" << endl;
    } else if (p.isOutput()) {
      if (p.name_.length())
        outputs.push_back(p.name_);
      else
        ls << "\nWARNING: got empty name in port_info" << endl;
    }
  }

  if (tr >= 2) {
    ls << " got " << inputs.size() << " inputs and " << outputs.size()
       << " outputs" << endl;
    if (tr >= 4) {
      lprintf("\n ---- inputs(%zu): ---- \n", inputs.size());
      for (size_t i = 0; i < inputs.size(); i++)
        lprintf("     in  %s\n", inputs[i].c_str());
      lprintf("\n ---- outputs(%zu): ---- \n", outputs.size());
      for (size_t i = 0; i < outputs.size(); i++)
        lprintf("    out  %s\n", outputs[i].c_str());
      lputs();
    }
  }

  return true;
}

}

