
#include "pinc_log.h"

// vpr/src/base
#include "atom_netlist.h"
#include "atom_netlist_utils.h"
#include "globals.h"
#include "logic_vec.h"
#include "vpr_types.h"
#include "vtr_version.h"
#include "netlist_walker.h"
#include "read_blif.h"

// vpr/src/timing
#include "AnalysisDelayCalculator.h"
#include "net_delay.h"

#include "sta_file_writer.h"
#include "sta_lib_data.h"
#include "sta_lib_writer.h"
#include "rsGlobal.h"
#include "rsVPR.h"

namespace rsbe {

using std::cout;
using std::endl;
using std::string;
using std::stringstream;
using namespace pinc;

// This pair cointains the following values:
//      - double: hold, setup or clock-to-q delays of the port
//      - string: port name of the associated source clock pin of the sequential
//      port
typedef std::pair<double, string> sequential_port_delay_pair;

/*enum class PortType {
 * IN,
 * OUT,
 * CLOCK
 * };*/

////////////////////////////////////////////////////////////////////////////////
// Local classes and functions
////////////////////////////////////////////////////////////////////////////////

// Unconnected net prefix
const string unconn_prefix = "__vpr__unconn";

///@brief Returns a blank string for indenting the given depth
string indent(size_t depth) {
  string indent_ = "    ";
  string new_indent;
  for (size_t i = 0; i < depth; ++i) {
    new_indent += indent_;
  }
  return new_indent;
}

///@brief Returns the delay in pico-seconds from a floating point delay
double get_delay_ps(double delay_sec) {
  return delay_sec * 1e12; // Scale to picoseconds
}

///@brief Returns the name of a unique unconnected net
string create_unconn_net(size_t &unconn_count) {
  // We increment unconn_count by reference so each
  // call generates a unique name
  return unconn_prefix + std::to_string(unconn_count++);
}

///@brief Escapes the given identifier to be safe for verilog
string escape_verilog_identifier(const string identifier) {
  // Verilog allows escaped identifiers
  //
  // The escaped identifiers start with a literal back-slash '\'
  // followed by the identifier and are terminated by white space
  //
  // We pre-pend the escape back-slash and append a space to avoid
  // the identifier gobbling up adjacent characters like commas which
  // are not actually part of the identifier
  string prefix = "\\";
  string suffix = " ";
  string escaped_name = prefix + identifier + suffix;

  return escaped_name;
}

/**
 * @brief Pretty-Prints a verilog port to the given output stream
 *
 * Handles special cases like multi-bit and disconnected ports
 */
void print_verilog_port(std::ostream &os, size_t &unconn_count,
                        const string &port_name,
                        const std::vector<string> &nets, PortType type,
                        int depth, struct t_analysis_opts &opts) {
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
        opts.post_synth_netlist_unconn_output_handling ==
            e_post_synth_netlist_unconn_handling::UNCONNECTED) {
      // Empty connection
      for (int ipin = (int)nets.size() - 1; ipin >= 0;
           --ipin) { // Reverse order to match endianess
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
      for (int ipin = (int)nets.size() - 1; ipin >= 0;
           --ipin) { // Reverse order to match endianess
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

///@brief Returns true if c is categorized as a special character in SDF
bool is_special_sdf_char(char c) {
  // From section 3.2.5 of IEEE1497 Part 3 (i.e. the SDF spec)
  // Special characters run from:
  //    ! to # (ASCII decimal 33-35)
  //    % to / (ASCII decimal 37-47)
  //    : to @ (ASCII decimal 58-64)
  //    [ to ^ (ASCII decimal 91-94)
  //    ` to ` (ASCII decimal 96)
  //    { to ~ (ASCII decimal 123-126)
  //
  // Note that the spec defines _ (decimal code 95) and $ (decimal code 36)
  // as non-special alphanumeric characters.
  //
  // However it inconsistently also lists $ in the list of special characters.
  // Since the spec allows for non-special characters to be escaped (they are
  // treated normally), we treat $ as a special character to be safe.
  //
  // Note that the spec appears to have rendering errors in the PDF availble
  // on IEEE Xplore, listing the 'LEFT-POINTING DOUBLE ANGLE QUOTATION MARK'
  // character (decimal code 171) in place of the APOSTROPHE character '
  // with decimal code 39 in the special character list. We assume code 39.
  if ((c >= 33 && c <= 35) || (c == 36) || // $
      (c >= 37 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 94) ||
      (c == 96) || (c >= 123 && c <= 126)) {
    return true;
  }

  return false;
}

///@brief Escapes the given identifier to be safe for sdf
string escape_sdf_identifier(const string identifier) {
  // SDF allows escaped characters
  //
  // We look at each character in the string and escape it if it is
  // a special character
  string escaped_name;

  for (char c : identifier) {
    if (is_special_sdf_char(c)) {
      // Escape the special character
      escaped_name += '\\';
    }
    escaped_name += c;
  }

  return escaped_name;
}

///@brief Joins two identifier strings
string join_identifier(string lhs, string rhs) {
  return lhs + '_' + rhs;
}

// A combinational timing arc
class Arc {
public:
  Arc(string src_port,  ///< Source of the arc
      int src_ipin,          ///< Source pin index
      string snk_port,  ///< Sink of the arc
      int snk_ipin,          ///< Sink pin index
      float del,             ///< Delay on this arc
      string cond = "") ///< Condition associated with the arc
      : source_name_(src_port), source_ipin_(src_ipin), sink_name_(snk_port),
        sink_ipin_(snk_ipin), delay_(del), condition_(cond) {}

  // Accessors
  string source_name() const { return source_name_; }
  int source_ipin() const { return source_ipin_; }
  string sink_name() const { return sink_name_; }
  int sink_ipin() const { return sink_ipin_; }
  double delay() const { return delay_; }
  string condition() const { return condition_; }

private:
  string source_name_;
  int source_ipin_;
  string sink_name_;
  int sink_ipin_;
  double delay_;
  string condition_;
};

/**
 * @brief Instance is an interface used to represent an element instantiated in
 * a netlist
 *
 * Instances know how to describe themselves in BLIF, Verilog and SDF
 *
 * This should be subclassed to implement support for new primitive types
 * (although see BlackBoxInst for a general implementation for a black box
 * primitive)
 */
class Instance {
public:
  virtual ~Instance() = default;

  virtual void print_verilog(std::ostream &os, size_t &unconn_count,
                             int depth = 0) = 0;
  virtual void print_sdf(std::ostream &os, int depth = 0) = 0;
  virtual void print_lib(rsbe::sta_lib_writer &lib_writer,
                         std::ostream &os) = 0;
  virtual string get_type_name() = 0;
};

///@brief An instance representing a Look-Up Table
class LutInst : public Instance {
public: ///< Public methods
  LutInst(
      size_t lut_size,       ///< The LUT size
      LogicVec lut_mask,     ///< The LUT mask representing the logic function
      string inst_name, ///< The name of this instance
      std::map<string, std::vector<string>>
          port_conns, ///< The port connections of this instance. Key: port
                      ///< name, Value: connected nets
      std::vector<Arc> timing_arc_values, ///< The timing arcs of this instance
      struct t_analysis_opts opts)
      : type_("LUT_K"), lut_size_(lut_size), lut_mask_(lut_mask),
        inst_name_(inst_name), port_conns_(port_conns),
        timing_arcs_(timing_arc_values), opts_(opts) {}

  // Accessors
  const std::vector<Arc> &timing_arcs() { return timing_arcs_; }
  string instance_name() { return inst_name_; }
  string type() { return type_; }

public: // Instance interface method implementations
  string get_type_name() override { return "LUT_K"; }
  void print_lib(rsbe::sta_lib_writer &lib_writer, std::ostream &os) override {

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

    return;
  }
  void print_sdf(std::ostream &os, int depth) override {
    os << indent(depth) << "(CELL\n";
    os << indent(depth + 1) << "(CELLTYPE \"" << type() << "\")\n";
    os << indent(depth + 1) << "(INSTANCE "
       << escape_sdf_identifier(instance_name()) << ")\n";

    if (!timing_arcs().empty()) {
      os << indent(depth + 1) << "(DELAY\n";
      os << indent(depth + 2) << "(ABSOLUTE\n";

      for (auto &arc : timing_arcs()) {
        double delay_ps = arc.delay();

        stringstream delay_triple;
        delay_triple << "(" << delay_ps << ":" << delay_ps << ":" << delay_ps
                     << ")";

        os << indent(depth + 3) << "(IOPATH ";
        // Note we do not escape the last index of multi-bit signals since they
        // are used to match multi-bit ports
        os << escape_sdf_identifier(arc.source_name()) << "["
           << arc.source_ipin() << "]"
           << " ";

        assert(arc.sink_ipin() == 0); // Should only be one output
        os << escape_sdf_identifier(arc.sink_name()) << " ";
        os << delay_triple.str() << " " << delay_triple.str() << ")\n";
      }
      os << indent(depth + 2) << ")\n";
      os << indent(depth + 1) << ")\n";
    }

    os << indent(depth) << ")\n";
    os << indent(depth) << "\n";
  }
  void print_verilog(std::ostream &os, size_t &unconn_count,
                     int depth) override {
    os << indent(depth) << type_ << "\n";
    os << indent(depth) << escape_verilog_identifier(inst_name_) << " (\n";

    assert(port_conns_.count("in"));
    assert(port_conns_.count("out"));
    assert(port_conns_.size() == 2);

    print_verilog_port(os, unconn_count, "in", port_conns_["in"],
                       PortType::INPUT, depth + 1, opts_);
    os << ","
       << "\n";
    print_verilog_port(os, unconn_count, "out", port_conns_["out"],
                       PortType::OUTPUT, depth + 1, opts_);
    os << "\n";

    os << indent(depth) << ");\n\n";
  }

private:
  string type_;
  size_t lut_size_;
  LogicVec lut_mask_;
  string inst_name_;
  std::map<string, std::vector<string>> port_conns_;
  std::vector<Arc> timing_arcs_;
  struct t_analysis_opts opts_;
};

class LatchInst : public Instance {
public: // Public types
  ///@brief Types of latches (defined by BLIF)
  enum class Type {
    RISING_EDGE,
    FALLING_EDGE,
    ACTIVE_HIGH,
    ACTIVE_LOW,
    ASYNCHRONOUS,
  };
  friend std::ostream &operator<<(std::ostream &os, const Type &type) {
    if (type == Type::RISING_EDGE)
      os << "re";
    else if (type == Type::FALLING_EDGE)
      os << "fe";
    else if (type == Type::ACTIVE_HIGH)
      os << "ah";
    else if (type == Type::ACTIVE_LOW)
      os << "al";
    else if (type == Type::ASYNCHRONOUS)
      os << "as";
    else
      assert(false);
    return os;
  }
  friend std::istream &operator>>(std::istream &is, Type &type) {
    string tok;
    is >> tok;
    if (tok == "re")
      type = Type::RISING_EDGE;
    else if (tok == "fe")
      type = Type::FALLING_EDGE;
    else if (tok == "ah")
      type = Type::ACTIVE_HIGH;
    else if (tok == "al")
      type = Type::ACTIVE_LOW;
    else if (tok == "as")
      type = Type::ASYNCHRONOUS;
    else
      assert(false);
    return is;
  }

public:
  LatchInst(
      string inst_name, ///< Name of this instance
      std::map<string, string>
          port_conns,             ///< Instance's port-to-net connections
      Type type,                  ///< Type of this latch
      vtr::LogicValue init_value, ///< Initial value of the latch
      double tcq =
          std::numeric_limits<double>::quiet_NaN(), ///< Clock-to-Q delay
      double tsu = std::numeric_limits<double>::quiet_NaN(),  ///< Setup time
      double thld = std::numeric_limits<double>::quiet_NaN()) ///< Hold time
      : instance_name_(inst_name), port_connections_(port_conns), type_(type),
        initial_value_(init_value), tcq_(tcq), tsu_(tsu), thld_(thld) {}

  string get_type_name() override { return "DFF"; }

  void print_lib(rsbe::sta_lib_writer &lib_writer, std::ostream &os) override {
    /*
    os << indent(depth + 1) << "LIBERTY FOR: (INSTANCE "
       << escape_sdf_identifier(instance_name_) << ")\n";
       */
  }
  void print_sdf(std::ostream &os, int depth = 0) override {
    assert(type_ == Type::RISING_EDGE);

    os << indent(depth) << "(CELL\n";
    os << indent(depth + 1) << "(CELLTYPE \""
       << "DFF"
       << "\")\n";
    os << indent(depth + 1) << "(INSTANCE "
       << escape_sdf_identifier(instance_name_) << ")\n";

    // Clock to Q
    if (!std::isnan(tcq_)) {
      os << indent(depth + 1) << "(DELAY\n";
      os << indent(depth + 2) << "(ABSOLUTE\n";
      double delay_ps = get_delay_ps(tcq_);

      stringstream delay_triple;
      delay_triple << "(" << delay_ps << ":" << delay_ps << ":" << delay_ps
                   << ")";

      os << indent(depth + 3) << "(IOPATH "
         << "(posedge clock) Q " << delay_triple.str() << " "
         << delay_triple.str() << ")\n";
      os << indent(depth + 2) << ")\n";
      os << indent(depth + 1) << ")\n";
    }

    // Setup/Hold
    if (!std::isnan(tsu_) || !std::isnan(thld_)) {
      os << indent(depth + 1) << "(TIMINGCHECK\n";
      if (!std::isnan(tsu_)) {
        stringstream setup_triple;
        double setup_ps = get_delay_ps(tsu_);
        setup_triple << "(" << setup_ps << ":" << setup_ps << ":" << setup_ps
                     << ")";
        os << indent(depth + 2) << "(SETUP D (posedge clock) "
           << setup_triple.str() << ")\n";
      }
      if (!std::isnan(thld_)) {
        stringstream hold_triple;
        double hold_ps = get_delay_ps(thld_);
        hold_triple << "(" << hold_ps << ":" << hold_ps << ":" << hold_ps
                    << ")";
        os << indent(depth + 2) << "(HOLD D (posedge clock) "
           << hold_triple.str() << ")\n";
      }
    }
    os << indent(depth + 1) << ")\n";
    os << indent(depth) << ")\n";
    os << indent(depth) << "\n";
  }
  void print_verilog(std::ostream &os, size_t & /*unconn_count*/,
                     int depth = 0) override {
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
    os << indent(depth) << ") " << escape_verilog_identifier(instance_name_)
       << " (\n";

    for (auto iter = port_connections_.begin(); iter != port_connections_.end();
         ++iter) {
      os << indent(depth + 1) << "." << iter->first << "("
         << escape_verilog_identifier(iter->second) << ")";

      if (iter != --port_connections_.end()) {
        os << ", ";
      }
      os << "\n";
    }
    os << indent(depth) << ");";
    os << "\n";
  }

private:
  string instance_name_;
  std::map<string, string> port_connections_;
  Type type_;
  vtr::LogicValue initial_value_;
  double tcq_;  ///< Clock delay + tcq
  double tsu_;  ///< Setup time
  double thld_; ///< Hold time
};

class BlackBoxInst : public Instance {
public:
  BlackBoxInst(
      string type_name, ///< Instance type
      string inst_name, ///< Instance name
      std::map<string, string>
          params, ///< Verilog parameters: Dictonary of <param_name,value>
      std::map<string, string>
          attrs, ///< Instance attributes: Dictonary of <attr_name,value>
      std::map<string, std::vector<string>>
          input_port_conns, ///< Port connections: Dictionary of <port,nets>
      std::map<string, std::vector<string>>
          output_port_conns, ///< Port connections: Dictionary of <port,nets>
      std::vector<Arc> timing_arcs, ///< Combinational timing arcs
      std::map<string, sequential_port_delay_pair>
          ports_tsu, ///< Port setup checks
      std::map<string, sequential_port_delay_pair>
          ports_thld, ///< Port hold checks
      std::map<string, sequential_port_delay_pair>
          ports_tcq, ///< Port clock-to-q delays
      struct t_analysis_opts opts)
      : type_name_(type_name), inst_name_(inst_name), params_(params),
        attrs_(attrs), input_port_conns_(input_port_conns),
        output_port_conns_(output_port_conns), timing_arcs_(timing_arcs),
        ports_tsu_(ports_tsu), ports_thld_(ports_thld), ports_tcq_(ports_tcq),
        opts_(opts) {}

  void print_lib(rsbe::sta_lib_writer &lib_writer, std::ostream &os) override {

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
      if (::getenv("stars_trace"))
        cout << i << endl; // TMP to suppress unused var warning

      // clock arch
      for (auto tcq_kv : ports_tcq_) {

        string pin_name = tcq_kv.second.second;
        lib_pin related_pin;
        if (written_in_pins.find(pin_name) == written_in_pins.end()) {
          related_pin.name(pin_name);
          related_pin.bus_width(1);
          related_pin.direction(INPUT);
          related_pin.type(CLOCK);
          written_in_pins.insert(
              std::pair<string, lib_pin>(pin_name, related_pin));
        } else {
          related_pin = written_in_pins[pin_name];
        }

        pin_name = tcq_kv.first;
        int port_size = find_port_size(pin_name);
        for (int port_idex = 0; port_idex < port_size; port_idex++) {
          string out_pin_name = pin_name;
          if (port_size > 1) {
            out_pin_name +=
                string("[") + std::to_string(port_idex) + string("]");
          }

          lib_pin pin_out;
          if (written_out_pins.find(out_pin_name) == written_out_pins.end()) {
            pin_out.type(DATA);
            pin_out.name(out_pin_name);
            pin_out.direction(OUTPUT);
            pin_out.bus_width(1);
            written_out_pins.insert(
                std::pair<string, lib_pin>(out_pin_name, pin_out));
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
          written_in_pins.insert(
              std::pair<string, lib_pin>(pin_name, related_pin));
        } else {
          related_pin = written_in_pins[pin_name];
        }

        pin_name = tsu_kv.first;
        int port_size = find_port_size(pin_name);
        for (int port_idex = 0; port_idex < port_size; port_idex++) {
          string in_pin_name = pin_name;
          if (port_size > 1) {
            in_pin_name +=
                string("[") + std::to_string(port_idex) + string("]");
          }
          lib_pin pin_in;
          if (written_in_pins.find(in_pin_name) == written_in_pins.end()) {
            pin_in.name(in_pin_name);
            pin_in.bus_width(1);
            pin_in.direction(INPUT);
            pin_in.type(DATA);
            written_in_pins.insert(
                std::pair<string, lib_pin>(in_pin_name, pin_in));
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
          written_in_pins.insert(
              std::pair<string, lib_pin>(pin_name, related_pin));
        } else {
          related_pin = written_in_pins[pin_name];
        }

        pin_name = thld_kv.first;
        int port_size = find_port_size(pin_name);
        for (int port_idex = 0; port_idex < port_size; port_idex++) {
          string in_pin_name = pin_name;
          if (port_size > 1) {
            in_pin_name +=
                string("[") + std::to_string(port_idex) + string("]");
          }
          lib_pin pin_in;
          if (written_in_pins.find(in_pin_name) == written_in_pins.end()) {
            pin_in.name(in_pin_name);
            pin_in.bus_width(1);
            pin_in.direction(INPUT);
            pin_in.type(DATA);
            written_in_pins.insert(
                std::pair<string, lib_pin>(in_pin_name, pin_in));
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
      for (std::map<string, lib_pin>::iterator itr =
               written_in_pins.begin();
           itr != written_in_pins.end(); itr++) {
        cell.add_pin(itr->second, INPUT);
      }
      for (std::map<string, lib_pin>::iterator itr =
               written_out_pins.begin();
           itr != written_out_pins.end(); itr++) {
        cell.add_pin(itr->second, OUTPUT);
      }
    } else if (!timing_arcs_.empty()) {
      // just write out timing arcs
      cell.name(type_name_);
      cell.type(BLACKBOX);
      for (auto &arc : timing_arcs_) {
        string in_pin_name = arc.source_name();
        if (find_port_size(in_pin_name) > 1) {
          in_pin_name += string("[") + std::to_string(arc.source_ipin()) +
                         string("]");
        }
        if (written_in_pins.find(in_pin_name) == written_in_pins.end()) {
          lib_pin pin_in;
          pin_in.name(in_pin_name);
          pin_in.bus_width(1);
          pin_in.direction(INPUT);
          pin_in.type(DATA);
          // cell.add_pin(pin_in, INPUT);
          written_in_pins.insert(
              std::pair<string, lib_pin>(in_pin_name, pin_in));
        }

        // todo: add timing arch
        string out_pin_name = arc.sink_name();
        if (find_port_size(out_pin_name) > 1) {
          out_pin_name += string("[") + std::to_string(arc.sink_ipin()) +
                          string("]");
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
          written_out_pins.insert(
              std::pair<string, lib_pin>(out_pin_name, pin_out));
        } else {
          written_out_pins[out_pin_name].add_timing_arch(timing);
        }
      }
      for (std::map<string, lib_pin>::iterator itr =
               written_in_pins.begin();
           itr != written_in_pins.end(); itr++) {
        cell.add_pin(itr->second, INPUT);
      }
      for (std::map<string, lib_pin>::iterator itr =
               written_out_pins.begin();
           itr != written_out_pins.end(); itr++) {
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

  void print_sdf(std::ostream &os, int depth = 0) override {
    if (!timing_arcs_.empty() || !ports_tcq_.empty() || !ports_tsu_.empty() ||
        !ports_thld_.empty()) {
      os << indent(depth) << "(CELL\n";
      os << indent(depth + 1) << "(CELLTYPE \"" << type_name_ << "\")\n";
      // instance name needs to match print_verilog
      os << indent(depth + 1) << "(INSTANCE "
         << escape_sdf_identifier(inst_name_) << ")\n";
      os << indent(depth + 1) << "(DELAY\n";

      if (!timing_arcs_.empty() || !ports_tcq_.empty()) {
        os << indent(depth + 2) << "(ABSOLUTE\n";

        // Combinational paths
        for (const auto &arc : timing_arcs_) {
          double delay_ps = get_delay_ps(arc.delay());

          stringstream delay_triple;
          delay_triple << "(" << delay_ps << ":" << delay_ps << ":" << delay_ps
                       << ")";

          // Note that we explicitly do not escape the last array indexing so
          // an SDF reader will treat the ports as multi-bit
          //
          // We also only put the last index in if the port has multiple bits

          // we need to blast it since all bus port has been blasted in print
          // verilog else, OpenSTA issues warning on that
          int src_port_size = find_port_size(arc.source_name());
          for (int src_port_idex = 0; src_port_idex < src_port_size;
               src_port_idex++) {
            string source_name = arc.source_name();
            if (src_port_size > 1) {
              source_name += string("[") + std::to_string(src_port_idex) +
                             string("]");
            }
            int snk_port_size = find_port_size(arc.sink_name());
            for (int snk_port_idex = 0; snk_port_idex < snk_port_size;
                 snk_port_idex++) {
              string sink_name = arc.sink_name();
              if (snk_port_size > 1) {
                sink_name += string("[") + std::to_string(snk_port_idex) +
                             string("]");
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
          delay_triple << "(" << clock_to_q_ps << ":" << clock_to_q_ps << ":"
                       << clock_to_q_ps << ")";

          int src_port_size = find_port_size(kv.second.second);
          for (int src_port_idex = 0; src_port_idex < src_port_size;
               src_port_idex++) {
            string source_name = kv.second.second;
            if (src_port_size > 1) {
              source_name += string("[") + std::to_string(src_port_idex) +
                             string("]");
            }
            int snk_port_size = find_port_size(kv.first);
            for (int snk_port_idex = 0; snk_port_idex < snk_port_size;
                 snk_port_idex++) {
              string sink_name = kv.first;
              if (snk_port_size > 1) {
                sink_name += string("[") + std::to_string(snk_port_idex) +
                             string("]");
              }
              os << indent(depth + 3) << "(IOPATH (posedge " << source_name
                 << ") " << sink_name << " " << delay_triple.str() << " "
                 << delay_triple.str() << ")\n";
            }
          }
        }
        os << indent(depth + 2) << ")\n"; // ABSOLUTE
        os << indent(depth + 1) << ")\n"; // DELAY

        if (!ports_tsu_.empty() || !ports_thld_.empty()) {
          // Setup checks
          os << indent(depth + 1) << "(TIMINGCHECK\n";
          for (auto kv : ports_tsu_) {
            double setup_ps = get_delay_ps(kv.second.first);
            stringstream delay_triple;
            delay_triple << "(" << setup_ps << ":" << setup_ps << ":"
                         << setup_ps << ")";
            int data_port_size = find_port_size(kv.first);
            for (int data_port_idex = 0; data_port_idex < data_port_size;
                 data_port_idex++) {
              string data_name = kv.first;
              if (data_port_size > 1) {
                data_name += string("[") + std::to_string(data_port_idex) +
                             string("]");
              }
              int clk_port_size = find_port_size(kv.second.second);
              for (int clk_port_idex = 0; clk_port_idex < clk_port_size;
                   clk_port_idex++) {
                string clk_name = kv.second.second;
                if (clk_port_size > 1) {
                  clk_name += string("[") + std::to_string(clk_port_idex) +
                              string("]");
                }
                os << indent(depth + 2) << "(SETUP " << data_name
                   << " (posedge  " << clk_name << ") " << delay_triple.str()
                   << ")\n";
              }
            }
          }
          for (auto kv : ports_thld_) {
            double hold_ps = get_delay_ps(kv.second.first);

            stringstream delay_triple;
            delay_triple << "(" << hold_ps << ":" << hold_ps << ":" << hold_ps
                         << ")";

            int data_port_size = find_port_size(kv.first);
            for (int data_port_idex = 0; data_port_idex < data_port_size;
                 data_port_idex++) {
              string data_name = kv.first;
              if (data_port_size > 1) {
                data_name += string("[") + std::to_string(data_port_idex) +
                             string("]");
              }
              int clk_port_size = find_port_size(kv.second.second);
              for (int clk_port_idex = 0; clk_port_idex < clk_port_size;
                   clk_port_idex++) {
                string clk_name = kv.second.second;
                if (clk_port_size > 1) {
                  clk_name += string("[") + std::to_string(clk_port_idex) +
                              string("]");
                }
                os << indent(depth + 2) << "(HOLD " << data_name << " (posedge "
                   << clk_name << ") " << delay_triple.str() << ")\n";
              }
            }
          }
          os << indent(depth + 1) << ")\n"; // TIMINGCHECK
        }
        os << indent(depth) << ")\n"; // CELL
      }
    }
  }

  void print_verilog(std::ostream &os, size_t &unconn_count,
                     int depth = 0) override {
    // Instance type
    os << indent(depth) << type_name_ << "\n";

    // Instance name
    os << indent(depth) << escape_verilog_identifier(inst_name_) << " (\n";

    // Input Port connections
    for (auto iter = input_port_conns_.begin(); iter != input_port_conns_.end();
         ++iter) {
      auto &port_name = iter->first;
      auto &nets = iter->second;
      print_verilog_port(os, unconn_count, port_name, nets, PortType::INPUT,
                         depth + 1, opts_);
      if (!(iter == --input_port_conns_.end() && output_port_conns_.empty())) {
        os << ",";
      }
      os << "\n";
    }

    // Output Port connections
    for (auto iter = output_port_conns_.begin();
         iter != output_port_conns_.end(); ++iter) {
      auto &port_name = iter->first;
      auto &nets = iter->second;
      print_verilog_port(os, unconn_count, port_name, nets, PortType::OUTPUT,
                         depth + 1, opts_);
      if (!(iter == --output_port_conns_.end())) {
        os << ",";
      }
      os << "\n";
    }
    os << indent(depth) << ");\n";
    os << "\n";
  }

  size_t find_port_size(string port_name) {
    auto iter = input_port_conns_.find(port_name);
    if (iter != input_port_conns_.end()) {
      return iter->second.size();
    }

    iter = output_port_conns_.find(port_name);
    if (iter != output_port_conns_.end()) {
      return iter->second.size();
    }
    VPR_FATAL_ERROR(VPR_ERROR_IMPL_NETLIST_WRITER,
                    "Could not find port %s on %s of type %s\n",
                    port_name.c_str(), inst_name_.c_str(), type_name_.c_str());

    return -1; // Suppress warning
  }

  string get_type_name() override { return type_name_; }

private:
  string type_name_;
  string inst_name_;
  std::map<string, string> params_;
  std::map<string, string> attrs_;
  std::map<string, std::vector<string>> input_port_conns_;
  std::map<string, std::vector<string>> output_port_conns_;
  std::vector<Arc> timing_arcs_;
  std::map<string, sequential_port_delay_pair> ports_tsu_;
  std::map<string, sequential_port_delay_pair> ports_thld_;
  std::map<string, sequential_port_delay_pair> ports_tcq_;
  struct t_analysis_opts opts_;
}; // namespace rsbe

/**
 * @brief Assignment represents the logical connection between two nets
 *
 * This is synonomous with verilog's 'assign x = y' which connects
 * two nets with logical identity, assigning the value of 'y' to 'x'
 */
class Assignment {
public:
  Assignment(string lval, ///< The left value (assigned to)
             string rval) ///< The right value (assigned from)
      : lval_(lval), rval_(rval) {}

  void print_verilog(std::ostream &os, string indent) {
    os << indent << "assign " << escape_verilog_identifier(lval_) << " = "
       << escape_verilog_identifier(rval_) << ";\n";
  }

private:
  string lval_;
  string rval_;
};

/**
 * @brief A class which writes post-synthesis netlists (Verilog and BLIF) and
 * the SDF
 *
 * It implements the NetlistVisitor interface used by NetlistWalker (see
 * netlist_walker.h)
 */
class StaWriterVisitor : public NetlistVisitor
{
public:
  StaWriterVisitor(std::ostream& verilog_os, std::ostream& sdf_os,
                   std::ostream& lib_os,
                   std::shared_ptr<const AnalysisDelayCalculator> delay_calc,
                   const t_analysis_opts& opts)
      : verilog_os_(verilog_os), sdf_os_(sdf_os), lib_os_(lib_os),
        delay_calc_(delay_calc), opts_(opts)
{
    auto& atom_ctx = g_vpr_ctx.atom();

    // Initialize the pin to tnode look-up
    for (AtomPinId pin : atom_ctx.nlist.pins()) {
      AtomBlockId blk = atom_ctx.nlist.pin_block(pin);
      ClusterBlockId clb_idx = atom_ctx.lookup.atom_clb(blk);

      const t_pb_graph_pin *gpin = atom_ctx.lookup.atom_pin_pb_graph_pin(pin);
      assert(gpin);
      int pb_pin_idx = gpin->pin_count_in_cluster;

      tatum::NodeId tnode_id = atom_ctx.lookup.atom_pin_tnode(pin);

      auto key = std::make_pair(clb_idx, pb_pin_idx);
      auto value = std::make_pair(key, tnode_id);
      auto ret = pin_id_to_tnode_lookup_.insert(value);
      VTR_ASSERT_MSG(ret.second, "Was inserted");
      assert(ret.second);
    }
  }

  // No copy, No move
  StaWriterVisitor(StaWriterVisitor& other) = delete;
  StaWriterVisitor(StaWriterVisitor&& other) = delete;
  StaWriterVisitor& operator=(StaWriterVisitor& rhs) = delete;
  StaWriterVisitor& operator=(StaWriterVisitor&& rhs) = delete;

private: // NetlistVisitor interface functions
  void visit_top_impl(const char *top_level_name) override {
    top_module_name_ = top_level_name;
  }

  void visit_atom_impl(const t_pb *atom) override {
    auto &atom_ctx = g_vpr_ctx.atom();

    auto atom_pb = atom_ctx.lookup.pb_atom(atom);
    if (atom_pb == AtomBlockId::INVALID()) {
      return;
    }
    const t_model *model = atom_ctx.nlist.block_model(atom_pb);

    if (model->name == string(MODEL_INPUT)) {
      inputs_.emplace_back(make_io(atom, PortType::INPUT));
    } else if (model->name == string(MODEL_OUTPUT)) {
      outputs_.emplace_back(make_io(atom, PortType::OUTPUT));
    } else if (model->name == string(MODEL_NAMES)) {
      cell_instances_.push_back(make_lut_instance(atom));
    } else if (model->name == string(MODEL_LATCH)) {
      cell_instances_.push_back(make_latch_instance(atom));
    } else if (model->name == string("single_port_ram")) {
      cell_instances_.push_back(make_ram_instance(atom));
    } else if (model->name == string("dual_port_ram")) {
      cell_instances_.push_back(make_ram_instance(atom));
    } else if (model->name == string("multiply")) {
      cell_instances_.push_back(make_multiply_instance(atom));
    } else if (model->name == string("adder")) {
      cell_instances_.push_back(make_adder_instance(atom));
    } else {
      cell_instances_.push_back(make_blackbox_instance(atom));
    }
  }

  void finish_impl() override {
    print_lib();
    print_sdf();
    print_verilog();
  }

protected:
  virtual void print_primary_io(int depth) {
    // Primary Inputs
    for (auto iter = inputs_.begin(); iter != inputs_.end(); ++iter) {
      verilog_os_ << indent(depth + 1) << "input "
                  << escape_verilog_identifier(*iter);
      if (iter + 1 != inputs_.end() || outputs_.size() > 0) {
        verilog_os_ << ",";
      }
      verilog_os_ << "\n";
    }
    // Primary Outputs
    for (auto iter = outputs_.begin(); iter != outputs_.end(); ++iter) {
      verilog_os_ << indent(depth + 1) << "output "
                  << escape_verilog_identifier(*iter);
      if (iter + 1 != outputs_.end()) {
        verilog_os_ << ",";
      }
      verilog_os_ << "\n";
    }
  }

  virtual void print_assignments(int depth) {
    verilog_os_ << "\n";
    verilog_os_ << indent(depth + 1) << "//IO assignments\n";
    for (auto &assign : assignments_) {
      assign.print_verilog(verilog_os_, indent(depth + 1));
    }
  }

  ///@brief Writes out the verilog netlist
  void print_verilog(int depth = 0) {
    verilog_os_ << indent(depth) << "//Verilog generated by VPR "
                << vtr::VERSION
                << " from post-place-and-route implementation\n";
    verilog_os_ << indent(depth) << "module " << top_module_name_ << " (\n";

    print_primary_io(depth);
    verilog_os_ << indent(depth) << ");\n";

    // Wire declarations
    verilog_os_ << "\n";
    verilog_os_ << indent(depth + 1) << "//Wires\n";
    for (auto &kv : logical_net_drivers_) {
      verilog_os_ << indent(depth + 1) << "wire "
                  << escape_verilog_identifier(kv.second.first) << ";\n";
    }
    for (auto &kv : logical_net_sinks_) {
      for (auto &wire_tnode_pair : kv.second) {
        verilog_os_ << indent(depth + 1) << "wire "
                    << escape_verilog_identifier(wire_tnode_pair.first)
                    << ";\n";
      }
    }

    // connections between primary I/Os and their internal wires
    print_assignments(depth);

    // Interconnect between cell instances
    verilog_os_ << "\n";
    verilog_os_ << indent(depth + 1) << "//Interconnect\n";
    for (const auto &kv : logical_net_sinks_) {
      auto atom_net_id = kv.first;
      auto driver_iter = logical_net_drivers_.find(atom_net_id);
      assert(driver_iter != logical_net_drivers_.end());
      const auto &driver_wire = driver_iter->second.first;

      for (auto &sink_wire_tnode_pair : kv.second) {
        string inst_name =
            interconnect_name(driver_wire, sink_wire_tnode_pair.first);
        verilog_os_ << indent(depth + 1) << "fpga_interconnect "
                    << escape_verilog_identifier(inst_name) << " (\n";
        verilog_os_ << indent(depth + 2) << ".datain("
                    << escape_verilog_identifier(driver_wire) << "),\n";
        verilog_os_ << indent(depth + 2) << ".dataout("
                    << escape_verilog_identifier(sink_wire_tnode_pair.first)
                    << ")\n";
        verilog_os_ << indent(depth + 1) << ");\n\n";
      }
    }

    // All the cell instances (to an internal buffer for now)
    stringstream instances_ss;

    size_t unconn_count = 0;
    for (auto &inst : cell_instances_) {
      inst->print_verilog(instances_ss, unconn_count, depth + 1);
    }

    // Unconnected wires declarations
    if (unconn_count) {
      verilog_os_ << "\n";
      verilog_os_ << indent(depth + 1) << "//Unconnected wires\n";
      for (size_t i = 0; i < unconn_count; ++i) {
        auto name = unconn_prefix + std::to_string(i);
        verilog_os_ << indent(depth + 1) << "wire "
                    << escape_verilog_identifier(name) << ";\n";
      }
    }

    // All the cell instances
    verilog_os_ << "\n";
    verilog_os_ << indent(depth + 1) << "//Cell instances\n";
    verilog_os_ << instances_ss.str();

    verilog_os_ << "\n";
    verilog_os_ << indent(depth) << "endmodule\n";
  }

private: // Internal Helper functions
  void print_lib(int depth = 0) {
    std::set<string> written_cells;

    sta_lib_writer lib_writer;
    lib_writer.write_header(lib_os_);

    // this is hard coded, need to be re-written
    lib_writer.write_bus_type(lib_os_, 0, 3, false);
    lib_writer.write_bus_type(lib_os_, 0, 4, false);
    lib_writer.write_bus_type(lib_os_, 0, 5, false);

    // Interconnect
    // create cell info
    lib_cell cell;
    cell.name("fpga_interconnect");
    cell.type(INTERCONNECT);
    lib_pin pin_in;
    pin_in.name("datain");
    pin_in.bus_width(1);
    pin_in.direction(INPUT);
    pin_in.type(DATA);
    cell.add_pin(pin_in, INPUT);
    lib_pin pin_out;
    pin_out.name("dataout");
    pin_out.bus_width(1);
    pin_out.direction(OUTPUT);
    pin_out.type(DATA);
    timing_arch timing;
    timing.sense(POSITIVE);
    timing.type(TRANSITION);
    timing.related_pin(pin_in);
    pin_out.add_timing_arch(timing);
    cell.add_pin(pin_out, OUTPUT);
    lib_writer.write_cell(lib_os_, cell);
    written_cells.insert("fpga_interconnect");

    // cells
    for (const auto &inst : cell_instances_) {
      if (written_cells.find(inst->get_type_name()) == written_cells.end()) {
        inst->print_lib(lib_writer, lib_os_);
        written_cells.insert(inst->get_type_name());
      }
    }

    // footer
    lib_writer.write_footer(lib_os_);

    return;
  }

  ///@brief Writes out the SDF
  void print_sdf(int depth = 0) {
    sdf_os_ << indent(depth) << "(DELAYFILE\n";
    sdf_os_ << indent(depth + 1) << "(SDFVERSION \"2.1\")\n";
    sdf_os_ << indent(depth + 1) << "(DESIGN \"" << top_module_name_ << "\")\n";
    sdf_os_ << indent(depth + 1) << "(VENDOR \"verilog-to-routing\")\n";
    sdf_os_ << indent(depth + 1) << "(PROGRAM \"vpr\")\n";
    sdf_os_ << indent(depth + 1) << "(VERSION \"" << vtr::VERSION << "\")\n";
    sdf_os_ << indent(depth + 1) << "(DIVIDER /)\n";
    sdf_os_ << indent(depth + 1) << "(TIMESCALE 1 ps)\n";
    sdf_os_ << "\n";

    // Interconnect
    for (const auto &kv : logical_net_sinks_) {
      auto atom_net_id = kv.first;
      auto driver_iter = logical_net_drivers_.find(atom_net_id);
      assert(driver_iter != logical_net_drivers_.end());
      auto driver_wire = driver_iter->second.first;
      auto driver_tnode = driver_iter->second.second;

      for (auto &sink_wire_tnode_pair : kv.second) {
        auto sink_wire = sink_wire_tnode_pair.first;
        auto sink_tnode = sink_wire_tnode_pair.second;

        sdf_os_ << indent(depth + 1) << "(CELL\n";
        sdf_os_ << indent(depth + 2) << "(CELLTYPE \"fpga_interconnect\")\n";
        sdf_os_ << indent(depth + 2) << "(INSTANCE "
                << escape_sdf_identifier(
                       interconnect_name(driver_wire, sink_wire))
                << ")\n";
        sdf_os_ << indent(depth + 2) << "(DELAY\n";
        sdf_os_ << indent(depth + 3) << "(ABSOLUTE\n";

        double delay = get_delay_ps(driver_tnode, sink_tnode);

        stringstream delay_triple;
        delay_triple << "(" << delay << ":" << delay << ":" << delay << ")";

        sdf_os_ << indent(depth + 4) << "(IOPATH datain dataout "
                << delay_triple.str() << " " << delay_triple.str() << ")\n";
        sdf_os_ << indent(depth + 3) << ")\n";
        sdf_os_ << indent(depth + 2) << ")\n";
        sdf_os_ << indent(depth + 1) << ")\n";
        sdf_os_ << indent(depth) << "\n";
      }
    }

    // Cells
    for (const auto &inst : cell_instances_) {
      inst->print_sdf(sdf_os_, depth + 1);
    }

    sdf_os_ << indent(depth) << ")\n";
  }

  /**
   * @brief Returns the name of a circuit-level Input/Output
   *
   * The I/O is recorded and instantiated by the top level output routines
   *   @param atom  The implementation primitive representing the I/O
   *   @param dir   The IO direction
   */
  string make_io(const t_pb *atom, PortType dir) {
    const t_pb_graph_node *pb_graph_node = atom->pb_graph_node;

    string io_name;
    int cluster_pin_idx = -1;
    if (dir == PortType::INPUT) {
      assert(pb_graph_node->num_output_ports == 1);   // One output port
      assert(pb_graph_node->num_output_pins[0] == 1); // One output pin
      cluster_pin_idx =
          pb_graph_node->output_pins[0][0]
              .pin_count_in_cluster; // Unique pin index in cluster

      io_name = atom->name;

    } else {
      assert(pb_graph_node->num_input_ports == 1);   // One input port
      assert(pb_graph_node->num_input_pins[0] == 1); // One input pin
      cluster_pin_idx =
          pb_graph_node->input_pins[0][0]
              .pin_count_in_cluster; // Unique pin index in cluster

      // Strip off the starting 'out:' that vpr adds to uniqify outputs
      // this makes the port names match the input blif file
      io_name = atom->name + 4;
    }

    const auto &top_pb_route = find_top_pb_route(atom);

    if (top_pb_route.count(cluster_pin_idx)) {
      // Net exists
      auto atom_net_id = top_pb_route[cluster_pin_idx]
                             .atom_net_id; // Connected net in atom netlist

      // Port direction is inverted (inputs drive internal nets, outputs sink
      // internal nets)
      PortType wire_dir =
          (dir == PortType::INPUT) ? PortType::OUTPUT : PortType::INPUT;

      // Look up the tnode associated with this pin (used for delay
      // calculation)
      tatum::NodeId tnode_id = find_tnode(atom, cluster_pin_idx);

      auto wire_name =
          make_inst_wire(atom_net_id, tnode_id, io_name, wire_dir, 0, 0);

      // Connect the wires to to I/Os with assign statements
      if (wire_dir == PortType::INPUT) {
        assignments_.emplace_back(io_name, wire_name);
      } else {
        assignments_.emplace_back(wire_name, io_name);
      }
    }

    return io_name;
  }

protected:
  /**
   * @brief Returns the name of a wire connecting a primitive and global net.
   *
   * The wire is recorded and instantiated by the top level output routines.
   */
  string make_inst_wire(
      AtomNetId atom_net_id,  ///< The id of the net in the atom netlist
      tatum::NodeId tnode_id, ///< The tnode associated with the primitive pin
      string
          inst_name,      ///< The name of the instance associated with the pin
      PortType port_type, ///< The port direction
      int port_idx,       ///< The instance port index
      int pin_idx) {      ///< The instance pin index

    string wire_name = inst_name;
    if (port_type == PortType::INPUT) {
      wire_name = join_identifier(wire_name, "input");
    } else if (port_type == PortType::CLOCK) {
      wire_name = join_identifier(wire_name, "clock");
    } else {
      assert(port_type == PortType::OUTPUT);
      wire_name = join_identifier(wire_name, "output");
    }

    wire_name = join_identifier(wire_name, std::to_string(port_idx));
    wire_name = join_identifier(wire_name, std::to_string(pin_idx));

    auto value = std::make_pair(wire_name, tnode_id);
    if (port_type == PortType::INPUT || port_type == PortType::CLOCK) {
      // Add the sink
      logical_net_sinks_[atom_net_id].push_back(value);

    } else {
      // Add the driver
      assert(port_type == PortType::OUTPUT);

      auto ret =
          logical_net_drivers_.insert(std::make_pair(atom_net_id, value));
      assert(ret.second); // Was inserted, drivers are unique
    }

    return wire_name;
  }

  ///@brief Returns an Instance object representing the LUT
  std::shared_ptr<Instance> make_lut_instance(const t_pb *atom) {
    // Determine what size LUT
    int lut_size = find_num_inputs(atom);

    // Determine the truth table
    auto lut_mask = load_lut_mask(lut_size, atom);

    // Determine the instance name
    auto inst_name = join_identifier("lut", atom->name);

    // Determine the port connections
    std::map<string, std::vector<string>> port_conns;

    const t_pb_graph_node *pb_graph_node = atom->pb_graph_node;
    assert(pb_graph_node->num_input_ports == 1); // LUT has one input port

    const auto &top_pb_route = find_top_pb_route(atom);

    assert(pb_graph_node->num_output_ports == 1); // LUT has one output port
    assert(pb_graph_node->num_output_pins[0] == 1); // LUT has one output
                                                        // pin
    int sink_cluster_pin_idx =
        pb_graph_node->output_pins[0][0]
            .pin_count_in_cluster; // Unique pin index in cluster

    // Add inputs connections
    std::vector<Arc> timing_arcs;
    for (int pin_idx = 0; pin_idx < pb_graph_node->num_input_pins[0];
         pin_idx++) {
      int cluster_pin_idx =
          pb_graph_node->input_pins[0][pin_idx]
              .pin_count_in_cluster; // Unique pin index in cluster

      string net;
      if (!top_pb_route.count(cluster_pin_idx)) {
        // Disconnected
        net = "";
      } else {
        // Connected to a net
        auto atom_net_id = top_pb_route[cluster_pin_idx]
                               .atom_net_id; // Connected net in atom netlist
        assert(atom_net_id);

        // Look up the tnode associated with this pin (used for delay
        // calculation)
        tatum::NodeId src_tnode_id = find_tnode(atom, cluster_pin_idx);
        tatum::NodeId sink_tnode_id = find_tnode(atom, sink_cluster_pin_idx);

        net = make_inst_wire(atom_net_id, src_tnode_id, inst_name,
                             PortType::INPUT, 0, pin_idx);

        // Record the timing arc
        float delay = get_delay_ps(src_tnode_id, sink_tnode_id);

        Arc timing_arc("in", pin_idx, "out", 0, delay);

        timing_arcs.push_back(timing_arc);
      }
      port_conns["in"].push_back(net);
    }

    // Add the single output connection
    {
      auto atom_net_id = top_pb_route[sink_cluster_pin_idx]
                             .atom_net_id; // Connected net in atom netlist

      string net;
      if (!atom_net_id) {
        // Disconnected
        net = "";
      } else {
        // Connected to a net

        // Look up the tnode associated with this pin (used for delay
        // calculation)
        tatum::NodeId tnode_id = find_tnode(atom, sink_cluster_pin_idx);

        net = make_inst_wire(atom_net_id, tnode_id, inst_name, PortType::OUTPUT,
                             0, 0);
      }
      port_conns["out"].push_back(net);
    }

    auto inst = std::make_shared<LutInst>(lut_size, lut_mask, inst_name,
                                          port_conns, timing_arcs, opts_);

    return inst;
  }

  ///@brief Returns an Instance object representing the Latch
  std::shared_ptr<Instance> make_latch_instance(const t_pb *atom) {
    string inst_name = join_identifier("latch", atom->name);

    const auto &top_pb_route = find_top_pb_route(atom);
    const t_pb_graph_node *pb_graph_node = atom->pb_graph_node;

    // We expect a single input, output and clock ports
    assert(pb_graph_node->num_input_ports == 1);
    assert(pb_graph_node->num_output_ports == 1);
    assert(pb_graph_node->num_clock_ports == 1);
    assert(pb_graph_node->num_input_pins[0] == 1);
    assert(pb_graph_node->num_output_pins[0] == 1);
    assert(pb_graph_node->num_clock_pins[0] == 1);

    // The connections
    std::map<string, string> port_conns;

    // Input (D)
    int input_cluster_pin_idx =
        pb_graph_node->input_pins[0][0]
            .pin_count_in_cluster; // Unique pin index in cluster
    assert(top_pb_route.count(input_cluster_pin_idx));
    auto input_atom_net_id = top_pb_route[input_cluster_pin_idx]
                                 .atom_net_id; // Connected net in atom netlist
    string input_net = make_inst_wire(
        input_atom_net_id, find_tnode(atom, input_cluster_pin_idx), inst_name,
        PortType::INPUT, 0, 0);
    port_conns["D"] = input_net;

    double tsu = pb_graph_node->input_pins[0][0].tsu;

    // Output (Q)
    int output_cluster_pin_idx =
        pb_graph_node->output_pins[0][0]
            .pin_count_in_cluster; // Unique pin index in cluster
    assert(top_pb_route.count(output_cluster_pin_idx));
    auto output_atom_net_id = top_pb_route[output_cluster_pin_idx]
                                  .atom_net_id; // Connected net in atom netlist
    string output_net = make_inst_wire(
        output_atom_net_id, find_tnode(atom, output_cluster_pin_idx), inst_name,
        PortType::OUTPUT, 0, 0);
    port_conns["Q"] = output_net;

    double tcq = pb_graph_node->output_pins[0][0].tco_max;

    // Clock (control)
    int control_cluster_pin_idx =
        pb_graph_node->clock_pins[0][0]
            .pin_count_in_cluster; // Unique pin index in cluster
    assert(top_pb_route.count(control_cluster_pin_idx));
    auto control_atom_net_id =
        top_pb_route[control_cluster_pin_idx]
            .atom_net_id; // Connected net in atom netlist
    string control_net = make_inst_wire(
        control_atom_net_id, find_tnode(atom, control_cluster_pin_idx),
        inst_name, PortType::CLOCK, 0, 0);
    port_conns["clock"] = control_net;

    // VPR currently doesn't store enough information to determine these
    // attributes, for now assume reasonable defaults.
    LatchInst::Type type = LatchInst::Type::RISING_EDGE;
    vtr::LogicValue init_value = vtr::LogicValue::FALSE;

    return std::make_shared<LatchInst>(inst_name, port_conns, type, init_value,
                                       tcq, tsu);
  }

  /**
   * @brief Returns an Instance object representing the RAM
   * @note  the primtive interface to dual and single port rams is nearly
   * identical, so we using a single function to handle both
   */
  std::shared_ptr<Instance> make_ram_instance(const t_pb *atom) {
    const auto &top_pb_route = find_top_pb_route(atom);
    const t_pb_graph_node *pb_graph_node = atom->pb_graph_node;
    const t_pb_type *pb_type = pb_graph_node->pb_type;

    assert(pb_type->class_type == MEMORY_CLASS);

    string type = pb_type->model->name;
    string inst_name = join_identifier(type, atom->name);
    std::map<string, string> params;
    std::map<string, string> attrs;
    std::map<string, std::vector<string>> input_port_conns;
    std::map<string, std::vector<string>> output_port_conns;
    std::vector<Arc> timing_arcs;
    std::map<string, sequential_port_delay_pair> ports_tsu;
    std::map<string, sequential_port_delay_pair> ports_thld;
    std::map<string, sequential_port_delay_pair> ports_tcq;

    params["ADDR_WIDTH"] = "0";
    params["DATA_WIDTH"] = "0";

    // Process the input ports
    for (int iport = 0; iport < pb_graph_node->num_input_ports; ++iport) {
      for (int ipin = 0; ipin < pb_graph_node->num_input_pins[iport]; ++ipin) {
        const t_pb_graph_pin *pin = &pb_graph_node->input_pins[iport][ipin];
        const t_port *port = pin->port;
        string port_class = port->port_class;

        int cluster_pin_idx = pin->pin_count_in_cluster;
        string net;
        if (!top_pb_route.count(cluster_pin_idx)) {
          // Disconnected
          net = "";
        } else {
          // Connected
          auto atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;
          assert(atom_net_id);
          net = make_inst_wire(atom_net_id, find_tnode(atom, cluster_pin_idx),
                               inst_name, PortType::INPUT, iport, ipin);
        }

        // RAMs use a port class specification to identify the purpose of each
        // port
        string port_name;
        if (port_class == "address") {
          port_name = "addr";
          params["ADDR_WIDTH"] = std::to_string(port->num_pins);
        } else if (port_class == "address1") {
          port_name = "addr1";
          params["ADDR_WIDTH"] = std::to_string(port->num_pins);
        } else if (port_class == "address2") {
          port_name = "addr2";
          params["ADDR_WIDTH"] = std::to_string(port->num_pins);
        } else if (port_class == "data_in") {
          port_name = "data";
          params["DATA_WIDTH"] = std::to_string(port->num_pins);
        } else if (port_class == "data_in1") {
          port_name = "data1";
          params["DATA_WIDTH"] = std::to_string(port->num_pins);
        } else if (port_class == "data_in2") {
          port_name = "data2";
          params["DATA_WIDTH"] = std::to_string(port->num_pins);
        } else if (port_class == "write_en") {
          port_name = "we";
        } else if (port_class == "write_en1") {
          port_name = "we1";
        } else if (port_class == "write_en2") {
          port_name = "we2";
        } else {
          VPR_FATAL_ERROR(
              VPR_ERROR_IMPL_NETLIST_WRITER,
              "Unrecognized input port class '%s' for primitive '%s' (%s)\n",
              port_class.c_str(), atom->name, pb_type->name);
        }

        input_port_conns[port_name].push_back(net);
        ports_tsu[port_name] =
            std::make_pair(pin->tsu, pin->associated_clock_pin->port->name);
      }
    }

    // Process the output ports
    for (int iport = 0; iport < pb_graph_node->num_output_ports; ++iport) {
      for (int ipin = 0; ipin < pb_graph_node->num_output_pins[iport]; ++ipin) {
        const t_pb_graph_pin *pin = &pb_graph_node->output_pins[iport][ipin];
        const t_port *port = pin->port;
        string port_class = port->port_class;

        int cluster_pin_idx = pin->pin_count_in_cluster;

        string net;
        if (!top_pb_route.count(cluster_pin_idx)) {
          // Disconnected
          net = "";
        } else {
          // Connected
          auto atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;
          assert(atom_net_id);
          net = make_inst_wire(atom_net_id, find_tnode(atom, cluster_pin_idx),
                               inst_name, PortType::OUTPUT, iport, ipin);
        }

        string port_name;
        if (port_class == "data_out") {
          port_name = "out";
        } else if (port_class == "data_out1") {
          port_name = "out1";
        } else if (port_class == "data_out2") {
          port_name = "out2";
        } else {
          VPR_FATAL_ERROR(
              VPR_ERROR_IMPL_NETLIST_WRITER,
              "Unrecognized input port class '%s' for primitive '%s' (%s)\n",
              port_class.c_str(), atom->name, pb_type->name);
        }
        output_port_conns[port_name].push_back(net);
        ports_tcq[port_name] =
            std::make_pair(pin->tco_max, pin->associated_clock_pin->port->name);
      }
    }

    // Process the clock ports
    for (int iport = 0; iport < pb_graph_node->num_clock_ports; ++iport) {
      assert(pb_graph_node->num_clock_pins[iport] ==
                 1); // Expect a single clock port

      for (int ipin = 0; ipin < pb_graph_node->num_clock_pins[iport]; ++ipin) {
        const t_pb_graph_pin *pin = &pb_graph_node->clock_pins[iport][ipin];
        const t_port *port = pin->port;
        string port_class = port->port_class;

        int cluster_pin_idx = pin->pin_count_in_cluster;
        assert(top_pb_route.count(cluster_pin_idx));
        auto atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;

        assert(atom_net_id); // Must have a clock

        string net =
            make_inst_wire(atom_net_id, find_tnode(atom, cluster_pin_idx),
                           inst_name, PortType::CLOCK, iport, ipin);

        if (port_class == "clock") {
          assert(pb_graph_node->num_clock_pins[iport] ==
                     1); // Expect a single clock pin
          input_port_conns["clock"].push_back(net);
        } else {
          VPR_FATAL_ERROR(
              VPR_ERROR_IMPL_NETLIST_WRITER,
              "Unrecognized input port class '%s' for primitive '%s' (%s)\n",
              port_class.c_str(), atom->name, pb_type->name);
        }
      }
    }

    return std::make_shared<BlackBoxInst>(
        type, inst_name, params, attrs, input_port_conns, output_port_conns,
        timing_arcs, ports_tsu, ports_thld, ports_tcq, opts_);
  }

  ///@brief Returns an Instance object representing a Multiplier
  std::shared_ptr<Instance> make_multiply_instance(const t_pb *atom) {
    auto &timing_ctx = g_vpr_ctx.timing();

    const auto &top_pb_route = find_top_pb_route(atom);
    const t_pb_graph_node *pb_graph_node = atom->pb_graph_node;
    const t_pb_type *pb_type = pb_graph_node->pb_type;

    string type_name = pb_type->model->name;
    string inst_name = join_identifier(type_name, atom->name);
    std::map<string, string> params;
    std::map<string, string> attrs;
    std::map<string, std::vector<string>> input_port_conns;
    std::map<string, std::vector<string>> output_port_conns;
    std::vector<Arc> timing_arcs;
    std::map<string, sequential_port_delay_pair> ports_tsu;
    std::map<string, sequential_port_delay_pair> ports_thld;
    std::map<string, sequential_port_delay_pair> ports_tcq;

    params["WIDTH"] = "0";

    // Delay matrix[sink_tnode] -> tuple of source_port_name, pin index, delay
    std::map<tatum::NodeId, std::vector<std::tuple<string, int, double>>>
        tnode_delay_matrix;

    // Process the input ports
    for (int iport = 0; iport < pb_graph_node->num_input_ports; ++iport) {
      for (int ipin = 0; ipin < pb_graph_node->num_input_pins[iport]; ++ipin) {
        const t_pb_graph_pin *pin = &pb_graph_node->input_pins[iport][ipin];
        const t_port *port = pin->port;
        params["WIDTH"] =
            std::to_string(port->num_pins); // Assume same width on all ports

        int cluster_pin_idx = pin->pin_count_in_cluster;

        string net;
        if (!top_pb_route.count(cluster_pin_idx)) {
          // Disconnected
          net = "";
        } else {
          // Connected
          auto atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;
          assert(atom_net_id);
          auto src_tnode = find_tnode(atom, cluster_pin_idx);
          net = make_inst_wire(atom_net_id, src_tnode, inst_name,
                               PortType::INPUT, iport, ipin);

          // Delays
          //
          // We record the souce sink tnodes and thier delays here
          for (tatum::EdgeId edge :
               timing_ctx.graph->node_out_edges(src_tnode)) {
            double delay = delay_calc_->max_edge_delay(*timing_ctx.graph, edge);

            auto sink_tnode = timing_ctx.graph->edge_sink_node(edge);
            tnode_delay_matrix[sink_tnode].emplace_back(port->name, ipin,
                                                        delay);
          }
        }

        input_port_conns[port->name].push_back(net);
      }
    }

    // Process the output ports
    for (int iport = 0; iport < pb_graph_node->num_output_ports; ++iport) {
      for (int ipin = 0; ipin < pb_graph_node->num_output_pins[iport]; ++ipin) {
        const t_pb_graph_pin *pin = &pb_graph_node->output_pins[iport][ipin];
        const t_port *port = pin->port;

        int cluster_pin_idx = pin->pin_count_in_cluster;
        string net;
        if (!top_pb_route.count(cluster_pin_idx)) {
          // Disconnected
          net = "";
        } else {
          // Connected
          auto atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;
          assert(atom_net_id);

          auto inode = find_tnode(atom, cluster_pin_idx);
          net = make_inst_wire(atom_net_id, inode, inst_name, PortType::OUTPUT,
                               iport, ipin);

          // Record the timing arcs
          for (auto &data_tuple : tnode_delay_matrix[inode]) {
            auto src_name = std::get<0>(data_tuple);
            auto src_ipin = std::get<1>(data_tuple);
            auto delay = std::get<2>(data_tuple);
            timing_arcs.emplace_back(src_name, src_ipin, port->name, ipin,
                                     delay);
          }
        }

        output_port_conns[port->name].push_back(net);
      }
    }

    assert(pb_graph_node->num_clock_ports == 0); // No clocks

    return std::make_shared<BlackBoxInst>(type_name, inst_name, params, attrs,
                                          input_port_conns, output_port_conns,
                                          timing_arcs, ports_tsu, ports_thld,
                                          ports_tcq, opts_);
  }

  ///@brief Returns an Instance object representing an Adder
  std::shared_ptr<Instance> make_adder_instance(const t_pb *atom) {
    auto &timing_ctx = g_vpr_ctx.timing();

    const auto &top_pb_route = find_top_pb_route(atom);
    const t_pb_graph_node *pb_graph_node = atom->pb_graph_node;
    const t_pb_type *pb_type = pb_graph_node->pb_type;

    string type_name = pb_type->model->name;
    string inst_name = join_identifier(type_name, atom->name);
    std::map<string, string> params;
    std::map<string, string> attrs;
    std::map<string, std::vector<string>> input_port_conns;
    std::map<string, std::vector<string>> output_port_conns;
    std::vector<Arc> timing_arcs;
    std::map<string, sequential_port_delay_pair> ports_tsu;
    std::map<string, sequential_port_delay_pair> ports_thld;
    std::map<string, sequential_port_delay_pair> ports_tcq;

    params["WIDTH"] = "0";

    // Delay matrix[sink_tnode] -> tuple of source_port_name, pin index, delay
    std::map<tatum::NodeId, std::vector<std::tuple<string, int, double>>>
        tnode_delay_matrix;

    // Process the input ports
    for (int iport = 0; iport < pb_graph_node->num_input_ports; ++iport) {
      for (int ipin = 0; ipin < pb_graph_node->num_input_pins[iport]; ++ipin) {
        const t_pb_graph_pin *pin = &pb_graph_node->input_pins[iport][ipin];
        const t_port *port = pin->port;

        if (port->name == string("a") || port->name == string("b")) {
          params["WIDTH"] =
              std::to_string(port->num_pins); // Assume same width on all ports
        }

        int cluster_pin_idx = pin->pin_count_in_cluster;

        string net;
        if (!top_pb_route.count(cluster_pin_idx)) {
          // Disconnected
          net = "";
        } else {
          // Connected
          auto atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;
          assert(atom_net_id);

          // Connected
          auto src_tnode = find_tnode(atom, cluster_pin_idx);
          net = make_inst_wire(atom_net_id, src_tnode, inst_name,
                               PortType::INPUT, iport, ipin);

          // Delays
          //
          // We record the souce sink tnodes and thier delays here
          for (tatum::EdgeId edge :
               timing_ctx.graph->node_out_edges(src_tnode)) {
            double delay = delay_calc_->max_edge_delay(*timing_ctx.graph, edge);

            auto sink_tnode = timing_ctx.graph->edge_sink_node(edge);
            tnode_delay_matrix[sink_tnode].emplace_back(port->name, ipin,
                                                        delay);
          }
        }

        input_port_conns[port->name].push_back(net);
      }
    }

    // Process the output ports
    for (int iport = 0; iport < pb_graph_node->num_output_ports; ++iport) {
      for (int ipin = 0; ipin < pb_graph_node->num_output_pins[iport]; ++ipin) {
        const t_pb_graph_pin *pin = &pb_graph_node->output_pins[iport][ipin];
        const t_port *port = pin->port;

        int cluster_pin_idx = pin->pin_count_in_cluster;

        string net;
        if (!top_pb_route.count(cluster_pin_idx)) {
          // Disconnected
          net = "";
        } else {
          // Connected
          auto atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;
          assert(atom_net_id);

          auto inode = find_tnode(atom, cluster_pin_idx);
          net = make_inst_wire(atom_net_id, inode, inst_name, PortType::OUTPUT,
                               iport, ipin);

          // Record the timing arcs
          for (auto &data_tuple : tnode_delay_matrix[inode]) {
            auto src_name = std::get<0>(data_tuple);
            auto src_ipin = std::get<1>(data_tuple);
            auto delay = std::get<2>(data_tuple);
            timing_arcs.emplace_back(src_name, src_ipin, port->name, ipin,
                                     delay);
          }
        }

        output_port_conns[port->name].push_back(net);
      }
    }

    return std::make_shared<BlackBoxInst>(type_name, inst_name, params, attrs,
                                          input_port_conns, output_port_conns,
                                          timing_arcs, ports_tsu, ports_thld,
                                          ports_tcq, opts_);
  }

  std::shared_ptr<Instance> make_blackbox_instance(const t_pb *atom) {
    const auto &top_pb_route = find_top_pb_route(atom);
    const t_pb_graph_node *pb_graph_node = atom->pb_graph_node;
    const t_pb_type *pb_type = pb_graph_node->pb_type;

    auto &timing_ctx = g_vpr_ctx.timing();
    string type_name = pb_type->model->name;
    string inst_name = join_identifier(type_name, atom->name);
    std::map<string, string> params;
    std::map<string, string> attrs;
    std::map<string, std::vector<string>> input_port_conns;
    std::map<string, std::vector<string>> output_port_conns;
    std::vector<Arc> timing_arcs;

    // Maps to store a sink's port with the corresponding timing edge to that
    // sink
    //  - key   : string corresponding to the port's name
    //  - value : pair with the delay and the associated clock pin port name
    //
    //  tsu : Setup
    //  thld: Hold
    //  tcq : Clock-to-Q
    std::map<string, sequential_port_delay_pair> ports_tsu;
    std::map<string, sequential_port_delay_pair> ports_thld;
    std::map<string, sequential_port_delay_pair> ports_tcq;

    // Delay matrix[sink_tnode] -> tuple of source_port_name, pin index, delay
    std::map<tatum::NodeId, std::vector<std::tuple<string, int, double>>>
        tnode_delay_matrix;

    // Process the input ports
    for (int iport = 0; iport < pb_graph_node->num_input_ports; ++iport) {
      for (int ipin = 0; ipin < pb_graph_node->num_input_pins[iport]; ++ipin) {
        const t_pb_graph_pin *pin = &pb_graph_node->input_pins[iport][ipin];
        const t_port *port = pin->port;

        int cluster_pin_idx = pin->pin_count_in_cluster;

        string net;
        if (!top_pb_route.count(cluster_pin_idx)) {
          // Disconnected
          net = "";

        } else {
          // Connected
          auto atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;
          assert(atom_net_id);

          auto src_tnode = find_tnode(atom, cluster_pin_idx);
          net = make_inst_wire(atom_net_id, src_tnode, inst_name,
                               PortType::INPUT, iport, ipin);
          // Delays
          //
          // We record the source's sink tnodes and their delays here
          for (tatum::EdgeId edge :
               timing_ctx.graph->node_out_edges(src_tnode)) {
            double delay = delay_calc_->max_edge_delay(*timing_ctx.graph, edge);

            auto sink_tnode = timing_ctx.graph->edge_sink_node(edge);
            tnode_delay_matrix[sink_tnode].emplace_back(port->name, ipin,
                                                        delay);
          }
        }

        input_port_conns[port->name].push_back(net);
        if (pin->type == PB_PIN_SEQUENTIAL) {
          if (!std::isnan(pin->tsu))
            ports_tsu[port->name] =
                std::make_pair(pin->tsu, pin->associated_clock_pin->port->name);
          if (!std::isnan(pin->thld))
            ports_thld[port->name] = std::make_pair(
                pin->thld, pin->associated_clock_pin->port->name);
        }
      }
    }

    // Process the output ports
    for (int iport = 0; iport < pb_graph_node->num_output_ports; ++iport) {
      for (int ipin = 0; ipin < pb_graph_node->num_output_pins[iport]; ++ipin) {
        const t_pb_graph_pin *pin = &pb_graph_node->output_pins[iport][ipin];
        const t_port *port = pin->port;

        int cluster_pin_idx = pin->pin_count_in_cluster;

        string net;
        if (!top_pb_route.count(cluster_pin_idx)) {
          // Disconnected
          net = "";
        } else {
          // Connected
          auto atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;
          assert(atom_net_id);

          auto inode = find_tnode(atom, cluster_pin_idx);
          net = make_inst_wire(atom_net_id, inode, inst_name, PortType::OUTPUT,
                               iport, ipin);
          // Record the timing arcs
          for (auto &data_tuple : tnode_delay_matrix[inode]) {
            auto src_name = std::get<0>(data_tuple);
            auto src_ipin = std::get<1>(data_tuple);
            auto delay = std::get<2>(data_tuple);
            timing_arcs.emplace_back(src_name, src_ipin, port->name, ipin,
                                     delay);
          }
        }

        output_port_conns[port->name].push_back(net);
        if (pin->type == PB_PIN_SEQUENTIAL && !std::isnan(pin->tco_max))
          ports_tcq[port->name] = std::make_pair(
              pin->tco_max, pin->associated_clock_pin->port->name);
      }
    }

    // Process the clock ports
    for (int iport = 0; iport < pb_graph_node->num_clock_ports; ++iport) {
      for (int ipin = 0; ipin < pb_graph_node->num_clock_pins[iport]; ++ipin) {
        const t_pb_graph_pin *pin = &pb_graph_node->clock_pins[iport][ipin];
        const t_port *port = pin->port;

        int cluster_pin_idx = pin->pin_count_in_cluster;

        string net;
        if (!top_pb_route.count(cluster_pin_idx)) {
          // Disconnected
          net = "";
        } else {
          // Connected
          auto atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;
          assert(atom_net_id); // Must have a clock

          auto src_tnode = find_tnode(atom, cluster_pin_idx);
          net = make_inst_wire(atom_net_id, src_tnode, inst_name,
                               PortType::CLOCK, iport, ipin);
        }

        input_port_conns[port->name].push_back(net);
      }
    }

    auto &atom_ctx = g_vpr_ctx.atom();
    AtomBlockId blk_id = atom_ctx.lookup.pb_atom(atom);
    for (auto param : atom_ctx.nlist.block_params(blk_id)) {
      params[param.first] = param.second;
    }

    for (auto attr : atom_ctx.nlist.block_attrs(blk_id)) {
      attrs[attr.first] = attr.second;
    }

    return std::make_shared<BlackBoxInst>(type_name, inst_name, params, attrs,
                                          input_port_conns, output_port_conns,
                                          timing_arcs, ports_tsu, ports_thld,
                                          ports_tcq, opts_);
  }

  ///@brief Returns the top level pb_route associated with the given pb
  const t_pb_routes &find_top_pb_route(const t_pb *curr) {
    const t_pb *top_pb = find_top_cb(curr);
    return top_pb->pb_route;
  }

  ///@brief Returns the tnode ID of the given atom's connected cluster pin
  tatum::NodeId find_tnode(const t_pb *atom, int cluster_pin_idx) {
    auto &atom_ctx = g_vpr_ctx.atom();

    AtomBlockId blk_id = atom_ctx.lookup.pb_atom(atom);
    ClusterBlockId clb_index = atom_ctx.lookup.atom_clb(blk_id);

    auto key = std::make_pair(clb_index, cluster_pin_idx);
    auto iter = pin_id_to_tnode_lookup_.find(key);
    assert(iter != pin_id_to_tnode_lookup_.end());

    tatum::NodeId tnode_id = iter->second;
    assert(tnode_id);

    return tnode_id;
  }

private:
  ///@brief Returns the top complex block which contains the given pb
  const t_pb *find_top_cb(const t_pb *curr) {
    // Walk up through the pb graph until curr
    // has no parent, at which point it will be the top pb
    const t_pb *parent = curr->parent_pb;
    while (parent != nullptr) {
      curr = parent;
      parent = curr->parent_pb;
    }
    return curr;
  }

  ///@brief Returns a LogicVec representing the LUT mask of the given LUT atom
  LogicVec load_lut_mask(size_t num_inputs,  // LUT size
                         const t_pb *atom) { // LUT primitive
    auto &atom_ctx = g_vpr_ctx.atom();

    const t_model *model =
        atom_ctx.nlist.block_model(atom_ctx.lookup.pb_atom(atom));
    assert(model->name == string(MODEL_NAMES));

#ifdef DEBUG_LUT_MASK
    cout << "Loading LUT mask for: " << atom->name << endl;
#endif

    // Figure out how the inputs were permuted (compared to the input netlist)
    std::vector<int> permute = determine_lut_permutation(num_inputs, atom);

    // Retrieve the truth table
    const auto &truth_table =
        atom_ctx.nlist.block_truth_table(atom_ctx.lookup.pb_atom(atom));

    // Apply the permutation
    auto permuted_truth_table =
        permute_truth_table(truth_table, num_inputs, permute);

    // Convert to lut mask
    LogicVec lut_mask =
        truth_table_to_lut_mask(permuted_truth_table, num_inputs);

#ifdef DEBUG_LUT_MASK
    cout << "\tLUT_MASK: " << lut_mask << endl;
#endif

    return lut_mask;
  }

  /**
   * @brief Helper function for load_lut_mask() which determines how the LUT
   * inputs were permuted compared to the input BLIF
   *
   * Since the LUT inputs may have been rotated from the input blif
   * specification we need to figure out this permutation to reflect the
   * physical implementation connectivity.
   *
   * We return a permutation map (which is a list of swaps from index to
   * index) which is then applied to do the rotation of the lutmask.
   *
   * The net in the atom netlist which was originally connected to pin i, is
   * connected to pin permute[i] in the implementation.
   */
  std::vector<int> determine_lut_permutation(size_t num_inputs,
                                             const t_pb *atom_pb) {
    auto &atom_ctx = g_vpr_ctx.atom();

    std::vector<int> permute(num_inputs, OPEN);

#ifdef DEBUG_LUT_MASK
    cout << "\tInit Permute: {";
    for (size_t i = 0; i < permute.size(); i++) {
      cout << permute[i] << ", ";
    }
    cout << "}" << endl;
#endif

    // Determine the permutation
    //
    // We walk through the logical inputs to this atom (i.e. in the original
    // truth table/netlist) and find the corresponding input in the
    // implementation atom (i.e. in the current netlist)
    auto ports =
        atom_ctx.nlist.block_input_ports(atom_ctx.lookup.pb_atom(atom_pb));
    if (ports.size() == 1) {
      const t_pb_graph_node *gnode = atom_pb->pb_graph_node;
      assert(gnode->num_input_ports == 1);
      assert(gnode->num_input_pins[0] >= (int)num_inputs);

      AtomPortId port_id = *ports.begin();

      for (size_t ipin = 0; ipin < num_inputs; ++ipin) {
        AtomNetId impl_input_net_id = find_atom_input_logical_net(
            atom_pb, ipin); // The net currently connected to input j

        // Find the original pin index
        const t_pb_graph_pin *gpin = &gnode->input_pins[0][ipin];
        BitIndex orig_index = atom_pb->atom_pin_bit_index(gpin);

        if (impl_input_net_id) {
          // If there is a valid net connected in the implementation
          AtomNetId logical_net_id =
              atom_ctx.nlist.port_net(port_id, orig_index);

          // Fatal error should be flagged when the net marked in
          // implementation does not match the net marked in input netlist
          if (impl_input_net_id != logical_net_id) {
            VPR_FATAL_ERROR(VPR_ERROR_IMPL_NETLIST_WRITER,
                            "Unmatch:\n\tlogical net is '%s' at pin "
                            "'%lu'\n\timplmented net is '%s' at pin '%s'\n",
                            atom_ctx.nlist.net_name(logical_net_id).c_str(),
                            size_t(orig_index),
                            atom_ctx.nlist.net_name(impl_input_net_id).c_str(),
                            gpin->to_string().c_str());
          }

          // Mark the permutation.
          //  The net originally located at orig_index in the atom netlist
          //  was moved to ipin in the implementation
          permute[orig_index] = ipin;
        }
      }
    } else {
      // May have no inputs on a constant generator
      assert(ports.size() == 0);
    }

    // Fill in any missing values in the permutation (i.e. zeros)
    std::set<int> perm_indicies(permute.begin(), permute.end());
    size_t unused_index = 0;
    for (size_t i = 0; i < permute.size(); i++) {
      if (permute[i] == OPEN) {
        while (perm_indicies.count(unused_index)) {
          unused_index++;
        }
        permute[i] = unused_index;
        perm_indicies.insert(unused_index);
      }
    }

#ifdef DEBUG_LUT_MASK
    cout << "\tPermute: {";
    for (size_t k = 0; k < permute.size(); k++) {
      cout << permute[k] << ", ";
    }
    cout << "}" << endl;

    cout << "\tBLIF = Input ->  Rotated" << endl;
    cout << "\t------------------------" << endl;
#endif
    return permute;
  }

  /**
   * @brief Helper function for load_lut_mask() which determines if the
   *        names is encodeing the ON (returns true) or OFF (returns false)
   * set.
   */
  bool names_encodes_on_set(vtr::t_linked_vptr *names_row_ptr) {
    // Determine the truth (output value) for this row
    // By default we assume the on-set is encoded to correctly handle
    // constant true/false
    //
    // False output:
    //      .names j
    //
    // True output:
    //      .names j
    //      1
    bool encoding_on_set = true;

    // We may get a nullptr if the output is always false
    if (names_row_ptr) {
      // Determine whether the truth table stores the ON or OFF set
      //
      //  In blif, the 'output values' of a .names must be either '1' or '0',
      //  and must be consistent within a single .names -- that is a single
      //  .names can encode either the ON or OFF set (of which only one will
      //  be encoded in a single .names)
      //
      const string names_first_row =
          (const char *)names_row_ptr->data_vptr;
      auto names_first_row_output_iter = names_first_row.end() - 1;

      if (*names_first_row_output_iter == '1') {
        encoding_on_set = true;
      } else if (*names_first_row_output_iter == '0') {
        encoding_on_set = false;
      } else {
        VPR_FATAL_ERROR(VPR_ERROR_IMPL_NETLIST_WRITER,
                        "Invalid .names truth-table character '%c'. Must be "
                        "one of '1', '0' or '-'. \n",
                        *names_first_row_output_iter);
      }
    }

    return encoding_on_set;
  }

  /**
   * @brief Helper function for load_lut_mask()
   *
   * Converts the given names_row string to a LogicVec
   */
  LogicVec names_row_to_logic_vec(const string names_row,
                                  size_t num_inputs, bool encoding_on_set) {
    // Get an iterator to the last character (i.e. the output value)
    auto output_val_iter = names_row.end() - 1;

    // Sanity-check, the 2nd last character should be a space
    auto space_iter = names_row.end() - 2;
    assert(*space_iter == ' ');

    // Extract the truth (output value) for this row
    if (*output_val_iter == '1') {
      assert(encoding_on_set);
    } else if (*output_val_iter == '0') {
      assert(!encoding_on_set);
    } else {
      VPR_FATAL_ERROR(VPR_ERROR_IMPL_NETLIST_WRITER,
                      "Invalid .names encoding both ON and OFF set\n");
    }

    // Extract the input values for this row
    LogicVec input_values(num_inputs, vtr::LogicValue::FALSE);
    size_t i = 0;
    // Walk through each input in the input cube for this row
    while (names_row[i] != ' ') {
      vtr::LogicValue input_val = vtr::LogicValue::UNKOWN;
      if (names_row[i] == '1') {
        input_val = vtr::LogicValue::TRUE;
      } else if (names_row[i] == '0') {
        input_val = vtr::LogicValue::FALSE;
      } else if (names_row[i] == '-') {
        input_val = vtr::LogicValue::DONT_CARE;
      } else {
        VPR_FATAL_ERROR(VPR_ERROR_IMPL_NETLIST_WRITER,
                        "Invalid .names truth-table character '%c'. Must be "
                        "one of '1', '0' or '-'. \n",
                        names_row[i]);
      }

      input_values[i] = input_val;
      i++;
    }
    return input_values;
  }

  ///@brief Returns the total number of input pins on the given pb
  int find_num_inputs(const t_pb *pb) {
    int count = 0;
    for (int i = 0; i < pb->pb_graph_node->num_input_ports; i++) {
      count += pb->pb_graph_node->num_input_pins[i];
    }
    return count;
  }
  ///@brief Returns the logical net ID
  AtomNetId find_atom_input_logical_net(const t_pb *atom, int atom_input_idx) {
    const t_pb_graph_node *pb_node = atom->pb_graph_node;

    int cluster_pin_idx =
        pb_node->input_pins[0][atom_input_idx].pin_count_in_cluster;
    const auto &top_pb_route = find_top_pb_route(atom);
    AtomNetId atom_net_id;
    if (top_pb_route.count(cluster_pin_idx)) {
      atom_net_id = top_pb_route[cluster_pin_idx].atom_net_id;
    }
    return atom_net_id;
  }

  ///@brief Returns the name of the routing segment between two wires
  string interconnect_name(string driver_wire,
                                string sink_wire) {
    string name = join_identifier("routing_segment", driver_wire);
    name = join_identifier(name, "to");
    name = join_identifier(name, sink_wire);

    return name;
  }

  ///@brief Returns the delay in pico-seconds from source_tnode to sink_tnode
  double get_delay_ps(tatum::NodeId source_tnode, tatum::NodeId sink_tnode) {
    auto &timing_ctx = g_vpr_ctx.timing();

    tatum::EdgeId edge = timing_ctx.graph->find_edge(source_tnode, sink_tnode);
    assert(edge);

    double delay_sec = delay_calc_->max_edge_delay(*timing_ctx.graph, edge);

    return rsbe::get_delay_ps(
        delay_sec); // Class overload hides file-scope by default
  }

private: // Data
  string
      top_module_name_; ///< Name of the top level module (i.e. the circuit)
protected:
  std::vector<string> inputs_;  ///< Name of circuit inputs
  std::vector<string> outputs_; ///< Name of circuit outputs
  std::vector<Assignment>
      assignments_; ///< Set of assignments (i.e. net-to-net connections)
  std::vector<std::shared_ptr<Instance>>
      cell_instances_; ///< Set of cell instances

private:
  // Drivers of logical nets.
  // Key: logic net id, Value: pair of wire_name and tnode_id
  std::map<AtomNetId, std::pair<string, tatum::NodeId>>
      logical_net_drivers_;

  // Sinks of logical nets.
  // Key: logical net id, Value: vector wire_name tnode_id pairs
  std::map<AtomNetId, std::vector<std::pair<string, tatum::NodeId>>>
      logical_net_sinks_;
  std::map<string, float> logical_net_sink_delays_;

  // Output streams
protected:
  std::ostream& verilog_os_;

private:
  std::ostream& sdf_os_;
  std::ostream& lib_os_;

  // Look-up from pins to tnodes
  std::map<std::pair<ClusterBlockId, int>, tatum::NodeId>
      pin_id_to_tnode_lookup_;

  std::shared_ptr<const AnalysisDelayCalculator> delay_calc_;
  struct t_analysis_opts opts_;
};

////////////////////////////////////////////////////////////////////////////////
// class methods
////////////////////////////////////////////////////////////////////////////////
bool sta_file_writer::write_sta_files(int argc, const char** argv) const
{
  // vpr context
  auto& atom_ctx = g_vpr_ctx.atom();
  auto& timing_ctx = g_vpr_ctx.timing();
  auto& design_name_ = atom_ctx.nlist.netlist_name();

  // io initialization
  cout << "STARS: Input design <" << design_name_ << ">" << endl;
  string lib_filename = design_name_ + "_stars.lib";
  string verilog_filename = design_name_ + "_stars.v";
  string sdf_filename = design_name_ + "_stars.sdf";
  string sdc_filename = design_name_ + "_stars.sdc";
  std::ofstream lib_os(lib_filename);
  std::ofstream verilog_os(verilog_filename);
  std::ofstream sdf_os(sdf_filename);
  std::ofstream sdc_os(sdc_filename);

  // timing data preparation
  auto &cluster_ctx = g_vpr_ctx.clustering();
  auto net_delay = make_net_pins_matrix<float>((const Netlist<>&)cluster_ctx.clb_nlist);

  load_net_delay_from_routing((const Netlist<>&)g_vpr_ctx.clustering().clb_nlist, net_delay, true);

  auto analysis_delay_calc = std::make_shared<AnalysisDelayCalculator>(
      atom_ctx.nlist, atom_ctx.lookup, net_delay, true);

  // walk netlist to dump info
  StaWriterVisitor visitor(verilog_os, sdf_os, lib_os, analysis_delay_calc,
                           get_vprSetup()->AnalysisOpts);
  NetlistWalker nl_walker(visitor);
  nl_walker.walk();

  // sdc file
  // sdc file is a copy of sdc file which is used by vpr
  // more and complete sdc file should be developed later
  for (int i = 0; i < argc; i++) {
    if (std::strcmp(argv[i], "--sdc_file") == 0) {
      std::ifstream in_file(argv[i + 1]);
      string line;
      if (in_file) {
        while (getline(in_file, line)) {
          sdc_os << line << endl;
        }
      }
      in_file.close();
      break;
    }
  }

  cout << "STARS: Created files <" << lib_filename << ">, <"
            << verilog_filename << ">, <" << sdf_filename << ">, <"
            << sdc_filename << ">." << endl;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// API methods
////////////////////////////////////////////////////////////////////////////////
bool create_sta_files(int argc, const char** argv) {
  sta_file_writer wr;
  return wr.write_sta_files(argc, argv);
}

}  // NS rsbe

