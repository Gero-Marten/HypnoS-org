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
#include <atomic>

namespace Hypnos::Eval {

// Do not depend on Shashin. This header exposes a minimal, thread-safe config
// for NNUE blending weights and a tiny API to update them from UCI handlers.

enum class WeightsMode : int { Default = 0, Manual = 1, Dynamic = 2 };

struct WeightsConfig {
    std::atomic<int> manualMat{125};       // UCI: StrategyMaterialWeight (default: 125)
    std::atomic<int> manualPos{131};       // UCI: StrategyPositionalWeight (default: 131)

    // Dynamic profiles (opening vs endgame) tuned to keep the original scale.
    std::atomic<int> dynOpenMat{115};
    std::atomic<int> dynOpenPos{145};
    std::atomic<int> dynEgMat{145};
    std::atomic<int> dynEgPos{115};

    // Global mode: 0=Default (fixed 125/131), 1=Manual, 2=Dynamic
    std::atomic<int> mode{0};

    // Percentage bonus on positional weight when complexity is high.
    std::atomic<int> dynComplexityGain{12}; // percent
};

extern WeightsConfig gEvalWeights;

// API called by UCI option handlers
void set_weights_mode(WeightsMode m);
void set_manual_weights(int mat, int pos);
void set_dynamic_profiles(int openMat, int openPos, int egMat, int egPos, int complexityGain);

} // namespace Hypnos::Eval
