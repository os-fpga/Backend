#pragma once
#ifndef __rsbe_staRS_Env_H_h_
#define __rsbe_staRS_Env_H_h_

#include "rsOpts.h"

namespace rsbe {

struct rsEnv
{
    rsOpts  opts_;

    std::string  arg0_, abs_arg0_;

    std::vector<std::string>  orig_argV_;
    std::vector<std::string>  filtered_argV_;

    std::string  selfPath_, parentPath_;

    std::string  shortVer_, longVer_, compTimeStr_;

    std::string  traceMarker_;

    int pid_ = 0, ppid_ = 0;

    int64_t rss0_ = 0; // initial maxRSS from getrusage(), in kilobytes

// -----
    rsEnv() noexcept;
    rsEnv(int argc, char** argv) noexcept { parse(argc, argv); }
    ~rsEnv() { opts_.reset(); }

    int argCount(bool orig=true) const noexcept {
        return orig ? orig_argV_.size() : filtered_argV_.size();
    }

    void parse(int argc, char** argv) noexcept;

    void init() noexcept;
    void initVersions(const char* vs) noexcept;
    void listDevEnv() const noexcept;

    void reset() noexcept;
    void dump(const char* prefix) const noexcept;
    void printPids(const char* prefix) const noexcept;

    double megaRss0() const noexcept { return kilo2mega(rss0_); }

    const char* shortVerCS() const noexcept { return shortVer_.c_str(); }
    const char*  longVerCS() const noexcept { return longVer_.c_str(); }
    const char* compTimeCS() const noexcept { return compTimeStr_.c_str(); }

    static double kilo2mega(int64_t k) noexcept { // kilobytes to megabytes, with rounding
        if (k <= 0)
            return 0;
        double m = 10.0 * (k + 512L) / 1024.0; // 10x megabytes
        return 0.1 * int64_t(m + 0.5); // megabytes, rounded to .1 M
    }

    static bool readLink(const std::string& path, std::string& out) noexcept;
    static bool getPidExePath(int pid, std::string& out) noexcept;
};

}

#endif

