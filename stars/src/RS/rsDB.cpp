#include "rsDB.h"
#include <map>
#include <set>

namespace rsbe {

using std::cout;
using std::endl;
using std::string;
using std::stringstream;
using std::ostream;
using namespace pinc;

///@brief Returns the name of a unique unconnected net
static string create_unconn_net(size_t& unconn_count) noexcept {
  // We increment unconn_count by reference so each
  // call generates a unique name
  return str::concat(sta_file_writer::unconn_prefix, std::to_string(unconn_count++));
}

/**
 * @brief Pretty-Prints a verilog port to the given output stream
 *
 * Handles special cases like multi-bit and disconnected ports
 */
static void print_verilog_port(ostream& os,
                        size_t& unconn_count,
                        const string& port_name,
                        const std::vector<string>& nets,
                        PortType type,
                        int depth,
                        struct t_analysis_opts& opts) {
  auto unconn_inp_name = [&]() {
    switch (opts.post_synth_netlist_unconn_input_handling) {
      case e_post_synth_netlist_unconn_handling::GND:
        return string("1'b0");
      case e_post_synth_netlist_unconn_handling::VCC:
        return string("1'b1");
      case e_post_synth_netlist_unconn_handling::NETS:
        return create_unconn_net(unconn_count);
      case e_post_synth_netlist_unconn_handling::UNCONNECTED:
      default:
        return string("1'bx");
    }
  };

  auto unconn_out_name = [&]() {
    switch (opts.post_synth_netlist_unconn_output_handling) {
      case e_post_synth_netlist_unconn_handling::NETS:
        return create_unconn_net(unconn_count);
      case e_post_synth_netlist_unconn_handling::UNCONNECTED:
      default:
        return string();
    }
  };

  // Pins
  if (nets.size() == 1) {
    // Port name
    os << indent(depth) << "." << port_name << "(";

    // Single-bit port
    if (nets[0].empty()) {
      // Disconnected
      if (type == PortType::INPUT || type == PortType::CLOCK) {
        os << unconn_inp_name();
      } else {
        assert(type == PortType::OUTPUT);
        os << unconn_out_name();
      }
    } else {
      // Connected
      os << escape_verilog_identifier(nets[0]);
    }
    os << ")";
  } else {
    // Check if all pins are unconnected
    bool all_unconnected = true;
    for (size_t i = 0; i < nets.size(); ++i) {
      if (!nets[i].empty()) {
        all_unconnected = false;
        break;
      }
    }

    // A multi-bit port, we explicitly concat the single-bit nets to build the
    // port, taking care to print MSB on left and LSB on right
    if (all_unconnected && type == PortType::OUTPUT &&
        opts.post_synth_netlist_unconn_output_handling == e_post_synth_netlist_unconn_handling::UNCONNECTED) {
      // Empty connection
      for (int ipin = (int)nets.size() - 1; ipin >= 0; --ipin) {  // Reverse order to match endianess
        // Port name
        os << indent(depth) << "." << port_name << "[" << ipin << "]"
           << "(";
        if (type == PortType::INPUT || type == PortType::CLOCK) {
          os << unconn_inp_name();
        } else {
          assert(type == PortType::OUTPUT);
          // When concatenating output connection there cannot
          // be an empty placeholder so we have to create a
          // dummy net.
          os << create_unconn_net(unconn_count);
        }
        os << " )";
        if (ipin != 0) {
          os << ",\n";
        }
      }
    } else {
      // Individual bits
      for (int ipin = (int)nets.size() - 1; ipin >= 0; --ipin) {  // Reverse order to match endianess
        // Port name
        os << indent(depth) << "." << port_name << "[" << ipin << "]"
           << "(";
        if (nets[ipin].empty()) {
          // Disconnected
          if (type == PortType::INPUT || type == PortType::CLOCK) {
            os << unconn_inp_name();
          } else {
            assert(type == PortType::OUTPUT);
            // When concatenating output connection there cannot
            // be an empty placeholder so we have to create a
            // dummy net.
            os << create_unconn_net(unconn_count);
          }
        } else {
          // Connected
          os << escape_verilog_identifier(nets[ipin]);
        }
        os << " )";
        if (ipin != 0) {
          os << ",\n";
        }
      }
    }
  }
}

LutInst::LutInst(
      size_t lut_size,                                   ///< The LUT size
      LogicVec lut_mask,                                 ///< The LUT mask representing the logic function
      const string& inst_name,                           ///< The name of this instance
      std::map<string, std::vector<string>> port_conns,  ///< The port connections of this instance. Key: port
                                                         ///< name, Value: connected nets
      std::vector<Arc> timing_arc_values,                ///< The timing arcs of this instance
      struct t_analysis_opts opts)

