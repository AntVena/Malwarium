// rank_up_notice.h: a Hacker Rank crossing (Operator::hackerRank), held from the scan that
// crossed it until the walk next shows its flavor line. Paying the rank is the game's
// (Game::checkHackerRankUp); this holds what the line will say.
#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>

namespace mal {

class RankUpNotice {
public:
    // A crossing to rank `rank` that paid `bits`. A second crossing before the walk shows
    // the first replaces it, since the newer line names the higher rank.
    void hold(int rank, int bits) {
        std::snprintf(line_, sizeof(line_), "RANK UP R%d +%dB", rank, bits);
        pending_ = true;
    }
    // Copy the held line into `out` and clear it. False, and `out` untouched, when none is held.
    bool take(char* out, size_t cap) {
        if (!pending_ || cap == 0) return false;
        std::strncpy(out, line_, cap - 1);
        out[cap - 1] = '\0';
        pending_ = false;
        return true;
    }

private:
    bool pending_ = false;
    char line_[32] = "";
};

}  // namespace mal
