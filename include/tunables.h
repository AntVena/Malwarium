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
// A Defrag costs Bits, and the price is THIS PET'S HISTORY with the tool rather than
// its stage: cost(n) = start * 2^floor(log2(n)) over Game::defragCount(), which is
// rigUpgradeCost's kLogStep — the softest ladder anything on the device is priced on.
// Flat for the first two runs, then a doubling each time the tally passes a power of
// two: the 8th defrag of a creature costs 64, the 100th 512, the 1000th 4096. Charged
// per attempt regardless of pass/fail.
//
// The shape is the point. Cleaning up after a young pet is pocket change, and keeping
// a long-serving favourite spotless is a bill that grows with how long you have kept
// it — including the Rig Shop's auto-defrag, whose upkeep is a multiple of this and
// whose runs count toward the same tally. A Boot-Sector egg pays nothing: there is
// nothing in it to defragment yet.
constexpr int kDefragCostStart = 8;
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

// --- CHROMATOPHORE: the Metamorphic egg's hatch minigame, played once at lay-time
//     (core/model/chromatophore.h for the rules, game_chroma.cpp for the screen). The
//     water under the bell takes one of three colours, one button wears each, and a
//     sweep crosses on a shrinking clock; every pass made in the right skin is
//     kChromaPassMs off the incubation clock, and being caught in the wrong one — or
//     mid-change — ends the run with what it already earned.
//
//     The economy is one identity, the Isolation Protocol's: kChromaRounds passes at
//     kChromaPassMs is the WHOLE of kBootHatchMs. So a clean run hatches the egg on the
//     spot and is worth playing perfectly, while a run that falls over at pass seven has
//     still bought most of the wait.
constexpr int kChromaRounds = 10;                    // passes in a full run
constexpr uint32_t kChromaPassMs = 180u * 1000u;     // -3 min of incubation per pass
// The opening window, and what each later round sheds off it — the difficulty ramp,
// and the only one the board has. Ten rounds takes 4s down to 1.75s, which the model's
// own kChromaWindowFloorMs catches on the last rung, so the ramp is felt across the
// whole run rather than bottoming out halfway. The arcade's dial scales the OPENING
// (arcadeStepMs) rather than re-cutting the ramp.
constexpr int kChromaWindowMs = 4000;
constexpr int kChromaWindowStepMs = 250;

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
// One notch down from the top. The backlight is the largest single draw on a lit panel
// and its current is roughly linear in duty, so this is a fifth of it back for a step
// nobody reads as dim indoors — where the top level is what a fresh device would
// otherwise burn by default. A stored level always wins; only a fresh save takes this.
constexpr int kBrightnessDefault = kBrightnessLevels - 2;   // 80%
constexpr inline int brightnessPercent(int level) {
    if (level < 0) level = 0;
    if (level >= kBrightnessLevels) level = kBrightnessLevels - 1;
    return (level + 1) * (100 / kBrightnessLevels);
}

// Evolution boundary — a per-stage time-in-stage gate, plus the care budget out of Dying
// (5/5 routes to Critical System Failure, not evolution). The clock counts real elapsed
// time, screen-sleep included, so a short pocket nap must NOT tip a stage over. Each stage
// asks a longer raise than the last: Process->Script clears the Egg->Process hatch
// (kBootHatchMs) by a wide margin, and Script->Daemon steps up again. The modal is a
// beat-paced cinematic — hold the current sprite, white-out flash, reveal. Dev/test forces
// the boundary via debugTriggerEvolution.
constexpr uint32_t kEvolveProcessToScriptMs = 16u * 60u * 60u * 1000u;   // min dwell, 16 h
constexpr uint32_t kEvolveScriptToDaemonMs  = 32u * 60u * 60u * 1000u;   // min dwell, 32 h
constexpr int kEvoHoldBeats = 4;    // hold on the current sprite before the flash
constexpr int kEvoFlashBeats = 2;   // FX_EVO_FLASH white-out, then the reveal