  : type_("LUT_K"),
    lut_size_(lut_size),
    lut_mask_(lut_mask),
    inst_name_(inst_name),
    port_conns_(port_conns),
    timing_arcs_(timing_arc_values),
    opts_(opts)
{ }

LutInst::~LutInst() { }

void LutInst::print_lib(rsbe::sta_lib_writer& lib_writer, ostream& os) {
  // lut only contains "in" and "out"
  assert(port_conns_.count("in"));
  assert(port_conns_.count("out"));
  assert(port_conns_.size() == 2);

  // count bus width to set bus type properly
  int in_bus_width = port_conns_["in"].size();
  int out_bus_width = port_conns_["out"].size();

  // create cell info
  lib_cell cell;
  cell.name(type_);
  cell.type(LUT);
  lib_pin pin_in;
  pin_in.name("in");
  pin_in.direction(INPUT);
  pin_in.bus_width(in_bus_width);
  cell.add_pin(pin_in, INPUT);
  lib_pin pin_out;
  pin_out.name("out");
  pin_out.direction(OUTPUT);
  timing_arch timing;
  timing.sense(POSITIVE);
  timing.type(TRANSITION);
  timing.related_pin(pin_in);
  pin_out.add_timing_arch(timing);
  pin_out.bus_width(out_bus_width);
  cell.add_pin(pin_out, OUTPUT);

  // write cell lib
  lib_writer.write_cell(os, cell);
}

void LutInst::print_sdf(ostream& os, int depth) {
  os << indent(depth) << "(CELL\n";
  os << indent(depth + 1) << "(CELLTYPE \"" << type() << "\")\n";
  os << indent(depth + 1) << "(INSTANCE " << escape_sdf_identifier(instance_name()) << ")\n";

  if (!timing_arcs().empty()) {
    os << indent(depth + 1) << "(DELAY\n";
    os << indent(depth + 2) << "(ABSOLUTE\n";

    for (auto& arc : timing_arcs()) {
      double delay_ps = arc.delay();

      stringstream delay_triple;
      delay_triple << "(" << delay_ps << ":" << delay_ps << ":" << delay_ps << ")";

      os << indent(depth + 3) << "(IOPATH ";
      // Note we do not escape the last index of multi-bit signals since they
      // are used to match multi-bit ports
      os << escape_sdf_identifier(arc.source_name()) << "[" << arc.source_ipin() << "]"
         << " ";

      assert(arc.sink_ipin() == 0);  // Should only be one output
      os << escape_sdf_identifier(arc.sink_name()) << " ";
      os << delay_triple.str() << " " << delay_triple.str() << ")\n";
    }
    os << indent(depth + 2) << ")\n";
    os << indent(depth + 1) << ")\n";
  }

  os << indent(depth) << ")\n";
  os << indent(depth) << "\n";
}

void LutInst::print_verilog(ostream& os, size_t& unconn_count, int depth) {
  os << indent(depth) << type_ << "\n";
  os << indent(depth) << escape_verilog_identifier(inst_name_) << " (\n";

  assert(port_conns_.count("in"));
  assert(port_conns_.count("out"));
  assert(port_conns_.size() == 2);

  print_verilog_port(os, unconn_count, "in", port_conns_["in"], PortType::INPUT, depth + 1, opts_);
  os << "," << "\n";
  print_verilog_port(os, unconn_count, "out", port_conns_["out"], PortType::OUTPUT, depth + 1, opts_);
  os << "\n";

  os << indent(depth) << ");\n\n";
}

void LatchInst::print_sdf(ostream& os, int depth) {
  assert(type_ == Type::RISING_EDGE);

  os << indent(depth) << "(CELL\n";
  os << indent(depth + 1) << "(CELLTYPE \""
     << "DFF"
     << "\")\n";
  os << indent(depth + 1) << "(INSTANCE " << escape_sdf_identifier(instance_name_) << ")\n";

  // Clock to Q
  if (!std::isnan(tcq_)) {
    os << indent(depth + 1) << "(DELAY\n";
    os << indent(depth + 2) << "(ABSOLUTE\n";
    double delay_ps = get_delay_ps(tcq_);

    stringstream delay_triple;
    delay_triple << "(" << delay_ps << ":" << delay_ps << ":" << delay_ps << ")";

    os << indent(depth + 3) << "(IOPATH "
       << "(posedge clock) Q " << delay_triple.str() << " " << delay_triple.str() << ")\n";
    os << indent(depth + 2) << ")\n";
    os << indent(depth + 1) << ")\n";
  }

  // Setup/Hold
  if (!std::isnan(tsu_) || !std::isnan(thld_)) {
    os << indent(depth + 1) << "(TIMINGCHECK\n";
    if (!std::isnan(tsu_)) {
      stringstream setup_triple;
      double setup_ps = get_delay_ps(tsu_);
      setup_triple << "(" << setup_ps << ":" << setup_ps << ":" << setup_ps << ")";
      os << indent(depth + 2) << "(SETUP D (posedge clock) " << setup_triple.str() << ")\n";
    }
    if (!std::isnan(thld_)) {
      stringstream hold_triple;
      double hold_ps = get_delay_ps(thld_);
      hold_triple << "(" << hold_ps << ":" << hold_ps << ":" << hold_ps << ")";
      os << indent(depth + 2) << "(HOLD D (posedge clock) " << hold_triple.str() << ")\n";
    }
  }
  os << indent(depth + 1) << ")\n";
  os << indent(depth) << ")\n";
  os << indent(depth) << "\n";
}

void LatchInst::print_verilog(ostream& os, size_t& /*unconn_count*/, int depth) {
  // Currently assume a standard DFF
  assert(type_ == Type::RISING_EDGE);

  os << indent(depth) << "DFF"
     << " #(\n";
  os << indent(depth + 1) << ".INITIAL_VALUE(";
  if (initial_value_ == vtr::LogicValue::TRUE)
    os << "1'b1";
  else if (initial_value_ == vtr::LogicValue::FALSE)
    os << "1'b0";
  else if (initial_value_ == vtr::LogicValue::DONT_CARE)
    os << "1'bx";
  else if (initial_value_ == vtr::LogicValue::UNKOWN)
    os << "1'bx";
  else
    assert(false);
  os << ")\n";
  os << indent(depth) << ") " << escape_verilog_identifier(instance_name_) << " (\n";

  for (auto iter = port_connections_.begin(); iter != port_connections_.end(); ++iter) {
    os << indent(depth + 1) << "." << iter->first << "(" << escape_verilog_identifier(iter->second) << ")";

    if (iter != --port_connections_.end()) {
      os << ", ";
    }
    os << "\n";
  }
  os << indent(depth) << ");";
  os << "\n";
}

void BlackBoxInst::print_lib(rsbe::sta_lib_writer& lib_writer, ostream& os) {
  // create cell info
  lib_cell cell;
  cell.name(type_name_);

  // to make memory management simple, we are using static memory for each
  // objects here
  std::map<string, lib_pin> written_in_pins;
  std::map<string, lib_pin> written_out_pins;

  // create pin and annotate with timing info
  if (ports_tcq_.size() || ports_tsu_.size() || ports_thld_.size()) {
    cell.type(SEQUENTIAL);

    // for debug
    int i = 0;
    if (::getenv("stars_trace")) cout << i << endl;  // TMP to suppress unused var warning

    // clock arch
    for (auto tcq_kv : ports_tcq_) {
      string pin_name = tcq_kv.second.second;
      lib_pin related_pin;
      if (written_in_pins.find(pin_name) == written_in_pins.end()) {
        related_pin.name(pin_name);
        related_pin.bus_width(1);
        related_pin.direction(INPUT);
        related_pin.type(CLOCK);
        written_in_pins.insert(std::pair<string, lib_pin>(pin_name, related_pin));
      } else {
        related_pin = written_in_pins[pin_name];
      }

      pin_name = tcq_kv.first;
      int port_size = find_port_size(pin_name);
      for (int port_idex = 0; port_idex < port_size; port_idex++) {
        string out_pin_name = pin_name;
        if (port_size > 1) {
          out_pin_name += string("[") + std::to_string(port_idex) + string("]");
        }

        lib_pin pin_out;
        if (written_out_pins.find(out_pin_name) == written_out_pins.end()) {
          pin_out.type(DATA);
          pin_out.name(out_pin_name);
          pin_out.direction(OUTPUT);
          pin_out.bus_width(1);
          written_out_pins.insert(std::pair<string, lib_pin>(out_pin_name, pin_out));
        } else {
          pin_out = written_out_pins[out_pin_name];
        }
        timing_arch timing;
        timing.sense(POSITIVE);
        timing.type(TRANSITION);
        timing.related_pin(related_pin);
        pin_out.add_timing_arch(timing);
        // update pin_out
        written_out_pins[out_pin_name] = pin_out;
      }
    }

    // setup arch
    i = 0;
    for (auto tsu_kv : ports_tsu_) {
      string pin_name = tsu_kv.second.second;
      lib_pin related_pin;
      if (written_in_pins.find(pin_name) == written_in_pins.end()) {
        related_pin.name(pin_name);
        related_pin.bus_width(1);
        related_pin.direction(INPUT);
        related_pin.type(CLOCK);
        written_in_pins.insert(std::pair<string, lib_pin>(pin_name, related_pin));
      } else {
        related_pin = written_in_pins[pin_name];
      }

      pin_name = tsu_kv.first;
      int port_size = find_port_size(pin_name);
      for (int port_idex = 0; port_idex < port_size; port_idex++) {
        string in_pin_name = pin_name;
        if (port_size > 1) {
          in_pin_name += string("[") + std::to_string(port_idex) + string("]");
        }
        lib_pin pin_in;
        if (written_in_pins.find(in_pin_name) == written_in_pins.end()) {
          pin_in.name(in_pin_name);
          pin_in.bus_width(1);
          pin_in.direction(INPUT);
          pin_in.type(DATA);
          written_in_pins.insert(std::pair<string, lib_pin>(in_pin_name, pin_in));
        } else {
          pin_in = written_in_pins[in_pin_name];
        }
        timing_arch timing;
        timing.sense(POSITIVE);
        timing.type(SETUP);
        timing.related_pin(related_pin);
        pin_in.add_timing_arch(timing);
        written_in_pins[in_pin_name] = pin_in;
      }
    }

    // hold arch
    i = 0;
    for (auto thld_kv : ports_thld_) {
      string pin_name = thld_kv.second.second;
      lib_pin related_pin;
      if (written_in_pins.find(pin_name) == written_in_pins.end()) {
        related_pin.name(pin_name);
        related_pin.bus_width(1);
        related_pin.direction(INPUT);
        related_pin.type(CLOCK);
        written_in_pins.insert(std::pair<string, lib_pin>(pin_name, related_pin));
      } else {
        related_pin = written_in_pins[pin_name];
      }

      pin_name = thld_kv.first;
      int port_size = find_port_size(pin_name);
      for (int port_idex = 0; port_idex < port_size; port_idex++) {
        string in_pin_name = pin_name;
        if (port_size > 1) {
          in_pin_name += string("[") + std::to_string(port_idex) + string("]");
        }
        lib_pin pin_in;
        if (written_in_pins.find(in_pin_name) == written_in_pins.end()) {
          pin_in.name(in_pin_name);
          pin_in.bus_width(1);
          pin_in.direction(INPUT);
          pin_in.type(DATA);
          written_in_pins.insert(std::pair<string, lib_pin>(in_pin_name, pin_in));
        } else {
          pin_in = written_in_pins[in_pin_name];
        }
        timing_arch timing;
        timing.sense(POSITIVE);
        timing.type(HOLD);
        timing.related_pin(related_pin);
        pin_in.add_timing_arch(timing);
        written_in_pins[in_pin_name] = pin_in;
      }
    }

    // add pins to cell
    for (std::map<string, lib_pin>::iterator itr = written_in_pins.begin(); itr != written_in_pins.end();
         itr++) {
      cell.add_pin(itr->second, INPUT);
    }
    for (std::map<string, lib_pin>::iterator itr = written_out_pins.begin(); itr != written_out_pins.end();
         itr++) {
      cell.add_pin(itr->second, OUTPUT);
    }
  } else if (!timing_arcs_.empty()) {
    // just write out timing arcs
    cell.name(type_name_);
    cell.type(BLACKBOX);
    for (auto& arc : timing_arcs_) {
      string in_pin_name = arc.source_name();
      if (find_port_size(in_pin_name) > 1) {
        in_pin_name += string("[") + std::to_string(arc.source_ipin()) + string("]");
      }
      if (written_in_pins.find(in_pin_name) == written_in_pins.end()) {
        lib_pin pin_in;
        pin_in.name(in_pin_name);
        pin_in.bus_width(1);
        pin_in.direction(INPUT);
        pin_in.type(DATA);
        // cell.add_pin(pin_in, INPUT);
        written_in_pins.insert(std::pair<string, lib_pin>(in_pin_name, pin_in));
      }

      // todo: add timing arch
      string out_pin_name = arc.sink_name();
      if (find_port_size(out_pin_name) > 1) {
        out_pin_name += string("[") + std::to_string(arc.sink_ipin()) + string("]");
      }
      timing_arch timing;
      timing.sense(POSITIVE);
      timing.type(TRANSITION);
      timing.related_pin(written_in_pins[in_pin_name]);
      if (written_out_pins.find(out_pin_name) == written_out_pins.end()) {
        lib_pin pin_out;
        pin_out.name(out_pin_name);
        pin_out.bus_width(1);
        pin_out.direction(OUTPUT);
        pin_out.type(DATA);
        pin_out.add_timing_arch(timing);
        // cell.add_pin(pin_out, OUTPUT);
        written_out_pins.insert(std::pair<string, lib_pin>(out_pin_name, pin_out));
      } else {
        written_out_pins[out_pin_name].add_timing_arch(timing);
      }
    }
    for (std::map<string, lib_pin>::iterator itr = written_in_pins.begin(); itr != written_in_pins.end();
         itr++) {
      cell.add_pin(itr->second, INPUT);
    }
    for (std::map<string, lib_pin>::iterator itr = written_out_pins.begin(); itr != written_out_pins.end();
         itr++) {
      cell.add_pin(itr->second, OUTPUT);
    }
  } else {
    std::cerr << "STARS: NYI - Create cell for blackbox.\n";
    return;
  }

  // write cell lib
  lib_writer.write_cell(os, cell);

  return;
}

void BlackBoxInst::print_sdf(ostream& os, int depth) {
  if (!timing_arcs_.empty() || !ports_tcq_.empty() || !ports_tsu_.empty() || !ports_thld_.empty()) {
    os << indent(depth) << "(CELL\n";
    os << indent(depth + 1) << "(CELLTYPE \"" << type_name_ << "\")\n";
    // instance name needs to match print_verilog
    os << indent(depth + 1) << "(INSTANCE " << escape_sdf_identifier(inst_name_) << ")\n";
    os << indent(depth + 1) << "(DELAY\n";

    if (!timing_arcs_.empty() || !ports_tcq_.empty()) {
      os << indent(depth + 2) << "(ABSOLUTE\n";

      // Combinational paths
      for (const auto& arc : timing_arcs_) {
        double delay_ps = get_delay_ps(arc.delay());

        stringstream delay_triple;
        delay_triple << "(" << delay_ps << ":" << delay_ps << ":" << delay_ps << ")";

        // Note that we explicitly do not escape the last array indexing so
        // an SDF reader will treat the ports as multi-bit
        //
        // We also only put the last index in if the port has multiple bits

        // we need to blast it since all bus port has been blasted in print
        // verilog else, OpenSTA issues warning on that
        int src_port_size = find_port_size(arc.source_name());
        for (int src_port_idex = 0; src_port_idex < src_port_size; src_port_idex++) {
          string source_name = arc.source_name();
          if (src_port_size > 1) {
            source_name += string("[") + std::to_string(src_port_idex) + string("]");
          }
          int snk_port_size = find_port_size(arc.sink_name());
          for (int snk_port_idex = 0; snk_port_idex < snk_port_size; snk_port_idex++) {
            string sink_name = arc.sink_name();
            if (snk_port_size > 1) {
              sink_name += string("[") + std::to_string(snk_port_idex) + string("]");
            }
            os << indent(depth + 3) << "(IOPATH ";
            // os << escape_sdf_identifier(source_name);
            os << source_name;
            os << " ";
            // os << escape_sdf_identifier(sink_name);
            os << sink_name;
            os << " ";
            os << delay_triple.str();
            os << ")\n";
          }
        }
      }

      // Clock-to-Q delays
      for (auto kv : ports_tcq_) {
        double clock_to_q_ps = get_delay_ps(kv.second.first);

        stringstream delay_triple;
        delay_triple << "(" << clock_to_q_ps << ":" << clock_to_q_ps << ":" << clock_to_q_ps << ")";

        int src_port_size = find_port_size(kv.second.second);
        for (int src_port_idex = 0; src_port_idex < src_port_size; src_port_idex++) {
          string source_name = kv.second.second;
          if (src_port_size > 1) {
            source_name += string("[") + std::to_string(src_port_idex) + string("]");
          }
          int snk_port_size = find_port_size(kv.first);
          for (int snk_port_idex = 0; snk_port_idex < snk_port_size; snk_port_idex++) {
            string sink_name = kv.first;
            if (snk_port_size > 1) {
              sink_name += string("[") + std::to_string(snk_port_idex) + string("]");
            }
            os << indent(depth + 3) << "(IOPATH (posedge " << source_name << ") " << sink_name << " "
               << delay_triple.str() << " " << delay_triple.str() << ")\n";
          }
        }
      }
      os << indent(depth + 2) << ")\n";  // ABSOLUTE
      os << indent(depth + 1) << ")\n";  // DELAY

      if (!ports_tsu_.empty() || !ports_thld_.empty()) {
        // Setup checks
        os << indent(depth + 1) << "(TIMINGCHECK\n";
        for (auto kv : ports_tsu_) {
          double setup_ps = get_delay_ps(kv.second.first);
          stringstream delay_triple;
          delay_triple << "(" << setup_ps << ":" << setup_ps << ":" << setup_ps << ")";
          int data_port_size = find_port_size(kv.first);
          for (int data_port_idex = 0; data_port_idex < data_port_size; data_port_idex++) {
            string data_name = kv.first;
            if (data_port_size > 1) {
              data_name += string("[") + std::to_string(data_port_idex) + string("]");
            }
            int clk_port_size = find_port_size(kv.second.second);
            for (int clk_port_idex = 0; clk_port_idex < clk_port_size; clk_port_idex++) {
              string clk_name = kv.second.second;
              if (clk_port_size > 1) {
                clk_name += string("[") + std::to_string(clk_port_idex) + string("]");
              }
              os << indent(depth + 2) << "(SETUP " << data_name << " (posedge  " << clk_name << ") "
                 << delay_triple.str() << ")\n";
            }
          }
        }
        for (auto kv : ports_thld_) {
          double hold_ps = get_delay_ps(kv.second.first);

          stringstream delay_triple;
          delay_triple << "(" << hold_ps << ":" << hold_ps << ":" << hold_ps << ")";

          int data_port_size = find_port_size(kv.first);
          for (int data_port_idex = 0; data_port_idex < data_port_size; data_port_idex++) {
            string data_name = kv.first;
            if (data_port_size > 1) {
              data_name += string("[") + std::to_string(data_port_idex) + string("]");
            }
            int clk_port_size = find_port_size(kv.second.second);
            for (int clk_port_idex = 0; clk_port_idex < clk_port_size; clk_port_idex++) {
              string clk_name = kv.second.second;
              if (clk_port_size > 1) {
                clk_name += string("[") + std::to_string(clk_port_idex) + string("]");
              }
              os << indent(depth + 2) << "(HOLD " << data_name << " (posedge " << clk_name << ") "
                 << delay_triple.str() << ")\n";
            }
          }
        }
        os << indent(depth + 1) << ")\n";  // TIMINGCHECK
      }
      os << indent(depth) << ")\n";  // CELL
    }
  }
}

void BlackBoxInst::print_verilog(ostream& os, size_t& unconn_count, int depth) {
  // Instance type
  os << indent(depth) << type_name_ << "\n";

  // Instance name
  os << indent(depth) << escape_verilog_identifier(inst_name_) << " (\n";

  // Input Port connections
  for (auto iter = input_port_conns_.begin(); iter != input_port_conns_.end(); ++iter) {
    auto& port_name = iter->first;
    auto& nets = iter->second;
    print_verilog_port(os, unconn_count, port_name, nets, PortType::INPUT, depth + 1, opts_);
    if (!(iter == --input_port_conns_.end() && output_port_conns_.empty())) {
      os << ",";
    }
    os << "\n";
  }

  // Output Port connections
  for (auto iter = output_port_conns_.begin(); iter != output_port_conns_.end(); ++iter) {
    auto& port_name = iter->first;
    auto& nets = iter->second;
    print_verilog_port(os, unconn_count, port_name, nets, PortType::OUTPUT, depth + 1, opts_);
    if (!(iter == --output_port_conns_.end())) {
      os << ",";
    }
    os << "\n";
  }
  os << indent(depth) << ");\n";
  os << "\n";
}

}

