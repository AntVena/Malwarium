// stat_screen.h — STAT: a 6-page status viewer (read-only) and the INDEX that jumps
// between the pages.
//   page 0  VITALS    the pet: gauges + care + level/XP + time-to-evolve (the landing)
//   page 1  TIERS     the investment ladder: each combat stat's points, the rungs it
//                     has reached and what the next one costs
//   page 2  LOADOUT   the pet's equipped moves + mods, WITH their effect text
//   page 3  BUFFS     currently-armed item buffs (Restore Point/Ambig-USB/Backup
//                     Drive), each with its effect text and remaining time if timed
//   page 4  SPECIES   the pet's own lore — line, one-line snarky hint, infosec ref
//   page 5  AUDIT LOG the rolling event history
// A cycles the pages, B scrolls the flowed ones, HOLD B opens the INDEX and C backs
// out. Each page dual-codes (grayscale-legible).
// STAT is pet-only. The device/account stats (Hacker Rank, Bits, lifetime breadth)
// live on the Hacker PROFILE slot (game_hacker.cpp), not here.
#pragma once

#include <cstdint>
#include <vector>

#include "core/content/defs.h"
#include "core/content/effect_text.h"
#include "core/model/event_log.h"
#include "core/model/pet_model.h"
#include "core/model/pet_upgrades.h"
#include "core/ui/prose_page.h"