// Evolution branch. At the Script->Daemon hop the care budget splits the line: 0-2 mistakes
// -> Good (durable), 3-4 -> Bad (glass cannon), 5/5 never evolves. The engine reads the
// branch as two combat multipliers off the successor CreatureDef — attack-power lean and
// loss-Frag. Slot COUNT is identical at Daemon (both at the kMaxMoveSlots cap), so the
// aggressive-vs-durable lean is carried entirely by these. Neutral = 100.
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
// STUN chaining (Combat::stunLands). A landed stun (MoveDef::lockTurns) ratchets the
// victim's lock resistance by the turns it actually froze, and every turn the victim
// spends acting sheds one of them back; the next stun rolls against what is left. So the
// first lock is free, the one that comes straight back onto it is a maybe, and the one
// after that mostly just a hit — a chain-stunned fighter always fights its way out,
// without a stun ever becoming a thing an attacker cannot land. The floor is what keeps
// the rider real: a pet that has eaten four locks can still be frozen by the fifth.
constexpr int kLockResistStepPct = 40;   // land chance lost per stacked resist point
constexpr int kLockResistFloorPct = 15;  // ...and the chance a stun never drops below
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

// --- MODS, the permanent hardware-passive layer, and how one enters play -----------
// Three independent axes on ModDef (defs.h): `rarity` = DROP WEIGHT within an area's loot
// table; `powerTier` (1..kModPowerTiers) = ladder DEPTH, which picks the area a mod lives
// in; `equipLevel` = the pet level it needs, authored on the row. ONE level per mod, shared
// by every copy — the picker lists mods by TYPE, so a per-copy roll is a number no player
// could see or choose between. The depth count is kModPowerTiers (areas/area_defs.h), the
// ladder's own length, so adding an area opens a rank rather than overflowing a table.
//
// Gates are authored per row against this ceiling, NOT derived from the tier: deriving
// gives one gate per area, so every mod in an area unlocks on the same level and the
// stretch between two areas has nothing new to slot.
//
// 100 is the ceiling, NOT the reachable top. The XP curve is geometric at
// kLevelXpGrowthPct, so cumulative cost roughly triples every ten levels — level 60 is
// ~301k XP, level 100 ~13.8M. The shipped roster fills 0..60, deep enough that a sixth area
// extends the ladder rather than forcing a re-band, and leaves the rest as headroom. A row
// gated above where a pet can reach is a content mistake this bound cannot catch, which is
// what test_mod_equip_ladder_is_ordered_and_dense is for.
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

