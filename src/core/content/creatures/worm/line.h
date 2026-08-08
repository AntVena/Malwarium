// The Worm family — the line that fights with a BOARD instead of a swing.
//
// Every row here is weak on its own, and that is the design rather than a tuning gap:
// a worm's damage comes from the copies it puts on the screen (content_moves.cpp's
// "worm" track, resolved in Combat::applyEffect), and its survival comes from those
// copies being what an attack usually hits. Its two passives are Shared Resources —
// speed matched to the opponent's forever, and a fixed pile of replication slots — and
// both live in content_passives.h.
//
// The line is NOT hatched from an egg yet (it has no EggLineDef, same as the Trojan
// family): this is the process-level slice, reachable for testing rather than through
// a raise. An egg line is what the WORM_WHISPERER achievement is still waiting on.
//
// Sprites here are stand-ins. The line's own art rule — a worm reads SMALLER in its
// cell than any other line's creature, because the replicas need the room beside it —
// is in assets/CREATURE_VISUAL_RULES.md, and the borrowed Buffer Wyrm frame below does
// not obey it.
#pragma once

#include "core/content/defs.h"
#include "tunables.h"

namespace mal {

inline constexpr CreatureDef kWormCreatures[] = {
    // Nodeatode is the family's Process pet, and the only row with real character so
    // far. Its sheet is the wild Buffer Wyrm's single 56x48 frame borrowed whole, so it
    // declares no clip and falls back to sprite.h's idle heuristic.
    //
    // Its slot typing alternates Attack/Defend from the first slot, unlike the other
    // lines' attack-leaning Process rows. A worm whose kit is all attacks spawns only
    // attacking copies, and an attacking copy with no defender behind it multiplies by
    // the floor — so the alternation is what makes the line's own arithmetic reachable
    // as the slots unlock, not a flavour choice.
    {"nodeatode", "Nodeatode", Stage::Process, "SPR_MALBEAST_BUFFER_WYRM", nullptr,
     /*good=*/"worm_placeholder_good", /*bad=*/"worm_placeholder_bad", 100, 100, "worm",
     "A thread-thin nematode that chews from one node to the next. Small, slow, and by morning there is never just the one.",
     "Worms / self-replicating network propagation",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    // The two successors are placeholders in the literal sense — the care branch and
    // its power/Frag lean are real and wired, the creatures on the end of it are not
    // designed yet. Generic Script frame, names that say so.
    {"worm_placeholder_good", "Worm Placeholder I", Stage::Script, "SPR_PET_GENERIC_SCRIPT",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "worm",
     "A worm that grew up cautious - it spends its copies on cover rather than on teeth.",
     "Worm payload, durable branch (placeholder)",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    {"worm_placeholder_bad", "Worm Placeholder II", Stage::Script, "SPR_PET_GENERIC_SCRIPT",
     nullptr, nullptr, nullptr, kBranchBadPowerPct, kBranchBadFragPct, "worm",
     "A worm that grew up hungry - it spends its copies on teeth and trusts there to be enough of them.",
     "Worm payload, aggressive branch (placeholder)",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
};
inline constexpr int kWormCreatureCount =
    sizeof(kWormCreatures) / sizeof(kWormCreatures[0]);

}  // namespace mal
