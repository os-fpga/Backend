#include "pin_loc/pin_placer.h"
#include "file_readers/blif_reader.h"
#include "file_readers/pinc_Fio.h"
#include "file_readers/nlohmann3_11_2_json.h"
#include "util/cmd_line.h"
#include <filesystem>

namespace pln {

using namespace std;
using fio::Fio;

#define CERROR std::cerr << "[Error] "
#define OUT_ERROR lout() << "[Error] "

bool s_is_fabric_eblif(const string& fn) noexcept {
  size_t len = fn.length();
  if (len < 12 or len > 5000)
    return false;
  char buf[len + 2] = {};
  ::strcpy(buf, fn.c_str());

  // replace '/' and '.' by spaces to use space-based tokenizer
  for (char* p = buf; *p; p++) {
    if (*p == '/' or *p == '.')
      *p = ' ';
  }

  vector<string> W;
  Fio::split_spa(buf, W);
  if (W.size() < 2)
    return false;
  if (W.back() != "eblif")
    return false;

  const string& f = W[W.size() - 2];
  if (f.length() < 7)
    return false;
  CStr s = f.c_str();

  return s[0] == 'f' and s[1] == 'a' and s[2] == 'b' and
         s[3] == 'r' and s[4] == 'i' and s[5] == 'c' and s[6] == '_';
}

bool PinPlacer::read_design_ports() {
  uint16_t tr = ltrace();
  if (tr >= 4) {
    flush_out(true);
    lputs("read_design_ports() __ getting port info");
  } else if (tr >= 2) {
    flush_out(true);
  }

  user_design_inputs_.clear();
  user_design_outputs_.clear();

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
      if (cl_.get_param("--blif").length() > 1) {
        lprintf("\nWARNING: port info file %s does not exist => using blif\n",
                port_info_fn.c_str());
      } else {
        lprintf("\nWARNING: port info file %s does not exist => exiting\n",
                port_info_fn.c_str());
        flush_out(true);
        std::quick_exit(0);
      }
    }
  } else {
    if (tr >= 4) lprintf("port_info cmd option not specified => using blif\n");
  }

  if (json_ifs.is_open()) {
    if (tr >= 2) lprintf("... reading %s\n", port_info_fn.c_str());
    if (!read_port_info(json_ifs, raw_design_inputs_, raw_design_outputs_)) {
      CERROR    << " failed reading " << port_info_fn << endl;
      OUT_ERROR << " failed reading " << port_info_fn << endl;
      flush_out(true);
      std::quick_exit(0);
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
      flush_out(true);
      err_puts();
      CERROR << err_lookup("PORT_INFO_PARSE_ERROR") << endl;
      OUT_ERROR << err_lookup("PORT_INFO_PARSE_ERROR") << endl;
      flush_out(true);
      return false;
    }
    raw_design_inputs_ = rd_blif.get_inputs();
    raw_design_outputs_ = rd_blif.get_outputs();
    if (raw_design_inputs_.empty() and raw_design_outputs_.empty()) {
      if (tr >= 2) {
        flush_out(true);
        lprintf2("read_design_ports() FAILED : both user_design_inputs and user_design_outputs are empty");
      }
      err_puts();
      CERROR << err_lookup("PORT_INFO_PARSE_ERROR") << endl;
      err_puts();
      return false;
    }

    is_fabric_eblif_ = s_is_fabric_eblif(blif_fn);
  }

  size_t sz = raw_design_inputs_.size();
  user_design_inputs_.clear();
  user_design_inputs_.resize(sz);
  for (size_t i = 0; i < sz; i++)
    user_design_inputs_[i].set_udes_pin_name(raw_design_inputs_[i]);

  sz = raw_design_outputs_.size();
  user_design_outputs_.clear();
  user_design_outputs_.resize(sz);
  for (size_t i = 0; i < sz; i++)
    user_design_outputs_[i].set_udes_pin_name(raw_design_outputs_[i]);

  if (tr >= 3) {
    flush_out(tr >= 5);
    lprintf(
      "DONE read_design_ports()  #udes_inputs= %zu  #udes_outputs= %zu\n",
      raw_design_inputs_.size(), raw_design_outputs_.size());
    if (tr >= 4)
      lprintf("is_fabric_eblif_: %s\n", is_fabric_eblif_ ? "TRUE" : "FALSE");
  }

  if (tr >= 5) {
    flush_out(true);

    sz = user_design_inputs_.size();
    lprintf(" ---- dumping user_design_inputs_ (%zu) ----\n", sz);
    for (uint i = 0; i < sz; i++)
      lprintf("  inp-%u  %s\n", i, user_design_input(i).c_str());
    lprintf(" ----\n");

    sz = user_design_outputs_.size();
    lprintf(" ---- dumping user_design_outputs_ (%zu) ----\n", sz);
    for (uint i = 0; i < sz; i++)
      lprintf("  out-%u  %s\n", i, user_design_output(i).c_str());
    lprintf(" ----\n");

    lprintf(
      "DONE read_design_ports()  #udes_inputs= %zu  #udes_outputs= %zu\n",
      raw_design_inputs_.size(), raw_design_outputs_.size());
  }

  flush_out(true);
  return true;
}

