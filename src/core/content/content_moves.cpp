// content_moves.cpp — the combat-move roster (MoveDef).
//
// One content table (see content_tables.h). Edit rows here; the registry picks
// them up unchanged via embedded_content.cpp.
#include "core/content/content_tables.h"

namespace mal {

// Seed moves. `quick_jab` is the innate DEFAULT: always
// available, outside the equip slots, never slotted. Single-turn unless
// channelTurns > 1 (Fork Bomb winds up for 2).
//
// These seven are the GENERIC roster (line = nullptr): no pet owns them from
// hatch — a pet's own line moves (below) are its nature and are what it starts
// with (MoveLoadout::startingForLine()). The generic pool is instead the rare,
// desirable kit earned by taming a malbeast — the wild-win move-drop roll in
// Game::applyCombatResult (game_combat.cpp) is the only source that grants
// these.: the trailing `minStage` is the evolution stage each move
// UNLOCKS at (defs.h) — buffer_overflow (Script) and rootkit_strike (Daemon)
// are the apex drops, fork_bomb + null_route open at Process.
//
// Never type a magnitude into the description text: write `{power}` / `{pierce}` /
// `{stackPower}` / `{evade}` and effect_text.h substitutes it from this same row, so
// retuning a number retunes the prose. The move picker also draws a derived stat line
// (statLine()) carrying power, channel and every rider whatever the prose says.
const MoveDef kMoves[] = {
    {"quick_jab", "Quick Jab", MoveDef::Kind::Attack, 6, 1,
     "Innate jab. Always available.", Stage::BootSector},
    {"packet_storm", "Packet Storm", MoveDef::Kind::Attack, 12, 1,
     "Floods the target with packets.", Stage::BootSector},
    {"fork_bomb", "Fork Bomb", MoveDef::Kind::Attack, 26, 2,
     "Winds up {turns} turns, then detonates.", Stage::Process},
    {"checksum_guard", "Checksum Guard", MoveDef::Kind::Defend, 14, 1,
     "Braces against the next incoming hit.", Stage::BootSector},
    {"buffer_overflow", "Buffer Overflow", MoveDef::Kind::Attack, 20, 1,
     "Overwrites the stack for a solid hit.", Stage::Script},
    {"rootkit_strike", "Rootkit Strike", MoveDef::Kind::Attack, 24, 1,
     "A stealthy hit that lands hard.", Stage::Daemon},
    {"null_route", "Null Route", MoveDef::Kind::Defend, 18, 1,
     "Reroutes the next hit to nowhere.", Stage::Process},

    // --- Ransomware LINE moves ---------
    // line = "ransomware" → only Ransomware pets can learn/equip these. Lockout track
    // (attack, single-turn) stacks the caster's Power on landing; Cipher track (defend)
    // stacks the caster's Defense on cast. Fields after minStage: line, stackPowerPct,
    // stackPowerCap, stackDefensePct, stackDefenseCap, armorPiercePct. The line NEVER
    // heals (a hard identity pillar) — no restore effect appears on any row here.
    {"payload_drop", "Payload Drop", MoveDef::Kind::Attack, 12, 1,
     "Drops a payload. +{stackPower}% Power on landing (stacks to +{stackPowerCap}%).", Stage::Process,
     "ransomware", 8, 24, 0, 0, 0},
    {"double_extortion", "Double Extortion", MoveDef::Kind::Attack, 20, 1,
     "Encrypt AND leak. +{stackPower}% Power on landing (stacks to +{stackPowerCap}%).", Stage::Script,
     "ransomware", 10, 40, 0, 0, 0},
    {"mbr_wipe", "MBR Wipe", MoveDef::Kind::Attack, 28, 1,
     "Overwrites the boot sector. Ignores {pierce}% armor; +{stackPower}% Power (to +{stackPowerCap}%).",
     Stage::Daemon, "ransomware", 12, 60, 0, 0, 50},
    {"aes_lockbox", "AES Lockbox", MoveDef::Kind::Defend, 14, 1,
     "Encrypts a brace. +{stackDef}% DEF on cast (stacks to +{stackDefCap}%).", Stage::Process,
     "ransomware", 0, 0, 10, 20, 0},
    {"rsa_vault", "RSA Vault", MoveDef::Kind::Defend, 20, 1,
     "Seals the AES key. +{stackDef}% DEF on cast (stacks to +{stackDefCap}%).", Stage::Script,
     "ransomware", 0, 0, 12, 35, 0},
    {"full_disk_encryption", "Full-Disk Encryption", MoveDef::Kind::Defend, 28, 1,
     "Locks the whole drive. +{stackDef}% DEF on cast (stacks to +{stackDefCap}%).", Stage::Daemon,
     "ransomware", 0, 0, 15, 50, 0},

    // Phishing LINE moves -------------------------
    // line = "phishing" → only Phishing pets can learn/equip these.
    // Obfuscation track: shieldPool=1 → these POOL into a second health bar (shieldHp)
    // that stacks on recast and pops when overrun, not the one-shot guard brace. Fields
    // after line: stackPowerPct, stackPowerCap, stackDefensePct, stackDefenseCap,
    // armorPiercePct, lockTurns, dotDamage, dotTurns, stealPowerPct, stealDefensePct,
    // stealSpeedPct, stealCurrentHpPct, stealMaxHpPct, shieldPool (defs.h has the
    // per-field detail).
    // stealPowerPct: a landed hit always siphons this of the target's CURRENT attack
    // power. stealSpeedPct/stealCurrentHpPct only fire while THIS move's caster has an
    // Obfuscation bubble up (shieldHp > 0, cast via spoof_bubble/proxy_shell/
    // bathyspoof) — the "Perfect Bite" passive (content_passives.h, Combat::
    // applyEffect) then has a stage-scaled chance to double whichever of the two lands.
    {"spoof_bubble", "Spoof-Bubble", MoveDef::Kind::Defend, 8, 1,
     "A decoy identity that soaks {power} damage before it pops.", Stage::Process,
     "phishing", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*shieldPool=*/1},
    {"proxy_shell", "Proxy-Shell", MoveDef::Kind::Defend, 24, 1,
     "A deeper false front - a {power}-damage pool to burn through.", Stage::Script,
     "phishing", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*shieldPool=*/1},
    {"bathyspoof", "Bathyspoof", MoveDef::Kind::Defend, 32, 2,
     "The deepest buried identity - a {power}-damage shield.", Stage::Daemon,
     "phishing", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /*shieldPool=*/1},
    {"smish_hook", "Smish-Hook", MoveDef::Kind::Attack, 8, 2,
     "Sprays a lure, then strikes - siphons {stealPower}% power, and mid-bite "
     "drains {stealHp}% Health and {stealSpeed}% speed.",
     Stage::Process, "phishing", 0, 0, 0, 0, 0, 0, 0, 0, /*stealPowerPct=*/8,
     /*stealDefensePct=*/0, /*stealSpeedPct=*/6, /*stealCurrentHpPct=*/6},
    {"spear_strike", "Spear-Strike", MoveDef::Kind::Attack, 12, 2,
     "Targets one mark - siphons {stealPower}% power, and mid-bite drains "
     "{stealHp}% Health and {stealSpeed}% speed.",
     Stage::Script, "phishing", 0, 0, 0, 0, 0, 0, 0, 0, /*stealPowerPct=*/16,
     /*stealDefensePct=*/0, /*stealSpeedPct=*/8, /*stealCurrentHpPct=*/4},
    {"whaling_harpoon", "Whaling-Harpoon", MoveDef::Kind::Attack, 16, 3,
     "Hunts the biggest catch - siphons {stealPower}% power, and mid-bite "
     "drains {stealHp}% Health and {stealSpeed}% speed.",
     Stage::Daemon, "phishing", 0, 0, 0, 0, 0, 0, 0, 0, /*stealPowerPct=*/32,
     /*stealDefensePct=*/0, /*stealSpeedPct=*/16, /*stealCurrentHpPct=*/8},

    // --- Trojan LINE moves -------------------------
    // line = "trojan" → only Trojan pets (Keyloggerhead + its Daemon) can learn/equip.
    // Attacks strike from "already inside": armorPiercePct = 100, so they ignore ALL of
    // the target's defense (% cut + brace). Defends ARM a trap (trapArm=1) that fires on
    // the enemy's next hit — evasion + rebound + armor-rot — and feeds the
    // Execution-Override passive. Fields after minStage/line: stackPowerPct, stackPowerCap,
    // stackDefensePct, stackDefenseCap, armorPiercePct, lockTurns, dotDamage, dotTurns,
    // stealPowerPct, stealDefensePct, stealSpeedPct, stealCurrentHpPct, stealMaxHpPct,
    // shieldPool, trapArm, trapEvasionPct, trapReboundPct, trapArmorRot, trapPassiveBonusPct.
    // Magnitudes scale by stage; the line never heals — evasion is its survival tool.
    {"backdoor_breach", "Backdoor-Breach", MoveDef::Kind::Attack, 12, 1,
     "Strikes from inside - ignores {pierce}% of armor.", Stage::Process,
     "trojan", 0, 0, 0, 0, /*armorPiercePct=*/100},
    {"payload_puncture", "Payload-Puncture", MoveDef::Kind::Attack, 20, 1,
     "Detonates past every wall - ignores {pierce}% of armor.", Stage::Script,
     "trojan", 0, 0, 0, 0, /*armorPiercePct=*/100},
    {"rootkit_rupture", "Rootkit-Rupture", MoveDef::Kind::Attack, 28, 1,
     "Ruptures from ring 0 - ignores {pierce}% of armor.", Stage::Daemon,
     "trojan", 0, 0, 0, 0, /*armorPiercePct=*/100},
    // Held traps also feed the Execution-Override hijack chance, so each names the
    // bonus it contributes ({trapBonus}) alongside what it does on its own.
    {"logic_bomb", "Logic-Bomb", MoveDef::Kind::Defend, 0, 1,
     "Arms a trap: evades {evade}%, reflects {rebound}% mitigated, rots {armorRot}% "
     "armor, and adds {trapBonus}% override chance while held.", Stage::Process,
     "trojan", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*trapArm=*/1, /*evasion=*/20, /*rebound=*/40, /*armorRot=*/5, /*passiveBonus=*/10},
    {"sandbox_snare", "Sandbox-Snare", MoveDef::Kind::Defend, 0, 1,
     "A deeper trap: evades {evade}%, reflects {rebound}% mitigated, rots {armorRot}% "
     "armor, and adds {trapBonus}% override chance while held.", Stage::Script,
     "trojan", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*trapArm=*/1, /*evasion=*/30, /*rebound=*/50, /*armorRot=*/8, /*passiveBonus=*/15},
    {"killswitch", "Killswitch", MoveDef::Kind::Defend, 0, 1,
     "The deadliest trap: evades {evade}%, reflects {rebound}% mitigated, rots "
     "{armorRot}% armor, and adds {trapBonus}% override chance while held.", Stage::Daemon,
     "trojan", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*trapArm=*/1, /*evasion=*/45, /*rebound=*/60, /*armorRot=*/12, /*passiveBonus=*/20},

    // --- Worm LINE moves -------------------------
    // line = "worm" → only Worm pets can learn/equip. The line does not fight with its
    // moves, it fights with the BOARD they build: every row's own `power` is the lowest
    // on any line's track, and the damage arrives from the attacking copies piling onto
    // each swing (Combat::applyEffect, wormReplicaDamage). Fields after minStage/line
    // run to replicaSpawnPct, replicaPowerPct, replicaHealthPct (defs.h), with the 19
    // untouched effect fields between zeroed.
    //
    // The two tracks are deliberately asymmetric. An ATTACK rolls for its copy — the
    // swing still lands if the roll misses, so replication is the bonus. A DEFEND is
    // certain, because a defender IS the move; the row keeps a real `power` only so the
    // turn still braces when every replication slot is already full.
    {"mass_mailer", "Mass-Mailer", MoveDef::Kind::Attack, 6, 1,
     "Mails itself everywhere - {replicaChance}% chance to spawn an attacking copy "
     "worth {replicaPower}% of this hit, multiplied by the defenders standing.",
     Stage::Process, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/50, /*replicaPowerPct=*/60},
    {"subnet_sweep", "Subnet-Sweep", MoveDef::Kind::Attack, 9, 1,
     "Sweeps the whole subnet - {replicaChance}% chance to spawn an attacking copy "
     "worth {replicaPower}% of this hit, multiplied by the defenders standing.",
     Stage::Script, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/60, /*replicaPowerPct=*/70},
    {"slammer_burst", "Slammer-Burst", MoveDef::Kind::Attack, 12, 1,
     "Saturates every link at once - {replicaChance}% chance to spawn an attacking copy "
     "worth {replicaPower}% of this hit, multiplied by the defenders standing.",
     Stage::Daemon, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/70, /*replicaPowerPct=*/80},
    {"host_squat", "Host-Squat", MoveDef::Kind::Defend, 10, 1,
     "Parks a copy in the way - a body with {replicaHealth}% of your Health per "
     "attacking copy out. Braces {power} instead when the slots are full.",
     Stage::Process, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/100, /*replicaPowerPct=*/0, /*replicaHealthPct=*/20},
    {"swarm_wall", "Swarm-Wall", MoveDef::Kind::Defend, 14, 1,
     "Stacks the copies into a wall - a body with {replicaHealth}% of your Health per "
     "attacking copy out. Braces {power} instead when the slots are full.",
     Stage::Script, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/100, /*replicaPowerPct=*/0, /*replicaHealthPct=*/25},
    {"botnet_bulwark", "Botnet-Bulwark", MoveDef::Kind::Defend, 18, 1,
     "The whole swarm takes the hit - a body with {replicaHealth}% of your Health per "
     "attacking copy out. Braces {power} instead when the slots are full.",
     Stage::Daemon, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/100, /*replicaPowerPct=*/0, /*replicaHealthPct=*/30},

    // --- The THREAT moves (Watchdog / Faraday counter these) -----------------------
    // Generic ENEMY-flavoured attacks that carry a rider (lockTurns / dot*). Never owned
    // or learnable — MOVES lists only moves in the pet's owned pool — they reach a player
    // as an area apex's signature: each is an AreaDef::apexThreatMoveId, debuting in the
    // area whose own loot table pays out its counter-mod. Fields after armorPiercePct(0):
    // lockTurns, dotDamage, dotTurns.
    {"system_hang", "System Hang", MoveDef::Kind::Attack, 10, 1,
     "A hit that freezes the target for {lock} turns.", Stage::BootSector,
     nullptr, 0, 0, 0, 0, 0, /*lockTurns=*/2, /*dot=*/0, 0},
    {"data_rot", "Data Rot", MoveDef::Kind::Attack, 6, 1,
     "A hit that corrupts - {dot} damage/turn for {dotTurns} turns.", Stage::BootSector,
     nullptr, 0, 0, 0, 0, 0, /*lockTurns=*/0, /*dotDamage=*/5, /*dotTurns=*/3},
    // The Net-Sea's apex (THE GREEN BUTTON) wields the STUN rider the Bayou debuted,
    // one turn longer: the fake download button doesn't corrupt anything, it just takes
    // the next few turns away from you. So the Net-Sea's own pool re-stocks Watchdog
    // Timer rather than paying out a third counter — a longer freeze is still a freeze,
    // and the mod clamps any of them to one turn.
    {"decoy_download", "Decoy Download", MoveDef::Kind::Attack, 8, 1,
     "The button that wasn't the button - frozen for {lock} turns.", Stage::BootSector,
     nullptr, 0, 0, 0, 0, 0, /*lockTurns=*/3, /*dot=*/0, 0},
    // Castle Rapidscare's apex carries BOTH riders at once — the keep asks for the two
    // counters the earlier areas handed out separately, and re-stocks them in its own
    // mod pool for a player who never rolled one.
    {"nag_screen", "Nag Screen", MoveDef::Kind::Attack, 8, 1,
     "A modal you can't dismiss - frozen {lock} turn, then {dot} damage/turn for "
     "{dotTurns} turns.", Stage::BootSector,
     nullptr, 0, 0, 0, 0, 0, /*lockTurns=*/1, /*dotDamage=*/6, /*dotTurns=*/3},
};
const int kMovesCount = sizeof(kMoves) / sizeof(kMoves[0]);

}  // namespace mal
