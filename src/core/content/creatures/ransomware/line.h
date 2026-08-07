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
    {"barkmail", "Barkmail", Stage::Script, "SPR_PET_GENERIC_SCRIPT", nullptr,
     /*good=*/"wire_heir", /*bad=*/"extorgi", 100, 100, "ransomware",
     "The pup grown into its armour, plated in overlapping links it calls correspondence. Every one of them is a letter, and every letter says the same thing louder.",
     "Blackmail / the ransom note that follows encryption",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    // Barkmail's two Daemons: the noble line and the one that took it.
    {"wire_heir", "Wire Heir", Stage::Daemon, "SPR_PET_GENERIC_DAEMON",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "ransomware",
     "Roaming the Napstorrent Moors and decrypting hard drives in need, it bears a suspicious resemblence the exiled heir to the throne of the Lockshund kingdom...",
     "Wire transfers - a ransom paid down, a debt taken up",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    {"extorgi", "Extorgi", Stage::Daemon, "SPR_PET_GENERIC_DAEMON",
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
    {"conkittenate", "Conkittenate", Stage::Process, "SPR_PET_GENERIC_PROCESS", "kalico",
     nullptr, nullptr, 100, 100, "ransomware",
     "A sly kitten that concatenates your files into one encrypted hairball and demands tuna to undo it.",
     "String concatenation / ransomware payloads",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    {"kalico", "Kalico", Stage::Script, "SPR_PET_GENERIC_SCRIPT", nullptr,
     /*good=*/"pwnther", /*bad=*/"breecheetah", 100, 100, "ransomware",
     "A patchy calico that longs for dank hacks. It's got all of the tools but not yet the knack.",
     "Ransomware payload staging",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    {"pwnther", "Pwnther", Stage::Daemon, "SPR_PET_GENERIC_DAEMON",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "ransomware",
     "Small signature. Big cat. Massive data loss",
     "System pwnage / privilege takeover",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
    {"breecheetah", "Breecheetah", Stage::Daemon, "SPR_PET_GENERIC_DAEMON",
     nullptr, nullptr, nullptr, kBranchBadPowerPct, kBranchBadFragPct, "ransomware",
     "The fastest cat on the wire - breaches the perimeter then bails before alerts even fire.",
     "Data breach / exfiltration",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
};
inline constexpr int kRansomwareCreatureCount =
    sizeof(kRansomwareCreatures) / sizeof(kRansomwareCreatures[0]);

}  // namespace mal
