// The Ransomware family — Boot->Script is LINEAR, then Script->Daemon BRANCHES.
//
// The CryptoShell egg hatches one of three Process pets, each heading a chain of
// its own: the CANINE chain (Paypup -> Barkmail -> { Wire Heir | Extorgi }), the
// URSINE chain (Pingcub -> Malbear -> { Bruinforce | Berserkernel }), and the
// FELINE chain (Conkittenate -> Kalico -> { Pwnther | Breecheetah }). Each
// `evolvesToId` is the linear hop; a BRANCH node leaves it null and defines the two
// successors instead, picked by the care budget at evolution time. A branch's pair
// carries DISTINCT art on a SHARED palette — SPR_PET_BRUINFORCE squares up,
// SPR_PET_BERSERKERNEL rears and roars — so which branch a player got reads at a
// glance while both still read as the same animal. They also differ mechanically
// via the power/Frag multipliers.
//
// Every row carries line = "ransomware", which gates the line-specific
// Lockout/Cipher moves + the Ransom Lock passive to these pets.
//
// The braced array before `locomotion` is CreatureDef::slotKinds (PLACEHOLDER
// layout — see the field comment in defs.h): slot0/1 lean Attack at Process; slot2
// Defend from the Script on; slot3 splits at the Daemon branch (Good -> Defend
// durable, Bad -> Attack glass cannon), mirroring the power/Frag lean. CryptoShell
// (egg) never reaches a real slot before hatching, so it carries an all-Attack
// default.
#pragma once

#include "core/content/defs.h"
#include "tunables.h"

