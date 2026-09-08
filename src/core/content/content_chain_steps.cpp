// content_chain_steps.cpp — the FOLLOW-UP halves of chained moves.
//
// One content table (see content_tables.h). A row here is reached only by casting the
// move whose `chainNextId` names it, which is why it is a table of its own: a step is
// never owned, equipped, taught, dropped or rolled, so keeping these out of kMoves means
// none of the code that enumerates the roster has to learn what a chain is.
//
// The pair is authored as a pair. An entry row (content_moves.cpp) does the SETUP — it
// bites for a little and takes what it came for — and the step here lands the real hit on
// the following turn. Both turns are full casts, which is the difference between a chain
// and the wind-up it replaces: a wind-up's first turn does nothing, and a turn that does
// nothing is the most expensive thing a move can spend.
//
// Read the pair's power together. Neither half is a whole move, and the entry's own power
// is deliberately small — enough that a landed hit carries its siphons, not enough to be
// the point.
#include "core/content/content_tables.h"

namespace mal {

const MoveDef kChainSteps[] = {
    // --- Phishing: the strike half of the lure/strike track ---------------------
    // The line's identity is the two-beat hunt — spray, then take the one that bit. The
    // siphons ride the LURE (it is the half that touches the mark); these are the close.
    // No steals here: a hunt takes once, and pricing the take twice would make the pair
    // the only attack in the game that pays two riders for two turns.
    //
    // A chained pair is compared on its TWO-TURN total against what a slot could have been
    // doing over the same two turns, and this pair shipped losing that comparison at every
    // stage: 6+16 against the generic Fork Bomb's 12+20 at Process, 8+22 against a single
    // Buffer Overflow cast twice (40) at Script, 10+30 against Rootkit Strike twice (48) at
    // Daemon. The siphons are the line's identity, not a discount it should pay for — a
    // hunt that takes something and still hits softer than a plain swing is a hunt nobody
    // equips. The strike halves now bring each pair level with the generic swing it is read
    // against (28 / 36 / 48), and the siphons ride on top, which is where the line's edge
    // is supposed to come from.
    {"smish_strike", "Smish-Strike", MoveDef::Kind::Attack, 22, 1,
     "The lure had a hook in it all along.", Stage::Process, "phishing"},
    {"spear_run", "Spear-Run", MoveDef::Kind::Attack, 28, 1,
     "One mark, chosen, and no second guess.", Stage::Script, "phishing"},
    {"harpoon_haul", "Harpoon-Haul", MoveDef::Kind::Attack, 38, 1,
     "What the harpoon set, the line brings in.", Stage::Daemon, "phishing"},

    // --- Generic: the two forks that used to wind up ----------------------------
    // A fork bomb does not detonate, it EXHAUSTS — so the pair is the fork (which hangs
    // the target while the table fills) and the moment there is nothing left to fork
    // with. The turn that used to be spent winding up is now the fork itself.
    {"process_flood", "Process-Flood", MoveDef::Kind::Attack, 20, 1,
     "Nothing left to fork with.", Stage::Process},
    // A scene release goes out, and then it gets nuked. Two events, in that order, which
    // is why it was never one turn of nothing followed by one turn of everything.
    {"scene_nuke", "Scene-Nuke", MoveDef::Kind::Attack, 24, 1,
     "NUKED: bad rip. The rot was already spreading.", Stage::Process},
};
const int kChainStepsCount = sizeof(kChainSteps) / sizeof(kChainSteps[0]);

}  // namespace mal