bool PinPlacer::read_edits() {
  flush_out(false);
  uint16_t tr = ltrace();
  if (tr >= 4) lputs("\nread_edits() __ getting io_config .json");

  all_edits_.clear();
  ibufs_.clear();
  obufs_.clear();

  string edits_fn = cl_.get_param("--edits");
  if (edits_fn.empty()) {
    if (tr >= 4) lputs("[Info] edits cmd option is not specified");
    return false;
  }

  if (tr >= 4)
    lprintf("--edits file name : %s\n", edits_fn.c_str());

  flush_out(false);

  if (! Fio::regularFileExists(edits_fn)) {
    flush_out(true);
    lprintf("[Error] specified <io edits>.json file %s does not exist\n", edits_fn.c_str());
    err_puts();
    CERROR << "specified <io edits>.json file does not exist: " << edits_fn << endl;
    flush_out(true);
    return false;
  }
  if (! Fio::nonEmptyFileExists(edits_fn)) {
    flush_out(true);
    lprintf("[Error] specified <io edits>.json file %s is empty or not accessible\n",
            edits_fn.c_str());
    err_puts();
    CERROR << "specified <io edits>.json file is empty or not accessible: " << edits_fn << endl;
    flush_out(true);
    return false;
  }

  ifstream edits_ifs(edits_fn);

  if (edits_ifs.is_open()) {
    if (tr >= 4) lprintf("... reading %s\n", edits_fn.c_str());
    if (not read_edit_info(edits_ifs)) {
      flush_out(true);
      err_puts();
      CERROR    << " failed reading " << edits_fn << endl;
      err_puts();
      OUT_ERROR << " failed reading " << edits_fn << endl;
      flush_out(true);
      return false;
    }
  }
  else {
    flush_out(true);
    OUT_ERROR << "  could not open <io edits>.json file : " << edits_fn << endl;
    err_puts();
    CERROR    << "  could not open <io edits>.json file : " << edits_fn << endl;
    err_puts();
    flush_out(true);
    return false;
  }

  bool check_ok = check_edit_info();
  if (!check_ok) {
    flush_out(true);
    err_puts();
    lprintf2(" [WARNING] NOTE: netlist modifications (config.json) have overlapping pins\n");
    // incrCriticalWarnings();
    err_puts();
    flush_out(true);
  }

  flush_out(true);
  if (tr >= 4) {
    lprintf("DONE read_edits()  #ibufs= %zu  #obufs= %zu\n",
            ibufs_.size(), obufs_.size());
  }

  flush_out(false);
  return bool(ibufs_.size()) or bool(obufs_.size());
}

