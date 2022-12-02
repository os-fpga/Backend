#pragma once
#ifndef __rs_pinc_CMD_LINE_H_
#define __rs_pinc_CMD_LINE_H_

#include <unordered_map>
#include <unordered_set>

#include "pinc_log.h"

namespace pinc {

using std::string;
using std::unordered_map;
using std::unordered_set;

struct cmd_line
{
    unordered_map<string, string>   params_;
    unordered_set<string>           flags_;

    cmd_line(int argc, const char** argv);

    const unordered_set<string>&   get_flag_set()  const { return flags_; }
    unordered_map<string, string>  get_param_map() const { return params_; }

    bool is_flag_set(const string &fl) const noexcept { return flags_.count(fl); }

    string get_param(const string& key) const noexcept
    {
        auto fitr = params_.find(key);
        if (fitr == params_.end())
            return "";
        return fitr->second;
    }

    void set_flag(const string &fl);
    void set_param_value(string &key, string &val);
    void print_options() const;
};

}

#endif

