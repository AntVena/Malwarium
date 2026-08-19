// tunables.h — engine balance constants (decay rates, thresholds, budgets).
//
// These are deliberately named constants, not magic numbers, so balance can be
// tuned without touching logic, and tests assert against the constants rather
// than hard-coded numbers. They are board-agnostic, so they live here rather
// than in the board-specific config.h. Values are first-cut defaults, not
// final balance — change here only.
#pragma once

#include <algorithm>
#include <cstdint>

namespace mal {

// Starting vitals -------------------------------------
constexpr int kStartHunger = 80;
constexpr int kStartFragmentation = 50;  // a fresh pet boots half-fragmented (Caution)
constexpr int kStartHappiness = 70;

// Decay (Process stage). Stored as game-minutes per point;
//     Fragmentation does not decay passively. ---------------------------------
constexpr int kHungerMinutesPerPoint = 15;     // -1 Hunger / 15 min
constexpr int kHappinessMinutesPerPoint = 30;  // -1 Happiness / 30 min

// Zone thresholds (— spec defaults). Vitality: more is healthier;
//     Hazard: more is worse. -------------------------------------------------
constexpr int kHungerCautionMax = 30;   // OK > 30
constexpr int kHungerCriticalMax = 15;  // Critical <= 15 (also idle hunger alert)
constexpr int kFragCautionMin = 40;     // OK < 40
constexpr int kFragCriticalMin = 75;    // Critical >= 75
constexpr int kHappyCautionMin = 30;    // OK >= 30
constexpr int kHappyCriticalMax = 9;    // Critical < 10

// --- Care-mistake budget (5-step) ------------------------------------------
constexpr int kCareGoodMax = 2;   // 0..2 Good (calm)
constexpr int kCareBadMax = 4;    // 3..4 Bad (hot)
constexpr int kCareDying = 5;     // 5/5 dying (hot, pulses)

// Maintenance (MAINT) -------------------------------
constexpr int kDefragReduction = 20;   // -20 Fragmentation on a successful Defrag
constexpr int kAvReduction = 10;       // -10 Fragmentation on a successful AV scan
constexpr int kMaintFailPenalty = 15;  // +15 Fragmentation on a failed run
// A Defrag costs Bits, scaled by the pet's evolution stage.
// Indexed by Stage (BootSector/Process/Script/Daemon). Boot is 0 —
// Charged per attempt regardless of pass/fail.
constexpr int kDefragCostByStage[4] = {0, 5, 15, 30};
// Three defrag variants share the stage-scaled Bits cost above, differing only in what
// ELSE they ask for: QUICK is Bits-only with the normal success roll (luck), TOOL
// additionally spends one Defrag Tool item (kDefragToolId, content_items.cpp) for a
// guaranteed clean (an item), and STACKER plays the minigame in core/model/stacker.h for
// one (skill). The per-pet `defragCount_` tallies successful defrags and persists through
// ARCH freeze/thaw (save v16); it's surfaced on the defrag screen and has no other effect.

// Lockout Timer. Durations are defaults still
//     open for balance (frequency is the primary lifecycle-difficulty lever). --
constexpr uint32_t kLockoutDurationMs = 30u * 1000u;  // on-device countdown window
// A fully-failed Lockout still nets 2 total care mistakes (the budget lever), now
// SPLIT: +1 fires the moment Hunger hits 0 ("went hungry"), the other +1 on expiry.
constexpr int kWentHungryMistakes    = 1;   // charged when Hunger reaches 0 (Lockout arms)
constexpr int kLockoutExpiryMistakes = 1;   // charged additionally if the Lockout EXPIRES
constexpr int kLockoutHappyPenalty = 10;    // Happiness hit on expiry ("Happy -10")
constexpr int kLockoutRecoveryHunger = 30;  // survive at a low recovery Hunger
constexpr int kLockoutBitsCost = 50;        // Pay-Bits resolution cost (sketch)
constexpr int kStartBits = 100;             // wallet seed (TBD balance —)

// Achievements (save v40) -----------------------------
// Per-achievement thresholds are NOT here — each is the `goal` on its own row in
// content/content_achievements.cpp, next to the prose that quotes it. What's left is the
// cross-cutting machinery every row shares.
// How long one unlock banner holds the home screen. Long enough to read a name at 4fps,
// short enough that a run of them drains while the player watches.
constexpr uint32_t kAchBannerMs = 2600;
// Above this many announcements waiting at once, they collapse into one summary banner
// instead of a parade — the case being a firmware update that retro-awards a back
// catalogue, where announcing 40 in a row would make the home screen unusable.
constexpr int kAchBannerBurstMax = 6;
// How often the countable rows are re-checked. An achievement landing a fraction of a
// second late is unnoticeable, and this keeps the sweep off the ~4fps repaint path.
constexpr uint32_t kAchSweepIntervalMs = 500;

// --- The Boot Accelerator. The one item that touches an egg: a flat bite out of its
//     incubation clock, never a way into a minigame — every line's hatch game is played
//     once, at lay-time, so there is nothing left for an item to open. Floored at
//     kHatchRevealMs by Game::useBootAccelerator, so it can shorten the wait but never
//     skip past the stretch where the player gets to crack the shell by hand. -----
constexpr uint32_t kBootAcceleratorCutMs = 10u * 60u * 1000u;   // -10 min off the incubation

// --- Boot-Sector incubation. The freshly laid egg
//     sits at IDLE as the Boot-Sector creature (CryptoShell) — inert but on-screen
//     and interactable — for kBootHatchMs before it can be decrypted. The
//     line's hatch minigame is played the instant it is laid, and its prize is spent
//     against this clock; the clock reaching 0 hatches straight to Process on its own.
//     The player can rush that window by carrying the egg
//     around: each explore STEP shaves kBootHatchStepAccelMs and each newly-seen
//     NETWORK shaves kBootHatchNetworkAccelMs off the incubation clock (an egg has
//     no reason to walk/eat, but letting it interact is the point — it gives the
//     otherwise-inert Boot stage something to do). Vitals are FROZEN while an egg.
// First-cut durations (balance TBD). Timer is game-ms, persisted (v9).
constexpr uint32_t kBootHatchMs           = 30u * 60u * 1000u;  // full incubation (~30 min)
constexpr uint32_t kBootHatchStepAccelMs  = 1u * 1000u;        // -1 s per explore step
constexpr uint32_t kBootHatchNetworkAccelMs = 60u * 1000u;     // -1 min per new network

// --- Hatch reveal. The home stretch of ANY incubation: with this
//     little left on the clock the egg is ready to crack, and the Exploit chord plays
//     the shell's full hatch one-shot and hatches on the spot. It's how a line whose
//     minigame happens elsewhere — which by now is every one of them, all played at
//     lay-time — still gets to SHOW its hatch animation instead of the egg silently
//     becoming a pet while the player isn't looking.
constexpr uint32_t kHatchRevealMs = 5u * 60u * 1000u;    // last 5 min: crackable on demand
constexpr int kHatchRevealHoldBeats = 2;                 // beats to hold the final frame

// --- Isolation Protocol: the Worm egg's hatch minigame, played once at lay-time
//     (core/model/isolation.h for the rules, game_isolation.cpp for the screen). The
//     worm inside the shell is turned loose in a quarantine buffer and eats; every byte
//     it swallows is kIsolationDotMs off the incubation clock, and grows it by
//     kIsolationGrowth cells. Crash into a wall or into itself and the run ends with
//     whatever it has already earned — there is no penalty for a bad run, only a smaller
//     prize, the same deal the Stacker offers.
//
//     The economy is set by one identity: kBootHatchMs / kIsolationDotMs = 30 bytes eats
//     the WHOLE clock, which is the clean run WORM_WHISPERER pays for. Thirty of them at
//     kIsolationGrowth apiece leaves the worm 63 cells long in a 176-cell buffer — about
//     a third of the board — so a clean protocol is a real arcade run rather than a
//     formality, and the growing body IS the difficulty ramp (which is why the cadence
//     below is flat: nothing needs to speed up when the room is disappearing).
constexpr uint32_t kIsolationDotMs = 60u * 1000u;   // -1 min of incubation per byte eaten
// The worm's step cadence, on its own real-ms clock rather than the shared 4fps
// heartbeat — the kStackerStepMs precedent. At 250ms a turn there is no game here; this
// is quick enough to demand attention and slow enough to steer with two buttons.
constexpr int kIsolationStepMs = 220;

// Idle-screen status icons (canvas). Transient reveals in the living
//     area, dual-coded by icon shape + position (not colour): the SD-present icon
//     flashes up for kSdIconRevealMs whenever the card becomes present (boot, or a
//     runtime CFG re-check), then hides; the hot/broadcasting icon shows for the
//     whole Audit hot window (no timer of its own — it tracks broadcasting()). ----
constexpr uint32_t kSdIconRevealMs = 4000;   // SD-present icon dwell after a mount

// Screen brightness. A CFG-adjustable, persisted (save v14) backlight
//     level as a discrete 0-based index (kBrightnessLevels steps). The device tier
//     maps a level to a PWM duty; percent(level) = (level+1) * (100/kBrightnessLevels)
//     so the top level is a full 100%. Default = brightest (no surprise on first boot).
//     NOTE: real PWM dimming is device-tier + needs on-device verification — the
//     shipped backlight stays plain on/off until HAS_BACKLIGHT_PWM is validated. ----
constexpr int kBrightnessLevels  = 5;                       // 20/40/60/80/100 %
constexpr int kBrightnessDefault = kBrightnessLevels - 1;   // top level (100%)
constexpr inline int brightnessPercent(int level) {
    if (level < 0) level = 0;
    if (level >= kBrightnessLevels) level = kBrightnessLevels - 1;
    return (level + 1) * (100 / kBrightnessLevels);
}

// Evolution boundary — a per-stage time-in-stage gate (plus the care budget
//     out of Dying; 5/5 routes to Critical System Failure, not evolution). The
//     clock counts real elapsed time (screen-sleep included — the device keeps
//     ticking), so a short pocket nap must NOT tip a stage over. Each stage asks
//     for a longer, more attentive raise than the last: Process->Script clears
//     the Egg->Process hatch (kBootHatchMs) by a wide margin, and Script->Daemon
//     is a further step up again — the long haul to the final form. The modal
//     is a beat-paced cinematic: hold the current sprite, white-out flash, then
//     reveal. Dev/test forces the boundary via debugTriggerEvolution.
constexpr uint32_t kEvolveProcessToScriptMs = 16u * 60u * 60u * 1000u;   // Process's min dwell (8 h)
constexpr uint32_t kEvolveScriptToDaemonMs  = 32u * 60u * 60u * 1000u;  // Script's min dwell (32 h)
constexpr int kEvoHoldBeats = 4;    // hold on the current sprite before the flash
constexpr int kEvoFlashBeats = 2;   // FX_EVO_FLASH white-out, then the reveal

// Evolution branch. At the Script->Daemon hop the care budget
// splits the line: 0-2 mistakes -> Good (durable), 3-4 -> Bad (glass
// cannon); 5/5 never evolves (routes to Critical System Failure). The engine
//     reads the branch as two combat multipliers off the successor CreatureDef
// ("two stat multipliers off the branch"): attack-power lean + loss-Frag.
//     Slot COUNT is identical at Daemon (both at the kMaxMoveSlots cap), so the
//     aggressive-vs-durable lean is carried by these multipliers. Neutral = 100. --
constexpr int kBranchGoodPowerPct = 80;    // Good: lower attack power (durable)
constexpr int kBranchGoodFragPct = 70;     // Good: takes less loss-Frag
constexpr int kBranchBadPowerPct = 135;    // Bad: higher attack power (glass cannon)
constexpr int kBranchBadFragPct = 160;     // Bad: takes MORE loss-Frag (fragile after)

// Critical System Failure (the ONLY death path). The 5/5 dying state
//     (care budget maxed) is recoverable by dropping below 5 — Backup Drive
// (-1 mistake) / Yubi-Cookie (protects 1) — within a grace/ageing window;
//     once it expires the pet is permanently lost -> a [CORRUPTED] ARCH record +
// a new-egg Hatch. Combat loss NEVER kills (Health is transient). ------
constexpr uint32_t kCsfDyingGraceMs = 2u * 60u * 1000u;  // 5/5 recovery window (TBD)
constexpr int kCsfHoldBeats = 3;   // FX_CRITICAL_FAIL crash hold before B acknowledges

// CFG / Factory Reset. The hidden Factory Reset is reached only by a
//     deliberate hold-B gesture: ~5s on System Info to reveal it, then ~5s on the
//     reset screen to commit (releasing early aborts). Two holds make an
// accidental wipe effectively impossible. -------------------------------
constexpr uint32_t kFactoryRevealMs = 5000;  // hold-B on System Info -> Factory Reset
constexpr uint32_t kFactoryCommitMs = 5000;  // hold-B on Factory Reset -> wipe

// Equip slots / rack slots (config-tunable counts) -------------
constexpr int kModSlots = 3;    // MODS equip slots (default; v1)
constexpr int kRackSlots = 4;   // ARCH cold-storage rack slots (default)

// Move slots. N grows with each evolution — slots are the
//     mechanical reward for raising a pet. kMaxMoveSlots is the Daemon cap; the
//     per-Stage array (Boot,Process,Script,Daemon) gives how many are UNLOCKED.
//     A pet always has an innate DEFAULT move outside the slots, so it's never
//     actionless even at Boot with no filled slot. --------------------------------
constexpr int kMaxMoveSlots = 4;
constexpr int kMoveSlotsByStage[4] = {1, 2, 3, 4};

// Combat. Transient Health resets to full each fight and
//     is NEVER persisted; max scales with Stage. Damage = per-move power; defend
//     mitigation = per-move power. Mod passives: Firewall cuts incoming damage,
//     Clock-Speed adds initiative speed, RAID Mirror negates one hit, Packet
// Sniffer boosts post-battle Bits. Flee escape chance is wild-only. ------
constexpr int kMaxHealthByStage[4] = {40, 60, 80, 100};
// Offensive complement to kMaxHealthByStage (combat-length pass). An
// evolved pet hits harder — without it, per-move power stays flat while enemy Health
// climbs with sector tier, so higher-stage fights drag well past the 4–8-exchange
// target (measured: Script ~9–10, Daemon ~19–20 exchanges before this scale). This
// per-stage % multiplies the branch power lean in makePlayerCombatant, so it
// composes cleanly with Good/Bad. Boot/Process unscaled (already in-band); Script/
// Daemon lifted to pull medians back into 4–8. First-cut — retune with the real
// per-stage move progression when that content lands. Indexed by stageIndex().
constexpr int kStagePowerScalePct[4] = {100, 100, 150, 230};
constexpr int kCombatBaseSpeed = 10;       // a pet's base initiative speed
// Speed action economy: each combatant accrues its speed into a gauge; crossing this
// threshold spends it on one action. Actions are dealt in proportion to relative speed
// (equal speed alternates strictly). Sized so base-speed pets act every few gauge ticks
// and a per-point speed edge shifts the action share smoothly.
constexpr int kSpeedActionThreshold = 100;
constexpr int kCombatBeats = 2;            // result-beat hold before B/C dismiss
constexpr int kFleeChancePct = 50;         // wild flee success (Sim quits free)
// The A+C Exploit override allowance per fight. One use
// per battle in v1; a rare reward item (shipped later) raises it — the knob is
// wired now so that item is pure data. begin() resets it every fight, so each
// gauntlet round gets its own fresh allowance.
constexpr int kExploitUsesPerBattle = 1;
// Wild-encounter difficulty (challenge pass). With the flat base stats a wild win
// was a near-untouched romp — the pet finished at ~80–86% Health, so EXPL fights
// carried no risk. These buff EXPL malbeasts only (isWild-gated in makeEnemyCombatant;
// bosses/gauntlets and Sim dummies are untouched) so a win costs real Health. NOTE
// The ≥99.7%-win figure this pass was tuned to now describes only the
// SUB-0 baseline of a sector — the per-sub-area difficulty ramp (applyWildSubAreaRamp,
// combat.cpp) escalates from there so the later sub-areas are genuinely
// losable and "steep/gated". This flat buff stays the sub-0 floor those rungs build on.
constexpr int kWildEnemyHealthPct = 120;   // +20% Health on wild (EXPL) encounters
constexpr int kWildEnemyDamagePct = 175;   // +75% damage dealt by wild enemies
// Auto-battle pacing: heartbeats between autonomous turns. At the ~4fps
// heartbeat, 1 = a turn every ~250ms (too fast to read the move/damage popups); this
// spaces autonomous turns out so each exchange is legible. The A "SKIP" fast-forward
// bypasses the wait (steps immediately), so this only governs the hands-off pace.
constexpr int kCombatBeatsPerTurn = 3;     // ~0.75s per autonomous turn
// Floor for the feeding-frenzy streak ramp (Game::combatBeatsForTurn): each
// consecutive same-actor turn subtracts a beat from kCombatBeatsPerTurn, clamped
// here so the fastest pace stays legible at kHeartbeatMs (~0.25s/turn).
constexpr int kCombatMinBeatsPerTurn = 1;
// Hands-off auto-explore event holds. While explore-mode runs in the
// background nobody is pressing buttons, so each full-screen event auto-continues:
//   - a REVEAL (a wild fight's result, a Wi-Fi roll outcome) holds ~3s so it's
//     readable, then dismisses/plays out on its own;
//   - a DECISION (a Shop's buy/leave) holds ~10s so the player CAN act if watching,
//     then auto-continues (leaves) only if no button was pressed.
// Any button press restarts the hold (the counter resets on input). At the ~4fps
// heartbeat, 12 beats ≈ 3s and 40 ≈ 10s. Boss/Sim fights are player-driven and never
// auto-dismiss.
constexpr int kExploreRevealHoldBeats   = 12;   // ~3s
constexpr int kExploreDecisionHoldBeats = 40;   // ~10s

// Post-encounter status readout. After an EXPLORE wild fight
// resolves (win or loss — never a flee, and never Sim-Battle/boss), briefly show
// the persistent BANDWIDTH spend + FRAGMENTATION change it caused before returning
// to the habitat, so the player knows whether to keep exploring or go defrag. A
// real-ms deadline (like kSdIconRevealMs), not a beat count — any button press
// dismisses early.
constexpr uint32_t kPostEncounterMs = 2000;   // ~2s auto-dismiss window

// Sim-Battle reward. A fixed small FLAT payout — a few Bits
//     (enough for food), a little combat XP, a small Happiness bump. Flat against
//     Stage-scaling XP needs, so it's self-limiting (meaningful only early). ------
constexpr int kSimBitsReward = 12;
constexpr int kSimXpReward = 5;
constexpr int kSimHappyReward = 4;
constexpr int kWildLossFrag = 18;          // +Frag on a wild (live-stakes) loss

// --- MODS into combat. Mods are the PERMANENT hardware-
//     passive layer (D3); this is the earn + power model that finally lets them enter
//     play. Three independent axes on ModDef (defs.h): `rarity` = DROP WEIGHT within an
//     area's loot table; `powerTier` (1..kModPowerTiers) = the ladder DEPTH, which picks
//     the area a mod lives in; `equipLevel` = the pet level it needs, authored on the row.
//     ONE level per mod, shared by every copy: the picker lists mods by TYPE and has
//     never had a way to show one copy over another, so a per-copy roll was a number
//     no player could see, act on, or choose between. ----------
//     The DEPTH count is not here: it is kModPowerTiers (areas/area_defs.h), which is
//     the ladder's own length, so adding an area opens a rank rather than overflowing
//     a fixed table.
// Gates are authored per row (ModDef::equipLevel) against this ceiling, NOT derived from
// the tier. The number of areas is the wrong bound on how finely mods can be spread
// across a raise: deriving the gate gives one gate per area, so every mod in an area
// unlocks on the same level and the whole stretch between two areas has nothing new to
// slot. Authoring against a ceiling lets a band hold as many rungs as it has mods.
//
// 100 is the ceiling, NOT the reachable top. The XP curve is geometric at
// kLevelXpGrowthPct, so cumulative cost roughly triples every ten levels: level 60 is
// ~301k XP (a long dive), level 100 is ~13.8M. The shipped roster therefore fills
// 0..60 — deep enough that a SIXTH area extends the ladder rather than forcing a re-band
// of every row already on it — and leaves the rest as authoring headroom for whatever the
// curve is when that area lands. A row gated above where a pet can actually reach is a
// content mistake this bound cannot catch, which is what
// test_mod_equip_ladder_is_ordered_and_dense is for.
constexpr int kModEquipLevelMax = 100;
// How many spare copies of ONE mod the pool will hold. A cap exists because the pool
// had none: mods drop from milestones and the only sink is equipping one, so copies
// accumulated for the life of the device — the measured save had 424 spares of 24 mods,
// 132 of them the same one, which is the save's single largest section and a number no
// player has ever had a use for. Raised by the Rig Shop's MOD STORAGE row.
//
// The ceiling is the SAVE's, not the shop's: the pool ships as a nibble per mod
// (content_tables.h kModWireCap), so 15 is what a count can say. Tiers may grow toward
// it freely; past it needs a wider cell and a save version.
constexpr int kModCopyCapBase = 2;
constexpr int kModCopyCapMax = 15;                // 4 bits per mod on the wire
constexpr int kModStorageMaxTier = 3;             // shop tiers above the base cap
constexpr int kModStorageCapByTier[kModStorageMaxTier] = {4, 6, 8};
constexpr int kModStorageStart = 512;             // doubling ladder, like the rack slot
// The cap in force at a MOD STORAGE purchase level (0 = never bought).
constexpr int modCopyCap(int tier) {
    if (tier <= 0) return kModCopyCapBase;
    const int t = tier < kModStorageMaxTier ? tier : kModStorageMaxTier;
    return kModStorageCapByTier[t - 1];
}
// Drop cadence (Q4: sub-boss roll + area-boss guaranteed + DeepWeb rare + Epic caches).
// A mod earned is permanent, so sources are milestone/rare — never common wild drops.
constexpr int kModSubBossDropPct = 35;            // sub-area boss FIRST clear: mod-drop chance
constexpr int kModDeepWebDropPct = 4;             // DeepWeb wild win: rare tier-4 mod roll
// A container's own chance to also yield a mod is on its row (ItemDef::cache.modChancePct,
// content_items.cpp) — it belongs to that one cache, not to every mod source.
// Rarity → relative draw weight when rolling an area's mod loot table. Rarer = scarcer.
constexpr int kModRarityWeight[4] = {50, 30, 15, 5};  // Common, Uncommon, Rare, Epic

// --- Battle fatigue: NOT a separate meter — "fatigue" IS a
//     small per-fight FRAGMENTATION tax: every resolved WILD NON-BOSS fight has a
//     chance to fragment the pet a little, so endless auto-farming steadily degrades it
//     and forces a defrag (the anti-farm lever). Hands-off auto-explore ALSO pauses once
//     frag climbs into the danger band, so the mode farms "a long time but not forever"
//     and can't spiral an unattended pet into Critical/CSF. Boss + safe Sim fights are
//     exempt. The chance/magnitude are isolated here so a future PURCHASABLE upgrade can
//     buy them down (deductions to max accrual / chance as purchasable upgrades).
constexpr int kBattleFatigueChancePct = 90;    // chance a wild non-boss fight frags
constexpr int kBattleFatigueFragMin   = 1;     // ...min frag added on a hit (inclusive)
constexpr int kBattleFatigueFragMax   = 5;     // ...max frag added on a hit (inclusive)
constexpr int kBattleFatigueAutoPauseFrag = 80; // hands-off auto-explore pauses at/above

// Creature levels & the XP curve. A per-pet build layer:
//     combat XP raises a LEVEL (starts at 0); each level-up grants +1 to ONE
//     randomly-chosen combat stat (power / defense / speed / max-Health). XP-to-
//     next is GEOMETRIC — round(kLevelXpBase * (kLevelXpGrowthPct/100)^level) —
// ~10% dearer each level (base + growth stay tunable). The per-point
//     magnitudes map an earned point into the combat maths (first-cut balance,
// ): power = +% attack lean, defense = +% incoming-damage cut (its own cap,
//     total dmg-cut clamped so defense can't null a hit), speed = +initiative,
//     max-Health = +HP. Level == the sum of earned points (an invariant Rollback
//     preserves: −1 point ⇒ −1 level). ------------------------------------------
constexpr int kLevelXpBase = 100;          // XP to reach level 1 (round(100*1.1^0))
constexpr int kLevelXpGrowthPct = 110;     // each level costs 1.1x the previous
constexpr int kLevelStatCount = 4;         // power / defense / speed / max-Health
constexpr int kLevelPowerPctPerPoint = 4;      // +4% attack power per power point
constexpr int kLevelDefensePctPerPoint = 3;    // +3% incoming-damage cut per defense
// ...at FULL rate only for the first kLevelDefenseSoftPoints; past that a point buys
// half as much (levelDefenseCutPct, combat.h). Defense is the one stat with a hard
// ceiling, so without a bend the last points before the cap were the most valuable
// purchase in the game and the wall was simply a matter of spending enough. The curve
// leaves early Defense untouched — the soft point sits above where a mid-game pet lands —
// and only taxes the stretch that was heading for immunity.
constexpr int kLevelDefenseSoftPoints = 10;    // full-rate points before the bend
constexpr int kLevelDefenseCapPct = 60;        // ...level defense contribution cap
constexpr int kLevelDmgReduceMaxPct = 85;      // ...total dmg-cut clamp (never immune)
constexpr int kLevelSpeedPerPoint = 1;         // +1 initiative per speed point
constexpr int kLevelHealthPerPoint = 3;        // +3 max-Health per max-Health point
// Defense stat ALSO scales DEFEND-move brace magnitude.
// Symmetric to Power→attack. +3% brace per Defense point via
// Combatant::defenseMultPct — leveling Defense visibly thickens the Cipher wall's
// absorb, on top of the always-on dmgReducePct cut above. Braces are one-shot and
// cost a turn, so this doesn't touch the 85% immunity clamp (that guards the % cut).
constexpr int kLevelDefenseBracePctPerPoint = 3;
// ...and that brace scaling now has a ceiling of its own. "One-shot and costs a turn" is
// a real cost in a short fight, but the endless zone is not a short fight: a turtle with
// unbounded absorb takes a whole turn to become unkillable for the next one, forever. The
// cap is the multiplier's BONUS half (defenseMultPct starts at 100), so +200 = a brace
// that absorbs at most three times its printed power from Defense alone.
constexpr int kLevelDefenseBraceCapPct = 200;

// --- Specialisation: what the SECOND half of an investment is worth ------------------
// Power and max-Health bend the OPPOSITE way to Defence above. Defence diminishes because
// it is chasing a ceiling; these two are chasing nothing, and a flat rate on them made a
// spread of one-point-in-everything the default outcome of a raise — which is also the
// weakest thing a pet can be, since the level-up grant picks the stat at random and a long
// raise averages out. Past the specialisation point a point is worth MORE, so committing
// to a stat is what pays and a pet that got lucky in one column has something to show for
// it. Capped, because the ladder runs to level 60 and an unbounded accelerating curve
// stops being a build and becomes the only build.
constexpr int kLevelPowerSpecPoints = 6;         // points before the accelerating band
constexpr int kLevelPowerPctPerSpecPoint = 10;   // ...and the rate past it (base is 4)
constexpr int kLevelPowerSpecCapPct = 300;       // total level-Power contribution ceiling
constexpr int kLevelHealthSpecPoints = 6;
constexpr int kLevelHealthPerSpecPoint = 8;      // ...vs kLevelHealthPerPoint's 3
constexpr int kLevelHealthSpecCap = 400;         // total level-Health contribution ceiling

// Defence's investment TIERS. The % cut has a ceiling and a bend, so more of it is the one
// thing Defence cannot be paid in — past a threshold it buys a different KIND of thing
// instead, and each of these answers a way the stat was being routed around rather than
// out-scaled:
//   pierce resist — armorPiercePct exists to make a wall irrelevant; a committed wall
//                   makes the pierce partly irrelevant back.
//   brace retain  — a one-shot `guard` discards whatever the hit it ate did not need, so
//                   an over-sized brace pays for absorption nobody asked for. Past this
//                   threshold the unspent remainder CARRIES to the next hit instead.
constexpr int kLevelDefensePierceResistPoints = 12;
constexpr int kLevelDefensePierceResistPct = 40;   // cuts an attack's effective pierce
constexpr int kLevelDefenseBraceRetainPoints = 18;
constexpr int kLevelDefenseBraceRetainPct = 25;    // ...ADDED to the baseline below

// The share of an unspent one-shot brace that carries to the next hit for ANY fighter,
// before Defence investment adds to it. A baseline exists because over-sizing is the
// normal case rather than the exceptional one: measured across legal loadouts, most braces
// swallow the whole hit they meet, so most of a brace's magnitude was being authored,
// paid for with a turn, and then thrown away. Retaining part of it is the one thing that
// makes a brace better without making it bigger — and brace magnitude is worth almost
// nothing per point, so bigger was never available.
constexpr int kBraceRetainBasePct = 25;

// What a SEIZED move hits for in Ransomware hands, as a % of the wall the pet is standing
// behind (Combatant::stackDefenseBonus, RansomSeizure). This is the whole reason the seizure
// is worth having: taking an attack for three turns is a small thing on its own, and
// measured that way it moved nothing at all. What the line needed was a way to SPEND its
// wall, because Cipher accumulates a damage cut and has nothing else to do with it — the
// four one-attack-slot Daemons rank almost exactly by how far their line converts defence
// into damage, and Ransomware was last with no conversion at all. The seized move is the
// conversion: the ransom note is written in the pet's own encryption.
constexpr int kRansomSeizedWallPct = 100;   // 100 = the full stacked cut, as bonus damage

// Per-line combat PASSIVE constants (Ransom Lock, the Phishing steal-track floors +
// Feed-Frenzy + Perfect Bite, Execution-Override + the Trojan trap cap) live beside
// their line's moves in content_passives.h, not here — they only ever get tuned
// together with that line's move magnitudes in content_moves.cpp.
//
// The cross-line infiltration roll: chance a Process pet with an evolvesToTrojanId
// diverts into the Trojan family instead of its normal Script successor.
constexpr int kTrojanDivertPct     = 10;

// Explore-mode. Arming a
//     sector in EXPL starts a background mode that runs on the IDLE habitat: the
//     game auto-STEPS on a timer (kWalkAutoStepBeats heartbeats, ~3s at the ~4fps
// beat) with NO cap, and EACH step resolves a GUARANTEED event (— no more
//     "quiet" rolls). Network Ping (A+C -> A) forces the next step now, bypassing the
//     timer. --------------------------------------------------------------------
constexpr int kWalkAutoStepBeats = 12;  // heartbeats between auto-steps (~3s @ ~4fps)
// Bandwidth is the per-fight
// FRAGMENTATION SHIELD for exploration. ANY resolved wild fight (win OR loss — first
// clear, cleared-sub re-farm, or DeepWeb dive; bosses/Sim excluded) spends 1 Bandwidth
// to SKIP the battle-fatigue frag (corruption) tax entirely. So a stocked pool
// lets the pet farm/explore longer with less risk, and Bits sunk into the Hacker SHOP's
// "Increase Bandwidth" upgrade buy real safety. Once the pool hits 0 the tax bites again
// (the "defrag or come home" signal). It never hard-stops stepping, and Bits+XP always
// stay full. The loot decay still gates on RE-FARMING a cleared sub, but now
// keys on "was this fight shielded" (full loot + frozen decay count while a charge
// covered it; diminishing loot once the pool is dry). Regenerates over real elapsed
// time. NOT persisted across a reboot (a power-cycle refills it — a v1 call).
// The BASE pool, not the cap: what an unupgraded rig starts with. Every read of the
// live ceiling goes through Game::bandwidthMax(), which adds the "Increase Bandwidth"
// upgrades on top — so this constant is the floor that ramp is measured from
// (game_rig_shop.h anchors the curve to it), and there is no third thing to delete.
constexpr int kBandwidthMax = 10;       // shielded fights before fragmentation resumes
constexpr uint32_t kBandwidthRegenMinutesPerPoint = 2;  // regen +1 / 3 min real time
// The floor that interval can be shaved to. A pet that has eaten a Tiramisudo carries a
// permanent -1 minute (ItemEffect::BandwidthRegenBonusMin, a magnitude on that item's
// own row); this is the shared limit, not the item's number — it belongs here because
// it protects the regen LOOP (Game::tick) rather than describing any one food, and a
// second such dish would be held to the same floor without touching it.
constexpr uint32_t kBandwidthRegenMinutesFloor = 1;
// The three screen-level hold gestures, all on B: a tap resolves the focused row as
// usual and the hold does the thing the SCREEN can do. They share one threshold because
// they are one gesture wearing three hats — a player who learns the dwell on any of them
// has learned it everywhere. A is never a hold-to-act button; it is the step, and holding
// it repeats that step (kListRepeatMs below), which is what keeps the vocabulary uniform.
constexpr uint32_t kItemFilterHoldMs = 800;   // ITEMS list: cycles the type filter
                                              // (the Rig Shop's Items Type-Tabs unlock gates this)
// TRAIN move picker: holding B past kMoveFilterHoldMs toggles moveShowAll_ (the
// full roster, including moves the pet can't equip into the focused slot right
// now); a shorter tap still drills into the focused move's detail. No unlock gate,
// unlike the ITEMS/VAULT hold gestures beside it.
constexpr uint32_t kMoveFilterHoldMs = 800;
// ROCK THE DOCK bracket: holding B past this opens the focused entrant's SCOUT sheet
// instead of starting a bout. The same 800ms as its three siblings — a hold that takes
// a different amount of time per screen is a gesture the hand has to re-learn.
constexpr uint32_t kTourneyScoutHoldMs = 800;
constexpr uint32_t kBulkOpenHoldMs = 800;     // Hacker VAULT: bulk-opens the focused
                                              // row's rarity (the Rig Shop's Bulk-Open unlock gates this)
// The list step's repeat. A is "next" on every list on the device, and a list long
// enough to scroll is a list you should not have to tap thirty times to cross — so
// holding A past the delay keeps stepping until it is released, on every list whose A
// is a plain step (Game::listRepeatEligible). Slower than the DECRYPTOGRAM's cadence
// below on purpose: a board cell is one glyph and a list row is a whole line of text
// that has to be read as it goes past, so this is set to about nine rows a second —
// fast enough to cross a full bag in a couple of seconds, slow enough to stop on the
// row you meant. Ticks on its own cadence (Game::tick), not the 4fps heartbeat.
constexpr uint32_t kListRepeatDelayMs = 400;
constexpr uint32_t kListRepeatMs = 110;
// C keeps its tap everywhere — Cancel is the one button whose meaning never bends — so
// a LIST puts its step BACKWARD on the hold instead: hold C and the cursor walks back
// up the rows at the same cadence A walks down them, which is how an overshoot is
// undone without a full lap. The threshold it starts at is kListRepeatDelayMs, the same
// dwell that starts A's repeat, so the two directions feel like one gesture. A press
// that never reaches it is an ordinary tap and cancels on release; a press that does
// has stepped, and its release cancels nothing.
// THE DECRYPTOGRAM's cursor repeat — the fastest of the two repeats, because it is the
// only place a cursor has thirty-odd stops to walk and each one is a single letter.
// Holding A or C past the delay steps every interval until it is released. The delay is
// long enough that an ordinary tap never triggers it, and the interval is set so a full
// lap of the longest quote takes a couple of seconds rather than the ten a 4fps
// heartbeat would cost — which is why this ticks on its own cadence
// (Game::tick) the way combat's sprites and the Stacker's slide do.
//
// The interval sits just inside kStackerStepMs, which is the fastest repaint the panel
// is known to sustain. It could go faster on paper — a repeat is a two-second burst,
// not a whole run like the Stacker's — but there is no reason to find the ceiling here:
// with the cursor running both ways the worst case is HALF a lap, so this is already
// about a second and a half to reach any gap on the board.
constexpr uint32_t kCryptogramRepeatDelayMs = 350;
constexpr uint32_t kCryptogramRepeatMs = 80;
// The Reduce-Explore-Frag-TRIGGER Rig Shop upgrade's effective chance per tier (tier
// 0 = the unchanged kBattleFatigueChancePct baseline). Dual-consumed: the Rig Shop's row
// text AND Game::applyBattleFatigue's combat calc both read this ladder, so it stays here
// rather than on the upgrade's own row (src/core/content/CONTENT_STANDARD.md rule 2).
constexpr int kFragTriggerReducedPct[6] = {80,70,60,50,45,40};   // effective chance, tiers 1..6

// The Rig Shop's (Hacker-face SHOP) reusable upgrade-cost engine. Every purchasable
// tiered/one-time row prices its NEXT purchase from two knobs — a starting price and n
// (purchases already made, 0-indexed) — run through one of a small set of named curves.
// The row (game_rig_shop.h) carries `start`/`step`/the curve; this is just the shape.
// floor(log2(m)) for m >= 1 (m < 1 clamps to 1, returning 0) — the shared building block
// for the two log-stepped curves below.
inline int floorLog2(int m) {
    if (m < 1) m = 1;
    int e = 0;
    while ((m >>= 1) != 0) ++e;
    return e;
}
enum class RigCostCurve : uint8_t {
    kFixed,       // Cost(n) = start                       — one-time unlocks (n is always 0)
    kLinear,      // Cost(n) = start + step*n               — arithmetic ladder
    kDoubling,    // Cost(n) = start * 2^n                  — doubles every single purchase
    kHalving,     // Cost(n) = start / 2^n, floored at 1     — halves every single purchase
    kLogStep,     // Cost(n) = start * 2^floor(log2(n))      — doubles every power-of-2 tier
    kLogStepHalf, // Cost(n) = start * 2^(floor(log2(n+1))-1) — same shape, offset a tier down
};
// The price of purchase n+1 (n = purchases already made). `step` is only read by
// kLinear. Every curve resolves to a value at n=0 derived purely from `start`, so a
// fresh upgrade always opens near its tunable sticker price.
inline int rigUpgradeCost(int start, int n, RigCostCurve curve, int step = 0) {
    if (n < 0) n = 0;
    switch (curve) {
        case RigCostCurve::kFixed:
            return start;
        case RigCostCurve::kLinear:
            return start + step * n;
        case RigCostCurve::kDoubling:
            return start << n;
        case RigCostCurve::kHalving: {
            const int v = start >> n;
            return v > 0 ? v : 1;
        }
        case RigCostCurve::kLogStep:
            return start << floorLog2(n);
        case RigCostCurve::kLogStepHalf: {
            const int e = floorLog2(n + 1) - 1;
            return e >= 0 ? (start << e) : std::max(1, start >> -e);
        }
    }
    return start;
}

// Guaranteed-event weights (balance). Every step is a real event —
// wild encounters DOMINATE (the streak needs wins), the rest are the breather +
// reward beats. Ordered thresholds summing to <100; the remainder types a wild
// encounter (the majority slot). No "quiet" any more.
constexpr int kExploreLootPct  = 12;   // guaranteed-event roll: loot cache (in place)
constexpr int kExploreWifiPct  = 8;    // guaranteed-event roll: Wi-Fi network (kept low so the
                                        // real-network sighting queue, kPendingNetworkQueueCap,
                                        // has time to refill between triggers instead of
                                        // running dry on a fast walk)
constexpr int kExploreShopPct  = 8;    // guaranteed-event roll: item storefront shop
constexpr int kExploreModShopPct = 6;  // guaranteed-event roll: mod storefront shop
constexpr int kExploreCachePct = 8;    // guaranteed-event roll: Sealed Cache (—
                                        // collected non-interrupting, opens later)
constexpr int kExploreKeyPct   = 6;    // guaranteed-event roll: Key-item find (—
                                        // drops a random warp key, non-interrupting)
                                        // (remainder = wild encounter, the majority)
// Win-streak -> boss unlock. Each wild-encounter WIN increments the
// active sector's streak; this many wins IN A ROW unlocks its boss (a durable flag,
// manually triggered from EXPL). A loss cancels the mode and resets the streak.
constexpr int kExploreStreakToBoss = 10;
constexpr int kLootBitsReward = 10;    // flat Bits on a loot-cache event (TBD)
// How deep a Safe-Mode Key's rest de-frags is NOT here: it belongs to that one key,
// so it is a negative Frag effect on its own row (content_items.cpp), applied by
// Game::resolveSafeRestEvent.
constexpr int kLootItemChancePct = 30; // chance a loot cache also drops an item

// Sealed caches. A cache find rolls one of the findable containers,
//     weighted; each Opens for a draw from its own pool. Everything about a cache —
//     its purse, its draw count, its pool, how often the walk drops it, whether it
//     also yields a mod — is on its own item row (ItemDef::cache,
//     content_items.cpp), so none of it is here.
// Wild-win Bits are the formula normalBitsReward(R), keyed to the opponent's
// stage-rank R (game.cpp) — not a flat number here.
constexpr int kWildWinXpReward = 15;   // BASE wild-win XP (scaled by level diff)
// The wild-win XP base is scaled by the ENEMY-vs-PET level
// difference (applyWildSubAreaRamp stamps the enemy level; combatLevel_ is the pet's).
// Per level of difference, XP shifts by kWildXpPerLevelDiffPct%; clamped to the
// [min,max] band so a way-under farm still trickles and a huge over-level fight caps.
// At parity (diff 0) → 100% → the flat base. Enemy +4 → 160%; enemy −5 → 25% (floor).
constexpr int kWildXpPerLevelDiffPct = 16;  // X% XP per level of (enemy − pet)
constexpr int kWildXpDiffMinPct      = 32;  // floor: farming under-level still trickles
constexpr int kWildXpDiffMaxPct      = 1024; // ceiling: cap the punching-up bonus
// The stat half of the per-sub-area LEVEL bump (the enemy's "rolled level-up stats",
// Between-AREA growth already rides the tier roster (wildMalbeast), so this
// only thickens WITHIN a sector: +Health per sub, +1 speed at the two deepest rungs.
constexpr int kWildSubAreaHealthStep = 6;   // +Health per sub-area index in the ramp
// The sub-area rung from which a wild also fields its AREA's Defend (AreaDef::
// wildDefendMoveId) on top of its Attack, which it carries at every rung. Cross-cutting
// because it is the shared SHAPE of the ramp — an area declares which two moves are its
// own and this one rule decides how deep the player has to walk for the second — so it
// sits beside the ramp's other steps rather than being restated on five rows.
//
// This rung and not a shallower one for two reasons that agree. The ladder's own kit
// THINS as it deepens (its last two rungs are two moves where the middle one is three),
// so the pair lands where there is room for it and no wild ever fights with more than
// kMaxMoveSlots moves — the count a fully-evolved pet itself holds, which is the honest
// ceiling for "an enemy is a peer". And an area's brace is a thing a player wants, so
// putting it on the two meanest rungs makes the wall worth walking to.
constexpr int kWildAreaDefendSub = 3;
constexpr int kWildItemDropPct = 35;   // chance a wild win also drops an item
constexpr int kWildMoveDropPct = 20;   // chance a wild win teaches a move off the
                                        // DEFEATED ENEMY'S kit — independent roll,
                                        // filtered against already-owned moves
                                        // (Game::rollEnemyMoveDrop)
// The same roll on a BOSS round, which gets its own rate for two reasons: a boss is
// reached deliberately rather than wandered into, and it is the only thing carrying its
// area's apexThreatMoveId, so the rate has to be generous enough that the marquee move
// is actually gettable. Rolled PER ROUND, so a 5-round area gauntlet is five chances and
// a sub-area boss is one. No refarm decay rides this — see finishBossRound.
constexpr int kBossMoveDropPct = 35;
// Re-farming an already-CLEARED sub-area gives DIMINISHING
// non-Bits rewards — Bits + XP stay full (so a done area is still a training ground for
// future pets), but the item/move DROP chances decay per re-farm win down to a floor
// (never a fully dead area, never an infinite loot fountain). refarmDropScalePct(count)
// = 100 − count*step, clamped to the floor; each re-farm win advances the per-sub count.
constexpr int kRefarmDropDecayStep = 2; // −X% of the base drop chance per re-farm win
constexpr int kRefarmDropFloorPct  = 15; // ...never below 15% of the base (a trickle)
constexpr int kRefarmCountCap      = 999; // saturate the persisted per-sub win count

// The DEEPWEB DIVE's endless-scaling constants + mod pool live in their own area
// folder (src/core/content/areas/deepweb_dive/) rather than here, since nothing
// outside the dive reads them — see that folder for the grind-rate tuning knobs.

// Sim-Battle dummy — the same "scale to the pet's level" trick as DeepWeb (reuses
// its linear bump, just gentler), so the still-hand-tuned Basic/Hardened tiers stay
// a fair practice target as the pet grows instead of trailing further behind every
// level. Deliberately NOT the general enemy-level rework (that's a bigger, still-open
// balance pass) — this only threads the existing DeepWeb-style scaling onto the one
// encounter where "match the pet's current power" is unambiguously the right call.
constexpr int kSimDummyHealthPerLevel = 8;   // +Health per pet level
constexpr int kSimDummySpeedPerNLevels = 8;  // +1 speed every N pet levels (gentler)

// Sub-area + area bosses. The
//     unit of progress is the SUB-AREA: a 10-win streak (kExploreStreakToBoss)
//     unlocks that sub-area's boss (a durable per-sub flag), a MANUAL FIGHT BOSS
// from the EXPL row launches it (a single strong malbeast), and beating it
//     marks the sub-area CLEARED. Clearing ALL 5 sub-areas unlocks the AREA boss =
// a 5-stage gauntlet of the sub-area bosses (carried Health, no heal);
//     beating that sets sectorCleared[] (area cleared → next area unlocks) + grants
// the Title. Sub-boss stats scale with the area tier + the sub index (sub 5 =
// the signature/curated apex); FIRST-CUT balance, generic frame (no new
//     art). ---------------------------------------------------------------------------
// A sub-area boss's Health = base(area tier) + sub*step, so sub 1 opens easy and the
// signature sub 5 is the wall. The area gauntlet then fights those five back-to-back.
constexpr int kSubBossHealthBase = 40;   // + areaTier*8 → area 0 (t1)=48, area 1 (t2)=56
constexpr int kSubBossHealthStep = 8;    // per sub-index climb within an area
constexpr int kSubBossSpeedBase  = 8;    // + areaTier (+2 for the signature sub 5)

// --- Shops as explore events A storefront is a self-contained walk event: each
//     area's own AreaShopDef (src/core/content/areas/) carries its OWN stock count —
//     restocked to that number each visit, at the item's own bitsPrice, and never
//     persisted (come back, it's refilled). kShopStock is the shared default every
//     area's shop initializes from; bumping one area's stock is a one-line edit on
//     that area's own file, not a change here. --------
constexpr int kShopStock = 5;              // default units in stock per storefront visit

// Wi-Fi network explore event (placeholder weights). A
//     self-contained event: rolls one of 4 sub-outcomes and plays it out in
//     place (no sub-activity/stepping). Friendly-visit is a v1 substitute for
// the doc-07 Met-Pets Roster (out of pet-side scope) — a generic
//     ally-buff rather than a named remembered pet. ---------------------------
constexpr int kWifiSleepingPct = 30;    // sleeping guardian -> free loot
constexpr int kWifiAwakenedPct = 25;    // awakened guardian -> wild combat
constexpr int kWifiOpenCachePct = 25;   // open cache -> straight loot
                                         // (remainder = friendly visit -> ally buff)
constexpr int kAllyBuffBattles = 3;     // friendly-visit buff duration
constexpr int kAllyBuffPowerPct = 20;   // +power for the buffed battles

// Real-network discovery (game_net.cpp: Game::registerNetwork / resolveNetworkDiscovery,
// core/net/network_ledger.h). The device-tier passive scan only QUEUES a sighting
// (BSSID + display name) into a small RAM-only pending list — dedup and crediting
// (unique-check against the SD-backed NetworkLedger, XP, rewards, flavor) happen
// only when the EXPL Wi-Fi event actually resolves one, never at scan time.
constexpr int kPendingNetworkQueueCap = 64;  // pending real-sighting queue; a dup sighting
                                              // is ignored, and a full queue evicts its
                                              // OLDEST entry to admit the new one (an
                                              // unwalked backlog must never make the radio
                                              // deaf to what it can hear now)

// The in-range snapshot the CREW home-network picker reads (game_net.cpp:
// Game::registerNetwork -> visibleNetworks_, game_crew.cpp: crewNetworkRows).
// Deliberately SEPARATE from the pending queue above: that one is a reward backlog
// drained a sighting at a time by the EXPL Wi-Fi event, this one answers only "what
// can the radio hear right now", so its size and drain rate can never influence the
// picker. kNetVisibleFreshMs must stay comfortably above the platform sweep cadence
// (NET_SNIFF_SCAN_INTERVAL_MS, config.h) so one missed sweep doesn't drop a network
// that never actually left.
constexpr int kNetVisibleCap = 48;                     // in-range snapshot size; a full
                                                        // snapshot evicts its STALEST entry
constexpr uint32_t kNetVisibleFreshMs = 90u * 1000u;   // a sighting counts as in-range for
                                                        // this long after it was last heard
// The in-range snapshot of OTHER MALWARIUM DEVICES (game_peers.cpp:
// Game::registerPeer -> livePeers_), the pet-to-pet twin of the network snapshot
// above. Same job, different subject: "which operators can I hear right now",
// which is what separates a LIVE row on the PEERS screen from a remembered one in
// the SD-backed PeerLedger.
//
// The cap is small on purpose — this counts people in one room, not access points,
// and a full snapshot evicts its stalest entry. The freshness window must stay
// comfortably above the beacon cadence (PEER_BEACON_INTERVAL_MS, config.h) so a
// single dropped frame doesn't blink someone out of the room; it doubles as the
// meeting boundary, since a peer aging out and coming back is what counts as
// meeting them again.
constexpr int kPeerVisibleCap = 12;
constexpr uint32_t kPeerVisibleFreshMs = 20u * 1000u;

// The 1v1 duel handshake (game_pvp.cpp, core/net/pvp_link.h). ESP-NOW unicast is
// fire-and-forget at this layer, so the sender simply repeats the frame that is holding
// the session open until the reply that supersedes it arrives — cheap ARQ for a
// three-frame exchange between two devices in the same room. Every frame is idempotent
// (the receiver dedupes on session id), so a repeat that wasn't needed costs nothing.
constexpr uint32_t kPvpRetryMs = 400;              // gap between repeats of the pending frame
constexpr int kPvpMaxRetries = 6;                  // ~2.4s of trying before giving up
// How long an unanswered stage of the handshake waits before it fails out. Generous
// against kPvpRetryMs * kPvpMaxRetries because the middle stage is waiting on a HUMAN
// pressing accept, not on the radio.
constexpr uint32_t kPvpInviteTimeoutMs = 20u * 1000u;
// Outbound frame slots the device tier drains each loop. A host mid-duel already
// queues two frames per retry (its fighter and the START), so the depth has to clear
// that plus the odd unrelated frame — a BUSY decline to a third operator who challenges
// mid-fight, or a BYE closing an old session — or those get dropped on a full outbox.
constexpr int kPvpOutboxCap = 4;

constexpr int kNetDiscoveryTopFavoritesCount = 8;  // NetworkLedger::inTopN threshold — a
                                              // repeat inside your top-8 by sighting count
                                              // reads as "home turf" (flat, unrewarded);
                                              // outside it still feeds the pet (see below)
constexpr int kNetDiscoveryFoundHappyBonus = 1;    // small Happiness bump whenever the
                                              // queue had something to resolve (new or an
                                              // outside-top-8 repeat) — the inverse of the
                                              // empty-queue penalty below
constexpr int kNetDiscoveryFondXpAmount = 8; // pet combat-XP lump for an outside-top-8
                                              // repeat sighting (Game::addCombatXp) —
                                              // authored by feel, unmeasured against play
constexpr int kNetDiscoveryNoneHappyPenalty = 5;      // Happiness hit when the queue is
                                              // empty and the cooldown below allows it
constexpr int kNetDiscoveryEmptyCooldownStrikes = 3;  // the penalty only actually applies
                                              // on the 1st empty-queue miss, then every
                                              // Nth miss after (streak 1,4,7,10…) — so a
                                              // long unmonitored dead-zone walk tapers off
                                              // instead of grinding Happiness to 0

// Audit-mode handshake capture's SHAKES dedup set (seenHandshakeBssids_,
// game_net.cpp registerHandshake) — a real-radio, session-scoped RAM cap sized so a
// normal walk never overflows it. Unrelated to network discovery above (that ledger
// moved to the SD-backed NetworkLedger); this one stays a small in-save vector.
constexpr int kBssidDedupCap = 256;

//     Hacker Rank: player/device-level progression tier driven by lifetime unique networks
//     seen.
constexpr int kHackerRankXpPerNetwork = 10;   // XP granted per newly-seen network
constexpr int kHackerRankXpPerRank    = 100;  // XP needed to climb one rank
constexpr int kHackerRankUpBitsReward = 128;   // flat Bits per rank gained

// Audit-mode handshake capture (AUTHORIZED USE, PASSIVE only).
//     Distinct from the AUDIT SCAN discovery toggle (J.21b): this is the WPA
//     4-way-handshake -> .pcap-on-SD capture path (net_capture.h device tier), gated
//     by the audit_capture.h policy SM. After a fresh capture the device is "hot &
//     broadcasting" for kAuditHotBroadcastMs (~2 min); turning the toggle off — or
//     the hot window expiring — SEALS and starts kAuditRearmCooldownMs before it
//     can arm/capture again. Design-intent cooldown is ~30 min; TEMPORARILY dropped
//     to 15 s for early testing so the capture path can be exercised repeatedly.
//     Both are first-cut durations (retune with the multi-device contest when
//     board two lands, and restore the 30-min cooldown before ship).
//     Timers are game-ms and reset on reboot (no RTC), so only the toggle persists. --
constexpr uint32_t kAuditHotBroadcastMs   = 2u * 60u * 1000u;   // "hot" broadcast window
constexpr uint32_t kAuditRearmCooldownMs  = 15u * 1000u;        // seal -> re-arm cooldown 

// Audit capture RF power budget (RF half). The radio is the
//     dominant draw: promiscuous RX + channel hopping keeps it hot continuously
//     (unlike the passive scan's periodic bursts), so every one of these bounds
//     exists to STOP it draining the pack. All first-cut — retune from measured
// current draw on the RF bench (armed-idle vs hot vs scan), per
//   • Arm window: Armed-but-never-captures would otherwise keep the radio hot
//     forever. After kAuditArmWindowMs of listening with NO handshake the SM
//     self-seals down the normal seal->cooldown path (toggle intent stays on, so
//     it re-arms after the cooldown) — a bounded listen/sleep duty cycle instead
//     of an open-ended drain. A real capture (-> Hot) cancels the window.
//   • Channel hop: a handshake lands on the AP's channel, so we must hop; every
//     hop is time-on-air. Dwell + the channel set are the coverage/battery lever.
//     Limiting to the 2.4GHz non-overlapping trio 1/6/11 skips time sitting on
//     empty channels (COVERAGE TRADE-OFF: 5GHz APs and 2.4GHz channels 2-5/7-10
//     are not heard — acceptable for v1's breadth-count intent; widen here if a
//     bench run shows misses).
//   • Low-battery guard: below kAuditMinBatteryPct SoC the device refuses to arm
//     and seals any live capture (device tier reads Game::powerStatus()).
constexpr uint32_t kAuditArmWindowMs      = 10u * 60u * 1000u;  // listen cap w/o a capture
constexpr uint32_t kAuditChannelDwellMs   = 300;               // per-channel time-on-air
constexpr int kAuditChannels[] = {1, 6, 11};                   // 2.4GHz non-overlapping trio
constexpr int kAuditChannelCount = 3;
constexpr int kAuditMinBatteryPct = 20;                        // refuse-to-arm SoC floor
// Channel-lock on EAPOL activity: a 4-way handshake bursts in ~100ms on the AP's
// channel, faster than a full hop cycle, so hopping alone misses most of them.
// When we hear ANY EAPOL frame we park on its channel for this long to catch the
// rest of the exchange (M1..M4) before resuming the hop. The dominant lever for
// real-world capture success; retune with the dwell on the bench.
constexpr uint32_t kAuditEapolHoldMs      = 3000;              // dwell-lock after EAPOL seen

// Real-radio discovery rewards (pcap-blowup fix). registerNetwork/
//     registerHandshake() (game_net.cpp) are the REAL BSSID-deduped ledgers (v7
//     save tail) — each fires its reward exactly once per genuinely-new BSSID, the
//     same guard that already stops SHAKES/NETS from re-crediting a repeat. A
//     network freshly heard grants a lightly-weighted Sealed Cache (mostly
//     Common); a genuinely NEW captured handshake — the rarer, harder event —
//     grants a richer one (no Common). Percent pairs/triples below must each sum
//     to 100; rolled with the shared LCG (rng_ >> 16) % 100, matching every other
//     weighted roll in game_items.cpp.
constexpr int kNetDiscoveryCacheCommonPct     = 70;  // new network: 70% common
constexpr int kNetDiscoveryCacheUncommonPct   = 30;  //              30% uncommon
constexpr int kHandshakeCaptureCacheUncommonPct = 50; // new handshake: 50% uncommon
constexpr int kHandshakeCaptureCacheRarePct     = 30; //                30% rare
constexpr int kHandshakeCaptureCacheEpicPct     = 20; //                20% epic

// Persistence (the save survives a reboot). Saves are
//     debounced after a meaningful change and capped by a periodic autosave so
//     passive decay is captured without thrashing NVS flash. Structural changes
//     (hatch / evolve / Store / Deploy / reset) persist immediately. ----------
constexpr uint32_t kSaveDebounceMs = 2000;    // coalesce rapid changes into one write
constexpr uint32_t kSaveAutosaveMs = 30000;   // periodic write (captures slow decay)
//     The smallest a serialize buffer is ever sized to, so a save is ONE allocation
//     instead of a doubling ladder that peaks at ~1.5x the blob (see save.cpp for why
//     that peak is dangerous). Sized well above a real populated save: a measured
//     device — generation 10, full rack, 51 items met, every 'Pedia tally — writes
//     ~6.6KB at v45. The same device read 18.5KB at v44, and 64% of that was the
//     owned-mod pool, which spent a 24-byte id cell per COPY held, uncapped, growing 28
//     bytes a drop forever. v45 made it a nibble per mod (save.h), so what is left grows
//     with the SIZE of the content tables rather than with how long a device has been
//     played. A floor, not a cap — overrunning it costs one realloc, not a failure.
//     Game owns a buffer of at least this size and writes every save into it
//     (game_persist.cpp), so the allocation is paid once rather than at each write.
constexpr size_t kSaveReserveBytes = 24576;   // 24KB
//     Room that owned buffer keeps above the last blob it wrote. A save whose blob
//     could come within this of the capacity counts as one that has to RESIZE, which
//     puts it back on the allocating path and behind kSaveGrowHeapFloorBytes — sized
//     so many saves' worth of growth lands inside it and a resize stays rare.
constexpr size_t kSaveBlobHeadroomBytes = 4096;   // 4KB
//     The largest-free-block an ordinary save needs before it will be attempted. The
//     blob goes into the buffer Game already owns, so what is left to allocate is
//     captureSave's own exact-sized vectors — ~15KB in total and none of them large.
//     Below this the write is DEFERRED, not failed — the dirty flag stays set and the
//     next tick retries, so the save lands as soon as whatever was eating the heap
//     (the audit capture arming is the measured case) gives it back. Deferring costs
//     at most a few seconds of pet state; attempting and failing costs a device
//     reset, because the Arduino build cannot catch bad_alloc.
constexpr uint32_t kSaveHeapFloorBytes = 12288;   // 12KB
//     The bar for the one save that must SIZE that buffer — the first after boot, and
//     any later one whose blob outgrew what it has. That save still wants
//     kSaveReserveBytes in a single piece, so it is held above the reservation, with
//     the margin covering the SaveData built alongside it.
constexpr uint32_t kSaveGrowHeapFloorBytes = 32768;   // 32KB

// HackerTag on-device editor. Fixed-cell arcade entry; the
//     default tag "NETRUNNER_99" fills exactly kHackerTagMax cells. ----------
constexpr int kHackerTagMax = 12;   // editable cells (also the buffer length)

// --- Modal / process pacing (event-driven, in ~4fps beats) -----------------
constexpr int kFeedBeats = 10;    // feeding beat before auto-dismiss
// FX_ABSORB / FX_SHRED dissolve length, in the FX clock's own beats (kFxAnimMs, ~16fps)
// rather than heartbeats — every screen that plays a dissolve drives it off Game::fxBeat_,
// so one number is the sweep length everywhere. ~2s: long enough to watch a glyph come
// apart rather than register that it went.
//
// The sweep fits inside each host's own hold (kFeedBeats, kExploreRevealHoldBeats), so
// it costs a player no extra waiting anywhere; the beats after it are the afterglow.
constexpr int kAbsorbBeats = 32;
// The pause before a dissolve starts, in the same beats: the thing being eaten stands
// whole long enough to be identified first.
constexpr int kAbsorbLeadBeats = 4;
// Lead-in plus sweep — how long a dissolve OCCUPIES its screen. The combat outro reads
// this to keep the verdict banner off the effect: a screen that has to wait for a
// dissolve waits on one number, not on a sum re-derived at each site.
constexpr int kAbsorbTotalBeats = kAbsorbLeadBeats + kAbsorbBeats;
constexpr int kProcessBeats = 8;  // MAINT process run length before the outcome
// The three ways to pay for the same clean: 0 Quick (Bits, may fail) · 1 Tool (an item,
// guaranteed) · 2 Stacker (the minigame, guaranteed if you clear it). Luck, an item, or
// skill — so a player with no Defrag Tool and no appetite for the roll still has a way
// through, and it is the one that costs only attention.
constexpr int kDefragVariants = 3;
constexpr int kDefragVariantStacker = 2;
// The Stacker's run slides on its own faster cadence rather than the shared 4fps
// heartbeat (the kCombatAnimMs precedent) — at 250ms a step the timing is trivial, and
// the whole variant is a timing test. Only ticks while the board is on screen.
constexpr int kStackerStepMs = 110;
// A played clean is priced by the BOARD, not by kDefragReduction. Clearing it is a full
// defrag — Fragmentation straight to zero, which is the only thing in the game that
// wipes a disk outright, and the reason the variant is worth the difficulty. Anything
// short of that still pays for the blocks it landed: Stacker::score, at this many points
// to one point of Fragmentation.
//
// The rate is set so that a board which stalls on the LAST row is worth roughly one
// ordinary defrag (kStackerMaxScore is 135; a near miss scores ~108, so ~21 off) and a
// board that stalls halfway is worth a third of that. A run therefore cannot be worse
// than not playing — there is no fail penalty and no care mistake down this path, only
// a smaller clean — because the player already paid the entry in Bits and in attention.
constexpr int kStackerScorePerFrag = 5;

// The arcade (GAMES) --------------------------------------------
// The same minigames, off their stakes. A cabinet run touches no egg and no disk, so
// what it pays has to come from somewhere else — and the answer is deliberately flat:
// the ATTEMPT is what earns, because the arcade's job is to be a thing worth doing on a
// pet that has nothing else to do right now, not a Bits farm that outpaces exploring.
constexpr int kArcadePlayBits = 32;    // paid for finishing a run, however it went
constexpr int kArcadePlayHappy = 10;   // ...and the pet enjoyed itself either way
// The skill half, on top of the flat one. An incremental game is paid this in
// proportion to its score; a win-or-lose game takes all of it or none. Capped equal to
// the participation payout so a perfect run is worth exactly twice a bad one — enough
// to be worth playing well, not enough to make losing feel wasted.
constexpr int kArcadeScoreBits = 32;
// What the difficulty dial does to a PACED game (the Stacker's slide, the worm's step),
// as a percentage of its shipped cadence. Medium is the game exactly as its own context
// plays it — 100 here is load-bearing, not a placeholder.
constexpr int kArcadeSpeedPctEasy = 160;
constexpr int kArcadeSpeedPctMedium = 100;
constexpr int kArcadeSpeedPctHard = 65;
// The Clutch's dial instead moves how many times the raft halves — a narrower survivor
// means the tell has to be found earlier. Medium matches the hatch's own three rounds.
constexpr int kArcadeClutchRoundsEasy = 2;
constexpr int kArcadeClutchRoundsMedium = 3;
constexpr int kArcadeClutchRoundsHard = 4;
// The Isolation run's goal off an egg. The hatch prices it in minutes of incubation
// (kBootHatchMs / kIsolationDotMs = 30 bytes); the arcade has no clock to price against,
// so it takes the same number directly and a clean run stays the same length of run.
constexpr int kArcadeIsolationGoal = 30;

// Menu navigation ------------------------------------------------
// One global idle timer governs the whole menu tree: silence collapses every
// layer back to idle. Suspended inside modal events / minigames. Tripled from
// the original 5s — plain rows read in under that, but the more text-heavy
// screens (detail panels, the 'Pedia, event log) didn't leave enough time to
// actually read before the tree collapsed out from under the operator.
constexpr uint32_t kAutoDefocusMs = 15000;
// A RADIO SCREEN (Game::radioScreenOpen — CREW's home-network picker, PEERS)
// gets its own, far longer budget. Both are HANDS-OFF screens the way the Pedia QR
// page is: the operator walks toward the network they mean to claim, or holds the
// device next to someone else's, and watches the list fill in.
//
// The shared reason they can't take the standard 5s is that on these screens the
// screen being open is WHAT ARMS THE RADIO (radioScanWanted / linkWanted), so
// collapsing the menu powers the radio down before it can produce the very result
// the operator is waiting for — the screen starves itself. Five seconds is shorter
// than a Wi-Fi sweep takes, and far shorter than two devices need to hear each
// other's beacons.
constexpr uint32_t kRadioScreenDefocusMs = 600u * 1000u;  // 10 min of hands-off watching

} // namespace mal
