// The Trojan family — NOT hatched from an egg (it has no EggLineDef), reached only
// by the cross-line infiltration divert: a pet whose `evolvesToTrojanId` is set has a
// ~kTrojanDivertPct chance of landing here instead of its own successor
// (Game::fireEvolution). The first divert unlocks the family.
//
// The family has no chain of its own, and that is the shape rather than a gap. Every row
// here is a substitution for one row of some OTHER line, so a Trojan is always read
// against the creature the player was expecting — which is the only way a disguise can
// be a disguise. What it inherits is decided by WHERE it was substituted in:
//
//   Phishlet (Phishing, Process)   -> Keyloggerhead, a Script, blue like its origin
//   Rootgrub (Worm,     Script)    -> Coaxeel / USBasilisk, Daemons, 1-bit like theirs
//
// so the two arms of the family look nothing alike on purpose. CREATURE_VISUAL_RULES §4
// says a Trojan wears the line it diverted from; the Worm line's signature is a STYLE
// rather than a hue, so wearing it means being drawn by tools/gen_worm_art.py in the
// same one ink as the Daemons these two were substituted for.
//
// Keyloggerhead is the first Trojan Script: a Phishlet that "went wrong". It keeps a
// Phishing-blue read but is now line = "trojan", so on divert it drops the Phishing
// kit for the Trojan moves (pierce attacks + trap defends) — hence the Defend-leaning
// slotKinds, which make the trap strategy available.
#pragma once

#include "core/content/defs.h"
#include "tunables.h"

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
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Swim,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/8, /*holdBeats=*/4},
                {"attack", /*row=*/1, /*frames=*/4}}},
    // First-cut Trojan Daemon terminus (the name says what it is; a disguise-themed
    // rename + real sprite are follow-ups). Generic Daemon frame for now.
    {"placeholder_daemon", "Trojan Placeholder", Stage::Daemon, "SPR_PET_GENERIC_DAEMON",
     nullptr, nullptr, nullptr, 100, 100, "trojan",
     "A Trojan still deciding what to impersonate - for now it just wears a name tag that reads 'Placeholder'.",
     "Trojan payload (placeholder)",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Walk},

    // --- The Worm arm: what Rootgrub's Script->Daemon hop diverts into ---------------
    //
    // These two are a BRANCH, resolved from the same care budget that would have picked
    // between Shenloop and Threadbore (Game::fireEvolution reads evolvesToTrojanBadId).
    // The divert costs the player the line, not the raise: a Rootgrub brought up well
    // still gets the durable Daemon, it is simply wearing a connector now.
    //
    // Both CRAWL, and they are the only Daemons of that shape in the roster. Leaving the
    // floor is the Worm line's one payoff — the whole family crawls up to and including
    // its Script, and both its real Daemons earn a mover (Swim, Fly) at the last hop.
    // A Trojan is what happens when that hop is spent on a disguise instead, so it does
    // not get the payoff: the divert is the trade, and the locomotion is where the
    // player can see it without reading a stat.
    {"coaxeel", "Coaxeel", Stage::Daemon, "SPR_PET_COAXEEL",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "trojan",
     "Most of it is cable. It lies along the floor of the rack pretending to be the run that was always there, and at the far end of it something has been stripped back to the bare conductor.",
     "Hardware implants / network taps spliced into the run",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Ground,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/4, /*holdBeats=*/2},
                {"attack", /*row=*/1, /*frames=*/4},
                {"droop", /*row=*/2, /*frames=*/2, /*holdBeats=*/3},
                {"weak", /*row=*/3, /*frames=*/2, /*holdBeats=*/3}}},
    {"usbasilisk", "USBasilisk", Stage::Daemon, "SPR_PET_USBASILISK",
     nullptr, nullptr, nullptr, kBranchBadPowerPct, kBranchBadFragPct, "trojan",
     "It rears up, raises a crown it has no business having, and holds perfectly still - because the front of it is a plug, and a plug only has to be picked up once.",
     "BadUSB / a peripheral that enumerates as something it is not",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Ground,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/4, /*holdBeats=*/2},
                {"attack", /*row=*/1, /*frames=*/4},
                {"droop", /*row=*/2, /*frames=*/2, /*holdBeats=*/3},
                {"weak", /*row=*/3, /*frames=*/2, /*holdBeats=*/3}}},
};
inline constexpr int kTrojanCreatureCount =
    sizeof(kTrojanCreatures) / sizeof(kTrojanCreatures[0]);

}  // namespace mal
