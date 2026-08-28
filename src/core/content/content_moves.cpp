// content_moves.cpp — the combat-move roster (MoveDef).
//
// One content table (see content_tables.h). Edit rows here; the registry picks
// them up unchanged via embedded_content.cpp.
#include "core/content/content_tables.h"

namespace mal {

// Seed moves. `quick_jab` is the innate DEFAULT: always
// available, outside the equip slots, never slotted. Single-turn unless
// channelTurns > 1 (Runaway Fork winds up for 3 — the one row where the wind-up IS the
// move, and priced for it; the rest of the two-beat kit CHAINS instead, so both of its
// turns do something. See MoveDef::chainNextId and content_chain_steps.cpp).
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
    // No stun on the fork. Enemy kits field this row (combat_factory), so a lock here
    // spends the PLAYER's turns doing nothing, and a turn where nothing happens is still a
    // turn — it pushed the measured fight length past its design band from the enemy side.
    // The ramp is the flavour, and the ramp is damage.
    {"fork_bomb", "Fork Bomb", MoveDef::Kind::Attack, 12, 1,
     "Forks until the table fills, then exhausts what is left.",
     Stage::Process, nullptr, 0, 0, 0, 0, /*armorPiercePct=*/0, /*lockTurns=*/0,
     /*dotDamage=*/0, /*dotTurns=*/0, 0, 0, 0, 0, /*stealMaxHpPct=*/0,
     /*shieldPool=*/0, /*trapArm=*/0, 0, 0, 0, 0, /*replicaSpawnPct=*/0, 0, 0,
     /*chainNextId=*/"process_flood"},
    braceRow("checksum_guard", "Checksum Guard", 14,
             "Braces {power}, and gives back {refund}% of the wait.",
             Stage::BootSector, /*speedRefundPct=*/60),
    {"buffer_overflow", "Buffer Overflow", MoveDef::Kind::Attack, 20, 1,
     "Overwrites the stack for a solid hit.", Stage::Script},
    {"rootkit_strike", "Rootkit Strike", MoveDef::Kind::Attack, 24, 1,
     "A stealthy hit that lands hard.", Stage::Daemon},
    braceRow("null_route", "Null Route", 18,
             "Reroutes the next hit to nowhere - gives back {refund}% of the wait.",
             Stage::Process, /*speedRefundPct=*/55),

    // --- Wild SIGNATURES — one per malbeast --------------------------------------
    // Generic on purpose (line == nullptr), because these are what a wild TEACHES: a win
    // drops out of the beaten enemy's own kit (rollEnemyMoveDrop, game_explore.cpp), and
    // a line-exclusive row would be a prize half the roster could never field. Which
    // creature carries which is the roster itself (CombatEnemy::signatureMoveId,
    // wildMalbeast in combat_factory.cpp); this file only says what each one DOES.
    //
    // Six creatures, six different RIDERS, and that is the whole design: two malbeasts
    // that differ only in power are still one fight, so each signature is a distinct
    // verb — a bleed, a way through armour, two drains, a wind-up and a ramp.
    //
    // POWER is small deliberately. A wild's kit is the depth rung it was met at plus its
    // area's pair plus this, and the rungs are ordered by EFFECTIVE per-turn damage
    // (applyWildSubAreaRamp) — a signature that out-hit the rung would flatten the ramp
    // the ladder exists to build. What a signature adds is character, never a tier.
    //
    // minStage climbs with the tier that carries it, so what a creature is worth learning
    // out of matches how deep you had to go to meet it — the same ladder buffer_overflow
    // and rootkit_strike sit on. No lockTurns anywhere here, for the reason fork_bomb's
    // row gives: a stun in an enemy kit spends the PLAYER's turns doing nothing.
    {"screen_tear", "Screen Tear", MoveDef::Kind::Attack, 5, 1,
     "Rips the frame open - the seam leaks {dot} a turn for {dotTurns}.",
     Stage::BootSector, nullptr, 0, 0, 0, 0, /*armorPiercePct=*/0, /*lockTurns=*/0,
     /*dotDamage=*/2, /*dotTurns=*/2},
    {"wild_pointer", "Wild Pointer", MoveDef::Kind::Attack, 6, 1,
     "Blunders into memory nobody was guarding - ignores {pierce}% armor.",
     Stage::BootSector, nullptr, 0, 0, 0, 0, /*armorPiercePct=*/40},
    {"dropped_packet", "Dropped Packet", MoveDef::Kind::Attack, 8, 1,
     "Some of what you send never arrives - takes {stealPower}% of the target's Power.",
     Stage::Process, nullptr, 0, 0, 0, 0, /*armorPiercePct=*/0, /*lockTurns=*/0,
     /*dotDamage=*/0, /*dotTurns=*/0, /*stealPowerPct=*/12},
    {"stale_read", "Stale Read", MoveDef::Kind::Attack, 7, 1,
     "Eats what you left cached - takes {stealDef}% of the target's armor.",
     Stage::Process, nullptr, 0, 0, 0, 0, /*armorPiercePct=*/0, /*lockTurns=*/0,
     /*dotDamage=*/0, /*dotTurns=*/0, /*stealPowerPct=*/0, /*stealDefensePct=*/12},
    {"coil_overrun", "Coil Overrun", MoveDef::Kind::Attack, 22, 2,
     "Coils for {turns} turns, then spills past the end of the buffer.", Stage::Script},
    {"ring_zero", "Ring Zero", MoveDef::Kind::Attack, 10, 1,
     "Surfaces in ring zero and settles in - +{stackPower}% Power a hit, "
     "up to {stackPowerCap}%.",
     Stage::Script, nullptr, /*stackPowerPct=*/8, /*stackPowerCap=*/24},

