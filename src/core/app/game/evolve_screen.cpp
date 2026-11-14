#include "core/app/game/evolve_screen.h"

#include <cstdio>

#include "tunables.h"
#include "core/content/defs.h"
#include "core/model/move_loadout.h"
#include "core/ui/modals.h"

namespace mal {

void EvolveScreen::tick() {
    if (beat_ < kEvoHoldBeats + kEvoFlashBeats) beat_++;
}

bool EvolveScreen::revealed() const { return beat_ >= kEvoHoldBeats + kEvoFlashBeats; }

void EvolveScreen::draw(Framebuffer& fb, const EvolveHost& host) const {
    const CreatureDef* from = host.pet();
    EvolveView v;
    v.phase = beat_ < kEvoHoldBeats                    ? EvolvePhase::Hold
              : beat_ < kEvoHoldBeats + kEvoFlashBeats ? EvolvePhase::Flash
                                                       : EvolvePhase::Reveal;
    v.from = from ? host.creatureSprite(*from) : nullptr;
    v.to = to_ ? host.creatureSprite(*to_) : nullptr;
    // What changed, in words: the path the errors so far put it on (the thing that
    // decides a Daemon's form), and any move slot the new stage opens.
    char path[28];
    const int errors = host.careMistakes();
    std::snprintf(path, sizeof path, "%s PATH - %d ERROR%s",
                  host.effectiveCareBranch() == CareBranch::Bad ? "BAD" : "GOOD", errors,
                  errors == 1 ? "" : "S");
    char slot[40];
    if (from && to_) {
        const int gained = MoveLoadout::slotsForStage(to_->stage) -
                           MoveLoadout::slotsForStage(from->stage);
        if (gained > 0) {
            std::snprintf(slot, sizeof slot, "+%d MOVE SLOT%s - SEE MODS", gained,
                          gained == 1 ? "" : "S");
            v.slotLine = slot;
        }
    }
    v.toName = to_ ? to_->displayName : "";
    v.toStage = to_ ? to_->stage : Stage::Process;
    v.pathLine = path;
    v.beat = host.screenBeat();
    drawEvolveModal(fb, v);
}

}  // namespace mal