// --- The investment LADDER: one set of rungs, the same on all four stats -------------
// Every stat pays out continuously per point AND unlocks three discrete TIERS, and the
// tiers sit at the same three point counts whatever stat they are on. That uniformity is
// the whole feature: before it, Defence's two thresholds were at 12 and 18 and nothing
// else had any, so "how far am I from the next thing" was a question only the source code
// could answer. One ladder means the STAT screen can draw one grid, and a player who has
// learned where Speed's rungs are has learned where every stat's are.
//
// 8 / 16 / 32 rather than 10 / 20 / 30 — the doubling reads as a real commitment curve
// (T2 costs what T1 did, T3 costs what both did together) and lands on the binary counts
// the rest of the device is written in. The ladder is also what the continuous curves
// bend at: T1 opens Power's and max-Health's accelerating band and starts Defence's
// diminishing one, so a rung is one fact rather than two coincidences.
//
// The top rung is deliberately past what a random raise hands out — the level-up grant
// picks its stat at random and the ladder runs to level 60, so ~15 points in a stat is
// the unremarkable outcome. T3 is for a pet that was BUILT, by luck, a Rollback or an
// Epic dish's off-level points (PetUpgrades::statBonus, which count here — a point is a
// point). Defence's cut ceiling lands exactly on T3: 8 full-rate points + 24 bent ones is
// 60%, kLevelDefenseCapPct, so the stat stops buying % on the same rung it starts buying
// something else. That coincidence is load-bearing and a native gate asserts it.
constexpr int kStatTierCount = 3;
constexpr int kStatTier1Points = 8;
constexpr int kStatTier2Points = 16;
constexpr int kStatTier3Points = 32;
constexpr int kLevelPowerPctPerPoint = 4;      // +4% attack power per power point
constexpr int kLevelDefensePctPerPoint = 3;    // +3% incoming-damage cut per defense
// ...at FULL rate only for the first kLevelDefenseSoftPoints; past that a point buys
// half as much (levelDefenseCutPct, combat.h). Defense is the one stat with a hard
// ceiling, so without a bend the last points before the cap were the most valuable
// purchase in the game and the wall was simply a matter of spending enough. The curve
// leaves early Defense untouched and only taxes the stretch that was heading for immunity.
// The bend sits on the ladder's first rung, so the point where the % stops paying full
// rate is the same point where the stat starts paying in pierce resist instead — one
// threshold the player can be told about, not two they have to discover separately.
constexpr int kLevelDefenseSoftPoints = kStatTier1Points;  // full-rate points before the bend
constexpr int kLevelDefenseCapPct = 60;        // ...level defense contribution cap
constexpr int kLevelDmgReduceMaxPct = 85;      // ...total dmg-cut clamp (never immune)
constexpr int kLevelSpeedPerPoint = 1;         // +1 initiative per speed point
                                               // (kLevelSpeedUnderdogPerPoint replaces this
                                               // rate outright while Speed T2 is paying)
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
constexpr int kLevelPowerSpecPoints = kStatTier1Points;  // points before the accelerating band
constexpr int kLevelPowerPctPerSpecPoint = 10;   // ...and the rate past it (base is 4)
constexpr int kLevelPowerSpecCapPct = 300;       // total level-Power contribution ceiling
constexpr int kLevelHealthSpecPoints = kStatTier1Points;
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
//   backscatter   — and the last rung, which lands on exactly the point count where the %
//                   cut stops growing (see the ladder above): a wall that can no longer be
//                   made thicker starts paying OUT. A share of what it absorbed this hit is
//                   dealt back to whoever swung, so the turtle finally has a win condition
//                   that is not "outlast everything". Deliberately small, and deliberately a
//                   fraction of damage ALREADY eaten rather than of the attack: it can only
//                   pay when something actually hit the wall, which is what keeps it from
//                   competing with Ransomware's line (kRansomSeizedWallPct), whose whole
//                   identity is converting a wall into offence on purpose.
constexpr int kLevelDefensePierceResistPoints = kStatTier1Points;
constexpr int kLevelDefensePierceResistPct = 40;   // cuts an attack's effective pierce
constexpr int kLevelDefenseBraceRetainPoints = kStatTier2Points;
constexpr int kLevelDefenseBraceRetainPct = 25;    // ...ADDED to the baseline below
constexpr int kLevelDefenseBackscatterPoints = kStatTier3Points;
constexpr int kLevelDefenseBackscatterPct = 20;    // % of absorbed damage dealt back

// The share of an unspent one-shot brace that carries to the next hit for ANY fighter,
// before Defence investment adds to it. A baseline exists because over-sizing is the
// normal case rather than the exceptional one: measured across legal loadouts, most braces
// swallow the whole hit they meet, so most of a brace's magnitude was being authored,
// paid for with a turn, and then thrown away. Retaining part of it is the one thing that
// makes a brace better without making it bigger — and brace magnitude is worth almost
// nothing per point, so bigger was never available.
constexpr int kBraceRetainBasePct = 25;