namespace mal {

inline constexpr CreatureDef kRansomwareCreatures[] = {
    {"cryptoshell", "CryptoShell", Stage::BootSector, "SPR_PET_CRYPTOSHELL", "paypup",
     nullptr, nullptr, 100, 100, "ransomware",
     "A digital shell so tough, even its mother has to brute-force the password to hatch it.",
     "Cryptographic shells / brute-force",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Attack, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    {"paypup", "Paypup", Stage::Process, "SPR_PET_PAYPUP", "barkmail",
     nullptr, nullptr, 100, 100, "ransomware",
     "Loves chasing code, fetching cookies, and dropping malicious payloads.",
     "Software payloads / tracking cookies",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    {"barkmail", "Barkmail", Stage::Script, "SPR_PET_BARKMAIL", nullptr,
     /*good=*/"wire_heir", /*bad=*/"extorgi", 100, 100, "ransomware",
     "The pup grown into its armour, plated in overlapping links it calls correspondence. Every one of them is a letter, and every letter says the same thing louder.",
     "Blackmail / the ransom note that follows encryption",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    // Barkmail's two Daemons: the noble line and the one that took it.
    // Wire Heir's sheet is one row of 8 frames at the 96x64 Daemon cell, so unlike every
    // 56x48 sheet it needs its frame width declared in gen_assets.py's FRAME_W_OVERRIDES —
    // 768 is not a whole number of 56px cells, and the default rule would read the strip
    // as one very wide frame.
    {"wire_heir", "Wire Heir", Stage::Daemon, "SPR_PET_WIRE_HEIR",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "ransomware",
     "Roaming the Napstorrent Moors and decrypting hard drives in need, it bears a suspicious resemblence the exiled heir to the throne of the Lockshund kingdom...",
     "Wire transfers - a ransom paid down, a debt taken up",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Defend},
     // Ground rather than Walk: its sheet already breathes on its own frames, and the
     // shelf bob on top of that lands on a different beat and reads as a bounce. The
     // slower Ground pace suits a long dog on short legs anyway.
     /*evolvesToTrojanId=*/nullptr, Locomotion::Ground,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/8, /*holdBeats=*/2}}},
    {"extorgi", "Extorgi", Stage::Daemon, "SPR_PET_EXTORGI",
     nullptr, nullptr, nullptr, kBranchBadPowerPct, kBranchBadFragPct, "ransomware",
     "Aspiring undemocratically self-elected regent of Castle Rapidscare. Other candidates have been warned to back down once if they want to keep their files, It expects to keep the racket going many terms running.",
     "Extortion / double-extortion ransomware",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    // Malbear's sheet is one row of 8 columns (56x48 each): the first 3 are the
    // resting loop, and the full 8 are the swing. Both clips therefore play row 0
    // — which columns the attack actually wants is an art call, not a code one.
    {"malbear", "Malbear", Stage::Script, "SPR_PET_MALBEAR", nullptr,
     /*good=*/"bruinforce", /*bad=*/"berserkernel", 100, 100, "ransomware",
     "A moody, oversized adolescent that sits directly on your user interface and refuses to move.",
     "UI hijacking / standard malware scripts",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/3, /*holdBeats=*/4},
                {"attack", /*row=*/0, /*frames=*/8, /*holdBeats=*/2}}},
    // Malbear's two Daemons: the same bear either way — one holds the line, the other
    // has stopped caring whether it survives the fight, which is what the branch
    // multipliers below say too.
    {"bruinforce", "Bruinforce", Stage::Daemon, "SPR_PET_BRUINFORCE",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "ransomware",
     "A colossal, corrupted grizzly of a process that leans on your save files until they let it in, then sits in the doorway so nothing else can.",
     "Brute force cracking",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    {"berserkernel", "Berserkernel", Stage::Daemon, "SPR_PET_BERSERKERNEL",
     nullptr, nullptr, nullptr, kBranchBadPowerPct, kBranchBadFragPct, "ransomware",
     "The same bear, reared up and screaming, having chewed its way down past every layer that was supposed to stop it. It does not intend to come back out.",
     "Memory-corruption",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},

    // --- Ursine cub ---------------------
    {"pingcub", "Pingcub", Stage::Process, "SPR_PET_PINGCUB", "malbear",
     nullptr, nullptr, 100, 100, "ransomware",
     "A round little cub that pings every address on the network to see who answers, and remembers every single one that does.",
     "ICMP echo / ping sweeps for host discovery",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},

    // --- Cat line ----
    // Conkittenate's sheet is a single 448x48 row of 8 columns, all of it one idle
    // loop — the cub has one thing to do and spends its whole row doing it, where
    // Kalico below splits four rows across four poses.
    {"conkittenate", "Conkittenate", Stage::Process, "SPR_PET_CONKITTENATE", "kalico",
     nullptr, nullptr, 100, 100, "ransomware",
     "A sly kitten that concatenates your files into one encrypted hairball and demands tuna to undo it.",
     "String concatenation / ransomware payloads",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Attack},
     // Ground, not Walk: a sheet that breathes on its own frames does not also want
     // the shelf bob under it (Locomotion, defs.h).
     /*evolvesToTrojanId=*/nullptr, Locomotion::Ground,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/8, /*holdBeats=*/2}}},
    // Kalico's sheet is four rows of 8 columns (56x48 each), one row per clip, so
    // unlike Malbear above it spends a row rather than a column range on each. All four
    // play: "idle" and "walk" split the habitat on whether the wander is moving the
    // anchor (drawHabitat, game_render.cpp), and "attack"/"hurt" pose the fight on the
    // swing and the recoil (fightPose, combat_screen.cpp). The clip names are the
    // contract — a creature that declares none of them draws its row 0 and nothing
    // else changes, which is what every single-row sheet in this file relies on.
    {"kalico", "Kalico", Stage::Script, "SPR_PET_KALICO", nullptr,
     /*good=*/"pwnther", /*bad=*/"breecheetah", 100, 100, "ransomware",
     "A patchy calico that longs for dank hacks. It's got all of the tools but not yet the knack.",
     "Ransomware payload staging",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Attack},
     // Ground, not Walk: all four of its rows are drawn motion, so the shelf bob
     // would land a second rise on top of them (Locomotion, defs.h).
     /*evolvesToTrojanId=*/nullptr, Locomotion::Ground,
     /*clips=*/{{"idle", /*row=*/0, /*frames=*/8, /*holdBeats=*/2},
                {"walk", /*row=*/1, /*frames=*/8, /*holdBeats=*/2},
                {"attack", /*row=*/2, /*frames=*/8, /*holdBeats=*/1},
                {"hurt", /*row=*/3, /*frames=*/8, /*holdBeats=*/1}}},
    // Pwnther spends the 96x64 Daemon cell (CREATURE_VISUAL_RULES §7) rather than the
    // 56x48 the Script rows above take — the stage's payoff is read as scale, and one
    // oversized frame is what gen_assets cuts a pet sheet into when its width is not a
    // whole number of 56px cells.
    {"pwnther", "Pwnther", Stage::Daemon, "SPR_PET_PWNTHER",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "ransomware",
     "Small signature. Big cat. Massive data loss",
     "System pwnage / privilege takeover",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    // Breecheetah takes the same 96x64 cell and the same palette as its Good sibling
    // above, and spends both differently — where Pwnther is bulk under grey plate,
    // this is a bare green sprinter wearing its shell as glowing pips and flat decal
    // tiles. Same line, same branch signature, opposite silhouettes: which one a
    // player got reads across the room (CREATURE_VISUAL_RULES §4).
    {"breecheetah", "Breecheetah", Stage::Daemon, "SPR_PET_BREECHEETAH",
     nullptr, nullptr, nullptr, kBranchBadPowerPct, kBranchBadFragPct, "ransomware",
     "The fastest cat on the wire - breaches the perimeter then bails before alerts even fire.",
     "Data breach / exfiltration",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
};
inline constexpr int kRansomwareCreatureCount =
    sizeof(kRansomwareCreatures) / sizeof(kRansomwareCreatures[0]);

}  // namespace mal
