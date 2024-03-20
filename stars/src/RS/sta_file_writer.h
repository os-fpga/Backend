#pragma once
#ifndef __pln__sta_file_writer_H_h_
#define __pln__sta_file_writer_H_h_

#include "globals.h"
#include "netlist_walker.h"
#include "vpr_context.h"
#include "vpr_types.h"

namespace rsbe {

using std::string;

class FileWriter {
private:
  string design_name_;

  string lib_fn_;      // = design_name_ + "_stars.lib";
  string verilog_fn_;  // = design_name_ + "_stars.v";
  string sdf_fn_;      // = design_name_ + "_stars.sdf";
  string sdc_fn_;      // = design_name_ + "_stars.sdc";

  FileWriter() { design_name_ = g_vpr_ctx.atom().nlist.netlist_name(); }

  bool do_files(int argc, const char** argv);
  void printStats() const;

public:

  // Unconnected net prefix
  static constexpr const char* unconn_prefix = "__vpr__unconn";

public:

  // API
  static bool create_files(int argc, const char** argv);
};

// This pair cointains the following values:
//      - double: hold, setup or clock-to-q delays of the port
//      - string: port name of the associated source clock pin of the sequential
//      port
using sequential_port_delay_pair = std::pair<double, std::string>;

inline string join_identifier(const string& lhs, const string& rhs) noexcept {
  return pln::str::concat(lhs, "_", rhs);
}

///@brief Returns a blank string for indenting the given depth
inline string indent(size_t depth) noexcept {
  const char* step = "    ";
  string new_indent;
  new_indent.reserve(depth + ::strlen(step) + 1);
  for (size_t i = 0; i < depth; ++i) {
    new_indent += step;
  }
  return new_indent;
}

///@brief Returns true if c is categorized as a special character in SDF
inline bool is_special_sdf_char(char c) noexcept {
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
  if ((c >= 33 && c <= 35) || (c == 36) ||  // $
      (c >= 37 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 94) || (c == 96) ||
      (c >= 123 && c <= 126)) {
    return true;
  }

  return false;
}

///@brief Escapes the given identifier to be safe for sdf
inline string escape_sdf_identifier(const string& identifier) noexcept {
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

///@brief Escapes the given identifier to be safe for verilog
inline string escape_verilog_identifier(const string& identifier) noexcept {
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

///@brief Returns the delay in pico-seconds from a floating point delay
inline double get_delay_ps(double delay_sec) noexcept {
  return delay_sec * 1e12;  // Scale to picoseconds
}

}

#endif
