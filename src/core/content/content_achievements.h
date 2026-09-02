// content_achievements.h — the achievement table: what the device recognises you for.
//
// An achievement is PLAYER-level: it outlives any one pet, like the HackerTag and the
// earned Titles. There is no achievement browser on the device (three buttons, 224px), so
// an unlock reaches the player two ways: a banner that flashes on the home screen the next
// time they are looking at it (Game::achBanner*), and the web 'Pedia's achievements map.
//
// ADDING ONE IS ONE ROW. That is the whole design goal, because this table is expected to
// grow by an order of magnitude. A row names a `series` — where its progress comes from —
// plus the `goal` that series must reach, so a ladder is N rows over one resolver and
// needs no new code:
//
//     {/*wire=*/40, "BOSS_10", "Boss Hunter", AchSeries::BossWins, /*goal=*/10, ...}
//
// Three escape hatches keep that true as the table grows:
//   * `goal = kGoalAll` compares against the series' own TOTAL, so "every area cleared",
//     "every Rare item held", "the rig fully maxed" and "a whole line raised" are ordinary
//     rows rather than one bespoke boolean series each.
//   * a new progress SOURCE is one AchSeries entry + one case in Game::achProgress.
//   * a new kind of REWARD is one AchievementReward::Kind + one case in
//     Game::applyAchievementRewards — never a fresh scalar field on this struct
//     (src/core/content/CONTENT_STANDARD.md rule 1).
//
// WIRE NUMBERS — read before editing. The save stores earned/notified state as a BITSET
// indexed by `wire`, not by row position, so rows may be reordered, regrouped by theme, or
// removed without invalidating a save. In exchange: **assign the next unused number and
// never reuse or renumber one.** A retired achievement's number stays burned. The native
// gate asserts uniqueness and that every number is under kAchievementWireCap.
//
// Compiled-in and id-addressed, like the crews (content_crews.h) and unlike the registry
// types: the triggers are engine-side, so there is nothing an SD-card content pack could
// author on its own today. Promoting this to a ContentSource later is additive (one
// virtual, one EmbeddedContent override) — it is not a rewrite of this shape.
//
// GROWTH SHAPE: rows are ~4 lines each, so they all live in one content_achievements.cpp.
// When that stops being skimmable, split it BY GROUP into a content_achievements/ folder
// (the way an EXPL area got one, src/core/content/areas/AREA_CONTENT_STANDARD.md) rather than inventing a
// second layout — and don't pre-split ahead of the growth.
#pragma once

#include <cstdint>

