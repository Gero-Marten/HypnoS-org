/*
  HypnoS, a UCI chess playing engine derived from Stockfish
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  HypnoS is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  HypnoS is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <string>
#include <filesystem>
#include <sstream>
#include <iomanip>
#include "uci.h"
#include "thread.h"
#include "engine.h"
#include "types.h"
#include "ucioption.h"  // Option, OptionsMap

namespace Hypnos {
namespace Utility {

// Replacement for map_path: in the new Hypnos path mapping is not needed

inline std::string map_path(const std::string& s) { return s; }

// Replacement for file_exists
inline bool file_exists(const std::string& s) { return std::filesystem::exists(s); }

// Replacement for unquote
inline std::string unquote(const std::string& s) {
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
        return s.substr(1, s.size() - 2);
    return s;
}

} // namespace Utility

// Replacement for format_bytes
inline std::string format_bytes(size_t bytes) {
    std::ostringstream oss;
    if (bytes < 1024) oss << bytes << " B";
    else if (bytes < 1024 * 1024) oss << (bytes / 1024) << " KB";
    else if (bytes < 1024ull * 1024ull * 1024ull) oss << (bytes / (1024 * 1024)) << " MB";
    else oss << (bytes / (1024ull * 1024ull * 1024ull)) << " GB";
    return oss.str();
}
// Overload because experience.cpp calls it with 2 arguments
inline std::string format_bytes(size_t bytes, int /*precision*/) {
    return format_bytes(bytes);
}

// Shim DEPTH_NONE
inline constexpr Depth DEPTH_NONE = Depth(0);

} // namespace Hypnos

// *** IMPORTANT ***
// NO Hypnos::Experience namespace here.
// Global pointer to Options used by the old experience.cpp
namespace Experience {
    inline Hypnos::OptionsMap* g_options = nullptr;
}

// Alias to preserve Options["..."] syntax
// (macro: this way operator[] works)
#ifndef HYP_EXPERIENCE_HAVE_OPTIONS_MACRO
#define HYP_EXPERIENCE_HAVE_OPTIONS_MACRO
#define Options (*::Experience::g_options)
#endif
