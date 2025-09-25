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

#include "eval_weights.h"
#include "eval.h"

namespace Hypnos::Eval {

// Global, thread-safe configuration for NNUE weights.
WeightsConfig gEvalWeights{};

// Set weights mode (Default, Manual, Dynamic)
void set_weights_mode(WeightsMode m) {
    // Keep it trivial and lock-free: atomics are enough here.
    gEvalWeights.mode = static_cast<int>(m);
}

// Set manual weights for material and positional components
void set_manual_weights(int mat, int pos) {
    gEvalWeights.manualMat = mat;
    gEvalWeights.manualPos = pos;
}

// Set dynamic profile parameters (opening/endgame) and complexity gain
void set_dynamic_profiles(int openMat, int openPos, int egMat, int egPos, int complexityGain) {
    gEvalWeights.dynOpenMat       = openMat;
    gEvalWeights.dynOpenPos       = openPos;
    gEvalWeights.dynEgMat         = egMat;
    gEvalWeights.dynEgPos         = egPos;
    gEvalWeights.dynComplexityGain = complexityGain;
}

} // namespace Hypnos::Eval