    // --- Metamorphic LINE moves -------------------------------------------------
    // line = "metamorphic". None of these casts itself: each rolls a move out of the
    // generic roster of its own KIND plus the two lines it names (Combat::resolveTurn via
    // wildPick), and the pet keeps whatever passive a borrowed row brings with it. The
    // pair is the only thing an operator tailors about a wildcard, which is what makes
    // nine rows worth owning separately instead of one row worth equipping four times.
    //
    // Every ATTACK pair appears once — six of them across the four other lines — and the
    // DEFEND rows pair only lines that HAVE a defend track. Phishing's defence is its
    // Obfuscation pool, authored on rows the shared roster already reaches, so a Defend
    // wildcard naming it would name a line with nothing of that kind to give and quietly
    // hand its whole weight back to generic.
    //
    // Power is 0 on all nine (see wildRow): what a cast is worth is whatever it rolled.
    wildRow("instruction_swap", "Instruction Swap", MoveDef::Kind::Attack,
            "Swaps in an instruction it saw somewhere else - a random Ransomware or "
            "Trojan attack, or one from the common pool.",
            Stage::Process, "ransomware", "trojan"),
    wildRow("register_rename", "Register Rename", MoveDef::Kind::Attack,
            "Same operation, different register - a random Phishing or Worm attack, or "
            "one from the common pool.",
            Stage::Process, "phishing", "worm"),
    wildRow("junk_insertion", "Junk Insertion", MoveDef::Kind::Attack,
            "Pads itself with something borrowed until nothing matches - a random "
            "Ransomware or Worm attack, or one from the common pool.",
            Stage::Script, "ransomware", "worm"),
    wildRow("subroutine_shuffle", "Subroutine Shuffle", MoveDef::Kind::Attack,
            "Reorders itself mid-run - a random Phishing or Trojan attack, or one from "
            "the common pool.",
            Stage::Script, "phishing", "trojan"),
    wildRow("permutation_pass", "Permutation Pass", MoveDef::Kind::Attack,
            "Rewrites every line and means the same thing - a random Ransomware or "
            "Phishing attack, or one from the common pool.",
            Stage::Daemon, "ransomware", "phishing"),
    wildRow("entry_point_swap", "Entry-Point Swap", MoveDef::Kind::Attack,
            "Starts somewhere nobody was watching - a random Trojan or Worm attack, or "
            "one from the common pool.",
            Stage::Daemon, "trojan", "worm"),
    wildRow("signature_drift", "Signature Drift", MoveDef::Kind::Defend,
            "Drifts out from under its own signature - a random Ransomware or Trojan "
            "defence, or one from the common pool.",
            Stage::Process, "ransomware", "trojan"),
    wildRow("decoy_rewrite", "Decoy Rewrite", MoveDef::Kind::Defend,
            "Rewrites the part they are looking at - a random Ransomware or Worm "
            "defence, or one from the common pool.",
            Stage::Script, "ransomware", "worm"),
    wildRow("shell_recompile", "Shell Recompile", MoveDef::Kind::Defend,
            "Builds a new shell out of whatever compiled - a random Trojan or Worm "
            "defence, or one from the common pool.",
            Stage::Daemon, "trojan", "worm"),

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
    // The Cipher ladder runs INVERTED, and the reason is the seizure (RansomSeizure): a
    // full wall is what lets a brace take the attack that hits it, so where a row's CAP
    // sits decides how soon that row can do the line's real job. The deep row therefore
    // caps LOW in one big step — it is finished the moment it is cast, and finished is the
    // point. The shallow row climbs in small steps to a much higher ceiling: slower to
    // arm anything, but the wall it eventually builds is far bigger, and a seized move
    // hits for the wall behind it. So the beginner's row is the long game and the
    // endgame's row is the fast one, which is the opposite of how a ladder usually reads
    // and exactly right here.
    {"aes_lockbox", "AES Lockbox", MoveDef::Kind::Defend, 14, 1,
     "Encrypts a brace. +{stackDef}% DEF on cast (stacks to +{stackDefCap}%).", Stage::Process,
     "ransomware", 0, 0, 6, 48, 0},
    {"rsa_vault", "RSA Vault", MoveDef::Kind::Defend, 20, 1,
     "Seals the AES key. +{stackDef}% DEF on cast (stacks to +{stackDefCap}%).", Stage::Script,
     "ransomware", 0, 0, 12, 36, 0},
    {"full_disk_encryption", "Full-Disk Encryption", MoveDef::Kind::Defend, 28, 1,
     "Locks the whole drive. +{stackDef}% DEF on cast (stacks to +{stackDefCap}%).", Stage::Daemon,
     "ransomware", 0, 0, 20, 20, 0},

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
    // The Obfuscation ladder is a decoy that learns to bite. The first rung is pure
    // padding; the second trades depth for POISONED DATA — read the decoy and something in
    // it reads back, which is the line's conversion from defence into damage and the one
    // that sits where a defend-heavy pet will actually hold it; the third does both
    // properly. A retaliation DoT passes no cut, brace, pool or hit cap, so it stays small.
    poolRow("spoof_bubble", "Spoof-Bubble", 8,
            "A decoy identity that soaks {power} damage before it pops.",
            Stage::Process, /*retaliateDot=*/0, /*retaliateTurns=*/0),
    poolRow("proxy_shell", "Proxy-Shell", 16,
            "A thinner false front, salted - {power}-damage pool, and reading it costs "
            "{dot}/turn for {dotTurns}.",
            Stage::Script, /*retaliateDot=*/5, /*retaliateTurns=*/2),
    poolRow("bathyspoof", "Bathyspoof", 32,
            "The deepest buried identity - a {power}-damage shield, salted at "
            "{dot}/turn for {dotTurns}.",
            Stage::Daemon, /*retaliateDot=*/8, /*retaliateTurns=*/3),
    // The LURE half of the line's two-beat hunt: it bites small, takes what it came for,
    // and hands the slot to its strike step (content_chain_steps.cpp) for the next turn.
    // Both turns are real casts — the track used to spend its first turn winding up,
    // which cost it more than every siphon here was worth.
    //
    // stealMaxHpPct is the lure's signature take, and it is the one steal that keeps
    // paying: the pool MOVES (combat.cpp), so a landed lure hands the pet a bigger tank
    // for the rest of the fight, which the frenzy heal is then able to fill. The volatile
    // pair (speed + current Health) stays bubble-gated as it always was — a separate
    // bargain from the chain, and one that still asks for the shield first.
    {"smish_hook", "Smish-Hook", MoveDef::Kind::Attack, 6, 1,
     "Sprays a lure - takes {stealMaxHp}% of the catch's size, siphons {stealPower}% "
     "power, and mid-bite drains {stealHp}% Health and {stealSpeed}% speed.",
     Stage::Process, "phishing", 0, 0, 0, 0, 0, 0, 0, 0, /*stealPowerPct=*/8,
     /*stealDefensePct=*/0, /*stealSpeedPct=*/6, /*stealCurrentHpPct=*/6,
     /*stealMaxHpPct=*/6, /*shieldPool=*/0, /*trapArm=*/0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/0, 0, 0, /*chainNextId=*/"smish_strike"},
    {"spear_strike", "Spear-Strike", MoveDef::Kind::Attack, 8, 1,
     "Picks one mark - takes {stealMaxHp}% of its size, siphons {stealPower}% power, "
     "and mid-bite drains {stealHp}% Health and {stealSpeed}% speed.",
     Stage::Script, "phishing", 0, 0, 0, 0, 0, 0, 0, 0, /*stealPowerPct=*/16,
     /*stealDefensePct=*/0, /*stealSpeedPct=*/8, /*stealCurrentHpPct=*/4,
     /*stealMaxHpPct=*/10, /*shieldPool=*/0, /*trapArm=*/0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/0, 0, 0, /*chainNextId=*/"spear_run"},
    {"whaling_harpoon", "Whaling-Harpoon", MoveDef::Kind::Attack, 10, 1,
     "Sets into the biggest catch - takes {stealMaxHp}% of its size, siphons "
     "{stealPower}% power, and mid-bite drains {stealHp}% Health and {stealSpeed}% speed.",
     Stage::Daemon, "phishing", 0, 0, 0, 0, 0, 0, 0, 0, /*stealPowerPct=*/32,
     /*stealDefensePct=*/0, /*stealSpeedPct=*/16, /*stealCurrentHpPct=*/8,
     /*stealMaxHpPct=*/14, /*shieldPool=*/0, /*trapArm=*/0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/0, 0, 0, /*chainNextId=*/"harpoon_haul"},

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
     "Arms a trap: evades {evade}%, reflects {rebound}%, rots {armorRot}% armor, "
     "+{trapBonus}% override chance.", Stage::Process,
     "trojan", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*trapArm=*/1, /*evasion=*/20, /*rebound=*/40, /*armorRot=*/5, /*passiveBonus=*/10},
    {"sandbox_snare", "Sandbox-Snare", MoveDef::Kind::Defend, 0, 1,
     "A deeper trap: evades {evade}%, reflects {rebound}%, rots {armorRot}% armor, "
     "+{trapBonus}% override chance.", Stage::Script,
     "trojan", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*trapArm=*/1, /*evasion=*/30, /*rebound=*/50, /*armorRot=*/8, /*passiveBonus=*/15},
    {"killswitch", "Killswitch", MoveDef::Kind::Defend, 0, 1,
     "The deadliest trap: evades {evade}%, reflects {rebound}%, rots {armorRot}% "
     "armor, +{trapBonus}% override chance.", Stage::Daemon,
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
     "Mails itself everywhere - {replicaChance}% chance to spawn a copy worth "
     "{replicaPower}%, per defender standing.",
     Stage::Process, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/50, /*replicaPowerPct=*/60},
    {"subnet_sweep", "Subnet-Sweep", MoveDef::Kind::Attack, 9, 1,
     "Sweeps the whole subnet - {replicaChance}% chance to spawn a copy worth "
     "{replicaPower}%, per defender standing.",
     Stage::Script, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/60, /*replicaPowerPct=*/70},
    {"slammer_burst", "Slammer-Burst", MoveDef::Kind::Attack, 12, 1,
     "Saturates every link at once - {replicaChance}% chance to spawn a copy worth "
     "{replicaPower}%, per defender standing.",
     Stage::Daemon, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/70, /*replicaPowerPct=*/80},
    {"host_squat", "Host-Squat", MoveDef::Kind::Defend, 10, 1,
     "Parks a copy in the way - a body worth {replicaHealth}% Health per copy out. "
     "Braces {power} when the slots are full.",
     Stage::Process, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/100, /*replicaPowerPct=*/0, /*replicaHealthPct=*/20},
    {"swarm_wall", "Swarm-Wall", MoveDef::Kind::Defend, 14, 1,
     "Stacks the copies into a wall - {replicaHealth}% Health per copy out; braces "
     "{power} when slots are full.",
     Stage::Script, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/100, /*replicaPowerPct=*/0, /*replicaHealthPct=*/25},
    {"botnet_bulwark", "Botnet-Bulwark", MoveDef::Kind::Defend, 18, 1,
     "The whole swarm takes the hit - {replicaHealth}% Health per copy out; braces "
     "{power} when slots are full.",
     Stage::Daemon, "worm", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
     /*replicaSpawnPct=*/100, /*replicaPowerPct=*/0, /*replicaHealthPct=*/30},

    // --- The THREAT moves (Watchdog / Faraday counter these) -----------------------
    // Generic ENEMY-flavoured attacks that carry a rider (lockTurns / dot*). Each is an
    // AreaDef::apexThreatMoveId, debuting on the signature boss of the area whose own loot
    // table pays out its counter-mod — and, like every move an enemy carries, LEARNABLE by
    // beating the thing that swung it (Game::rollEnemyMoveDrop). Fields after
    // armorPiercePct(0): lockTurns, dotDamage, dotTurns.
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

    // === THE BOSS POOL — one move per boss, learned by beating the boss that swings it ==
    //
    // Generic (line = nullptr), so any pet can be taught any of them. Each is named by
    // exactly one boss's AreaDef::subBosses[].teaches (or its area's areaBossMoveId), which
    // is what makes it findable at all: drops come from the defeated enemy's kit, so a move
    // no boss carries cannot be earned. test_every_generic_move_is_carried holds that line.
    //
    // These are NICHE-first, not ladder-first: the rows do not sort by power, because what
    // they are for is giving a build something it could not do before. Two engine facts
    // make the extremes real rather than decorative:
    //
    //   * The STUN and DoT riders fire on any landed cast, damage or not (combat.cpp's
    //     applyEffect) — only the steal track is gated on dmg > 0. So a ~0-power move with
    //     a big rider is a working move, not a wasted turn: pure control, no impact.
    //   * A hit whose PRE-mitigation damage was non-zero is floored at 1 (the minimum
    //     penetration rule in Combat::applyEffect). So a low-power shred lands through any
    //     wall however braced the target is — the shred is the payload, the damage the
    //     delivery.
    //
    // Nothing here goes below power 3: `power * powerMultPct / 100` is what the floor tests,
    // and a Good-branch pet's softer multiplier truncates 1 and 2 to zero, which would take
    // the steal track down with it.
    //
    // Fields after minStage: line, stackPowerPct, stackPowerCap, stackDefensePct,
    // stackDefenseCap, armorPiercePct, lockTurns, dotDamage, dotTurns, stealPowerPct,
    // stealDefensePct, ... (defs.h). The stack*/shieldPool/trap*/replica* fields and
    // stealPowerPct stay ZERO on every row here — each belongs to a LINE's identity, and
    // stealPowerPct additionally feeds the Phishing frenzy combo on any move that carries
    // it, which a generic move must not do.

    // --- Citrus Circuit ------------------------------------------------------------
    {"fake_seed", "Fake Seed", MoveDef::Kind::Attack, 3, 1,
     "It isn't what the filename said - strips {stealDef}% of the target's armor.",
     Stage::BootSector, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, /*stealPower=*/0,
     /*stealDefensePct=*/25},
    {"stall_loop", "Stall Loop", MoveDef::Kind::Attack, 3, 1,
     "Hangs the target for {lock} turn. Hits for almost nothing - the turn is the point.",
     Stage::Process, nullptr, 0, 0, 0, 0, 0, /*lockTurns=*/1},
    {"infinite_loop", "Infinite Loop", MoveDef::Kind::Attack, 3, 1,
     "Never terminates - {dot} damage/turn for {dotTurns} turns, the longest rot there is.",
     Stage::Process, nullptr, 0, 0, 0, 0, 0, 0, /*dotDamage=*/5, /*dotTurns=*/6},
    {"shared_folder", "Shared Folder", MoveDef::Kind::Attack, 4, 1,
     "Everything in the folder at once - {dot} damage/turn for {dotTurns} turns, and "
     "strips {stealDef}% armor.",
     Stage::BootSector, nullptr, 0, 0, 0, 0, 0, 0, /*dotDamage=*/3, /*dotTurns=*/3,
     /*stealPower=*/0, /*stealDefensePct=*/15},
    {"toll_charge", "Toll Charge", MoveDef::Kind::Attack, 6, 1,
     "Bills by the minute - takes {stealMaxHp}% of the target's max Health for the fight.",
     Stage::Process, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, /*stealPower=*/0, /*stealDef=*/0,
     /*stealSpeed=*/0, /*stealHp=*/0, /*stealMaxHpPct=*/8},
    {"helper_monkey", "Helper Monkey", MoveDef::Kind::Attack, 5, 1,
     "Won't be dismissed - freezes {lock} turn AND rots {dot}/turn for {dotTurns} turns.",
     Stage::Process, nullptr, 0, 0, 0, 0, 0, /*lockTurns=*/1, /*dotDamage=*/4,
     /*dotTurns=*/3},

    // --- The Pirate Bayou — the cracking area, so ARMOR PIERCE is its family --------
    {"remote_handle", "Remote Handle", MoveDef::Kind::Attack, 10, 1,
     "Drives it from somewhere else - ignores {pierce}% of armor.", Stage::Process,
     nullptr, 0, 0, 0, 0, /*armorPiercePct=*/25},
    {"seed_leech", "Seed Leech", MoveDef::Kind::Attack, 5, 1,
     "Takes and never gives - {stealMaxHp}% of the target's max Health, for the fight.",
     Stage::Process, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, /*stealPower=*/0, /*stealDef=*/0,
     /*stealSpeed=*/0, /*stealHp=*/0, /*stealMaxHpPct=*/15},
    {"keygen_cut", "Keygen Cut", MoveDef::Kind::Attack, 14, 1,
     "Generates the key rather than asking - ignores {pierce}% of armor.", Stage::Process,
     nullptr, 0, 0, 0, 0, /*armorPiercePct=*/50},
    {"nuked_release", "Nuked Release", MoveDef::Kind::Attack, 8, 1,
     "Dumps a bad rip - {dot} damage/turn for {dotTurns}, and the nuke follows.",
     Stage::Process, nullptr, 0, 0, 0, 0, /*armorPiercePct=*/0, /*lockTurns=*/0,
     /*dotDamage=*/6, /*dotTurns=*/3, 0, 0, 0, 0, /*stealMaxHpPct=*/0,
     /*shieldPool=*/0, /*trapArm=*/0, 0, 0, 0, 0, /*replicaSpawnPct=*/0, 0, 0,
     /*chainNextId=*/"scene_nuke"},
    {"backdoor_knock", "Backdoor Knock", MoveDef::Kind::Attack, 9, 1,
     "Already had a key - ignores ALL {pierce}% armor and freezes {lock} turn.",
     Stage::Process, nullptr, 0, 0, 0, 0, /*armorPiercePct=*/100, /*lockTurns=*/1},
    {"crack_the_keys", "Crack The Keys", MoveDef::Kind::Attack, 20, 1,
     "No wall was ever the problem - ignores ALL {pierce}% of armor.", Stage::Script,
     nullptr, 0, 0, 0, 0, /*armorPiercePct=*/100},

    // --- Net-Sea Crossing — everything here arrives attached to something else ------
    {"bundle_wrap", "Bundle Wrap", MoveDef::Kind::Attack, 7, 1,
     "Three things you didn't ask for - {dot}/turn for {dotTurns} turns and "
     "{stealMaxHp}% of max Health.",
     Stage::Process, nullptr, 0, 0, 0, 0, 0, 0, /*dotDamage=*/3, /*dotTurns=*/3,
     /*stealPower=*/0, /*stealDef=*/0, /*stealSpeed=*/0, /*stealHp=*/0,
     /*stealMaxHpPct=*/10},
    {"popup_storm", "Pop-Up Storm", MoveDef::Kind::Attack, 3, 1,
     "They keep opening - {dot} damage/turn for {dotTurns} turns.", Stage::Script,
     nullptr, 0, 0, 0, 0, 0, 0, /*dotDamage=*/9, /*dotTurns=*/5},
    braceRow("cert_spoof", "Cert Spoof", 26,
             "Wears a certificate that isn't its own - braces {power}, {refund}% back.",
             Stage::Script, /*speedRefundPct=*/45),
    {"fake_codec", "Fake Codec", MoveDef::Kind::Attack, 8, 1,
     "The video was never a video - frozen for {lock} turns.", Stage::Process,
     nullptr, 0, 0, 0, 0, 0, /*lockTurns=*/2},
    {"mirror_click", "Mirror Click", MoveDef::Kind::Attack, 3, 1,
     "The other download button - strips {stealDef}% of the target's armor.",
     Stage::Script, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, /*stealPower=*/0,
     /*stealDefensePct=*/60},
    {"toolbar_convoy", "Toolbar Convoy", MoveDef::Kind::Attack, 6, 1,
     "Arrives in a stack - {dot}/turn for {dotTurns}, strips {stealDef}% armor, takes "
     "{stealMaxHp}% max Health.",
     Stage::Script, nullptr, 0, 0, 0, 0, 0, 0, /*dotDamage=*/5, /*dotTurns=*/3,
     /*stealPower=*/0, /*stealDefensePct=*/30, /*stealSpeed=*/0, /*stealHp=*/0,
     /*stealMaxHpPct=*/6},

    // --- Napstorrent Moors ---------------------------------------------------------
    {"admin_reversal", "Admin Reversal", MoveDef::Kind::Attack, 4, 1,
     "Reads the privilege backwards - strips ALL {stealDef}% of the target's armor.",
     Stage::Script, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, /*stealPower=*/0,
     /*stealDefensePct=*/100},
    {"mail_storm", "Mail Storm", MoveDef::Kind::Attack, 38, 1,
     "Mails itself to everyone at once. No rider, no wind-up - just the biggest swing.",
     Stage::Script},
    {"self_reference", "Self-Reference", MoveDef::Kind::Attack, 12, 1,
     "A copy that describes how to copy it - {dot} damage/turn for {dotTurns} turns.",
     Stage::Script, nullptr, 0, 0, 0, 0, 0, 0, /*dotDamage=*/8, /*dotTurns=*/4},
    braceRow("evade_trace", "Evade Trace", 32,
             "Catch it if you can - braces {power}, {refund}% of the wait back.",
             Stage::Script, /*speedRefundPct=*/40),
    {"attachment_bait", "Attachment Bait", MoveDef::Kind::Attack, 16, 1,
     "You opened it - frozen {lock} turns, then {dot}/turn for {dotTurns}.", Stage::Script,
     nullptr, 0, 0, 0, 0, 0, /*lockTurns=*/2, /*dotDamage=*/4, /*dotTurns=*/3},
    {"runaway_fork", "Runaway Fork", MoveDef::Kind::Attack, 52, 3,
     "Winds up {turns} turns, reinfecting what it already took - then {dot}/turn for "
     "{dotTurns}.",
     Stage::Script, nullptr, 0, 0, 0, 0, 0, 0, /*dotDamage=*/8, /*dotTurns=*/4},

    // --- Castle Rapidscare ---------------------------------------------------------
    {"rank_advance", "Rank Advance", MoveDef::Kind::Attack, 12, 1,
     "One square, every square, at once - ignores {pierce}% armor, {dot}/turn for "
     "{dotTurns}.",
     Stage::Script, nullptr, 0, 0, 0, 0, /*armorPiercePct=*/30, 0, /*dotDamage=*/4,
     /*dotTurns=*/2},
    {"mail_merge", "Mail Merge", MoveDef::Kind::Attack, 3, 1,
     "Fifty at a time, from your own address book - {dot} damage/turn for {dotTurns} turns.",
     Stage::Daemon, nullptr, 0, 0, 0, 0, 0, 0, /*dotDamage=*/14, /*dotTurns=*/4},
    {"false_positive", "False Positive", MoveDef::Kind::Attack, 5, 1,
     "Sells the cure for the disease it invented - takes {stealMaxHp}% of max Health.",
     Stage::Script, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, /*stealPower=*/0, /*stealDef=*/0,
     /*stealSpeed=*/0, /*stealHp=*/0, /*stealMaxHpPct=*/20},
    {"wild_card", "Wild Card", MoveDef::Kind::Attack, 22, 1,
     "Whatever it needs to be - ignores {pierce}% armor, freezes {lock} turn, {dot}/turn "
     "for {dotTurns}.",
     Stage::Daemon, nullptr, 0, 0, 0, 0, /*armorPiercePct=*/60, /*lockTurns=*/1,
     /*dotDamage=*/5, /*dotTurns=*/3},
    braceRow("premium_wait", "Premium Wait", 44,
             "Your download will begin shortly - braces {power}, {refund}% back.",
             Stage::Daemon, /*speedRefundPct=*/30),
    {"domain_flux", "Domain Flux", MoveDef::Kind::Attack, 18, 1,
     "A new address every day - {dot}/turn for {dotTurns} turns, and strips {stealDef}% "
     "armor.",
     Stage::Daemon, nullptr, 0, 0, 0, 0, 0, 0, /*dotDamage=*/10, /*dotTurns=*/5,
     /*stealPower=*/0, /*stealDefensePct=*/40},

    // === THE WILD POOL — one Attack + one Defend per zone, farmed off its own wilds ==
    //
    // Generic (line = nullptr), like the boss pool above and reachable the same way: a
    // drop is drawn from the defeated enemy's kit, so a move is findable exactly where
    // something carries it. What carries these is not a boss but the ORDINARY wild — each
    // zone names its pair (AreaDef::wildAttackMoveId/wildDefendMoveId, and the dive's own
    // kDeepWebWildAttackMoveId/kDeepWebWildDefendMoveId), and combat_factory's
    // applyWildSubAreaRamp/applyDeepWebScale hand it to every malbeast met there.
    //
    // So this pool answers a different question from the one above it. A boss move is a
    // prize for beating the wall at the end of a stretch; a wild move is what the stretch
    // is MADE of, and so what makes an area read as itself in a fight rather than as a
    // difficulty tier. Each pair is a weaker echo of the family its zone's bosses teach, so
    // the boss's move reads as the full-strength version of one already met.
    //
    // The ATTACK carries its zone's family rider (the Bayou pierces, the Moors just swings,
    // the crossing freezes) and the DEFEND is a plain brace, because a brace IS its power:
    // every rider fires on the Attack branch only (Combat::applyEffect), and the defensive
    // tracks — shieldPool, trapArm, replica* — each belong to a LINE. So the six braces are
    // a ladder of one number, deepest zone highest.
    //
    // Same field discipline as the boss pool: stack*/shieldPool/trap*/replica* and
    // stealPowerPct stay ZERO on every row here.

    // --- Citrus Circuit — nothing here ever finishes -------------------------------
    {"partial_download", "Partial Download", MoveDef::Kind::Attack, 5, 1,
     "Stops at ninety-nine and stays there - {dot} damage/turn for {dotTurns} turns.",
     Stage::BootSector, nullptr, 0, 0, 0, 0, 0, 0, /*dotDamage=*/2, /*dotTurns=*/3},
    braceRow("cache_miss", "Cache Miss", 10,
             "The file was never really there - braces {power}, {refund}% back.",
             Stage::BootSector, /*speedRefundPct=*/60),

    // --- The Pirate Bayou — the cracking water, so its wilds pierce too -------------
    {"keygen_hum", "Keygen Hum", MoveDef::Kind::Attack, 8, 1,
     "The chiptune plays while the wall opens - ignores {pierce}% of armor.",
     Stage::Process, nullptr, 0, 0, 0, 0, /*armorPiercePct=*/15},
    braceRow("rar_password", "RAR Password", 16,
             "The archive wants a password nobody posted - braces {power}, {refund}% back.",
             Stage::Process, /*speedRefundPct=*/55),

    // --- Net-Sea Crossing — it takes the turn, not the Health ----------------------
    {"install_wizard", "Install Wizard", MoveDef::Kind::Attack, 7, 1,
     "Next, next, next, finish - frozen for {lock} turn.", Stage::Process,
     nullptr, 0, 0, 0, 0, 0, /*lockTurns=*/1},
    braceRow("eula_wall", "EULA Wall", 22,
             "Forty pages nobody has ever read - braces {power}, {refund}% back.",
             Stage::Process, /*speedRefundPct=*/50),

    // --- Napstorrent Moors — the mail area, and mail is just the swing --------------
    {"chain_letter", "Chain Letter", MoveDef::Kind::Attack, 18, 1,
     "Forward it to ten more. No rider, no wind-up - just the swing.", Stage::Script},
    braceRow("private_tracker", "Private Tracker", 30,
             "Invite only, and you weren't invited - braces {power}, {refund}% back.",
             Stage::Script, /*speedRefundPct=*/45),

    // --- Castle Rapidscare — the keep charges for everything ------------------------
    {"bandwidth_cap", "Bandwidth Cap", MoveDef::Kind::Attack, 9, 1,
     "You have used your quota - takes {stealMaxHp}% of the target's max Health for "
     "the fight.",
     Stage::Script, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, /*stealPower=*/0, /*stealDef=*/0,
     /*stealSpeed=*/0, /*stealHp=*/0, /*stealMaxHpPct=*/10},
    braceRow("captcha_gate", "Captcha Gate", 38,
             "Prove you are not a robot - braces {power}, {refund}% back.",
             Stage::Script, /*speedRefundPct=*/35),

    // --- DeepWeb Dive — the deepest pair there is ------------------------------------
    {"exit_node", "Exit Node", MoveDef::Kind::Attack, 10, 1,
     "Everything you sent, read on the way out - strips {stealDef}% of the target's armor.",
     Stage::Daemon, nullptr, 0, 0, 0, 0, 0, 0, 0, 0, /*stealPower=*/0,
     /*stealDefensePct=*/35},
    braceRow("onion_layer", "Onion Layer", 48,
             "One more hop, one more layer - braces {power}, {refund}% back.",
             Stage::Daemon, /*speedRefundPct=*/30),
};
const int kMovesCount = sizeof(kMoves) / sizeof(kMoves[0]);

}  // namespace mal