namespace mal {

class Framebuffer;
class ContentRegistry;
class MoveLoadout;
class Loadout;

// The pages A cycles, and the dots the header's pager draws — one number, so a page
// added to the roster can never leave the pager short.
inline constexpr int kStatPages = 6;

// Build the LOADOUT page's rows (ProseRow, core/ui/prose_page.h — the shared
// name+prose flow this page, BUFFS, and ROCK THE DOCK's opponent sheet all use): a
// MOVES section (the innate default move, tagged, followed by each equipped —
// unlocked-slot — move) then a MODS section (each equipped mod, or a single
// "- NONE -" row when nothing's equipped). EMPTY slots are skipped entirely —
// nothing to describe. `isEgg` collapses the whole page to a single "- NO LOADOUT -"
// row (MODS is inert for an egg, `Game::slotLocked`), since there is no combat
// loadout to show.
//
// Pure over its two loadouts, which is what lets the ARENA render a rolled opponent's
// kit through the identical builder the player reads their own kit through
// (game_tourney.cpp) — an opponent sheet that laid its own page out would be a second
// answer to "what does this move do".
std::vector<ProseRow> buildLoadoutRows(const ContentRegistry& reg,
                                       const MoveLoadout& moveLoad,
                                       const Loadout& modLoad,
                                       Stage stage, bool isEgg);

// How many rows starting at `top` fit the LOADOUT page — the flow's own count
// (proseRowsFitting) at this page's row top. Exported because the engine advances the
// B-scroll by exactly what is on screen.
int loadoutRowsFitting(const std::vector<ProseRow>& rows, int top);

// STAT page 1 — TIERS row model: the investment ladder, as the same flowed prose rows
// LOADOUT and BUFFS use (ProseRow). One `header` row per combat stat carrying that stat's
// point total, then one row per rung — its name, what it does (straight off the shared
// table, core/model/stat_tiers.h) and a tag saying where the player stands on it.
//
// The TAG is the whole point of the page and is deliberately the only channel that
// reports state: "HELD" on a rung already earned, "N TO GO" on the one being climbed,
// and the bare threshold on the rest. Text rather than colour, because the status pages
// have to stay readable in grayscale — and because "how far to the next one" is a NUMBER,
// which no amount of tinting can say.
//
// `statPoints` is TOTAL points per stat (Game::totalStatPoint — earned plus an Epic
// dish's off-level grant), the same count the fight resolves tiers from, so the page can
// never promise a rung the engine will not honour.
std::vector<ProseRow> buildTierRows(const int statPoints[kLevelStatCount]);

// The TIERS page's window, same flow and same reason as loadoutRowsFitting: sixteen rows
// of authored prose is several screens of it.
int tierRowsFitting(const std::vector<ProseRow>& rows, int top);

// STAT page 1 — TIERS: the windowed ladder from buildTierRows, scrolling on B like the
// two prose pages after it.
void drawTiersScreen(Framebuffer& fb, const std::vector<ProseRow>& rows, int scrollTop,
                     int beat);

// STAT page 2 — LOADOUT: the windowed row list from buildLoadoutRows, each row's
// name + its effect text wrapped WHOLE beneath it, a scrollbar + "B SCROLL" hint
// band when the list outruns one screen.
// `scrollTop` is engine-owned — this page has no cursor to drive scrolling off
// (it's read-only), so STAT's B press advances it instead (game_core.cpp).
void drawLoadoutScreen(Framebuffer& fb, const std::vector<ProseRow>& rows,
                       int scrollTop, int beat);

// STAT page 0 — VITALS: name + generation + stage indicator, the three vitals
// gauges (Hunger / Fragmentation / Happiness) with tabular numerics, the creature
// level + an XP bar toward the next level, the care-pip budget, and the
// time-to-next-evolution readout. `generation` reads in the name row.
// `beat` supplies the Critical-pulse phase. `level` is the per-pet creature level
// `combatXp`/`xpToNext` fill the XP bar (banked XP / next-level cost).
// `hasNextEvo` false = a terminus (shows MAX); `evoRemainMs` = ms to the next
// boundary (the hatch clock for an egg — labelled HATCH there, EVOLVE otherwise).
void drawStatScreen(Framebuffer& fb, const PetModel& m, const char* name,
                    Stage stage, int generation, int level, int combatXp,
                    int xpToNext, int beat, bool hasNextEvo, uint32_t evoRemainMs);

// STAT page 3 — BUFFS row model: one armed item buff, its effect text (rendered
// from the item's own ItemDef row — single source of truth, no duplicated
// description), and a countdown for the timed ones (Backup Drive).
struct BuffRow {
    const char* label;     // the item's displayName
    EffectText effect;     // the item's description, its magnitudes substituted in
    bool hasTimer;         // true only for a time-limited buff (Backup Drive)
    uint32_t remainingMs;  // valid only when hasTimer
    // A section heading (ARMED / PERMANENT) rather than a buff: no effect text, no
    // timer, and it opens a window of its own the way a ProseRow header does
    // (prose_page.h). The two kinds answer different questions — what is running out,
    // and what this pet simply has now — and the page is unreadable as one flat list
    // that mixes them.
    bool header = false;
};

// Build the BUFFS page's rows from the armed-buff state Game tracks as plain
// per-pet flags (game.h) — Restore Point's mistake shield, Ambig-USB's forced
// Trojan divert, Backup Drive's timed combat shield, and the two DeepWeb Dive
// depth buffs (Deep-Learning Module/Core's win multiplier, a Bell's armed start
// depth). Each armed flag becomes one row naming the ITEM that armed it —
// resolved by scanning the registry for the item whose effect matches the raw
// state, so the display and the item's own 'Pedia/detail text never drift
// apart even if the item roster is renamed or rebalanced later.
// branchOverride/evolveSoakFactor/evolveHoldArmed: the USB port — Game::evolveBranchOverride()
// (None = nothing forced), Game::evolveSoakFactor() (1 = no soak armed) and
// Game::evolveHoldArmed(). Each resolved to a row the same way the depth buffs are, by
// finding the item whose effect matches the state. The hold earns its row more than any
// of them: it is the reason STAT's own EVOLVE readout says MAX, and this page is where
// that gets explained.
// depthMultiplier: Game::deepWebDepthMultiplier_ (1 = none armed).
// startDepthArmed/startDepthUsesBest/startDepthValue: resolved from
// Game::pendingDeepWebStartDepth_ (armed = != -1; usesBest = the
// kDeepWebStartDepthUseBest sentinel, in which case startDepthValue is unused).
// upgrades: Game::petUpgrades() — the PERMANENT entries, one row per grant an Epic dish
// has made this pet (core/model/pet_upgrades.h). They arm nothing and never lapse, so
// they carry no timer and no home-screen icon (the idle status row is for states the
// operator has to act on); the BUFFS page is where a pet's standing upgrades are
// readable, which is the only place they belong.
std::vector<BuffRow> buildBuffRows(const ContentRegistry& reg,
                                    bool restorePointArmed,
                                    bool trojanDivertArmed,
                                    bool backupDriveArmed,
                                    uint32_t backupDriveRemainMs,
                                    int depthMultiplier,
                                    bool startDepthArmed,
                                    bool startDepthUsesBest,
                                    int startDepthValue,
                                    BranchOverride branchOverride,
                                    int evolveSoakFactor,
                                    bool evolveHoldArmed,
                                    const PetUpgrades& upgrades);

// The BUFFS page's own window, same flow and same reason as loadoutRowsFitting:
// five buffs can be armed at once and their descriptions run to four lines, which
// is more than one screen holds.
int buffRowsFitting(const std::vector<BuffRow>& rows, int top);

// STAT page 3 — BUFFS: the armed-buff list from buildBuffRows, each with its
// effect text wrapped WHOLE below the name (and a remaining-time readout for the
// timed ones), scrolling on B like LOADOUT when more are armed than fit. Empty
// list shows a plain "no active buffs" line.
void drawBuffsScreen(Framebuffer& fb, const std::vector<BuffRow>& rows, int scrollTop,
                     int beat);

// STAT page 4 — SPECIES: the pet's own lore, straight off its CreatureDef row
// (`hint`/`context`, defs.h) — the game owns this copy, same as an item's
// `effect` text. `line` is the raw line id (e.g. "ransomware"), upper-cased
// for display; `hint` is the snarky read, `context` the real infosec reference
// behind the pun, set dim a paragraph below it as an unlabelled footnote. Either
// may be null (no lore authored). Both are wrapped to as many lines as they need:
// `context` is measured first and held back, and `hint` takes the room that
// leaves, so a Daemon's long read is shown whole instead of stopping mid-sentence
// with the foot of the screen still empty.
void drawSpeciesScreen(Framebuffer& fb, const char* name, const char* line,
                       const char* hint, const char* context, int beat);

// STAT page 5 — AUDIT LOG: the rolling event history newest-first, each
// a type glyph + terse text. The lifetime/uptime footer lives on the Hacker
// PROFILE slot, so this page is the log alone and shows the whole ring.
void drawAuditLog(Framebuffer& fb, const EventLog& log, int beat);

// --- The INDEX -------------------------------------------------------------
//
// One row per destination in STAT, with the readout that destination is about beside
// it. It is the random-access half of the section: the A-cycle walks the six pages in
// their fixed order, which is a lap of the whole reader to reach the last of them, and
// on a levelled pet each flowed page is several screens deep on top of that.
//
// The readouts are the other half of the point. A row that says "TIERS 7/12" has
// answered the question that would otherwise cost the page open plus a scroll, so the
// index is a status screen the way the LOADOUT hub (mods_screen.h) is — you enter a
// page when the summary is not enough, not to find out whether it is.

// One index destination: a page, and the row of that page to open it AT.
//
// `anchor` is what makes a section reachable in one selection rather than in a page
// open plus n scroll steps: it is an index into the flowed page's own rows
// (buildTierRows / buildLoadoutRows), so choosing MAX HEALTH opens TIERS with MAX
// HEALTH's heading at the top of the window. 0 on the pages that do not flow rows.
// `sub` rows are the sections INSIDE a page (a stat's rungs, the MOVES half of a
// loadout) and draw indented under the row that names their page.
// `label` is borrowed — every producer names a string literal or a row label that
// outlives the frame — and `value` is formatted, so it is stored.
struct StatIndexRow {
    const char* label = "";
    char value[14] = {0};
    bool sub = false;
    int page = 0;
    int anchor = 0;
};

// Build the INDEX from what the pages themselves report, so the two can never
// disagree: the TIERS rows carry the rungs held, the LOADOUT rows carry the sections,
// and the counts beside MOVES/MODS are the ones the LOADOUT hub already counts
// (`movesOn`/`moveSlots`, `modsOn`/`modSlots`). `line` is the raw line id, upper-cased
// for the SPECIES row; `buffs` and `logEvents` are the two roster sizes.
//
// The sub-rows are derived from the header rows of the two flowed pages rather than
// listed here, which is what keeps an egg's collapsed LOADOUT (one "- NO LOADOUT -"
// row, no headers) from offering sections it does not have.
std::vector<StatIndexRow> buildStatIndexRows(const std::vector<ProseRow>& tierRows,
                                             const std::vector<ProseRow>& loadoutRows,
                                             int level, int movesOn, int moveSlots,
                                             int modsOn, int modSlots, int buffs,
                                             const char* line, int logEvents);

// The INDEX: `rows` from top to bottom, the focused one banded, each with its readout
// right-aligned. The roster has a ceiling (six pages, four stats, two loadout halves)
// and the pitch is set so all of it fits one screen — nothing here scrolls, which is
// the whole reason it is worth opening.
void drawStatIndex(Framebuffer& fb, const std::vector<StatIndexRow>& rows, int cursor,
                   int beat);

} // namespace mal
