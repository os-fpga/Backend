#pragma once
#ifndef __rsbe__stars__rsDB_H_h_
#define __rsbe__stars__rsDB_H_h_

#include "pinc_log.h"

// vpr/src/base
#include "atom_netlist.h"
#include "atom_netlist_utils.h"
#include "globals.h"
#include "logic_vec.h"
#include "netlist_walker.h"
#include "vpr_types.h"
#include "vtr_version.h"

// vpr/src/timing
#include "AnalysisDelayCalculator.h"
#include "net_delay.h"

#include "rsGlobal.h"
#include "rsVPR.h"
#include "LCell.h"
#include "sta_file_writer.h"
#include "sta_lib_writer.h"

#include <map>
#include <set>

namespace rsbe {

using std::endl;
using std::string;
using std::stringstream;
using std::ostream;
using std::vector;
using namespace pinc;
using CStr = const char*;

// A combinational timing arc
struct Arc
{
  Arc(const string& src_port,  ///< Source of the arc
      int src_ipin,            ///< Source pin index
      const string& snk_port,  ///< Sink of the arc
      int snk_ipin,            ///< Sink pin index
      float del,               ///< Delay on this arc
      const string& cond = "") noexcept
  : source_name_(src_port),
    sink_name_(snk_port),
    condition_(cond),
    source_ipin_(src_ipin),
    sink_ipin_(snk_ipin),
    delay_(del)
  { }

  // Accessors
  const string& source_name() const noexcept { return source_name_; }
  int source_ipin() const noexcept { return source_ipin_; }
  const string& sink_name() const noexcept { return sink_name_; }
  int sink_ipin() const noexcept { return sink_ipin_; }
  double delay() const noexcept { return delay_; }
  const string& condition() const noexcept { return condition_; }

//DATA:
  string source_name_;
  string sink_name_;
  string condition_;
  int source_ipin_ = -1;
  int sink_ipin_ = -1;
  double delay_ = -1;
};

/**
 * @brief Cell is an interface used to represent an element instantiated in a netlist
 *
 * Cells know how to describe themselves in BLIF, Verilog and SDF
 *
 * This should be subclassed to implement support for new primitive types
 * (although see BlackBoxInst for a general implementation for a black box
 * primitive)
 */
class Cell {
public:
  virtual ~Cell() = default;

  virtual void printVerilog(ostream& os, size_t& unconn_count, int depth = 0) const = 0;
  virtual void printSDF(ostream& os, int depth = 0) const = 0;
  virtual void printLib(LibWriter& lib_writer, ostream& os) const = 0;
  virtual CStr get_type_name() const = 0;
};

///@brief An instance representing a Look-Up Table
class LutCell : public Cell {
public:
  LutCell(uint lut_size,
          LogicVec lut_mask,                            ///< The LUT mask representing the logic function
          const string& inst_name,                      ///< The name of this instance
          std::map<string, vector<string>> port_conns,  ///< The port connections of this instance. Key: port
                                                        ///< name, Value: connected nets
          vector<Arc> timing_arc_values,                ///< The timing arcs of this instance
          const t_analysis_opts& opts);

  virtual ~LutCell();

  const vector<Arc>& timing_arcs() const { return timing_arcs_; }
  const string& instance_name() const { return inst_name_; }
  const string& lut_type() const { return lut_type_; }

public:  // Cell interface method implementations
  virtual CStr get_type_name() const override { return lut_type_.c_str(); }

  virtual void printLib(LibWriter& lib_writer, ostream& os) const override;

  virtual void printSDF(ostream& os, int depth) const override;

  virtual void printVerilog(ostream& os, size_t& unconn_count, int depth) const override;

private:
  string lut_type_;
  uint lut_size_ = 0;
  LogicVec lut_mask_;
  string inst_name_;

  mutable std::map<string, vector<string>> port_conns_; // TODO: no need for std::map here