static bool read_json_ports(const nlohmann::ordered_json& from,
                            vector<PinPlacer::PortInfo>& ports) {
  ports.clear();

  nlohmann::ordered_json portsObj = from["ports"];
  if (!portsObj.is_array()) return false;

  uint16_t tr = ltrace();
  auto& ls = lout();

  if (tr >= 4) ls << " .... portsObj.size() = " << portsObj.size() << endl;

  string s;
  for (auto I = portsObj.cbegin(); I != portsObj.cend(); ++I) {
    const nlohmann::ordered_json& obj = *I;
    if (obj.is_array()) {
      ls << " [Warning] unexpected json array..." << endl;
      continue;
    }

    // 1. check size and presence of 'name'
    if (tr >= 8) ls << " ...... obj.size() " << obj.size() << endl;
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
    if (tr >= 8) ls << " ........ last.name_ " << last.name_ << endl;

    // 3. read 'direction'
    if (obj.contains("direction")) {
      s = str::s2lower(obj["direction"]);
      if (s == "input")
        last.dir_ = PinPlacer::PortDir::Input;
      else if (s == "output")
        last.dir_ = PinPlacer::PortDir::Output;
    }

    // 4. read 'type'
    if (obj.contains("type")) {
      s = str::s2lower(obj["type"]);
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
        if (tr >= 9) ls << " ........ has range..." << endl;
        const auto& lsb = rangeObj["lsb"];
        const auto& msb = rangeObj["msb"];
        if (lsb.is_number_integer() && msb.is_number_integer()) {
          int l = lsb.get<int>();
          int m = msb.get<int>();
          if (l >= 0 && m >= 0) {
            if (tr >= 8)
              ls << " ........ has valid range  l= " << l << "  m= " << m
                 << endl;
            last.range_.set(l, m);
          }
        }
      }
    }

    // 6. read 'define_order'
    if (obj.contains("define_order")) {
      s = str::s2lower(obj["define_order"]);
      if (s == "lsb_to_msb")
        last.order_ = PinPlacer::Order::LSB_to_MSB;
      else if (s == "msb_to_lsb")
        last.order_ = PinPlacer::Order::MSB_to_LSB;
      if (tr >= 8 && last.hasOrder())
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
    CERROR << "pin_c: Failed json input stream reading" << endl;
    lputs();
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
    CERROR << "pin_c: json: rootObj.size() == 0" << endl;
    return false;
  }

  vector<PortInfo> ports;
  if (rootObj.is_array())
    read_json_ports(rootObj[0], ports);
  else
    read_json_ports(rootObj, ports);

  if (tr >= 3) ls << "pin_c json: got ports.size()= " << ports.size() << endl;

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

  if (tr >= 4) {
    ls << " got " << inputs.size() << " inputs and " << outputs.size()
       << " outputs" << endl;
    if (tr >= 5) {
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

static bool s_read_json_items(const nlohmann::ordered_json& from,
                              vector<PinPlacer::EditItem>& items) {
  flush_out(false);
  items.clear();

  nlohmann::ordered_json itemsObj = from["instances"];

  if (!itemsObj.is_array()) {
    flush_out(true);
    lputs("[Error] pin_c: read_edits: json schema error");
    CERROR << "pin_c: read_edits: json schema error" << endl;
    flush_out(true);
    return false;
  }

  uint16_t tr = ltrace();
  auto& ls = lout();

  if (tr >= 4) ls << " .... itemsObj.size() = " << itemsObj.size() << endl;

  for (auto I = itemsObj.cbegin(); I != itemsObj.cend(); ++I) {
    const nlohmann::ordered_json& obj = *I;
    if (obj.is_array()) {
      ls << " [Warning] unexpected json array..." << endl;
      continue;
    }

    // 1. check size and presence of 'name'
    if (tr >= 8) ls << " ...... obj.size() " << obj.size() << endl;
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

    items.emplace_back();

    // 2. read 'name'
    PinPlacer::EditItem& last = items.back();
    last.name_ = obj["name"];
    if (tr >= 8) ls << " ........ last.name_ " << last.name_ << endl;

    // 3. read 'module'
    if (obj.contains("module")) {
      last.module_ = obj["module"];
    }

    // 3. read 'location'
    if (obj.contains("location")) {
      last.location_ = obj["location"];
    }

    // 4. read 'mode'
    if (obj.contains("properties")) {
      const auto& propObj = obj["properties"];
      if (propObj.contains("mode"))
        last.mode_ = propObj["mode"];
    }

    // 5. read oldPin_/newPin_
    if (obj.contains("connectivity")) {
      const auto& propObj = obj["connectivity"];
      if (propObj.contains("I"))
        last.newPin_ = propObj["I"];
      if (propObj.contains("O"))
        last.oldPin_ = propObj["O"];
      if (last.isInput()) // swap pins if I_BUF
        std::swap(last.newPin_, last.oldPin_);
    }
  }

  return not items.empty();
}

bool PinPlacer::read_edit_info(std::ifstream& ifs) {
  flush_out(false);
  all_edits_.clear();
  ibufs_.clear();
  obufs_.clear();

  if (!ifs.is_open()) return false;

  uint16_t tr = ltrace();
  auto& ls = lout();

  nlohmann::ordered_json rootObj;

  try {
    ifs >> rootObj;
  } catch (...) {
    flush_out(true);
    ls << "[Error] pin_c: read_edits: Failed json input stream reading" << endl;
    err_puts();
    CERROR << "pin_c: read_edits: Failed json input stream reading" << endl;
    err_puts();
    flush_out(true);
    return false;
  }

  size_t root_sz = rootObj.size();
  if (tr >= 4) {
    ls << "json: rootObj.size() = " << root_sz
       << "  rootObj.is_array() : " << std::boolalpha << rootObj.is_array()
       << endl;
  }

  flush_out(false);

  if (root_sz == 0) {
    ls << "[Error] pin_c: json: rootObj.size() == 0" << endl;
    CERROR << "pin_c: json: rootObj.size() == 0" << endl;
    return false;
  }
  if (rootObj.is_array()) {
    ls << "[Error] pin_c: read_edits: json schema error" << endl;
    CERROR << "pin_c: read_edits: json schema error" << endl;
    lputs();
    return false;
  }

  bool ok = false;
  try {

    ok = s_read_json_items(rootObj, all_edits_);

  } catch (...) {
    flush_out(true);
    ls << "[Error] pin_c: read_edits: json schema exception" << endl;
    CERROR << "pin_c: read_edits: json schema exception" << endl;
    flush_out(true);
    all_edits_.clear();
    ibufs_.clear();
    obufs_.clear();
    return false;
  }

  if (tr >= 4) {
    lprintf("pin_c json: got all_edits_.size()= %zu  ok:%i\n",
             all_edits_.size(), ok);
  }

  flush_out(false);

  if (ok) {
    size_t sz = all_edits_.size();
    ibufs_.reserve(sz);
    obufs_.reserve(sz);
    for (size_t i = 0; i < sz; i++) {
      EditItem& item = all_edits_[i];
      if (item.isInput())
        ibufs_.push_back(&item);
      else if (item.isOutput())
        obufs_.push_back(&item);
    }
  } else {
    all_edits_.clear();
    ibufs_.clear();
    obufs_.clear();
  }

  size_t num_inp_out = ibufs_.size() + obufs_.size();
  if (num_inp_out == 0) {
    all_edits_.clear();
    return false;
  }

  // sort for binary search
  ibufs_SortedByOld_ = ibufs_;
  obufs_SortedByOld_ = obufs_;
  std::sort(ibufs_SortedByOld_.begin(), ibufs_SortedByOld_.end(), EditItem::CmpOldPin());
  std::sort(obufs_SortedByOld_.begin(), obufs_SortedByOld_.end(), EditItem::CmpOldPin());

  // link obuf chains:
  for (EditItem* e : obufs_) {
    EditItem& ed = *e;
    ed.parent_ = findObufByOldPin(ed.newPin_);
  }
  // link ibuf chains:
  for (EditItem* e : ibufs_) {
    EditItem& ed = *e;
    ed.parent_ = findIbufByOldPin(ed.newPin_);
  }

  flush_out(false);

  if (tr >= 4) {
    uint esz = all_edits_.size();
    lprintf("  all_edits_.size()= %u   #input= %zu   #output= %zu\n",
            esz, ibufs_.size(), obufs_.size());
    if (tr >= 5) {
      lprintf("  ==== dumping all_edits ====\n");
      for (uint i = 0; i < esz; i++) {
        const EditItem& ed = all_edits_[i];
        lprintf(
            "    |%u|  name_:%s  loc_:%s  mode_:%s  input:%i  output:%i  oldPin:%s  newPin:%s\n",
            i+1, ed.cname(), ed.location_.c_str(), ed.mode_.c_str(),
            ed.isInput(), ed.isOutput(), ed.oldPin_.c_str(), ed.newPin_.c_str());
      }
      lprintf("  ---- obufs (%zu) ----\n", obufs_.size());
      for (const EditItem* e : obufs_SortedByOld_) {
        const EditItem& ed = *e;
        lprintf("   name_:%s  loc_:%s  input:%i  output:%i  oldPin:%s  newPin:%s  isRoot:%i\n",
                ed.cname(), ed.location_.c_str(), ed.isInput(), ed.isOutput(),
                ed.oldPin_.c_str(), ed.newPin_.c_str(), ed.isRoot());
      }
      lprintf("  ---- ibufs (%zu) ----\n", ibufs_.size());
      for (const EditItem* e : ibufs_SortedByOld_) {
        const EditItem& ed = *e;
        lprintf("   name_:%s  loc_:%s  input:%i  output:%i  oldPin:%s  newPin:%s  isRoot:%i\n",
                ed.cname(), ed.location_.c_str(), ed.isInput(), ed.isOutput(),
                ed.oldPin_.c_str(), ed.newPin_.c_str(), ed.isRoot());
      }
      lputs("  ====");
    }
  }

  flush_out(false);
  return true;
}

bool PinPlacer::check_edit_info() const {
  flush_out(false);
  if (all_edits_.empty())
    return true;

  vector<const EditItem*> obuf_ov, ibuf_ov;

  if (obufs_SortedByOld_.size() > 1) {
    uint sz = obufs_SortedByOld_.size();
    for (uint i = 0; i < sz; i++) {
      const EditItem& a = *obufs_SortedByOld_[i];
      for (uint j = i + 1; j < sz; j++) {
        const EditItem& b = *obufs_SortedByOld_[j];
        if (a.newPin_ == b.newPin_) {
          obuf_ov.push_back(&a);
          obuf_ov.push_back(&b);
        }
      }
    }
  }

  if (ibufs_SortedByOld_.size() > 1) {
    uint sz = ibufs_SortedByOld_.size();
    for (uint i = 0; i < sz; i++) {
      const EditItem& a = *ibufs_SortedByOld_[i];
      for (uint j = i + 1; j < sz; j++) {
        const EditItem& b = *ibufs_SortedByOld_[j];
        if (a.newPin_ == b.newPin_) {
          ibuf_ov.push_back(&a);
          ibuf_ov.push_back(&b);
        }
      }
    }
  }

  if (! obuf_ov.empty()) {
    flush_out(true);
    err_puts();
    lprintf2("    [CRITICAL_WARNING] netlist modifications (config.json) have overlapping OBUF pins (%zu)\n", obuf_ov.size());
    err_puts();
    lprintf2("    [CRITICAL_WARNING] the following OBUF pins are overlapping:\n");
    err_puts();
    for (const EditItem* ep : obuf_ov) {
      const EditItem& ed = *ep;
      lprintf2(" [CRITICAL_WARNING] [OBUF-OVERLAP]   name_:%s  input:%i  output:%i  oldPin:%s  newPin:%s\n",
               ed.cname(), ed.isInput(), ed.isOutput(),
               ed.oldPin_.c_str(), ed.newPin_.c_str());
    }

    flush_out(true);
  }

  if (! ibuf_ov.empty()) {
    flush_out(true);
    err_puts();
    lprintf2("    [CRITICAL_WARNING] netlist modifications (config.json) have overlapping IBUF pins (%zu)\n", ibuf_ov.size());
    err_puts();
    lprintf2("    [CRITICAL_WARNING] the following IBUF pins are overlapping:\n");
    err_puts();
    for (const EditItem* ep : ibuf_ov) {
      const EditItem& ed = *ep;
      lprintf2(" [CRITICAL_WARNING] [IBUF-OVERLAP]   name_:%s  input:%i  output:%i  oldPin:%s  newPin:%s\n",
               ed.cname(), ed.isInput(), ed.isOutput(),
               ed.oldPin_.c_str(), ed.newPin_.c_str());
    }

    flush_out(true);
  }

  flush_out(false);
  return obuf_ov.empty() and ibuf_ov.empty();
}

PinPlacer::EditItem* PinPlacer::findObufByOldPin(const string& old_pin) const noexcept {
  if (old_pin.empty())
    return nullptr;
  EditItem x;
  x.oldPin_ = old_pin;
  auto I = std::lower_bound(obufs_SortedByOld_.begin(), obufs_SortedByOld_.end(), &x, EditItem::CmpOldPin());
  if (I == obufs_SortedByOld_.end())
    return nullptr;
  if ((*I)->oldPin_ == old_pin)
    return *I;
  return nullptr;
}

PinPlacer::EditItem* PinPlacer::findIbufByOldPin(const string& old_pin) const noexcept {
  if (old_pin.empty())
    return nullptr;
  EditItem x;
  x.oldPin_ = old_pin;
  auto I = std::lower_bound(ibufs_SortedByOld_.begin(), ibufs_SortedByOld_.end(), &x, EditItem::CmpOldPin());
  if (I == ibufs_SortedByOld_.end())
    return nullptr;
  if ((*I)->oldPin_ == old_pin)
    return *I;
  return nullptr;
}

string PinPlacer::translatePinName(const string& pinName, bool is_input) const noexcept {
  assert(not pinName.empty());
  if (is_input) {
    EditItem* buf = findIbufByOldPin(pinName);
    if (buf) {
      assert(not buf->newPin_.empty());
      EditItem* root = buf->getRoot();
      assert(root);
      assert(not root->newPin_.empty());
      return root->newPin_;
    } else {
      return pinName;
    }
  }
  else {
    EditItem* buf = findObufByOldPin(pinName);
    if (buf) {
      assert(not buf->newPin_.empty());
      EditItem* root = buf->getRoot();
      assert(root);
      assert(not root->newPin_.empty());
      return root->newPin_;
    } else {
      return pinName;
    }
  }
}

}

