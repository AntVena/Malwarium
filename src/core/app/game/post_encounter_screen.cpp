#include "core/app/game/post_encounter_screen.h"

#include <cstdio>

#include "core/app/game_internal.h"   // levelStatName
#include "core/model/fight.h"
#include "core/ui/expl_screen.h"

namespace mal {

void PostEncounterScreen::draw(Framebuffer& fb, const PostEncounterHost& host) const {
    const FightResult& r = host.lastFightResult();
    // Frag state, dual-coded (a distinct WORD per state). The ACTUAL delta wins:
    // if fragmentation rose we say so honestly (a Bandwidth-shielded LOSS still takes the
    // separate loss penalty, so it can be both shielded AND up). Only a genuinely-clean
    // fight where a charge was spent reads SHIELDED; no charge + no rise reads None.
    const PostEncounterFragState st =
        (r.fragAfter > r.fragBefore) ? PostEncounterFragState::Rose
        : (r.shielded ? PostEncounterFragState::Shielded : PostEncounterFragState::None);
    // Compose the level-up line (nullptr if this fight didn't level up): "LVL n" +
    // one "STAT +k" chip per stat that gained (usually one. A wild win grants a
    // single level → a single random stat). The stat labels are the game's
    // (levelStatName), so drawPostEncounter stays label-agnostic.
    char lvlBuf[64];   // "LVL n" + up to kLevelStatCount " STAT +k" chips (multi-level lump)
    const char* lvlLine = nullptr;
    int gained = 0;
    for (int i = 0; i < kLevelStatCount; ++i) gained += r.statGain[i];
    if (gained > 0) {
        int off = std::snprintf(lvlBuf, sizeof(lvlBuf), "LVL %d", r.levelAfter);
        for (int i = 0; i < kLevelStatCount && off > 0 && off < (int)sizeof(lvlBuf); ++i)
            if (r.statGain[i] > 0)
                off += std::snprintf(lvlBuf + off, sizeof(lvlBuf) - off, "  %s +%d",
                                     levelStatName(i), r.statGain[i]);
        lvlLine = lvlBuf;
    }
    PostEncounterView v;
    v.bwBefore = r.bwBefore;
    v.bwAfter = r.bwAfter;
    v.bwMax = host.bandwidthMax();
    v.fragState = st;
    v.fragDelta = r.fragAfter - r.fragBefore;
    v.levelLine = lvlLine;
    drawPostEncounter(fb, v);
}

}  // namespace mal