  vector<Arc> timing_arcs_;
  t_analysis_opts opts_;
};

class LatchInst : public Cell {
public:  // Public types
  ///@brief Types of latches (defined by BLIF)
  enum class Type {
    RISING_EDGE,
    FALLING_EDGE,
    ACTIVE_HIGH,
    ACTIVE_LOW,
    ASYNCHRONOUS,
  };
  friend ostream& operator<<(ostream& os, const Type& type) {
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
  friend std::istream& operator>>(std::istream& is, Type& type) {
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
  LatchInst(string inst_name,                                        ///< Name of this instance
            std::map<string, string> port_conns,                     ///< Cell's port-to-net connections
            Type type,                                               ///< Type of this latch
            vtr::LogicValue init_value,                              ///< Initial value of the latch
            double tcq = std::numeric_limits<double>::quiet_NaN(),   ///< Clock-to-Q delay
            double tsu = std::numeric_limits<double>::quiet_NaN(),   ///< Setup time
            double thld = std::numeric_limits<double>::quiet_NaN())  ///< Hold time
      : instance_name_(inst_name),
        port_connections_(port_conns),
        type_(type),
        initial_value_(init_value),
        tcq_(tcq),
        tsu_(tsu),
        thld_(thld) {}

  virtual CStr get_type_name() const override { return "DFF"; }

  virtual void printLib(LibWriter& lib_writer, ostream& os) const override {
    /*
    os << indent(depth + 1) << "LIBERTY FOR: (INSTANCE "
       << escape_sdf_identifier(instance_name_) << ")\n";
       */
  }

  virtual void printSDF(ostream& os, int depth = 0) const override;

  virtual void printVerilog(ostream& os, size_t& unconn_count, int depth = 0) const override;

private:
  string instance_name_;
  std::map<string, string> port_connections_;
  Type type_;
  vtr::LogicValue initial_value_;
  double tcq_  = -1;  ///< Clock delay + tcq
  double tsu_  = -1;  ///< Setup time
  double thld_ = -1;  ///< Hold time
};

class BlackBoxInst : public Cell {
public:
  BlackBoxInst(string type_name,                 ///< Cell type
               string inst_name,                 ///< Cell name
               std::map<string, string> params,  ///< Verilog parameters: Dictonary of <param_name,value>
               std::map<string, string> attrs,   ///< Cell attributes: Dictonary of <attr_name,value>
               std::map<string, vector<string>>
                   input_port_conns,  ///< Port connections: Dictionary of <port,nets>
               std::map<string, vector<string>>
                   output_port_conns,         ///< Port connections: Dictionary of <port,nets>
               vector<Arc> timing_arcs,  ///< Combinational timing arcs
               std::map<string, sequential_port_delay_pair> ports_tsu,   ///< Port setup checks
               std::map<string, sequential_port_delay_pair> ports_thld,  ///< Port hold checks
               std::map<string, sequential_port_delay_pair> ports_tcq,   ///< Port clock-to-q delays
               const t_analysis_opts& opts)
      : type_name_(type_name),
        inst_name_(inst_name),
        params_(params),
        attrs_(attrs),
        input_port_conns_(input_port_conns),
        output_port_conns_(output_port_conns),
        timing_arcs_(timing_arcs),
        ports_tsu_(ports_tsu),
        ports_thld_(ports_thld),
        ports_tcq_(ports_tcq),
        opts_(opts) {}

  virtual void printLib(LibWriter& lib_writer, ostream& os) const override;

  virtual void printSDF(ostream& os, int depth = 0) const override;

  virtual void printVerilog(ostream& os, size_t& unconn_count, int depth = 0) const override;

  uint find_port_size(const string& port_name) const noexcept;

  virtual CStr get_type_name() const override { return type_name_.c_str(); }

private:
  string type_name_;
  string inst_name_;

  std::map<string, string> params_;
  std::map<string, string> attrs_;
  std::map<string, vector<string>> input_port_conns_;
  std::map<string, vector<string>> output_port_conns_;
  vector<Arc> timing_arcs_;
  std::map<string, sequential_port_delay_pair> ports_tsu_;
  std::map<string, sequential_port_delay_pair> ports_thld_;
  std::map<string, sequential_port_delay_pair> ports_tcq_;

  t_analysis_opts opts_;
};  // BlackBoxInst

/**
 * @brief Assignment represents the logical connection between two nets
 *
 * This is synonomous with verilog's 'assign x = y' which connects
 * two nets with logical identity, assigning the value of 'y' to 'x'
 */
class Assignment {
public:
  Assignment(string lval,  ///< The left value (assigned to)
             string rval)  ///< The right value (assigned from)
      : lval_(lval), rval_(rval) {}

  void printVerilog(ostream& os, string indent) const {
    os << indent << "assign " << escape_verilog_identifier(lval_) << " = " << escape_verilog_identifier(rval_)
       << ";\n";
  }

private:
  string lval_;
  string rval_;
};

}

#endif

