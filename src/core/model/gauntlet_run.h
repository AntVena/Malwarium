// gauntlet_run.h: one boss gauntlet, a run (CONTRIBUTING, "Where state lives"). From a
// boss trigger (a sub-area boss, or an area's five-round gauntlet) until its last round is
// won, or any round is lost or fled. Each round is its own Fight; this holds what carries
// between them. Runtime-only: a reboot ends it, and the save keeps only what a clear
// records on the Operator.
//
// Game drops it the moment it ends: nothing reads a gauntlet after its last round, so
// unlike the walk and the fight there is no reason to keep it.
#pragma once

#include <cstddef>
#include <utility>

#include "core/model/combat.h"

namespace mal {

struct GauntletRun {
    BossGauntlet gauntlet{"", 0, {}};
    int sector = 0;       // the area it belongs to
    int sub = -1;         // the sub-area whose boss this is, or -1 for the area gauntlet
    int round = 0;        // the round being fought, or the count won once it is cleared
    int bitsAccrued = 0;  // each won round's roll, paid as one lump on a full clear

    static GauntletRun started(int sector, int sub, BossGauntlet gauntlet) {
        GauntletRun r;
        r.gauntlet = std::move(gauntlet);
        r.sector = sector;
        r.sub = sub;
        return r;
    }
    // Which clear a full win records: the area's (and its Title), or one sub-area's.
    bool clearsArea() const { return sub < 0; }
    bool cleared() const { return static_cast<size_t>(round) >= gauntlet.rounds.size(); }
    // The boss of the round being fought; nullptr once every round is won.
    const CombatEnemy* foe() const { return cleared() ? nullptr : &gauntlet.rounds[round]; }
};

}  // namespace mal
