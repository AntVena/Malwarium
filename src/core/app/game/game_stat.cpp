// game_stat.cpp (STAT): what the reader's pages read off the game (StatReader,
// game/stat_reader.h).
#include "core/app/game.h"

#include "core/ui/stat_pages.h"

namespace mal {

StatSources Game::statSources() const {
    StatSources s{pet_, registry_, log_};
    s.xpToNext = xpToNextLevel();
    s.hasNextEvo = hasNextEvolution();
    s.evoRemainMs = evolveRemainMs();
    // The deadline is on the device's uptime clock, which the pet does not keep.
    s.backupRemainMs =
        backupShieldArmed() ? pet_.armed.backupShieldUntilMs - lifetimeUptimeMs() : 0;
    return s;
}

}  // namespace mal
