#pragma once
////////////////////////////////////////////////////////////////////////////////
// Important:                                                                 //
//    This is for rs internally - don't make this part public until a written //
//    approval is obtained from rapidsilicon open source review commitee      //
////////////////////////////////////////////////////////////////////////////////

#ifndef __rs_PIN_LOCATION_H
#define __rs_PIN_LOCATION_H

#include <set>

#include "cmd_line.h"
#include "geo/iv.h"
#include "pinc_log.h"

namespace pinc
{

  class rapidCsvReader;

  using std::string;
  using std::vector;

  class pin_location
  {
  private:
    // error messages to be printed out with std::cerr
    enum
    {
      MISSING_IN_OUT_FILES = 0,
      PIN_LOC_XML_PARSE_ERROR,
      PIN_MAP_CSV_PARSE_ERROR,
      PIN_CONSTRAINT_PARSE_ERROR,
      INPUT_DESIGN_PARSE_ERROR,
      CONSTRAINED_PORT_NOT_FOUND,
      CONSTRAINED_PIN_NOT_FOUND,
      RE_CONSTRAINED_PORT,
      OVERLAP_PIN_IN_CONSTRAINT,
      GENERATE_CSV_FILE_FOR_OS_FLOW,
      OPEN_FILE_FAILURE,
      CLOSE_FILE_FAILURE,
      PIN_SOURCE_NO_SURFFICENT,
      FAIL_TO_CREATE_TEMP_PCF,
      FAIL_TO_CREATE_CLKS_CONSTRAINT_XML,
      INCORRECT_ASSIGN_PIN_METHOD,
      GENERATE_PCF_FILE_FOR_OS_FLOW,
      MAX_MESSAGE_ID
    };

    string error_messages[MAX_MESSAGE_ID] = {
        "Missing input or output file arguments",              // MISSING_IN_OUT_FILES
        "Pin location file parse error",                       // PIN_LOC_XML_PARSE_ERROR
        "Pin map file parse error",                            // PIN_MAP_CSV_PARSE_ERROR
        "Pin constraint file parse error",                     // PIN_CONSTRAINT_PARSE_ERROR
        "Input design parse error",                            // INPUT_DESIGN_PARSE_ERROR
        "Constrained port not found in design",                // CONSTRAINED_PORT_NOT_FOUND
        "Constrained pin not found in device",                 // CONSTRAINED_PIN_NOT_FOUND
        "Re-constrained port",                                 // RE_CONSTRAINED_PORT
        "Overlap pin found in constraint",                     // OVERLAP_PIN_IN_CONSTRAINT
        "Fail to generate csv file for open source flow",      // GENERATE_CSV_FILE_FOR_OS_FLOW
        "Open file failure",                                   // OPEN_FILE_FAILURE
        "Close file failure",                                  // CLOSE_FILE_FAILURE
        "Design requires more IO pins than device can supply", // PIN_SOURCE_NO_SURFFICENT
        "Fail to create temp pcf file",                        // FAIL_TO_CREATE_TEMP_PCF
        "Fail to create fpga constraint xml file",             // FAIL_TO_CREATE_CLKS_CONSTRAINT_XML
        "Incorrect assign pin method selection"                // INCORRECT_ASSIGN_PIN_METHOD
        "Fail to generate pcf file for open source flow",      // GENERATE_PCF_FILE_FOR_OS_FLOW
    };

    enum
    {
      ASSIGN_IN_RANDOM = 0,
      ASSIGN_IN_DEFINE_ORDER
    } pin_assign_method_;

#define CERROR std::cerr << "[Error] "

    // port direction
    enum PortDirection
    {
      INPUT = 0,
      OUTPUT
    };

// use fix to detemined A2F and F2A GBX mode
#define INPUT_MODE_FIX "_RX"
#define OUTPUT_MODE_FIX "_TX"

// for mpw1  (no gearbox, a Mode_GPIO is created)
#define GPIO_MODE_FIX "_GPIO"

    cmd_line cl_;

    string temp_csv_file_name_;
    string temp_pcf_file_name_;
    string temp_os_pcf_file_name_;

    vector<string> user_design_inputs_;
    vector<string> user_design_outputs_;

    vector<vector<string>> pcf_pin_cmds_;
    std::set<string> used_bump_pins_;
    vector<string> set_clks;

  public:
    enum class PortDir : uint8_t
    {
      Undefined = 0,
      Input = 1,
      Output = 2,
      Bidi = 3
    };

    enum class PortType : uint8_t
    {
      Undefined = 0,
      Wire = 1,
      Reg = 2
    };

    enum class Order : uint8_t
    {
      Undefined = 0,
      LSB_to_MSB = 1,
      MSB_to_LSB = 2
    };

    struct PortInfo
    {
      string name_;
      PortDir dir_ = PortDir::Undefined;
      PortType type_ = PortType::Undefined;
      Order order_ = Order::Undefined;
      Iv range_;

      PortInfo() = default;

      bool isInput() const noexcept { return dir_ == PortDir::Input; }
      bool isOutput() const noexcept { return dir_ == PortDir::Output; }
      bool hasRange() const noexcept { return range_.valid(); }
      bool hasOrder() const noexcept { return order_ != Order::Undefined; }

      // define_order: "LSB_TO_MSB"
      bool is_LSB_to_MSB() const noexcept
      {
        return hasRange() && order_ == Order::LSB_to_MSB;
      }
      bool is_MSB_to_LSB() const noexcept
      {
        return hasRange() && order_ == Order::MSB_to_LSB;
      }
    };

    pin_location(const cmd_line &cl);
    ~pin_location();

    const cmd_line &get_cmd() const noexcept { return cl_; }

    bool reader_and_writer();

    bool generate_csv_file_for_os_flow();
    bool read_csv_file(rapidCsvReader &);
    bool read_user_design();
    bool read_user_pinloc_constrain_file();

    bool create_place_file(rapidCsvReader &);

    bool create_temp_constrain_file(rapidCsvReader &rs_csv_rd);

    static void shuffle_candidates(vector<int> &v);

    // void collect_left_available_device_pins(set<string>
    // &constrained_device_pins,
    //                                   vector<int>
    //                                   &left_available_device_pin_idx,
    //                                   rapidCsvReader &rs_csv_reader);

    bool convert_pcf_file_for_os_flow(string pcf_file_name);

    bool get_available_bump_pin(rapidCsvReader &rs_csv_rd,
                                std::pair<string, string> &bump_pin_and_mode,
                                PortDirection port_direction);

    bool is_input_mode(const string &mode_name) const;
    bool is_output_mode(const string &mode_name) const;

    static bool read_port_info(std::ifstream &json_ifs, vector<string> &inputs,
                               vector<string> &outputs);
    bool logical_clocks_to_GEMINI_clks();
  };

} // namespace pinc

int pin_constrain_location(const pinc::cmd_line &cmd);

#endif
