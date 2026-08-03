// content_mods.cpp — the hardware-mod roster (ModDef + ModEffect).
//
// One content table (see content_tables.h). Edit rows here; the registry picks
// them up unchanged via embedded_content.cpp.
#include "core/content/content_tables.h"

namespace mal {

// Mods roster. A mod's combat effect is a structured effectKind + magnitude
// (magnitude on the row — docs/CONTENT_STANDARD.md), applied data-driven in
// combat.cpp/game_combat. Two independent axes drive earn + power: `rarity` = DROP WEIGHT
// within an area's loot table; `powerTier` (1..4) = the EFFECTIVENESS rank that both picks
// the area a mod drops in (tiers 1/2/3 → Citrus / Pirate Bayou / Napstorrent; tier 4 is the
// endgame, split between Castle Rapidscare and the DeepWeb Dive)
// and sets its nominal equip-LEVEL band (each dropped copy rolls its own gate within it,
// kModEquipLevel* in tunables). Fields: id, name, tag, effect-text, oneShot, rarity,
// powerTier, effectKind, magnitude, magnitude2, line, affinityBonus. Per-mod glyph is
// ICON_MOD_<UPPER ID> (generic icon until art lands).
//
// Never type a magnitude into the description text: write `{mag}` / `{mag2}` /
// `{magBonus}` and effect_text.h substitutes it from this same row, so retuning a
// number retunes the prose. MODS detail also draws a derived stat line (statLine())
// that reports the effect kind + both magnitudes regardless of what the prose says.
const ModDef kMods[] = {
    // --- Area 0 · CITRUS CIRCUIT (tier 1) — the starter mods ---------------
    {"clock_speed_boost", "Clock-Speed Boost", "+SPD",
     "Raises battle initiative speed by {mag}.", false,
     ItemDef::Rarity::Common, 1, ModEffect::Speed, 4, 0, nullptr, 0},
    {"packet_sniffer", "Packet Sniffer", "+BITS",
     "Earns {mag} extra Bits from a won fight.", false,
     ItemDef::Rarity::Common, 1, ModEffect::PostBattleBits, 8, 0, nullptr, 0},
    {"crypto_coprocessor", "Crypto Coprocessor", "+POW",
     "Raises attack power by {mag}%.", false,
     ItemDef::Rarity::Uncommon, 1, ModEffect::PowerPct, 10, 0, nullptr, 0},

    // --- Area 1 · THE PIRATE BAYOU (tier 2) --------------------------------
    {"tpm_chip", "TPM Chip", "+DEF",
     "Cuts incoming damage by {mag}%.", false,
     ItemDef::Rarity::Uncommon, 2, ModEffect::DamageCutPct, 15, 0, nullptr, 0},
    {"solid_state_cache", "Solid-State Cache", "+HP",
     "Raises max Health by {mag}.", false,
     ItemDef::Rarity::Uncommon, 2, ModEffect::MaxHealth, 12, 0, nullptr, 0},
    {"firewall_patch", "Firewall Patch", "+DEF",
     "Cuts incoming damage by {mag}%.", false,
     ItemDef::Rarity::Rare, 2, ModEffect::DamageCutPct, 40, 0, nullptr, 0},

    // --- Area 2 · NAPSTORRENT MOORS (tier 3) -------------------------------
    {"overclock_chip", "Overclock Chip", "+SPD",
     "Raises battle initiative speed by {mag}; costs {mag2}% power.", false,
     ItemDef::Rarity::Uncommon, 3, ModEffect::Speed, 5, 8, nullptr, 0},
    {"heat_sink", "Heat Sink", "-FRAG",
     "Cuts battle-fatigue Frag by {mag}%.", false,
     ItemDef::Rarity::Rare, 3, ModEffect::FatigueFragCut, 60, 0, nullptr, 0},
    {"honeytoken", "Honeytoken", "THORNS",
     "Chips any attacker that hits you for {mag}.", false,
     ItemDef::Rarity::Rare, 3, ModEffect::Thorns, 4, 0, nullptr, 0},
    // Signature LINE-AFFINITY mod (still line-agnostic — anyone can slot it — but a
    // Ransomware pet gets a bonus, on-identity for its Cipher wall). Proves the affinity
    // hook; a line-TYPED boss drop is a future content pass (bosses aren't typed yet).
    {"cipher_asic", "Cipher ASIC", "+DEF",
     "Cuts damage {mag}% ({magBonus}% for Ransomware).", false,
     ItemDef::Rarity::Rare, 3, ModEffect::DamageCutPct, 10, 0, "ransomware", 10},

    // --- DeepWeb Dive (tier 4) — the endgame mods --------------------------
    {"deadman_switch", "Deadman Switch", "ON-KO",
     "On your KO, blasts the enemy for {mag}.", false,
     ItemDef::Rarity::Epic, 4, ModEffect::DeathBlast, 12, 0, nullptr, 0},
    {"raid_mirror", "RAID Mirror", "1-SHOT",
     "Survives one fatal hit, then is consumed.", true,
     ItemDef::Rarity::Epic, 4, ModEffect::RaidMirror, 0, 0, nullptr, 0},

    // --- Deferred-mod pass · ECC Memory ------------------------------------
    // The FIRST of the four deferred mods to get its primitive (max single-hit cap;
    // MaxHitCapPct clamp in combat.cpp). Content SIGNED OFF: Epic / tier 4 / cap = 35%
    // of max Health, earned from the DeepWeb Dive's mod pool
    // (src/core/content/areas/deepweb_dive/area.cpp) alongside deadman_switch /
    // raid_mirror — the defensive endgame pool.
    {"ecc_memory", "ECC Memory", "HIT-CAP",
     "No single hit exceeds {mag}% of max Health.", false,
     ItemDef::Rarity::Epic, 4, ModEffect::MaxHitCapPct, 35, 0, nullptr, 0},

    // --- Deferred-mod pass · Load Balancer ---------------------------------
    // The SECOND deferred mod: a big hit (>= magnitude% of max Health) is SPLIT — magnitude2%
    // deferred to the victim's next turn-start (the Load Balancer's queue in combat.cpp), the rest
    // lands now. It does NOT reduce total damage (unlike ECC) — it buys a turn to heal / land
    // a KO first; if outrun, the debt still lands (can KO). Content SIGNED OFF: Epic / tier 4
    // / split hits >= 30% of max Health, deferring 50% (the "balance" half), earned from the
    // DeepWeb Dive's mod pool with the other defensive-endgame Epics. Calibrated
    // to fire on the ×1.75 DeepWeb detonations (fork_bomb 45 / rootkit 42), just under ECC's
    // 35% cap so the two compose (LB spreads the merely-big; ECC hard-caps the very biggest).
    {"load_balancer", "Load Balancer", "SPLIT",
     "Hits over {mag}% of max Health split - {mag2}% lands next turn.", false,
     ItemDef::Rarity::Epic, 4, ModEffect::LoadBalance, 30, 50, nullptr, 0},

    // --- Deferred-mod pass · Watchdog Timer + Faraday Cage (the last two) ----------
    // COUNTERS to the threat riders above (system_hang stun / data_rot DoT), primitives
    // built + unit-tested (WatchdogClamp / FaradayCut in combat.cpp). Content SIGNED OFF
    // with THREAT-ADJACENT placement: each drops in the area where its threat first
    // appears, so its powerTier mirrors that area (NOT tier 4 like ECC/Load Balancer).
    //  • Watchdog (magnitude = max turns a lock lasts; 1 = never lose >1 turn): earned in
    //    tier 2, The Pirate Bayou's own mod pool (areas/pirate_bayou/area.cpp), whose
    //    signature boss wields system_hang (the stun, AreaDef::apexThreatMoveId).
    //  • Faraday (magnitude = % of incoming DoT cut; 100 = fully immune): earned in tier 3,
    //    Napstorrent Moors' own mod pool (areas/napstorrent_moors/area.cpp), whose
    //    signature boss wields data_rot.
    // Both stay Epic (rarest drop weight) — a rare hard-counter find in a mid area.
    {"watchdog_timer", "Watchdog Timer", "UNLOCK",
     "Never frozen more than {mag} turn.", false,
     ItemDef::Rarity::Epic, 2, ModEffect::WatchdogClamp, 1, 0, nullptr, 0},
    {"faraday_cage", "Faraday Cage", "SHIELD",
     "Cuts corruption damage-over-time by {mag}%.", false,
     ItemDef::Rarity::Epic, 3, ModEffect::FaradayCut, 100, 0, nullptr, 0},


    // --- Area 0 · CITRUS CIRCUIT (tier 1) ---
    {"canary_trap", "Canary Trap", "1ST-CUT",
     "First hit taken each fight is cut an extra {mag}%.", false,
     ItemDef::Rarity::Rare, 1, ModEffect::FirstHitCutPct, 50, 0, nullptr, 0},
    {"scratch_disk_buffer", "Scratch Disk Buffer", "+DEF",
     "Cuts incoming damage by {mag}%.", false,
     ItemDef::Rarity::Common, 1, ModEffect::DamageCutPct, 8, 0, nullptr, 0},

    // --- Area 1 · THE PIRATE BAYOU (tier 2) ---
    {"botnet_swarm", "Botnet Swarm", "+POW/ATK",
     "Attack power rises {mag}% per equipped Attack move.", false,
     ItemDef::Rarity::Uncommon, 2, ModEffect::AttackCountPowerPct, 6, 0, nullptr, 0},
    {"airgap_ward", "Air-Gap Ward", "+DEF/DEF",
     "Damage cut rises {mag}% per equipped Defend move.", false,
     ItemDef::Rarity::Uncommon, 2, ModEffect::DefendCountCutPct, 6, 0, nullptr, 0},
    {"tripwire", "Tripwire", "THORNS",
     "Below {mag2}% Health, reflects {mag} damage to any attacker.", false,
     ItemDef::Rarity::Rare, 2, ModEffect::ConditionalThorns, 10, 40, nullptr, 0},
    {"cold_storage", "Cold Storage", "+HP/-SPD",
     "Raises max Health by {mag}; costs {mag2} initiative.", false,
     ItemDef::Rarity::Uncommon, 2, ModEffect::MaxHealth, 20, 2, nullptr, 0},

    // --- Area 2 · NAPSTORRENT MOORS (tier 3) ---
    {"prowlware", "Prowlware", "1ST HIT",
     "First damaging hit multiplies by your Attack-move power rank.", false,
     ItemDef::Rarity::Rare, 3, ModEffect::FirstStrikeRankMult, 0, 0, nullptr, 0},
    {"meltdown_core", "Meltdown Core", "COMEBACK",
     "Below {mag}% Health, attack power rises {mag2}%.", false,
     ItemDef::Rarity::Rare, 3, ModEffect::LowHealthPowerPct, 30, 40, nullptr, 0},
    {"zero_day_exploit", "Zero-Day Exploit", "GAMBLE",
     "{mag}% chance to raise attack power {mag2}% for the whole fight.", false,
     ItemDef::Rarity::Rare, 3, ModEffect::GambleBattlePowerPct, 25, 60, nullptr, 0},

    // --- DeepWeb Dive (tier 4) — the endgame mods, incl. the two hard-gated signatures --
    {"phishing_rod", "Phishing Rod", "SIPHON+",
     "Waiting for that perfect bite: while your bubble's up, amplifies the bonus "
     "siphon by {mag}%. Phishing pets only.", false,
     ItemDef::Rarity::Epic, 4, ModEffect::StealAmplifyPct, 75, 0, nullptr, 0,
     /*requiresLine=*/"phishing"},
    {"extortion_ledger", "Extortion Ledger", "+POW",
     "Raises attack power by {mag}%. Ransomware pets only.", false,
     ItemDef::Rarity::Epic, 4, ModEffect::PowerPct, 30, 0, nullptr, 0,
     /*requiresLine=*/"ransomware"},
    {"backup_uplink", "Backup Uplink", "+BITS",
     "Earns {mag} extra Bits from a won fight.", false,
     ItemDef::Rarity::Rare, 4, ModEffect::PostBattleBits, 20, 0, nullptr, 0},

    // --- Area 3 · CASTLE RAPIDSCARE (tier 4) — the keep's own signature ---
    // The Heat Sink's endgame answer: where the tier-3 Rare shaves battle fatigue, the
    // keep's Epic erases it, so a long gauntlet costs no MAINT afterwards. Sold at THE
    // GHOST IN THE MACHINE as well as dropped, since it is what the area is FOR.
    {"ghost_process", "Ghost Process", "-FRAG",
     "Battle fatigue leaves no trace: cuts Frag by {mag}%.", false,
     ItemDef::Rarity::Epic, 4, ModEffect::FatigueFragCut, 100, 0, nullptr, 0},
};
const int kModsCount = sizeof(kMods) / sizeof(kMods[0]);

}  // namespace mal
