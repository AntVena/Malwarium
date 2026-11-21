// lcg.h: the one random step every seeded roll in the engine advances on. A seed held
// anywhere (Game, a run, a fight, a board) walks the same sequence, which is what lets a
// duel's two devices replay one fight from one seed and a frame gate dump any step.
#pragma once

#include <cstdint>

namespace mal {

// Advance `s` one step and return the new state. Callers that want a smaller range take
// the high bits (`>> 16`), which carry the period the low bits lack.
constexpr uint32_t lcgAdvance(uint32_t& s) {
    s = s * 1664525u + 1013904223u;
    return s;
}

} // namespace mal
