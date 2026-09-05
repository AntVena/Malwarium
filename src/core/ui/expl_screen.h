// expl_screen.h — EXPL submenu + the wild-encounter intro / event screens it
// launches. The list is the NESTED area/sub-area ladder: a linear
// ladder of AREAS (Citrus Circuit → The Pirate Bayou), each holding 5 numbered
// SUB-AREAS. Area gating is LINEAR complete-to-advance: area N
// unlocks once area N-1's 5-stage gauntlet is cleared (`sectorCleared[]`); area 0 is
// always open. Within an open area every sub-area is reachable — you arm one (→
// explore-mode on the idle habitat) and clear it via a 10-win streak → its
// boss; clearing all 5 unlocks the AREA boss. Hacker Rank gates nothing (reward-only,
// ). Selecting a row hands off to explore-mode or the shared combat core.
#pragma once

#include "core/content/areas/area_defs.h"

namespace mal {

class ContentRegistry;
class Framebuffer;
struct SpriteData;

// Aliases of the canonical area-ladder constants (area_defs.h) — kAreaCount there
// is derived from the area-folder list itself, so this is the only place the
// ladder's size is named; adding an area (see that header) grows both without a
// second count to keep in sync. kExplSubAreas is the per-area sub-area count (5,
// the named stretches now the unit of progress rather than flavour).
constexpr int kExplSectors = kAreaCount;
constexpr int kExplSubAreas = kSubAreasPerArea;

// Nested EXPL list --------------------------------------------
// The list is a fixed sequence of rows: for each area, one AREA header row followed
// by its kExplSubAreas numbered SUB-AREA rows. Row `row` decodes to an (area, sub)
// pair via the helpers below (sub == -1 → the area header / area-boss row). The
// count is `explRowCount()`. State/selectability is derived from three durable flag
// blocks the caller owns:
//   areaCleared      — [kExplSectors]                    (area's 5-stage gauntlet beaten)
//   subCleared       — [kExplSectors * kExplSubAreas]    (sub-area's boss beaten)
//   subBossUnlocked  — [kExplSectors * kExplSubAreas]    (10-win streak unlocked its boss)
// Any block may be null (treated as all-false). Row-major index = area*kExplSubAreas+sub.
constexpr int kExplRowsPerArea = 1 + kExplSubAreas;      // header + 5 sub rows
// The ladder is BRACKETED by two SPECIAL rows, neither of them an AreaDef: the DEEPWEB
// DIVE above it (which draws as kDeepWebSector, area_defs.h — one past the real ladder,
// so it stays the terminal zone even as the ladder grows) and ROCK THE DOCK below it, the
// operator bracket held in The Pirate Bayou (content_tournament.h). Each is a single
// row with no sub-areas, and while locked each is an inert "??????" teaser — which is
// what makes both read as a promise rather than as rows that appear from nowhere.
//
// The POSITIONS say what each is for. DeepWeb leads because it is the most-rewarding
// farming zone once unlocked, so an entry with nothing else to resume parks the cursor
// on it (Game::openExplList) — the fewest presses to the best grind. The arena trails
// for the mirror-image reason: it pays nothing until a whole bracket is taken, so it
// must never be what an operator lands on by default when they came to go exploring.
constexpr int kExplLeadRows = 1;   // the DeepWeb Dive, above the ladder
constexpr int kExplTailRows = 1;   // ROCK THE DOCK, below it
inline int explRowCount() {
    return kExplLeadRows + kExplSectors * kExplRowsPerArea + kExplTailRows;
}
inline bool explRowIsDeepWeb(int row) { return row == 0; }
inline bool explRowIsTourney(int row) { return row == explRowCount() - 1; }
inline bool explRowIsSpecial(int row) {
    return explRowIsDeepWeb(row) || explRowIsTourney(row);
}
// Area/sub decode is offset by the leading row. Only meaningful for a LADDER row
// (callers gate on explRowIsSpecial first); a special row returns a harmless sentinel.
inline int explRowArea(int row) { return (row - kExplLeadRows) / kExplRowsPerArea; }
// -1 for a header row, else the sub-area index (0..kExplSubAreas-1).
inline int explRowSub(int row) {
    return ((row - kExplLeadRows) % kExplRowsPerArea) - 1;
}

// The rendered/navigable state of a row. The WORD carries the meaning
// (dual-coded, grayscale-safe); colour is only emphasis. Selectable states are the
// ones A-cycle lands on and B acts upon (explRowSelectable).
enum class ExplRowState {
    AreaLocked,     // area's predecessor not cleared → "??????" + LOCKED
    AreaProgress,   // area open, boss not yet reachable → "n/5" cleared-sub count
    AreaBossReady,  // all 5 sub-areas cleared, area not → "> AREA BOSS" (selectable)
    AreaCleared,    // area's gauntlet beaten → CLEARED
    SubLocked,      // its AREA is locked → "??????" + LOCKED (open areas' subs never lock)
    SubOpen,        // armable, not yet won → OPEN (selectable → arm explore)
    SubExploring,   // the armed/running sub, boss not yet unlocked → EXPLORING (selectable)
    SubBossReady,   // 10-win streak unlocked its boss → "> FIGHT BOSS" (selectable;
                    // takes priority over EXPLORING so the action stays reachable)
    SubCleared,     // its boss beaten → CLEARED, but still RE-FARMABLE:
                    // selectable to re-arm explore for full XP+Bits + diminishing loot,
                    // so a done area stays a training ground (esp. for a fresh pet).
    DeepWebLocked,  // DeepWeb Dive, not all areas cleared → "??????" + LOCKED
    DeepWebOpen,    // every area cleared → "> DIVE" (selectable → arm the endless dive)
    DeepWebDiving,  // the endless dive is armed → DIVING (selectable → re-arm)
    TourneyLocked,  // the arena, its area not reached yet → "??????" + LOCKED
    TourneyOpen,    // no run in progress → "> ENTER" (selectable → draw a fresh bracket)
    TourneyRunning, // a bracket is part-fought → IN PLAY (selectable → resume it)
};
// `tourneyRunning` is the one piece of state not derivable from the flag blocks: The
// arena's UNLOCK is just "is its area open" (explSectorOpen over `areaCleared`), but
// whether a bracket is half-fought lives in the run, not the ladder.
ExplRowState explRowState(int row, const bool* areaCleared, const bool* subCleared,
                          const bool* subBossUnlocked, int exploringSector,
                          int exploringSub, bool tourneyRunning = false);
// A-cycle lands / B acts only on selectable rows (skips headers-that-aren't-boss and
// locked rows). Pure function of the state.
bool explRowSelectable(ExplRowState s);

// THE LEVEL IS THE LIST: which rows are drawn at nav level `navArea` (-1 = top).
// The top level is a ZONE PICKER — the DeepWeb row plus one row per area, and none of
// the sub-areas; inside area N it is N's own block — the area-boss row plus N's
// sub-areas. So the drawn list is 1+kExplSectors rows or 1+kExplSubAreas rows, never
// the whole explRowCount() ladder, and the screen a player reads is the level they are
// navigating. `explRowLandable` (Game) is the further "does the cursor stop here"
// filter on top of this — every landable row is in-level, but not the reverse (a
// locked area is shown as "??????" and skipped by the cursor).
inline bool explRowInLevel(int row, int navArea) {
    if (explRowIsSpecial(row)) return navArea < 0;
    if (navArea < 0) return explRowSub(row) < 0;      // area headers only
    return explRowArea(row) == navArea;               // that area's header + subs
}

// Everything drawExplList renders. A view struct rather than a parameter list because
// the screen reads a dozen unrelated facts and a positional call of that length is
// unreadable at both ends (same reason as ShopRowView, below).
struct ExplListView {
    int cursor = 0;                  // focused row, 0..explRowCount()-1
    // The durable flag blocks — see the contract above. Any may be null (all-false).
    const bool* areaCleared = nullptr;
    const bool* subCleared = nullptr;
    const bool* subBossUnlocked = nullptr;
    int exploringSector = -1;        // the armed/running sub-area (or -1) → EXPLORING
    int exploringSub = -1;
    int navArea = -1;                // the nav level — see explRowInLevel above
    // The armed sub-area's win streak and the count that unlocks its boss, so the
    // frontier row answers "how close am I" where the choice is made, instead of only
    // on the habitat badge (drawExploreBadge).
    int streakWins = 0;
    int winsToBoss = 0;
    int bestDeepWebDepth = 0;        // the DeepWeb row's own progress readout (0 = none)
    // The arena row's own progress readout: how many entrants are still standing in the
    // run in progress (0 = no run), and which round it is on. Same job bestDeepWebDepth
    // does for the dive — the frontier row answers "where am I up to" where the choice
    // is made, rather than only once the screen is open.
    bool tourneyRunning = false;
    int tourneyAlive = 0;
    int tourneyRound = 0;
    // `beat` drives one thing: a focused ZONE title (an area, the area-boss row, the
    // DeepWeb row) pulses, so a heading-shaped row still reads as the armed selection.
    int beat = 0;
};

// L2 nested area/sub-area list, drawn one LEVEL at a time (explRowInLevel). Areas
// carry their own sector glyph (ICON_SECTOR_<AREA_ID>, resolved through `reg` — a
// missing one draws as an empty frame rather than shifting the row).
void drawExplList(Framebuffer& fb, const ContentRegistry& reg, const ExplListView& v);

// Explore-mode idle badge: a thin status line under the top track, drawn
// over the idle habitat while explore-mode is active — a pulsing cursor +
// "EXPL <label>" (the armed sub-area) and, right-anchored, the field of whichever
// mode the walk is in. Dual-coded (glyph + text + fraction), grayscale-safe.
// The badge's right-field state, dual-coded (a distinct WORD per mode so it reads in
// grayscale): WINS n/N (progress to the sub-boss) · BOSS READY · NEXT IN n (wins left
// before auto-progress steps off this rung) · FARM n/N | FARM LOW (a re-armed cleared
// sub — Bandwidth remaining) · DEPTH n (the DeepWeb Dive win count).
//
// AutoNext is what a CLEARED sub-area reads while auto-progress is armed, and it is the
// one mode that answers a question about the FUTURE: a re-farm has no boss left to
// unlock, so the streak is spent entirely on when the walk moves on, and the number the
// operator wants is how many wins are between them and the next rung. The Bandwidth pool
// FARM would show in that slot is already spelled out on the status line under the badge
// (Game::drawHabitat), so this costs nothing and stops the same number being drawn twice.
enum class ExploreBadgeMode { Wins, BossReady, AutoNext, Farming, DeepDive };
// `count`/`countMax` feed the numeric modes: Wins → "WINS count/countMax"; AutoNext →
// countMax-count wins remain ("NEXT IN n", or "NEXT NOW" at zero); DeepDive → "DEPTH
// count"; Farming → count is Bandwidth left / countMax the pool. BossReady ignores them.
void drawExploreBadge(Framebuffer& fb, const char* label, int count, int countMax,
                      ExploreBadgeMode mode);

// Explore-control overlay: the walk's own action list, opened by the A+C chord over the
// habitat. A CURSOR list, exactly like combat's Exploit picker — the chord opens it and
// then plain A/B/C drive it (A next row, B do it, C back out). The chord itself is never
// a navigation key inside a screen it opened: that is not what the Exploit chord is for,
// and A+C is awkward to hit besides, since A registers on its own first.
enum class ExploreControlRow { Ping, Warp, AutoProgress, Stop };
constexpr int kExploreControlRows = 4;

// `hasWarpKey` dims the WARP row (B on it is inert with no key held); `autoProgress` is
// the mode row's ON/OFF state, dual-coded by the word. `xpEfficiencyPct` is a FOOTER
// stat under the rows, not a row — what a wild win on the armed rung actually pays as a
// percentage of the flat base (Game::exploreXpEfficiencyPct). It belongs on this screen
// because this is where the two decisions it informs are made: whether to leave a rung
// the pet has outgrown, and whether to let auto-progress keep walking it.
void drawExploreControl(Framebuffer& fb, int cursor, bool hasWarpKey,
                        bool autoProgress, int xpEfficiencyPct);

// Sector accessors ("difficulty-scaled by sector tier") — sector identity
// is data owned by this file; Game reads it through these rather than duplicating
// the roster. `explSectorOpen` implements the linear gate: sector 0 is always open,
// sector N>0 opens once `sectorCleared[N-1]` is set (a null array = only sector 0).
// `explSubAreaName` returns sector `sector`'s sub-area `idx` (0..kExplSubAreas-1).
bool explSectorOpen(int idx, const bool* sectorCleared);
int explSectorTier(int idx);
const char* explSectorName(int idx);
// The area's SHORT name for the walk badge (AreaDef::badge) — the display name whole when
// a row names none, which the badge-fit gate then measures like any other.
const char* explSectorBadge(int idx);
const char* explSubAreaName(int sector, int idx);

// Zone-completion Title: clearing sector `idx`'s boss/gauntlet grants
// this equippable, player-level Title (Citrus Circuit → "CERTIFIED DOWNLOADER",
// The Pirate Bayou → "LEECH LORD"). One Title per sector — the unlock/equip state
// lives in Game (persisted); the name string is sector-owned data. Out-of-range
// returns "".
const char* sectorTitle(int idx);

// Encounter intro: a wild malbeast + the Fight/Flee/Sinkhole choice.
// `sinkholeAvailable` shows the Sinkhole row only when a Sinkhole Trap is held.
// `level` is the depth level — always set for a wild encounter (this
// screen never fires for Sim dummies/bosses), so it's shown as a plain number.
void drawEncounterIntro(Framebuffer& fb, const char* enemyName, int diffPips,
                        int level, const SpriteData* enemySprite,
                        bool sinkholeAvailable, int choice, int beat);

// How much of the network glyph the pet gets through on a Wi-Fi event — the picture of
// the discovery beat's reward, matched to Game::NetDiscovery. Eaten whole for a network
// it has never seen, nibbled for one it is merely fond of, left standing for its own
// home turf, and absent entirely when the radio queued nothing to find.
enum class WifiAbsorb { None, Whole, Nibble, Untouched };

// Wi-Fi network event: a self-contained typed event that resolves in
// place — the "NEW WI-FI NETWORK" banner + the rolled sub-outcome's flavor line +
// a B-continue hint band. `discoveryLine` (may be empty) is the independent
// real-network-discovery beat (Game::resolveNetworkDiscovery) drawn above the
// banner — a second, unrelated flavor line on the same screen.
//
// The pet stands on this screen the way it does on the walk it was interrupted from,
// and takes the network in: `absorb` picks how far the ICON_SYS_WIFI glyph dissolves
// (FX_ABSORB, core/render/absorb.h) over kAbsorbBeats of `beat`. The sweep is a second
// channel for what `discoveryLine` already says in words, so a grayscale screen still
// reads — and an untouched glyph is as much a statement as an eaten one.
// `beat` is the heartbeat (the marquee's own pace); `fxBeat` is the faster dissolve
// clock (Game::fxBeat_) the absorb is paced on.
// `banner` is what the radio actually FOUND, in words — the caller derives it from the
// same discovery state `absorb` is derived from (Game::drawWifiScreen), so the headline,
// the sweep and the two lines below can never disagree about whether anything was found.
void drawWifiEvent(Framebuffer& fb, const char* sectorName, const char* banner,
                   const char* outcomeLine, const char* discoveryLine,
                   const SpriteData* pet, WifiAbsorb absorb, int beat, int fxBeat);

// Storefront name accessors: each sector has an item shop AND a mod shop.
// `shopName` is the item storefront's banner ("BYTE TO EAT" / "PIER-TO-PEER" /
// "MOOR-TO-MOOR"); `modShopName` is the mod storefront's ("CHIP SHOP" /
// "PLUNDER PORT" / "MOOR TO MODS"). Both fall back to sector 0's storefront for an
// out-of-range sector. A storefront's actual LISTINGS (arbitrary-length, priced in
// Bits + item costs) are resolved through Game's shopListing* accessors, not here —
// those need the ContentRegistry to turn an id into a display name.
const char* shopName(int sector);
const char* modShopName(int sector);

// One rendered shop row for drawShop — resolved display strings/counts so the
// render layer stays registry-agnostic (Game resolves ids -> names via
// shopListingName/shopListingDescription/etc, game_render.cpp's drawShopScreen).
struct ShopRowView {
    const char* name = "";
    int bitsPrice = 0;
    int stock = 0;                                    // remaining THIS visit
    int costCount = 0;                                 // 0..kMaxShopCostItems
    const char* costName[kMaxShopCostItems] = {};
    int costQty[kMaxShopCostItems] = {};
    // What the SHOPPER already has of this row's goods, and the ceiling on it
    // (0 = uncapped). A mod row is the un-equipped spare pool against MOD STORAGE
    // (Game::shopListingHeld / shopListingHeldCap), drawn as the "HAVE n/cap" the mod
    // detail panel uses (mods_screen.h) — and a full pool is what the STORAGE FULL buy
    // reason names, so the row carries the number that reason is derived from. An
    // uncapped row (an item stack) draws no HAVE column at all: with no ceiling to
    // measure against it would only cost the name the width it reads in.
    int held = 0;
    int heldCap = 0;
};

// The tightest description budget on any screen: a storefront squeezes the selected
// row's own prose between the last listing and the buy reason. This is the WORST case
// — a storefront stocking the full kShopMaxRows; a one- or two-listing shop's adaptive
// list leaves several times as much. Public so a content row stocked by an area shop
// (content/areas/<id>/area.cpp) can be held to the worst case — anything longer is
// silently truncated when a shop happens to be full.
constexpr int kShopDescLines = 3;

// Shop event: a storefront explore event (item shop OR mod shop — same chrome) —
// the storefront banner, an arbitrary-length list of `rows` (windowed + scrollable
// past kShopVisibleRows, mirroring the Rig Shop's list, game_rig_shop.h), the
// player's wallet, the selected row's description, and a hint band. Dual-coded
// (counts + text) — grayscale-safe. `statusLine` states why a buy is (un)available
// for the SELECTED (`cursor`) row — A NEXT / B BUY / C LEAVE. No new art; reuses the
// event chrome.
void drawShop(Framebuffer& fb, const char* storeName, int walletBits,
              const ShopRowView* rows, int rowCount, int cursor,
              const char* selectedDescription, const char* statusLine, int beat);

// Warp-key picker: the walk's B overlay listing the held warp keys
// (Access Token / Safe-Mode Key), one row each, cursor-highlighted, with an
// A-next / B-use / C-back hint band. Using one jumps to its target event. Dual-coded
// (the row list + spelled-out hint) — grayscale-safe. No new art; reuses list chrome.
void drawWarpPicker(Framebuffer& fb, const char* const* keyNames, int keyCount,
                    int cursor);

// Post-encounter status readout: after a WILD (non-boss,
// non-Sim) fight resolves win or loss, a brief ~2s overlay shows the persistent
// BANDWIDTH spend + FRAGMENTATION change it just caused, before returning to the
// habitat — so the player knows whether to keep exploring or go defrag. The
// fragmentation half is tri-state, dual-coded by WORD (never colour alone):
//   None     — frag wasn't in play this encounter (row omitted entirely).
// Shielded — 's Bandwidth shield covered the tax ("FRAG SHIELDED").
//   Rose     — the tax landed ("FRAG +n", `fragDelta` is that positive n).
enum class PostEncounterFragState { None, Shielded, Rose };
// `bwBefore`/`bwAfter` are this fight's Bandwidth before/after (bwMax = the cap);
// bwBefore==0 reads as "BANDWIDTH x/max  LOW" (nothing was left to spend) instead
// of a numeric delta, since 0 -> 0 would otherwise look like "nothing happened".
// `levelLine` (nullptr = no level-up this fight) names the stat gained from a
// level-up (e.g. "LVL 5  POWER +1") — the caller pre-formats it (it owns the stat
// labels); a gain is drawn in the CALM zone, dual-coded by the WORD + signed number.
void drawPostEncounter(Framebuffer& fb, int bwBefore, int bwAfter, int bwMax,
                       PostEncounterFragState fragState, int fragDelta,
                       const char* levelLine = nullptr);

} // namespace mal