// --- The other three stats' tiers (Defence's two are above, with the curve they bend) ---
// Each rung answers a way its stat was being ROUTED AROUND rather than out-scaled, which
// is why none of them is simply "more of the same number". Power and max-Health already
// buy a bigger number per point and their T1 is that acceleration turning on; T2 and T3
// have to be a different kind of thing or the rung is invisible.
//
// POWER. Its whole output is deleted by a wall — at the 85% clamp a hit arrives at 15% of
// itself — so committed Power buys the two things that get PAST a wall rather than over
// it. T2 is innate pierce, the same currency the PIERCE mod family deals in and the same
// currency Defence's own T1 blunts: a Power build and a Defence build now argue with each
// other on one axis instead of talking past each other. T3 is the brace's turn: a
// one-shot `guard` is the other half of what a defender spends a turn on, and pierce
// alone left it untouched.
constexpr int kLevelPowerPiercePct = 20;       // T2: hits ignore this much of a wall...
constexpr int kLevelPowerGuardSmashPct = 50;   // T3: ...and this much of a brace

// SPEED. The one stat that was flat in both directions and had nothing but initiative to
// sell, which made it the stat a raise was disappointed to land on. Its three rungs are
// all TEMPO rather than magnitude, and each is worth something in a different fight.
// T1 pays for winning the opening roll — a fast pet already acted first and got nothing
// extra for it. T2 is the catch-up rung, and deliberately the odd one out: it pays only
// while this fighter's Speed points TRAIL the opponent's, so a pet that invested and
// still got out-sped is not simply beaten on the axis it bought. T3 pays as the fight
// goes badly, which is the one stretch initiative is worth most and the pet has least.
constexpr int kLevelSpeedFirstStrikeMult = 2;      // T1: the fight's first landed hit, doubled
constexpr int kLevelSpeedUnderdogPerPoint = 2;     // T2: initiative/pt while behind (base 1)
constexpr int kLevelSpeedAdrenalineStepPct = 10;   // T3: per this much max Health missing...
constexpr int kLevelSpeedAdrenalinePerStep = 1;    // ...this much initiative, live

