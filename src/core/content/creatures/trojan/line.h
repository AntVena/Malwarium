// The Trojan family — NOT hatched from an egg (it has no EggLineDef), reached only
// by the cross-line infiltration divert: a Process pet whose `evolvesToTrojanId` is
// set has a ~kTrojanDivertPct chance of landing here instead of its own successor
// (Game::fireEvolution). The first divert unlocks the family.
//
// Keyloggerhead is the first Trojan Script: a Phishlet that "went wrong". It keeps a
// Phishing-blue read but is now line = "trojan", so on divert it drops the Phishing
// kit for the Trojan moves (pierce attacks + trap defends) — hence the Defend-leaning
// slotKinds, which make the trap strategy available.
#pragma once

#include "core/content/defs.h"

namespace mal {

inline constexpr CreatureDef kTrojanCreatures[] = {
    // Sprite is the existing (placeholder) Keyloggerhead frame; the "right blue with
    // a small tell" disguise art is a follow-up. Its sheet is two rows of 8 columns,
    // so the resting loop plays row 0 in full and the swing has row 1 to itself.
    {"keyloggerhead", "Keyloggerhead", Stage::Script, "SPR_PET_KEYLOGGERHEAD",
     "placeholder_daemon", nullptr, nullptr, 100, 100, "trojan",
     "A slow, stubborn turtle in a keyboard shell - it passes for one of your fish, but it's logging every key you press.",
     "Keyloggers / Trojan disguise",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Swim,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/8, /*holdBeats=*/4},
                {"attack", /*row=*/1, /*frames=*/4}}},
    // First-cut Trojan Daemon terminus (the name says what it is; a disguise-themed
    // rename + real sprite are follow-ups). Generic Daemon frame for now.
    {"placeholder_daemon", "Trojan Placeholder", Stage::Daemon, "SPR_PET_GENERIC_DAEMON",
     nullptr, nullptr, nullptr, 100, 100, "trojan",
     "A Trojan still deciding what to impersonate - for now it just wears a name tag that reads 'Placeholder'.",
     "Trojan payload (placeholder)",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
};
inline constexpr int kTrojanCreatureCount =
    sizeof(kTrojanCreatures) / sizeof(kTrojanCreatures[0]);

}  // namespace mal
