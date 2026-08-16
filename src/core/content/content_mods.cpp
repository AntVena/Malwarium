// content_mods.cpp — the hardware-mod roster (ModDef + ModEffect).
//
// One content table (see content_tables.h). Edit rows here; the registry picks
// them up unchanged via embedded_content.cpp.
#include "core/content/content_tables.h"

namespace mal {

// Mods roster. A mod's combat effect is a structured effectKind + magnitude
// (magnitude on the row — docs/CONTENT_STANDARD.md), applied data-driven in
// combat.cpp/game_combat. THREE independent axes drive earn + power:
//
//   `rarity`     — DROP WEIGHT within an area's loot table. How common, not how good.
//   `powerTier`  — ladder DEPTH (1..kModPowerTiers). Picks WHICH AREA's pool drops it,
//                  and nothing else. A rank IS a depth — rank N is what the Nth rung
//                  hands out — so the section headers below name the area, never its
//                  index: splice an area into kAreaList and the rows under and after it
//                  move up a rank together, which is the one edit this table needs to
//                  stay true.
//   `equipLevel` — the pet level the mod needs before it can be EQUIPPED. Authored per
//                  row, ONE level per mod, the same for every copy, because the player
//                  picks a mod by TYPE and the picker has never had a way to show them
//                  one copy over another.
//
// The gate is authored, not derived, and that is what makes the ladder DENSE: within a
// tier's band the gates step, so a raising pet gains something new every level or two
// rather than everything in an area at once and then nothing until the next one. What the
// tier guarantees is ORDER — a deeper tier never gates lower than a shallower one — which
// is the property test_mod_equip_ladder_is_ordered_and_dense pins, along with the absence
// of dead bands.
//
// Bands are TWELVE levels wide, by tier: 0-11 · 12-23 · 24-35 · 36-47 · 48-60. Twelve
// rather than ten because the roster is sized to fill 0-60, which is where a SIXTH area
// would begin rather than where the fifth ends — a ladder that already runs that deep
// takes a new rung by extending past 60 into the headroom, instead of having to re-band
// every row that shipped. The ceiling itself is kModEquipLevelMax (tunables.h, 100),
// which explains why it sits so far above the roster.
//
// LINE mods come in two shapes, and the shape follows the EFFECT:
//   • soft affinity (`line` + `affinityBonus`) sits on a GENERIC effect kind — anyone can
//     slot it and it does something for them; a matching pet just gets more. One per line
//     in the Bayou band, so a pet's line starts paying at ~level 14 rather than 39.
//   • hard gate (`requiresLine`) sits on a LINE-PASSIVE amplifier — off-line it would be
//     inert rather than weak, so it is blocked at equip time instead. One per line at the
//     bottom of the ladder.
//
// `wire` is the save identity and is spent forever once used (defs.h). Fields: wire, id,
// name, tag, effect-text, oneShot, rarity, powerTier, equipLevel, effectKind, magnitude,
// magnitude2, line, affinityBonus, requiresLine. Per-mod glyph is
// ICON_MOD_<UPPER ID> — every row here has one, so a NEW row needs a new PNG or it draws
// nothing at all (mods_screen skips a missing sprite rather than substituting).
//
// Never type a magnitude into the description text: write `{mag}` / `{mag2}` /
// `{magBonus}` and effect_text.h substitutes it from this same row, so retuning a
// number retunes the prose. MODS detail also draws a derived stat line (statLine())
// that reports the effect kind + both magnitudes regardless of what the prose says.
const ModDef kMods[] = {
    // --- CITRUS CIRCUIT (tier 1) — the starter mods ------------------------
    {/*wire=*/1, "clock_speed_boost", "Clock-Speed Boost", "+SPD",
     "Raises battle initiative speed by {mag}.", false,
     ItemDef::Rarity::Common, 1, 2, ModEffect::Speed, 4, 0, nullptr, 0},
    {/*wire=*/2, "packet_sniffer", "Packet Sniffer", "+BITS",
     "Earns {mag} extra Bits from a won fight.", false,
     ItemDef::Rarity::Common, 1, 0, ModEffect::PostBattleBits, 8, 0, nullptr, 0},
    {/*wire=*/3, "crypto_coprocessor", "Crypto Coprocessor", "+POW",
     "Raises attack power by {mag}%.", false,
     ItemDef::Rarity::Uncommon, 1, 6, ModEffect::PowerPct, 10, 0, nullptr, 0},

    // --- THE PIRATE BAYOU (tier 2) -----------------------------------------
    {/*wire=*/4, "tpm_chip", "TPM Chip", "+DEF",
     "Cuts incoming damage by {mag}%.", false,
     ItemDef::Rarity::Uncommon, 2, 13, ModEffect::DamageCutPct, 15, 0, nullptr, 0},
    {/*wire=*/5, "solid_state_cache", "Solid-State Cache", "+HP",
     "Raises max Health by {mag}.", false,
     ItemDef::Rarity::Uncommon, 2, 12, ModEffect::MaxHealth, 12, 0, nullptr, 0},
    {/*wire=*/6, "firewall_patch", "Firewall Patch", "+DEF",
     "Cuts incoming damage by {mag}%.", false,
     ItemDef::Rarity::Rare, 2, 22, ModEffect::DamageCutPct, 40, 0, nullptr, 0},

    // --- NAPSTORRENT MOORS (tier 4) ----------------------------------------
    {/*wire=*/7, "overclock_chip", "Overclock Chip", "+SPD",
     "Raises battle initiative speed by {mag}; costs {mag2}% power.", false,
     ItemDef::Rarity::Uncommon, 4, 36, ModEffect::Speed, 5, 8, nullptr, 0},
    {/*wire=*/8, "heat_sink", "Heat Sink", "-FRAG",
     "Cuts battle-fatigue Frag by {mag}%.", false,
     ItemDef::Rarity::Rare, 4, 43, ModEffect::FatigueFragCut, 60, 0, nullptr, 0},
    {/*wire=*/9, "honeytoken", "Honeytoken", "THORNS",
     "Chips any attacker that hits you for {mag}.", false,
     ItemDef::Rarity::Rare, 4, 37, ModEffect::Thorns, 4, 0, nullptr, 0},
    // Signature LINE-AFFINITY mod (still line-agnostic — anyone can slot it — but a
    // Ransomware pet gets a bonus, on-identity for its Cipher wall). The DEEP end of the
    // soft-affinity pattern the Bayou band now opens for all four lines.
    {/*wire=*/10, "cipher_asic", "Cipher ASIC", "+DEF",
     "Cuts damage {mag}% ({magBonus}% for Ransomware).", false,
     ItemDef::Rarity::Rare, 4, 39, ModEffect::DamageCutPct, 10, 0, "ransomware", 10},

    // --- DeepWeb Dive (tier 5) — the endgame mods --------------------------
    {/*wire=*/11, "deadman_switch", "Deadman Switch", "ON-KO",
     "On your KO, blasts the enemy for {mag}.", false,
     ItemDef::Rarity::Epic, 5, 49, ModEffect::DeathBlast, 12, 0, nullptr, 0},
    {/*wire=*/12, "raid_mirror", "RAID Mirror", "1-SHOT",
     "Survives one fatal hit, then is consumed.", true,
     ItemDef::Rarity::Epic, 5, 52, ModEffect::RaidMirror, 0, 0, nullptr, 0},

    // --- Deferred-mod pass · ECC Memory ------------------------------------
    // The FIRST of the four deferred mods to get its primitive (max single-hit cap;
    // MaxHitCapPct clamp in combat.cpp). Content SIGNED OFF: Epic / DeepWeb rank / cap = 35%
    // of max Health, earned from the DeepWeb Dive's mod pool
    // (src/core/content/areas/deepweb_dive/area.cpp) alongside deadman_switch /
    // raid_mirror — the defensive endgame pool.
    {/*wire=*/13, "ecc_memory", "ECC Memory", "HIT-CAP",
     "No single hit exceeds {mag}% of max Health.", false,
     ItemDef::Rarity::Epic, 5, 50, ModEffect::MaxHitCapPct, 35, 0, nullptr, 0},

    // --- Deferred-mod pass · Load Balancer ---------------------------------
    // The SECOND deferred mod: a big hit (>= magnitude% of max Health) is SPLIT — magnitude2%
    // deferred to the victim's next turn-start (the Load Balancer's queue in combat.cpp), the rest
    // lands now. It does NOT reduce total damage (unlike ECC) — it buys a turn to heal / land
    // a KO first; if outrun, the debt still lands (can KO). Content SIGNED OFF: Epic / DeepWeb rank
    // / split hits >= 30% of max Health, deferring 50% (the "balance" half), earned from the
    // DeepWeb Dive's mod pool with the other defensive-endgame Epics. Calibrated
    // to fire on the ×1.75 DeepWeb detonations (fork_bomb 45 / rootkit 42), just under ECC's
    // 35% cap so the two compose (LB spreads the merely-big; ECC hard-caps the very biggest).
    {/*wire=*/14, "load_balancer", "Load Balancer", "SPLIT",
     "Hits over {mag}% of max Health split - {mag2}% lands next turn.", false,
     ItemDef::Rarity::Epic, 5, 51, ModEffect::LoadBalance, 30, 50, nullptr, 0},

    // --- Deferred-mod pass · Watchdog Timer + Faraday Cage (the last two) ----------
    // COUNTERS to the threat riders above (system_hang stun / data_rot DoT), primitives
    // built + unit-tested (WatchdogClamp / FaradayCut in combat.cpp). Content SIGNED OFF
    // with THREAT-ADJACENT placement: each drops in the area where its threat first
    // appears, so its powerTier mirrors that area (NOT the DeepWeb rank like ECC/Load Balancer).
    //  • Watchdog (magnitude = max turns a lock lasts; 1 = never lose >1 turn): earned in
    //    The Pirate Bayou's own mod pool (areas/pirate_bayou/area.cpp), whose
    //    signature boss wields system_hang (the stun, AreaDef::apexThreatMoveId). The
    //    Net-Sea Crossing RE-stocks it, because its own apex wields the same rider a turn
    //    longer (decoy_download) — the same restock the keep does, for the same reason.
    //  • Faraday (magnitude = % of incoming DoT cut; 100 = fully immune): earned in
    //    Napstorrent Moors' own mod pool (areas/napstorrent_moors/area.cpp), whose
    //    signature boss wields data_rot.
    // Both stay Epic (rarest drop weight) — a rare hard-counter find in a mid area.
    {/*wire=*/15, "watchdog_timer", "Watchdog Timer", "UNLOCK",
     "Never frozen more than {mag} turn.", false,
     ItemDef::Rarity::Epic, 2, 23, ModEffect::WatchdogClamp, 1, 0, nullptr, 0},
    {/*wire=*/16, "faraday_cage", "Faraday Cage", "SHIELD",
     "Cuts corruption damage-over-time by {mag}%.", false,
     ItemDef::Rarity::Epic, 4, 47, ModEffect::FaradayCut, 100, 0, nullptr, 0},


    // --- CITRUS CIRCUIT (tier 1) ---
    {/*wire=*/17, "canary_trap", "Canary Trap", "1ST-CUT",
     "First hit taken each fight is cut an extra {mag}%.", false,
     ItemDef::Rarity::Rare, 1, 10, ModEffect::FirstHitCutPct, 50, 0, nullptr, 0},
    {/*wire=*/18, "scratch_disk_buffer", "Scratch Disk Buffer", "+DEF",
     "Cuts incoming damage by {mag}%.", false,
     ItemDef::Rarity::Common, 1, 0, ModEffect::DamageCutPct, 8, 0, nullptr, 0},

    // --- THE PIRATE BAYOU (tier 2) ---
    {/*wire=*/19, "botnet_swarm", "Botnet Swarm", "+POW/ATK",
     "Attack power rises {mag}% per equipped Attack move.", false,
     ItemDef::Rarity::Uncommon, 2, 16, ModEffect::AttackCountPowerPct, 6, 0, nullptr, 0},
    {/*wire=*/20, "airgap_ward", "Air-Gap Ward", "+DEF/DEF",
     "Damage cut rises {mag}% per equipped Defend move.", false,
     ItemDef::Rarity::Uncommon, 2, 17, ModEffect::DefendCountCutPct, 6, 0, nullptr, 0},
    {/*wire=*/21, "tripwire", "Tripwire", "THORNS",
     "Below {mag2}% Health, reflects {mag} damage to any attacker.", false,
     ItemDef::Rarity::Rare, 2, 21, ModEffect::ConditionalThorns, 10, 40, nullptr, 0},
    {/*wire=*/22, "cold_storage", "Cold Storage", "+HP/-SPD",
     "Raises max Health by {mag}; costs {mag2} initiative.", false,
     ItemDef::Rarity::Uncommon, 2, 20, ModEffect::MaxHealth, 20, 2, nullptr, 0},

    // --- NET-SEA CROSSING (tier 3) — the open-water pool -------------------
    // The crossing's own mods are the seamanship ones: keep the hull intact, see what
    // is coming, and strip the junk off whatever you hauled aboard.
    {/*wire=*/23, "hardened_shell", "Hardened Shell", "+DEF",
     "Cuts incoming damage by {mag}%.", false,
     ItemDef::Rarity::Uncommon, 3, 27, ModEffect::DamageCutPct, 20, 0, nullptr, 0},
    {/*wire=*/24, "bundle_stripper", "Bundle Stripper", "1ST-CUT",
     "First hit taken each fight is cut an extra {mag}%.", false,
     ItemDef::Rarity::Rare, 3, 35, ModEffect::FirstHitCutPct, 60, 0, nullptr, 0},
    {/*wire=*/25, "ballast_cache", "Ballast Cache", "+HP",
     "Raises max Health by {mag}.", false,
     ItemDef::Rarity::Uncommon, 3, 25, ModEffect::MaxHealth, 30, 0, nullptr, 0},
    {/*wire=*/26, "sonar_ping", "Sonar Ping", "+SPD",
     "Raises battle initiative speed by {mag}.", false,
     ItemDef::Rarity::Uncommon, 3, 26, ModEffect::Speed, 7, 0, nullptr, 0},
    {/*wire=*/27, "salvage_rig", "Salvage Rig", "+BITS",
     "Earns {mag} extra Bits from a won fight.", false,
     ItemDef::Rarity::Common, 3, 24, ModEffect::PostBattleBits, 14, 0, nullptr, 0},

    // --- NAPSTORRENT MOORS (tier 4) ---
    {/*wire=*/28, "prowlware", "Prowlware", "1ST HIT",
     "First damaging hit multiplies by your Attack-move power rank.", false,
     ItemDef::Rarity::Rare, 4, 45, ModEffect::FirstStrikeRankMult, 0, 0, nullptr, 0},
    {/*wire=*/29, "meltdown_core", "Meltdown Core", "COMEBACK",
     "Below {mag}% Health, attack power rises {mag2}%.", false,
     ItemDef::Rarity::Rare, 4, 40, ModEffect::LowHealthPowerPct, 30, 40, nullptr, 0},
    {/*wire=*/30, "zero_day_exploit", "Zero-Day Exploit", "GAMBLE",
     "{mag}% chance to raise attack power {mag2}% for the whole fight.", false,
     ItemDef::Rarity::Rare, 4, 42, ModEffect::GambleBattlePowerPct, 25, 60, nullptr, 0},

    // --- DeepWeb Dive (tier 5) — the endgame mods, incl. the two hard-gated signatures --
    {/*wire=*/31, "phishing_rod", "Phishing Rod", "SIPHON+",
     "Waiting for that perfect bite: while your bubble's up, amplifies the bonus "
     "siphon by {mag}%.", false,
     ItemDef::Rarity::Epic, 5, 56, ModEffect::StealAmplifyPct, 75, 0, nullptr, 0,
     /*requiresLine=*/"phishing"},
    {/*wire=*/32, "extortion_ledger", "Extortion Ledger", "+POW",
     "Raises attack power by {mag}%. Ransomware pets only.", false,
     ItemDef::Rarity::Epic, 5, 58, ModEffect::PowerPct, 30, 0, nullptr, 0,
     /*requiresLine=*/"ransomware"},
    {/*wire=*/33, "backup_uplink", "Backup Uplink", "+BITS",
     "Earns {mag} extra Bits from a won fight.", false,
     ItemDef::Rarity::Rare, 5, 48, ModEffect::PostBattleBits, 20, 0, nullptr, 0},

    // --- CASTLE RAPIDSCARE (tier 5) — the keep's own signature ---
    // The Heat Sink's endgame answer: where the Moors' Rare shaves battle fatigue, the
    // keep's Epic erases it, so a long gauntlet costs no MAINT afterwards. Sold at THE
    // GHOST IN THE MACHINE as well as dropped, since it is what the area is FOR.
    {/*wire=*/34, "ghost_process", "Ghost Process", "-FRAG",
     "Battle fatigue leaves no trace: cuts Frag by {mag}%.", false,
     ItemDef::Rarity::Epic, 5, 60, ModEffect::FatigueFragCut, 100, 0, nullptr, 0},

    // ==== LADDER COVERAGE =========================================================
    // The rows below fill the ladder rather than extend it: they exist to keep the two
    // shallowest bands stocked with a spread of effect FAMILIES, and to give every
    // creature line both an early soft-affinity mod and a hard-gated build-around. Read
    // them as coverage, so a gap either one leaves is the reason to add the next row.

    // --- CITRUS CIRCUIT (tier 1) — the two families the starter band lacked ---
    // A starter pet could raise every stat but Health, and could not answer a hitter at
    // all. Both are the smallest possible version of a family that ladders up later
    // (Solid-State Cache -> Ballast Cache; Honeytoken is still the top of the thorns
    // ladder — this is the first taste of it, not a rival to it).
    {/*wire=*/35, "spare_ram_stick", "Spare RAM Stick", "+HP",
     "Raises max Health by {mag}.", false,
     ItemDef::Rarity::Common, 1, 4, ModEffect::MaxHealth, 8, 0, nullptr, 0},
    {/*wire=*/36, "capacitor_bank", "Capacitor Bank", "THORNS",
     "Holds a charge: chips any attacker that hits you for {mag}.", false,
     ItemDef::Rarity::Uncommon, 1, 8, ModEffect::Thorns, 1, 0, nullptr, 0},

    // --- THE PIRATE BAYOU (tier 2) — one soft-affinity mod per LINE ---
    // The first taste of line identity, one per line, at gates within five levels of each
    // other so no line waits much longer than another for it. The Moors' Cipher ASIC is
    // the same shape a full band deeper — these are what keep it from being the earliest.
    // Every one sits on a GENERIC effect kind: a Worm can slot the Escrow Buffer and get
    // real Health out of it, a Ransomware pet just gets more. That is what keeps these
    // soft — the hard gates are reserved for the passive amplifiers at the bottom.
    {/*wire=*/37, "spoof_header", "Spoof Header", "+SPD",
     "Forged and first in the queue: {mag} initiative "
     "({magBonus} for Phishing).", false,
     ItemDef::Rarity::Uncommon, 2, 14, ModEffect::Speed, 4, 0, "phishing", 3},
    {/*wire=*/38, "escrow_buffer", "Escrow Buffer", "+HP",
     "Raises max Health by {mag} ({magBonus} for Ransomware).", false,
     ItemDef::Rarity::Uncommon, 2, 15, ModEffect::MaxHealth, 14, 0, "ransomware", 6},
    {/*wire=*/39, "dropper_payload", "Dropper Payload", "+POW",
     "Raises attack power by {mag}% ({magBonus}% for Trojan).", false,
     ItemDef::Rarity::Uncommon, 2, 18, ModEffect::PowerPct, 13, 0, "trojan", 5},
    {/*wire=*/40, "fork_spur", "Fork Spur", "THORNS",
     "Chips any attacker that hits you for {mag} ({magBonus} for Worm).", false,
     ItemDef::Rarity::Rare, 2, 19, ModEffect::Thorns, 2, 0, "worm", 2},

    // --- NET-SEA CROSSING (tier 3) — the mid rungs the crossing was missing ---
    // The crossing stocked five mods, all of them hull-and-lookout: a pet crossing it had
    // no mid-ladder answer for fatigue, no thorns between the starter's chip and the
    // Moors', and nothing offensive at all. Same seamanship read as the rows above.
    {/*wire=*/41, "bilge_pump", "Bilge Pump", "-FRAG",
     "Pumps the fatigue back overboard: cuts Frag by {mag}%.", false,
     ItemDef::Rarity::Uncommon, 3, 28, ModEffect::FatigueFragCut, 35, 0, nullptr, 0},
    {/*wire=*/42, "barnacle_plating", "Barnacle Plating", "THORNS",
     "Chips any attacker that hits you for {mag}.", false,
     ItemDef::Rarity::Rare, 3, 30, ModEffect::Thorns, 3, 0, nullptr, 0},
    {/*wire=*/43, "harpoon_mount", "Harpoon Mount", "+POW",
     "Raises attack power by {mag}%.", false,
     ItemDef::Rarity::Uncommon, 3, 31, ModEffect::PowerPct, 18, 0, nullptr, 0},
    {/*wire=*/44, "distress_beacon", "Distress Beacon", "COMEBACK",
     "Below {mag}% Health, attack power rises {mag2}%.", false,
     ItemDef::Rarity::Rare, 3, 33, ModEffect::LowHealthPowerPct, 25, 30, nullptr, 0},

    // --- DeepWeb Dive (tier 5) — the Trojan and Worm build-arounds ---
    // Parity with Phishing Rod and Extortion Ledger: each of the four lines now has one
    // hard-gated Epic at the bottom of the ladder that multiplies its own passive. Both
    // are requiresLine for the reason Phishing Rod is — the passive is what they scale,
    // so off-line they would be dead rows rather than weak ones, and a dead row in the
    // picker is worse than a blocked one that says why.
    {/*wire=*/45, "ring_zero_shim", "Ring-0 Shim", "HIJACK+",
     "Runs your code a ring too deep: {mag}% more chance to hijack "
     "the enemy's move. Trojan pets only.", false,
     ItemDef::Rarity::Epic, 5, 54, ModEffect::ExecOverridePct, 12, 0, nullptr, 0,
     /*requiresLine=*/"trojan"},
    {/*wire=*/46, "replication_bus", "Replication Bus", "COPY+",
     "Widens the bus every copy travels: {mag}% more chance to replicate. "
     "Worm pets only.", false,
     ItemDef::Rarity::Epic, 5, 55, ModEffect::ReplicaSpawnPct, 20, 0, nullptr, 0,
     /*requiresLine=*/"worm"},
};
const int kModsCount = sizeof(kMods) / sizeof(kMods[0]);

}  // namespace mal