// MAX-HEALTH. A pool is only ever worth the damage it outlasts, so past its accelerating
// band it stops buying pool and starts buying ways to SPEND the pool twice: T2 recovers
// from the fight (never from the tick currently killing you — it rides the same turn-start
// ordering the Regen mod does), T3 is a free death-save, ahead of a Backup Drive so a pet
// carrying both spends the tier and keeps the item.
constexpr int kLevelHealthScrubPct = 3;        // T2: % of max Health healed each turn
                                               // T3 (failover) is a flag, not a magnitude

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
// Bandwidth is the per-fight FRAGMENTATION SHIELD for exploration. Any resolved wild fight
// — win or loss, first clear, re-farm or dive; bosses and Sim excluded — spends 1 Bandwidth
// to SKIP the battle-fatigue frag tax entirely, so a stocked pool lets the pet explore
// longer with less risk and the Hacker SHOP's "Increase Bandwidth" upgrade buys real
// safety. At 0 the tax bites again — the "defrag or come home" signal. It never hard-stops
// stepping, and Bits and XP always stay full. Loot decay still gates on RE-FARMING a
// cleared sub, keyed on whether the fight was shielded. Regenerates over real elapsed time,
// and is not persisted across a reboot.
//
// The BASE pool, not the cap: what an unupgraded rig starts with. Every read of the live
// ceiling goes through Game::bandwidthMax(), which adds the upgrades, so this is the floor
// that ramp is measured from (game_rig_shop.h anchors its curve to it).
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
// Hacker SHOP > SERVICES: holding B past this opens the focused service's info page
// (what it does, and what a run of it costs) instead of switching it. Same 800ms as
// every other hold on the device — a gesture that means "tell me more" should feel
// like the others, and the tap it shares the button with is the common one.
constexpr uint32_t kServiceInfoHoldMs = 800;
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
// C keeps its tap everywhere — Cancel is the one button whose meaning never bends — so a
// LIST puts its step BACKWARD on the hold: hold C and the cursor walks back up the rows at
// the same cadence A walks down them. It starts at kListRepeatDelayMs, the dwell that
// starts A's repeat, so the two directions feel like one gesture. A press that never
// reaches it is an ordinary tap and cancels on release; one that does has stepped, and its
// release cancels nothing.
//
// THE DECRYPTOGRAM's cursor repeat is the faster of the two, this being the only place a
// cursor has thirty-odd stops to walk, each a single letter. Sized so a full lap of the
// longest quote takes a couple of seconds rather than the ten a 4fps heartbeat would cost,
// which is why it ticks on its own cadence (Game::tick) like combat's sprites and the
// Stacker's slide. The interval sits just inside kStackerStepMs, the fastest repaint the
// panel is known to sustain; with the cursor running both ways the worst case is half a
// lap, so there is no reason to push it further.
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
// so the pair lands where there is room for it. And an area's brace is a thing a player
// wants, so putting it on the two meanest rungs makes the wall worth walking to.
constexpr int kWildAreaDefendSub = 3;
// The most moves a wild ever fights with — its depth RUNG plus three riders: the area's
// Attack, the area's Defend at the rung above, and the creature's own signature
// (CombatEnemy::signatureMoveId).
//
// One more than a fully-evolved pet holds, deliberately. The pet's slot count is a budget
// the player SPENDS; this is how many things one encounter has to say, and a wild already
// answers to kWildEnemyHealthPct/kWildEnemyDamagePct either side of it. Fitting the third
// rider inside four would mean taking a slot off the depth ladder, whose rungs are ordered
// by effective per-turn damage — thinning the deep ones inverts that order, where appending
// the same rider to every rung alike cannot.
//
// The dilution runs the safe way: Combat::chooseMove is uniform, so a fifth move takes each
// of the others from a quarter of the turns to a fifth, the apex's hardest hitter included.
// The sharp rule is unchanged — at most ONE brace in a kit, or a wild spends half the fight
// holding.
constexpr int kWildKitMax = kMaxMoveSlots + 1;
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

// --- Sub-area + area bosses -------------------------------------------------------
//     The unit of progress is the SUB-AREA: a 10-win streak (kExploreStreakToBoss) unlocks
//     that sub-area's boss as a durable per-sub flag, a MANUAL FIGHT BOSS from the EXPL row
//     launches it, and beating it marks the sub-area CLEARED. Clearing all five unlocks the
//     AREA boss — a 5-stage gauntlet of those sub-bosses with carried Health — and beating
//     that sets sectorCleared[] and grants the Title. Sub-boss stats scale with the area
//     tier plus the sub index, sub 5 being the signature apex.
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

// Wi-Fi network explore event. The event is ROUTED by what the radio actually
//     queued rather than rolled independently of it: a sighting to resolve makes this
//     the discovery beat (the pet takes the network in), and an EMPTY queue is what
//     summons the area's guardian instead. So the two halves of the screen can never
//     contradict each other, and a walk through a dead zone reaches new content rather
//     than only a Happiness tax. game_net.cpp routes it; game_shibboleth.cpp owns the
//     guardian half. --------------------------------------------------------------
//
// The weights below are the SIGHTING branch only — what a resolved network turns out to
// have been standing next to. Friendly-visit is a v1 substitute for the doc-07 Met-Pets
// Roster (out of pet-side scope) — a generic ally-buff rather than a named remembered
// pet.
constexpr int kWifiSleepingPct = 30;    // sleeping guardian -> free loot
constexpr int kWifiAwakenedPct = 25;    // awakened guardian -> wild combat
constexpr int kWifiOpenCachePct = 25;   // open cache -> straight loot
                                         // (remainder = friendly visit -> ally buff)
constexpr int kAllyBuffBattles = 3;     // friendly-visit buff duration
constexpr int kAllyBuffPowerPct = 20;   // +power for the buffed battles

