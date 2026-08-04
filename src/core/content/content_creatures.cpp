// content_creatures.cpp — the petware/malbeast roster (CreatureDef).
//
// One content table (see content_tables.h). Edit rows here; the registry picks
// them up unchanged via embedded_content.cpp.
#include "core/content/content_tables.h"
#include "tunables.h"

namespace mal {

const CreatureDef kCreatures[] = {
    // The Ransomware line — Boot->Script is LINEAR, then Script->Daemon BRANCHES.
    // The CryptoShell egg hatches one of three Process pets, each heading a chain of
    // its own: the CANINE chain (Paypup -> Barkmail -> { Wire Heir | Extorgi }) below,
    // the URSINE chain (Pingcub -> Malbear -> { Bruinforce | Berserkernel }), and the
    // FELINE chain (Conkittenate -> Kalico -> { Pwnther | Breecheetah }). Each
    // `evolvesToId` is the linear hop; a BRANCH node leaves it null and defines the two
    // successors instead, picked by the care budget at evolution time. A branch's pair
    // carries DISTINCT art on a SHARED palette — SPR_PET_BRUINFORCE squares up,
    // SPR_PET_BERSERKERNEL rears and roars — so which branch a player got reads at a
    // glance while both still read as the same animal. They also differ mechanically
    // via the power/Frag multipliers below.
    // (sprite.h idleFrame() is single-frame-safe; both frames are pre-sized 56x48)
    // The whole Ransomware family carries line = "ransomware" (last field) — it gates
    // the line-specific Lockout/Cipher moves + the Ransom Lock passive to these pets.
    // Trailing braced array = CreatureDef::slotKinds (move-slot rework #12,
    // PLACEHOLDER layout — see the field comment in defs.h): slot0/1 lean Attack at
    // Process; slot2 Defend from the Script on; slot3 splits at the Daemon branch
    // (Good -> Defend durable, Bad -> Attack glass cannon), mirroring the power/Frag
    // lean. CryptoShell (egg) never reaches a real slot before hatching, so it just
    // carries a sane all-Attack default.
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
    {"malbear", "Malbear", Stage::Script, "SPR_PET_MALBEAR", nullptr,
     /*good=*/"bruinforce", /*bad=*/"berserkernel", 100, 100, "ransomware",
     "A moody, oversized adolescent that sits directly on your user interface and refuses to move.",
     "UI hijacking / standard malware scripts",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
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

    // --- Ursine cub (Ransomware) ---------------------
    {"pingcub", "Pingcub", Stage::Process, "SPR_PET_PINGCUB", "malbear",
     nullptr, nullptr, 100, 100, "ransomware",
     "A round little cub that pings every address on the network to see who answers, and remembers every single one that does.",
     "ICMP echo / ping sweeps for host discovery",
     {MoveKind::Attack, MoveKind::Attack, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},

    // --- Cat line (Ransomware)----
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

    // Frog line (Phishing) 
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

    // --- Anglerfish line (Phishing) — a SECOND hatch outcome for the Phrogspawn egg --
    // A deep-water lure line: an anglerfish dangles bait exactly like a phisher. Phishlet
    // is a Process pet on the "phishing" line, so it joins Tadpoll in the egg's random
    // hatch pool (rollHatchProcess draws by line) — BUT it's DEEP-DIVE-GATED: Phishlet
    // only enters the pool once the 2nd DeepWeb-depth achievement milestone is
    // earned.
    {"phishlet", "Phishlet", Stage::Process, "SPR_PET_PHISHLET", "clickbait",
     nullptr, nullptr, 100, 100, "phishing",
     "A tiny anglerfish that dangles a glowing 'you've won!' lure in the dark and waits for one careless nibble.",
     "Phishing lures / bait",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/"keyloggerhead", Locomotion::Swim},
    {"clickbait", "ClickBait", Stage::Script, "SPR_PET_GENERIC_SCRIPT", nullptr,
     /*good=*/"spamwhale", /*bad=*/"baitracuda", 100, 100, "phishing",
     "A flashing deep-water sign promising ten secrets doctors hate - every click is a hook that just tempts its victim deeper deeper.",
     "Clickbait / drive-by phishing",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Swim},
    {"spamwhale", "Spamwhale", Stage::Daemon, "SPR_PET_GENERIC_DAEMON",
     nullptr, nullptr, nullptr, kBranchGoodPowerPct, kBranchGoodFragPct, "phishing",
     "A vast filter-feeder that swallows whole floods of spam to sustain itself and protect its network.",
     "Containment / defensive filtering",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Swim},
    {"baitracuda", "Baitracuda", Stage::Daemon, "SPR_PET_GENERIC_DAEMON",
     nullptr, nullptr, nullptr, kBranchBadPowerPct, kBranchBadFragPct, "phishing",
     "It's long, it's strong, it's got its email bait kit on! You can't filter what you can't catch!",
     "Aggressive spear-phishing credential theft.",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Defend, MoveKind::Attack},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Swim},

    // --- Trojan line — NOT hatched from an egg (no EggLineDef); reached only by the
    // cross-line infiltration divert (a Process pet's evolvesToTrojanId, ~kTrojanDivertPct).
    // Keyloggerhead is the first Trojan Script: a Phishlet that "went wrong" — it keeps a
    // Phishing-blue read but is now a Trojan (line = "trojan"), so on divert it drops the
    // Phishing kit for the Trojan moves (pierce attacks + trap defends). Defend-leaning
    // slotKinds so the trap strategy is available. Sprite is the existing (placeholder)
    // Keyloggerhead frame; the "right blue with a small tell" disguise art is a follow-up.
    {"keyloggerhead", "Keyloggerhead", Stage::Script, "SPR_PET_KEYLOGGERHEAD",
     "placeholder_daemon", nullptr, nullptr, 100, 100, "trojan",
     "A slow, stubborn turtle in a keyboard shell - it passes for one of your fish, but it's logging every key you press.",
     "Keyloggers / Trojan disguise",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Swim},
    // First-cut Trojan Daemon terminus (name literal "Placeholder"; a disguise-themed
    // rename + real sprite are follow-ups). Generic Daemon frame for now.
    {"placeholder_daemon", "Placeholder", Stage::Daemon, "SPR_PET_GENERIC_DAEMON",
     nullptr, nullptr, nullptr, 100, 100, "trojan",
     "A Trojan still deciding what to impersonate - for now it just wears a name tag that reads 'Placeholder'.",
     "Trojan payload (placeholder)",
     {MoveKind::Attack, MoveKind::Defend, MoveKind::Attack, MoveKind::Defend},
     /*evolvesToTrojanId=*/nullptr, Locomotion::Walk},
};
const int kCreaturesCount = sizeof(kCreatures) / sizeof(kCreatures[0]);

}  // namespace mal
