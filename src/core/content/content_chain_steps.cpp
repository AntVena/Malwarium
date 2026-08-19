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
    {"smish_strike", "Smish-Strike", MoveDef::Kind::Attack, 16, 1,
     "The lure had a hook in it all along.", Stage::Process, "phishing"},
    {"spear_run", "Spear-Run", MoveDef::Kind::Attack, 22, 1,
     "One mark, chosen, and no second guess.", Stage::Script, "phishing"},
    {"harpoon_haul", "Harpoon-Haul", MoveDef::Kind::Attack, 30, 1,
     "What the harpoon set, the line brings in.", Stage::Daemon, "phishing"},
};
const int kChainStepsCount = sizeof(kChainSteps) / sizeof(kChainSteps[0]);

}  // namespace mal