// THE SHIBBOLETH — the guardian encounter an empty sighting queue routes to
//     (game_shibboleth.cpp, core/model/cant.h). A guardian grades its welcome on how
//     much of the CANT the pet can read, so these three bands are read against
//     cantFluencyPct: below the first it is turned away outright, above the second it is
//     simply received, and the stretch between is where it is TESTED. ------------------
// The bands are a LIKELIHOOD ladder and deliberately not a threshold one. Thresholds
// deadlock: a pet is refused until it is fluent, and the only way to become fluent is to
// be asked, so the whole system would be unreachable from a fresh save. As chances, every
// fluency can reach every band — what climbing the Cant buys is that the good one gets
// commoner and the bad one gets rarer, which is also what "the more they know, the more
// likely a quiet word" actually means.
constexpr int kShibbolethAffrontBasePct = 25;  // AFFRONT chance at ZERO fluency: a
                                              // guardian turns an illiterate pet away a
                                              // quarter of the time and hears it out the
                                              // rest. Scaled DOWN by fluency, to nothing
                                              // at a complete Cant
constexpr int kShibbolethBoonMaxPct = 60;     // BOON chance at a COMPLETE Cant — no
                                              // riddle at all, the two simply talk.
                                              // Scaled UP from nothing at zero fluency.
                                              // Under 100 on purpose: a guardian that
                                              // never asked again would retire the
                                              // riddles the moment they stopped being
                                              // needed, and the pool is the content
constexpr int kShibbolethHailHoldBeats = 20;  // ~5s on the guardian's HAIL — the beat
                                              // before the question, where the thing
                                              // that stopped the walk gets to be a
                                              // character instead of a puzzle prompt. A
                                              // REVEAL rather than a decision, so it is
                                              // the short hold the Wi-Fi event uses and
                                              // not the shop's
constexpr int kShibbolethVerdictHoldBeats = 24; // ~6s on the VERDICT: what the guardian
                                              // made of the answer, and the only place
                                              // a wrong reply is ever explained before
                                              // the fight it causes. Longer than the
                                              // hail because it carries the ledger of
                                              // what the meeting cost or paid
constexpr int kShibbolethReplyHoldBeats = 60; // ~15s to answer, then the guardian takes
                                              // the silence as an answer and the reply
                                              // resolves as a wrong one. Longer than the
                                              // shop's kExploreDecisionHoldBeats because
                                              // there is genuinely something to READ here
                                              // before there is anything to decide
constexpr int kShibbolethWinHappy = 6;        // Happiness for answering correctly — paid
                                              // whether or not a shake was there to buy
                                              // the sigil with
constexpr int kShibbolethWinFragCut = 4;      // ...and Fragmentation shed with it
constexpr int kShibbolethLoseHappy = 4;       // Happiness lost on a wrong or unanswered
                                              // reply. Deliberately under the win, since
                                              // the fight that follows carries the rest
                                              // of the cost
constexpr int kShibbolethLoseFrag = 5;        // ...and Fragmentation taken with it
constexpr int kShibbolethBoonHappy = 10;      // a BOON's Happiness — the largest single
                                              // lump the walk pays, and the reason to
                                              // learn the Cant at all
constexpr int kShibbolethBoonFragCut = 12;    // ...and the Fragmentation it clears, which
                                              // is a free defrag in all but name
constexpr int kShibbolethBoonEscortPct = 34;  // ...and how often it instead sends the pet
                                              // on with an escort (the friendly-visit
                                              // ally buff, at kShibbolethEscortBattles)
constexpr int kShibbolethEscortBattles = 5;   // a guardian's escort outlasts a friendly
                                              // visit's — it is owed to a pet that can
                                              // ASK, not one that happened to be passed

