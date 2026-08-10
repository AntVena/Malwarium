// The Phishing family — two chains off one egg, both about bait.
//
// The FROG chain (Phrogspawn -> Tadpoll -> Croaken -> Goliauth) is linear the whole
// way. The ANGLERFISH chain (Phishlet -> ClickBait -> { Spamwhale | Baitracuda })
// is a SECOND hatch outcome for the same Phrogspawn egg: Phishlet is a Process pet
// on this line, so it joins Tadpoll in the egg's random hatch pool
// (rollHatchProcess draws by line) — BUT it is DEEP-DIVE-GATED, entering the pool
// only once the 2nd DeepWeb-depth achievement milestone is earned.
//
// Phishlet is also the one row in this family with an `evolvesToTrojanId`: the
// cross-line infiltration divert that can turn it into a Keyloggerhead instead of a
// ClickBait (see creatures/trojan/line.h).
#pragma once

#include "core/content/defs.h"
#include "tunables.h"

namespace mal {

inline constexpr CreatureDef kPhishingCreatures[] = {
    // The 8-frame sheet is both the idle loop (frames 0-1) and the hatch sequence
    // (0-7, walked by Game::hatchCrackFrame as the incubation clock runs down).
    {"phrogspawn", "Phrogspawn", Stage::BootSector, "SPR_PET_EGG_PHISH_HATCH", "tadpoll",
     nullptr, nullptr, 100, 100, "phishing",
     "A raft of identical eggs, and only one is really yours - the rest are decoys waiting for a careless click.",
     "Frogspawn / phishing decoys",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Attack, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Swim},
    {"tadpoll", "Tadpoll", Stage::Process, "SPR_PET_TADPOLL", "croaken",
     nullptr, nullptr, 100, 100, "phishing",
     "A tiny wide-eyed tadpole that swims nearby networks running polls nobody agreed to take. It's small now, but it'll grow a little every time someone takes the bait.",
     "Phishing surveys / credential polls",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Swim},
    {"croaken", "Croaken", Stage::Script, "SPR_PET_CROAKEN", "goliauth",
     nullptr, nullptr, 100, 100, "phishing",
     "A warty toad-kraken hybrid that has grown fat camped atop a corporate email server. It croaks in hunger as its tentacles search for just one more credential.",
     "Credential harvesting / phishing",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    {"goliauth", "Goliauth", Stage::Daemon, "SPR_PET_GOLIAUTH", nullptr,
     nullptr, nullptr, 100, 100, "phishing",
     "It hasn't had to move in years since it started using its deep croak to impersonate IT helpdesk. It probably has a tentacle in the desk-phone of every C-level exec in the country.",
     "Vishing / phone phishing (phreaking)",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},

    // --- Anglerfish chain ---
    {"phishlet", "Phishlet", Stage::Process, "SPR_PET_PHISHLET", "clickbait",
     nullptr, nullptr, 100, 100, "phishing",
     "A tiny anglerfish that dangles a glowing 'you've won!' lure in the dark and waits for one careless nibble.",
     "Phishing lures / bait",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/"keyloggerhead", Locomotion::Swim},
    {"clickbait", "ClickBait", Stage::Script, "SPR_PET_CLICKBAIT", nullptr,
     /*good=*/"spamwhale", /*bad=*/"baitracuda", 100, 100, "phishing",
     "A flashing deep-water sign promising ten secrets doctors hate - every click is a hook that just tempts its victim deeper deeper.",
     "Clickbait / drive-by phishing",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Swim},
    {"spamwhale", "Spamwhale", Stage::Daemon, "SPR_PET_SPAMWHALE",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "phishing",
     "A vast filter-feeder that swallows whole floods of spam to sustain itself and protect its network.",
     "Containment / defensive filtering",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Swim},
    {"baitracuda", "Baitracuda", Stage::Daemon, "SPR_PET_BAITRACUDA",
     nullptr, nullptr, nullptr, kBranchBadPowerPct, kBranchBadFragPct, "phishing",
     "It's long, it's strong, it's got its email bait kit on! You can't filter what you can't catch!",
     "Aggressive spear-phishing credential theft.",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Swim},
};
inline constexpr int kPhishingCreatureCount =
    sizeof(kPhishingCreatures) / sizeof(kPhishingCreatures[0]);

}  // namespace mal
