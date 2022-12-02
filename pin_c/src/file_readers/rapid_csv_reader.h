#pragma once

#include "rapidcsv.h"
#include "geo/xyz.h"
#include "pinc_log.h"
#include <map>
#include <unordered_map>

namespace pinc {

using std::string;
using std::vector;

class pin_location;

class rapidCsvReader
{
    std::map<string, vector<string>>  modes_map_;
    vector<string>                    mode_names_;

    // vectors are indexed by csv row, size() == #rows

    vector<string> bump_pin_name_; // "Bump/Pin Name"
    vector<string> gbox_name_;     // "GBOX_NAME"
    vector<string> io_tile_pin_;   // "IO_tile_pin"

    vector<XYZ> io_tile_pin_xyz_;  // "IO_tile_pin_x", "_y", "_z"

    int start_position_ = 0;  // "GBX GPIO" group start position in pin table row

public:
    rapidCsvReader() = default;

    // file i/o
    bool read_csv(const std::string &f, bool check);

    void write_csv(string csv_file_name) const;
    void print_csv() const;
    bool sanity_check(const rapidcsv::Document& doc) const;

    // data query
    XYZ get_pin_xyz_by_bump_name(const string& mode, const string& bump_name,
                                 const string& gbox_pin_name) const;

    bool has_io_pin(const string& pin_name) const noexcept {
        auto I = std::find(bump_pin_name_.begin(), bump_pin_name_.end(), pin_name);
        return I != bump_pin_name_.end();
    }

    int get_pin_x_by_pin_idx(uint i) const noexcept {
        assert(i < io_tile_pin_xyz_.size());
        return io_tile_pin_xyz_[i].x_;
    }

    int get_pin_y_by_pin_idx(uint i) const noexcept {
        assert(i < io_tile_pin_xyz_.size());
        return io_tile_pin_xyz_[i].y_;
    }

    int get_pin_z_by_pin_idx(uint i) const noexcept {
        assert(i < io_tile_pin_xyz_.size());
        return io_tile_pin_xyz_[i].z_;
    }

    friend class pin_location;
};

}

