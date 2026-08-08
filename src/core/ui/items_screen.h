// items_screen.h — ITEMS submenu: grouped inventory list (L2) + item
// detail (L3). The list is built from the ContentRegistry against the live
// Inventory, so it stays data-driven (no hardcoded item table here).
#pragma once

#include <vector>

#include "core/content/defs.h"
#include "core/ui/ui_state.h"  // ItemFilter

namespace mal {

class Framebuffer;
class ContentRegistry;
class Inventory;
struct SpriteData;

// One rendered row: a non-selectable section header, or an item with its count.
struct InvRow {
    bool header;
    const char* label;        // header text (FOOD/BUFFS/QUEST) or item display name
    const ItemDef* def;       // null on a header row
    int qty;
    const SpriteData* icon;   // ICON_ITEM_* (null on a header row)
};

// Two Rig Shop rows read ItemFilter (ui_state.h) on two different axes:
//   * ITEMS Type-Tabs (hold-A on the list) walks the TYPE axis, ALL/FOOD/BUFFS/QUEST.
//   * ITEMS Type-Picker (the L2 tile screen) walks the finer CATEGORY axis,
//     ALL/FOOD/BUFFS/KEYS/TOOLS — QUEST split into the keys you spend and the
//     tools you carry (ItemDef::Category). Owning it upgrades hold-A to that axis
//     too, so the picker and the gesture never disagree about what a tab means.
// All = the full grouped list, and what every player without an upgrade sees.

// Short chip word for the active filter (ALL/FOOD/BUFFS/QUEST/KEYS/TOOLS) — shown
// in the ITEMS header once a filter upgrade is owned, and on the picker's tiles.
const char* itemFilterLabel(ItemFilter f);
// The hold-A cycle order. `categoryAxis` (the type-picker is owned) walks
// ALL -> FOOD -> BUFFS -> KEYS -> TOOLS -> ALL; otherwise ALL -> FOOD -> BUFFS ->
// QUEST -> ALL. Either way an off-axis filter lands back on ALL.
ItemFilter nextItemFilter(ItemFilter f, bool categoryAxis = false);

// Resolve an item's inventory icon (ICON_ITEM_<UPPER ID>) via the registry.
const SpriteData* itemIcon(const ContentRegistry& reg, const char* id);

// True if Use of this item resolves a Lockout (food, or the two active quest
// items). Shared by the list sort and the engine's Use handler.
bool itemResolvesLockout(const ItemDef& d);

// Build the grouped, sorted inventory: only stacks with qty>0, ordered
// FOOD -> BUFFS -> QUEST with a header row per group, and RAREST FIRST inside each
// group (Epic -> Common, alphabetical between items of equal rarity).
// `lockoutSort` floats Lockout-resolving items to the very top.
// `filter` (d) narrows the set to one type first; All = no narrowing.
std::vector<InvRow> buildInventoryRows(const ContentRegistry& reg,
                                       const Inventory& inv,
                                       bool lockoutSort = false,
                                       ItemFilter filter = ItemFilter::All);

// First selectable (non-header) row index, or -1 if the list is empty.
int firstSelectableRow(const std::vector<InvRow>& rows);
// Step from `cur` by `dir` (+1/-1), skipping headers and wrapping.
int stepSelectableRow(const std::vector<InvRow>& rows, int cur, int dir);

// --- ITEMS type-picker (the Rig Shop's "ITEMS Type-Picker" upgrade) ----------
// The L2 tile screen the ITEMS submenu opens on once the upgrade is owned: pick a
// category, then B drills into that filtered list (C on the list walks back here).

// One picker tile: a filter, its chip word, and how many units of it the bag holds
// right now. `units` == 0 draws dim — the tile stays selectable and openable, so the
// screen's shape never changes under the player.
struct ItemPickRow {
    ItemFilter filter;
    const char* label;
    int units;
};

// The picker's fixed tile order: ALL, FOOD, BUFFS, KEYS, TOOLS. There is no CACHES
// tile by construction — buildInventoryRows never lists a Sealed Cache (they decrypt
// in the Hacker VAULT), so no tile could hold one. Counts come from
// buildInventoryRows itself, so a tile can never disagree with the list behind it.
std::vector<ItemPickRow> buildItemPickerRows(const ContentRegistry& reg,
                                             const Inventory& inv);

// L2 type-picker. `cursor` indexes `tiles` (every tile is selectable).
void drawItemTypePicker(Framebuffer& fb, const std::vector<ItemPickRow>& tiles,
                        int cursor);

// L2 inventory list. `cursor` is a row index into `rows` (must be selectable).
// Each item row carries its rarity twice: the icon takes the rarity tint, and a
// four-cell ladder beside the count lights one cell per tier — so the ordering's
// "best first" claim is checkable on the row, in grayscale.
// `tabsOwned`/`pickerOwned` are the two Rig Shop filter rows: EITHER puts the
// active-tab chip in the header (you always know which category you're looking at),
// and each contributes its own word to the bottom hint band — "HOLD A - FILTER" for
// the gesture, "C - TYPES" for the walk back to the tiles. With neither owned the
// list is pixel-identical to a rig that has bought no filter upgrade at all.
void drawItemsList(Framebuffer& fb, const std::vector<InvRow>& rows, int cursor,
                   bool lockout, int beat, ItemFilter filter = ItemFilter::All,
                   bool tabsOwned = false, bool pickerOwned = false);

// L3 item detail. `usable` gates the Use action; when false `gateMsg` states
// where the item applies instead.
// Lines the ITEMS detail panel holds — the name, the readout grid and the prose all
// share this band. Published because the content tests hold every item's readout +
// description to it, and a copy of the number in the test is a copy that goes stale
// the first time the band or the line height moves.
int itemDetailPanelLines();

void drawItemDetail(Framebuffer& fb, const ItemDef& def, const SpriteData* icon,
                    int qty, bool usable, const char* gateMsg, int beat);

// Cache-yield reveal: what a just-opened Sealed Cache produced. `cache`
// names the container (+ its rarity, for the tag colour); `bits` is the Bits paid;
// `items`/`count` are the 0–2 item defs drawn (with their icons for the row). All
// dual-coded (names + counts + rarity word), so it reads in grayscale.
void drawCacheYield(Framebuffer& fb, const ItemDef& cache, int bits,
                    const ItemDef* const* items, const SpriteData* const* icons,
                    int count);

// Bulk cache-open aggregate tally row (e): one distinct item id + how many
// were drawn across every cache opened in a single bulk action.
struct BulkYieldRow {
    const ItemDef* def;
    const SpriteData* icon;
    int count;
};

// Bulk cache-open reveal (e, the Hacker VAULT hold-B "open all"): `cache`
// names the rarity group that was opened (its rarity tag colours the header),
// `cachesOpened`/`bits` are the totals, and `rows`/`count` are the aggregated
// per-item tally. `cursor` is windowed exactly like drawItemsList/drawMovePicker
// (kVisibleRows-style, scrollbar past the window) so an arbitrarily long tally
// never overruns the screen. Dual-coded, grayscale-safe.
void drawBulkYield(Framebuffer& fb, const ItemDef& cache, int cachesOpened, int bits,
                   const BulkYieldRow* rows, int count, int cursor);

// Rollback stat picker: shed one earned combat-stat point (−1 level) to
// re-roll it. `points` are the 4 earned counts (power/defense/speed/max-Health);
// `cursor` is the focused row. Rows with 0 points read "can't reduce" and are
// ineligible (greyed) — the cursor only ever lands on an eligible row. Dual-coded
// (cursor + `+n` / `can't reduce` text), grayscale-safe.
void drawRollbackPicker(Framebuffer& fb, const int points[4], int cursor);

} // namespace mal