namespace mal {

// Where a row's progress comes from. Each entry is one case in Game::achProgress, which
// answers with {value, total} — the total being what kGoalAll compares against.
//
// A series that needs a SUBJECT takes it from the row: `key` for an id (a creature line,
// an item), `param` for a number (a rarity, a qualifying depth).
enum class AchSeries : uint8_t {
    // No automatic check: fired by an explicit Game::unlockAchievement() at the moment
    // the thing happens, because the event leaves no counter behind to test for.
    Event,
    DeepWebDepth,       // deepest DeepWeb Dive depth this device has ever reached
    DeepWebDepthLine,   // ... reached by a pet of line `key`
    SpeciesAtDepth,     // distinct species whose own deepest dive reached `param`
    BossWins,           // lifetime boss rounds won (a gauntlet round counts as one)
    AreasCleared,       // EXPL areas whose area boss is beaten (total = every area)
    SubAreasCleared,    // sub-areas cleared, summed across every area
    MalbeastsDefeated,  // distinct wild malbeast species defeated (total = the roster)
    SpeciesRaised,      // distinct species this device has raised
    LineRaised,         // creatures of line `key` raised (total = that line's roster)
    FoodsCollected,     // distinct Food items ever held (total = every food)
    RarityCollected,    // distinct items of rarity `param` ever held (total = all of them)
    ItemCollected,      // whether item `key` has ever been held — goal 1
    // MERGE HUB recipes known (total = every recipe that ships). Distinct from
    // FoodsCollected: holding a dish says somebody handed you one, knowing the method
    // says a Decryptogram taught you to make it, and only the second is a ladder the
    // player climbs deliberately.
    RecipesKnown,
    RigRowsOwned,       // Rig Shop rows bought at least once (total = every row)
    // Rig Shop levels bought on the CAPPED rows only (total = every one of them maxed).
    // Bandwidth is deliberately excluded: it has no purchase cap, so counting it would
    // make "fully upgraded" both unreachable and farmable by buying the same row forever.
    RigCappedLevels,
    NetworksSeen,       // lifetime unique Wi-Fi networks resolved by a walk
    Handshakes,         // lifetime unique WPA handshakes captured
    BitsHeld,           // Bits held at once (a high-water check, never spent back down)
    PetsRaised,         // pets raised across lifecycles
    StackerWins,        // lifetime DEFRAG minigame boards cleared (core/model/stacker.h)
    // The GAMES arcade (content_arcade.h). Runs and wins are summed across every
    // cabinet; losses are the difference, counted in its own series so "kept playing
    // and kept losing" is a row rather than arithmetic a reader has to do.
    ArcadePlays,
    ArcadeWins,
    ArcadeLosses,
    ArcadeCabinetWins,  // ... wins on the one cabinet named by `key`
    // The best SCORE ever reached on the cabinet named by `key`, against a goal that is
    // that score. The two endless cabinets (the CHROMATOPHORE, the Isolation buffer) run
    // until the player fails rather than until they finish, so a win tally says only
    // that a run cleared the till's line — this is the series that can ask how far past
    // it they went. Meaningless on a win-or-lose cabinet, which never reports a score.
    ArcadeCabinetBest,
    // ROCK THE DOCK (content_tournament.h). Brackets TAKEN, not matches won: the arena
    // is single-elimination, so a run either ends in the title or ends, and the only
    // number it can honestly report is how many times the whole thing was taken.
    TourneyWins,
    PvpWins,            // lifetime LINK duels won (AchSeries::Event's FIRST_DUEL counts the meeting)
    MergesCooked,       // dishes actually cooked at the MERGE HUB — see RecipesKnown
    QuotesSolved,       // distinct Decryptograms cracked (total = the whole quote roster)
    TitlesUnlocked,     // zone-completion Titles earned (total = every area)
    PeersMet,           // distinct operators this device has ever met over the radio
    // Distinct species ever LAID EYES ON (total = the roster). The third axis beside
    // SpeciesRaised and MalbeastsDefeated, and the only one a duel, an arena entrant or
    // a branch-sibling reveal can move — you can meet far more of the roster than you
    // will ever raise.
    SpeciesSeen,
    // Pets held in the ARCH rack at once. UNBOUNDED on purpose though the rack has a
    // ceiling: that ceiling is a rig upgrade, so it is player state, and
    // achievementSeriesTotal must stay pure (game_achievements.h). Fixed rungs instead.
    RackHeld,
    // The rack read as a COLLECTION rather than as a headcount — which is what the
    // 64-slot shelf is FOR (game_rig_shop.h): room to keep a live copy of one of
    // everything, not just somewhere to park spares. All of these are "at once", so a
    // value FALLS when a pet is deployed or released; the earned bit never does, because
    // an achievement records something the player did, not something still true.
    RackSpecies,        // distinct species on the shelf at once (total = the roster)
    RackLines,          // distinct families represented on it (total = every line)
    RackLineHeld,       // creatures of line `key` on it at once (total = that line)
    // The biggest stack of ONE species on the shelf. AchSeries::Event's SECOND_INSTANCE
    // is the first rung of this ladder in all but name, and stays an Event because it
    // GATES the Worm egg line: a gate has to fire the moment the pair meets rather than
    // on the next sweep.
    RackTwins,
    StepsWalked,        // lifetime exploration steps
};

// `goal` sentinel: compare against the series' TOTAL rather than a fixed number, so the
// row means "all of them" and keeps meaning it when the set it counts over grows.
inline constexpr int kGoalAll = -1;

// One thing unlocking an achievement hands over. A structured kind+magnitude list rather
// than named `rewardBits`/`rewardItem` fields, so a future Title/mod/move reward is a new
// Kind and one applier case instead of another field every row has to leave defaulted.
struct AchievementReward {
    enum class Kind : uint8_t {
        None = 0,
        Bits,   // +magnitude Bits
        Item,   // +magnitude copies of item `id` (a Commendation Cache, usually)
    };
    Kind kind = Kind::None;
    int magnitude = 0;
    const char* id = nullptr;
};

// The most rewards any one row carries (Bits + a cache today). Unused slots stay None.
inline constexpr int kMaxAchievementRewards = 2;

struct AchievementDef {
    // Stable save identity — the bitset index. Never reused, never renumbered; see the
    // banner above.
    int wire;
    // The 'Pedia's map key and the id every engine call site names
    // (Game::unlockAchievement). UPPER_SNAKE by convention.
    const char* id;
    const char* displayName;   // the home-screen banner line + the 'Pedia title
    // How it is earned, as a TEMPLATE over this row: `{n}` is the effective goal and
    // `{p}` is `param`, so a whole ladder shares one sentence and a retuned threshold
    // retunes the prose with it (core/content/effect_text.h). Never type the number.
    const char* trigger;
    // The asset NAME (no path, no extension) the web 'Pedia shows beside the row —
    // tools/gen_pedia_data.py resolves it to wherever that PNG sits under assets/.
    // Nothing on the device draws it; the 'Pedia is the only surface that does.
    const char* icon;
    AchSeries series;
    int goal;                  // the threshold `series` must reach, or kGoalAll
    const char* key = nullptr; // the series' subject when it is an id
    int param = 0;             // the series' subject when it is a number
    AchievementReward rewards[kMaxAchievementRewards] = {};
    // The 'Pedia masks the trigger text — for the ones whose fun is not knowing.
    bool hidden = false;
};

extern const AchievementDef kAchievements[];
extern const int kAchievementCount;

// Bitset capacity for the save's earned/notified masks, in wire numbers. Raise it (in
// multiples of 8, with a save-version note) when the highest number approaches it; the
// masks are length-prefixed on the wire, so a longer one loads into an older build's
// shorter view harmlessly and a shorter one reads back as "not earned".
inline constexpr int kAchievementWireCap = 256;

// The row with this id, or nullptr. Every engine call site names an achievement by id, so
// a renamed row fails loudly at its trigger instead of silently unlocking nothing.
const AchievementDef* achievementById(const char* id);

// The row at table position `index` (0..kAchievementCount-1), or nullptr out of range.
// Position, NOT wire number — for sweeping the table, never for storage.
const AchievementDef* achievementAt(int index);

// The ids the ENGINE fires by hand (AchSeries::Event rows, plus the two depth milestones
// that gate content). Named here so the spelling lives with the table: a typo is a
// compile error at the call site rather than an unlock that never happens.
namespace ach {
inline constexpr const char* kFirstBruteForce  = "FIRST_BRUTE_FORCE";
inline constexpr const char* kSurvivedLockout  = "SURVIVED_LOCKOUT";
inline constexpr const char* kFlawlessRun      = "FLAWLESS_RUN";
inline constexpr const char* kGoneRogue        = "GONE_ROGUE";
inline constexpr const char* kWormWhisperer    = "WORM_WHISPERER";
inline constexpr const char* kNeverSeen        = "NEVER_SEEN";
inline constexpr const char* kAirGapped        = "AIR_GAPPED";
inline constexpr const char* kDevtoolsIntruder = "DEVTOOLS_INTRUDER";
inline constexpr const char* kTrojanUnleashed  = "TROJAN_UNLEASHED";
inline constexpr const char* kFirstDuel        = "FIRST_DUEL";
inline constexpr const char* kSecondInstance   = "SECOND_INSTANCE";
// The mirror, and the gate on the Metamorphic egg line: the operator's own species
// walking on as the opponent. Fired from Game::noteMirrorMatch, which BOTH places a
// real species can be faced call — the LINK duel and the arena — because what earns it
// is the meeting, not which screen arranged it.
inline constexpr const char* kHashCollision    = "HASH_COLLISION";
// The two DEFRAG-minigame rows that a board's SHAPE decides rather than a tally: how
// wide the run was when it reached the top. Both fire from Game::finishStacker.
inline constexpr const char* kPerfectDefrag    = "PERFECT_DEFRAG";
inline constexpr const char* kHangingByABit    = "HANGING_BY_A_BIT";
// The other end of the same board, and the reason it is here rather than beside the
// arcade's counted rows: losing on the second row is a shape, not a tally, so it fires
// wherever the board is played.
inline constexpr const char* kStackOverflow    = "STACK_OVERFLOW";
// The three endings a spent Backup Drive can buy, all fired from Game::settleBackupDrive
// off Combatant::BackupUse and the fight's outcome.
inline constexpr const char* kBackUpAndDriven  = "BACK_UP_AND_DRIVEN";
inline constexpr const char* kNeededMoreBackup = "NEEDED_MORE_BACKUP";
inline constexpr const char* kShatteredPlatter = "SHATTERED_PLATTER";
// Read as CONTENT GATES, not just badges: depth 8 unlocks the Phishing egg line at
// line-select and depth 64 adds Phishlet to that egg's hatch pool. Neither is named
// here — a gated row names the achievement that earns it (EggLineDef::gatedBy,
// CreatureDef::gatedBy), so this direction of the link is read off the content.
// kSecondInstance above gates the Worm line the same way.
// ROCK THE DOCK's two SHAPE rows — what a bracket was taken WITH, which the win tally
// beside them cannot say. Both fire from Game::awardTourneyPurse, where the final
// opponent is still seated in tourneyOpponent_ and every entrant's level is still
// derivable from the run seed, so neither costs the save a byte.
//
// Note what is deliberately NOT here: "took a bracket without dropping a match". The
// arena is single elimination (core/model/tournament.h), so dropping a match ENDS the
// run — that row and DOCK_FIRST would be the same row wearing two names.
inline constexpr const char* kDockUnderdog     = "DOCK_UNDERDOG";
inline constexpr const char* kDockPunchingUp   = "DOCK_PUNCHING_UP";
// Enlisting in a crew (content_crews.h). An Event rather than a tally because a player
// belongs to at most one crew at a time and may switch freely — there is no count here
// that could mean anything, only the moment of first taking a side.
inline constexpr const char* kCrewEnlisted     = "CREW_ENLISTED";
// The ARCH rack left with no free slot, fired from Game::archStoreActive. The one rack
// row that cannot be a counted rung: "full" is a comparison against a ceiling the player
// bought themself (Game::rackSlots), so there is no fixed number to hold it to.
inline constexpr const char* kNoVacancy        = "NO_VACANCY";
inline constexpr const char* kDeepWebDepth8    = "DEEPWEB_DEPTH_8";
inline constexpr const char* kDeepWebDepth64   = "DEEPWEB_DEPTH_64";
}  // namespace ach

}  // namespace mal
