/*********************************************************************
 * This file includes functions that output the content of
 * object Command and CommandContext
 ********************************************************************/
#include "vtr_assert.h"
#include "vtr_log.h"
#include "command_echo.h"

/* Begin namespace openfpga */
namespace openfpga {

/*********************************************************************
 * Print all the options that are defined in an object Command
 * This function is mainly used to create help desk for a command
 ********************************************************************/
void print_command_options(const Command& cmd) {
  VTR_LOG("Command '%s' usage:\n%lu options available\n",
          cmd.name().c_str(), cmd.options().size());
  for (const CommandOptionId& opt : cmd.options()) {
    if (true == cmd.option_short_name(opt).empty()) {
      VTR_LOG("\t--%s : %s\n",
              cmd.option_name(opt).c_str(),
              cmd.option_description(opt).c_str());
      continue;
    } 
    VTR_LOG("\t--%s, -%s : %s\n",
            cmd.option_name(opt).c_str(),
            cmd.option_short_name(opt).c_str(),
            cmd.option_description(opt).c_str());
  } 
}

/*********************************************************************
 * Print all the options that have been parsed to an object of CommandContext
 * This function is mainly used to validate what options have been enabled
 * for users' confirmation
 ********************************************************************/
void print_command_context(const Command& cmd, 
                           const CommandContext& cmd_context) {
  VTR_LOG("\nConfirm selected options when call command '%s':\n",
          cmd.name().c_str());
  for (const CommandOptionId& opt : cmd.options()) {
    VTR_LOG("--%s",
            cmd.option_name(opt).c_str());
    if (false == cmd.option_short_name(opt).empty()) {
      VTR_LOG(", -%s",
              cmd.option_short_name(opt).c_str());
    }
            
    if (false == cmd.option_require_value(opt)) {
      VTR_LOG(": %s\n",
              cmd_context.option_enable(cmd, opt) ? "on" : "off");
    } else {
      VTR_ASSERT_SAFE (true == cmd.option_require_value(opt));
      /* If the option is disabled, we output an off */
      std::string opt_value;
      if (false == cmd_context.option_enable(cmd, opt)) {
        opt_value = std::string("off");
      } else {
        opt_value = cmd_context.option_value(cmd, opt);
      }
      VTR_LOG(": %s\n",
              opt_value.c_str());
    }
  } 
}

} /* End namespace minshell */