// A guardian's SWARM in a fight (FX_SWARM, core/render/swarm.h): the two Health bands its
//     body reads off, as percentages of its own maximum. The flock's mood is the second
//     channel on the rival's Health gauge — the creature is visibly holding together while
//     it is winning and visibly coming apart when it is not — so these are stated here
//     beside the rest of the guardian's fight rather than inside the renderer.
constexpr int kGuardianSwarmPressedPct = 60;  // ...below this it stops appraising and fights
constexpr int kGuardianSwarmFailingPct = 25;  // ...and below this it draws into a knot

// The GUARDIAN itself, as a fight. Built off the same depth spine every boss uses
//     (combat_factory.cpp's guardianEnemy) at its area's deepest rung, then stepped up —
//     it is the thing that has been watching the whole area, so it out-classes the
//     gauntlet's last stage rather than matching it. Shared, not per-area, per
//     src/core/content/CONTENT_STANDARD.md rule 2: every area's guardian is its own
//     ladder plus the SAME step.
constexpr int kGuardianHealthBonusPct = 25;   // Health over the area's deepest sub-boss
constexpr int kGuardianPowerMultPct = 125;    // attack lean — what "stronger mods
                                              // equipped" actually is on an enemy, which
                                              // fields stat leans rather than a mod rack
constexpr int kGuardianDmgReducePct = 15;     // ...and the damage cut on the other side
constexpr int kGuardianSpeedBonus = 2;        // and it moves first more often than not

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
// The in-range snapshot of OTHER MALWARIUM DEVICES (Game::registerPeer -> livePeers_), the
// pet-to-pet twin of the network snapshot above: which operators can I hear right now,
// which is what separates a LIVE row on the PEERS screen from a remembered one in the
// SD-backed PeerLedger.
//
// The cap is small on purpose — this counts people in one room, not access points — and a
// full snapshot evicts its stalest entry. The freshness window must stay comfortably above
// the beacon cadence (PEER_BEACON_INTERVAL_MS, config.h) so a dropped frame doesn't blink
// someone out of the room, and it doubles as the meeting boundary.
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
constexpr int kNetDiscoveryEmptyGuardianStrikes = 3;  // an empty queue summons the area's
                                              // GUARDIAN every Nth miss (streak 3,6,9…),
                                              // never the 1st: arming a walk resets the
                                              // streak, so a 1st-miss trigger would head
                                              // every fresh walk with a guardian and make
                                              // re-arming a way to farm them. The beat
                                              // that used to sting a dry walk for
                                              // Happiness is now the beat that puts
                                              // something in front of the pet — walking
                                              // somewhere with no new networks costs
                                              // nothing and leads to the Cant instead
                                              // (game_shibboleth.cpp). The dry events
                                              // BETWEEN still resolve their ordinary
                                              // sub-outcome, so the walk's event density
                                              // is unchanged either way

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
//     Distinct from the AUDIT SCAN discovery toggle: this is the WPA
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

// Audit capture RF power budget. The radio is the dominant draw — promiscuous RX plus
//     channel hopping keeps it hot continuously, unlike the passive scan's periodic bursts
//     — so every bound here exists to stop it draining the pack. Retune them from measured
//     current draw on the RF bench (armed-idle vs hot vs scan).
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
// The two ENDLESS cabinets' win lines. Neither run has a finish any more — the worm
// eats until it crashes and the bell wears skins until it is spotted — so these are not
// goals the game stops at but the score a run is PAID in full for, and the line its win
// tally is drawn at. A player can and should go past them; the payout simply stops
// growing (finishArcadeRun clamps), because past this point the reward is the high score
// itself and the achievements hanging off it.
//
// Both are set at the length the run used to be, so "a good run" means the same thing it
// meant when these boards had endings: the worm's is the 30 bytes that once ate a whole
// incubation clock, and the bell's is the 10 passes that once hatched an egg.
constexpr int kArcadeIsolationWinBytes = 30;
constexpr int kArcadeChromaWinPasses = kChromaRounds;

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
