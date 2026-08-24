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
//
// WHAT THE TWO CHAINS ARGUE, and why they cannot be drawn alike. Both halves of this family
// are bait, and bait works by making the victim come to it — so nothing here reaches, and
// every row is a planted mass with its weight on the floor. What separates the chains is the
// SENSE the bait is aimed at: the anglerfish chain SHOWS you something, the frog chain TELLS
// you something. That is why the anglers carry a lure and the frogs carry a throat, and it is
// the whole reason Goliauth's concept is a voice rather than a light.
//
// Each chain inherits ONE organ down its whole length, and that organ is the slot the pun gets
// drawn in. ASSET_MANIFEST's rule is that a punned creature has to draw BOTH halves of its pun;
// an organ is where the interface half goes. The anglers spend theirs on the widget they bait
// with — a notification panel, a popup, a dialog. The frogs spend theirs on the credential the
// row is named for, and the frog names are a ladder up that credential: Tad-POLL is a survey,
// Croa-KEN is a token, Goli-AUTH is the whole authentication. So the throat carries a
// one-time-passcode display on the Script and a permission prompt on the Daemon.
//
// The frog chain is also the roster's one LITERAL metamorphosis, and is drawn as one: a
// tadpole, then a froglet whose tail has not finished reabsorbing, then the finished adult
// with no tail at all. THE TAIL LEAVING IS THE STAGE READ, which is what lets the three share
// a skeleton without sharing an envelope.
//
// Nothing on this chain is a cephalopod, and that is a rule rather than a preference: a teal
// octopus standing beside the Metamorphic line's coral one fails CREATURE_VISUAL_RULES §5,
// whose silhouette test is run against the rest of the roster rather than on its own. A frog's
// eyes break the TOP of its outline and a cephalopod's do not — that one difference survives
// being filled black, and it is the cheapest thing holding the two lines apart.
#pragma once

#include "core/content/content_achievements.h"
#include "core/content/defs.h"
#include "tunables.h"

namespace mal {

inline constexpr CreatureDef kPhishingCreatures[] = {
    // The 8-frame sheet is both the resting loop and the hatch one-shot (0-7, walked
    // by Game::hatchRevealFrame on the HATCHING modal). Frames 0-2 all have to read as
    // a sealed egg, not just 0-1: sprite.h's idleFrame() breathes between 0 and 1 and
    // blinks to frame 2 every twelfth beat, so a shell that has visibly opened by then
    // plays that opening while the egg is only sitting there. The cracking starts at 3.
    {"phrogspawn", "Phrogspawn", Stage::BootSector, "SPR_PET_EGG_PHISH_HATCH", "tadpoll",
     nullptr, nullptr, 100, 100, "phishing",
     "A raft of identical eggs, and only one is really yours - the rest are decoys waiting for a careless click.",
     "Frogspawn / phishing decoys",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Attack, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Static},
    {"tadpoll", "Tadpoll", Stage::Process, "SPR_PET_TADPOLL", "croaken",
     nullptr, nullptr, 100, 100, "phishing",
     "A tiny wide-eyed tadpole that swims nearby networks running polls nobody agreed to take. It's small now, but it'll grow a little every time someone takes the bait.",
     "Phishing surveys / credential polls",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Swim},
    {"croaken", "Croaken", Stage::Script, "SPR_PET_CROAKEN", "goliauth",
     nullptr, nullptr, 100, 100, "phishing",
     "Half-grown and pleased about it. The legs have come in, the tail has not quite gone, and the throat has already learned to flash a six-digit code at anyone who asks. The number is real. The reason it wants yours is not.",
     "Token theft / one-time passcode phishing",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Walk},
    // Its sheet is 96x64, which is ONE oversized Daemon frame rather than a row of 56px cells:
    // gen_assets cuts a SPR_PET_ sheet into 56px frames only when the width divides by 56, so a
    // width that does not is the larger Daemon box (up to 128x64). The extra room is the stage
    // read — a Daemon held to the Script cell cannot pay off CREATURE_VISUAL_RULES §0's arrival.
    {"goliauth", "Goliauth", Stage::Daemon, "SPR_PET_GOLIAUTH", nullptr,
     nullptr, nullptr, 100, 100, "phishing",
     "It has not moved in years and has never needed to. The throat does the work: a croak pitched exactly like the helpdesk, over a prompt everyone has approved a hundred times without reading. It only has to be believed once.",
     "Vishing / phone phishing (phreaking)",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Walk},

    // --- Anglerfish chain ---
    {"phishlet", "Phishlet", Stage::Process, "SPR_PET_PHISHLET", "clickbait",
     nullptr, nullptr, 100, 100, "phishing",
     "A tiny anglerfish that dangles a glowing 'you've won!' lure in the dark and waits for one careless nibble.",
     "Phishing lures / bait",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/"keyloggerhead", /*evolvesToTrojanBadId=*/nullptr, Locomotion::Swim,
     /*clips=*/{}, /*gatedBy=*/ach::kDeepWebDepth64},
    {"clickbait", "ClickBait", Stage::Script, "SPR_PET_CLICKBAIT", nullptr,
     /*good=*/"spamwhale", /*bad=*/"baitracuda", 100, 100, "phishing",
     "A flashing deep-water sign promising ten secrets doctors hate - every click is a hook that just tempts its victim deeper deeper.",
     "Clickbait / drive-by phishing",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Swim},
    {"spamwhale", "Spamwhale", Stage::Daemon, "SPR_PET_SPAMWHALE",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "phishing",
     "A vast filter-feeder that swallows whole floods of spam to sustain itself and protect its network.",
     "Containment / defensive filtering",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Swim},
    {"baitracuda", "Baitracuda", Stage::Daemon, "SPR_PET_BAITRACUDA",
     nullptr, nullptr, nullptr, kBranchBadPowerPct, kBranchBadFragPct, "phishing",
     "It's long, it's strong, it's got its email bait kit on! You can't filter what you can't catch!",
     "Aggressive spear-phishing credential theft.",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, /*evolvesToTrojanBadId=*/nullptr, Locomotion::Swim},
};
inline constexpr int kPhishingCreatureCount =
    sizeof(kPhishingCreatures) / sizeof(kPhishingCreatures[0]);

}  // namespace mal
