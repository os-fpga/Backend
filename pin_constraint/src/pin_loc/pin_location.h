////////////////////////////////////////////////////////////////////////////////
// Important:                                                                 //
//    This is for rs internally - don't make this part public until a written //
//    approval is obtained from rapidsilicon open source review commitee      //
//////////////////////////////////////////////////////////////////////////////// 

#ifndef PIN_LOCATION
#define PIN_LOCATION
#include "cmd_line.h"
#include <set>


class rapidCsvReader;
class pin_location {
private:
    // error messages to be printed out with std::cerr 
    enum {
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
        INCORRECT_ASSIGN_PIN_METHOD,
        MAX_MESSAGE_ID
    };
    std::string error_messages[MAX_MESSAGE_ID] = {
       "Missing input or output file arguments",    // MISSING_IN_OUT_FILES
       "Pin location file parse error",             // PIN_LOC_XML_PARSE_ERROR 
       "Pin map file parse error",                  // PIN_MAP_CSV_PARSE_ERROR 
       "Pin constraint file parse error",           // PIN_CONSTRAINT_PARSE_ERROR
       "Input design parse error",                  // INPUT_DESIGN_PARSE_ERROR
       "Constrained port not found in design",      // CONSTRAINED_PORT_NOT_FOUND
       "Constrained pin not found in device",       // CONSTRAINED_PIN_NOT_FOUND
       "Re-constrained port",                       // RE_CONSTRAINED_PORT
       "Overlap pin found in constraint",           // OVERLAP_PIN_IN_CONSTRAINT
       "Fail to generate csv file for open source flow", // GENERATE_CSV_FILE_FOR_OS_FLOW
       "Open file failure",                         // OPEN_FILE_FAILURE
       "Close file failure",                         // CLOSE_FILE_FAILURE
       "Design requires more IO pins than device can supply", // PIN_SOURCE_NO_SURFFICENT
       "Fail to create temp pcf file",              // FAIL_TO_CREATE_TEMP_PCF
       "Incorrect assign pin method selection"      // INCORRECT_ASSIGN_PIN_METHOD
    };
    enum {
      ASSIGN_IN_RANDOM = 0,
      ASSIGN_IN_DEFINE_ORDER 
    } pin_assign_method_;
    #define CERROR std::cerr << "[Error] "

    cmd_line cl_;
    string temp_csv_file_name_;
    string temp_pcf_file_name_;
    vector<string> user_design_inputs_;
    vector<string> user_design_outputs_;
    vector<vector<string>> pcf_pin_cstr_;

public:
    pin_location(cmd_line& cl): cl_(cl){
      // explicitly initialize to null string
      temp_csv_file_name_ = string("");
      temp_pcf_file_name_ = string("");
      // default pin assign method
      pin_assign_method_ = ASSIGN_IN_DEFINE_ORDER;
    }
    ~pin_location() {
      // remove temporarily generated file
      if (temp_csv_file_name_.size()) {
        if (remove(temp_csv_file_name_.c_str())) {
          CERROR << error_messages[CLOSE_FILE_FAILURE] << std::endl;
        }
      }
      if (temp_pcf_file_name_.size()) {
        if (remove(temp_pcf_file_name_.c_str())) {
          CERROR << error_messages[CLOSE_FILE_FAILURE] << std::endl;
        }
      }
    }
    const cmd_line& get_cmd()const;
    bool reader_and_writer(); 
    bool generate_csv_file_for_os_flow();
    bool read_csv_file(rapidCsvReader&);
    bool read_user_design();
    bool read_user_pin_locatin_constrain_file();
    bool create_place_file(rapidCsvReader&);
    void shuffle_candidates(vector<int>& v);
    bool create_temp_constrain_file(rapidCsvReader& rs_csv_rd);
    void collect_left_available_device_pins(set<string>& constrained_device_pins, vector<int>& left_available_device_pin_idx, rapidCsvReader& rs_csv_reader);

};


#endif
