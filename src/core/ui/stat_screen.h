// stat_screen.h (STAT): the pet's status pages' draws and row models, and the INDEX
// that jumps between the pages. The pages themselves are objects (stat_pages.h), and
// StatReader (core/app/game/stat_reader.h) pages through them in this order:
//   VITALS    the pet: gauges + care + level/XP + time-to-evolve (the landing)
//   BOND      the pet's Bond: its XP bonus and a bar per source (PetUpgrades::bondBp)
//   TIERS     the investment ladder: each combat stat's points, the rungs it
//                     has reached and what the next one costs
//   LOADOUT     the pet's equipped moves + mods, WITH their effect text
//   MOVES       every move this pet could learn, and which it has (collect_screen.h)
//   FOODS       every dish in the game, and which this pet has eaten (ditto)
//   BUFFS       currently-armed item buffs (Restore Point/Ambig-USB/Backup
//                     Drive), each with its effect text and remaining time if timed
//   SPECIES     the pet's own lore: line, one-line snarky hint, infosec ref
//   AUDIT LOG the rolling event history
// A cycles the pages, B scrolls the flowed ones, HOLD B opens the INDEX and C backs
// out. Each page dual-codes (grayscale-legible).
// STAT is pet-only. The device/account stats (Hacker Rank, Bits, lifetime breadth)
// live on the Hacker PROFILE slot (game_hacker.cpp), not here.
#pragma once

#include <cstdint>
#include <vector>

#include "core/content/defs.h"
#include "core/model/pet_model.h"
#include "core/ui/prose_page.h"
#include "core/ui/stat_pages.h"

namespace mal {

class Framebuffer;
class ContentRegistry;
class MoveLoadout;
class ModSlots;

// The band every STAT page ends with. Two facts belong on it and they are not the same
// kind: what the key under the reader's thumb does right now, and that the INDEX exists
// at all. The second is why the band is drawn even on a page with nothing to scroll: a
// hold gesture nothing names is a gesture nobody finds.
//
// `window`/`windows` are the page's own window count and which one is open, 1-based for
// the reader. Exported because STAT's pages are drawn from two units (this one and
// collect_screen.h) and a reader stepping between them must not find the key legend
// changing shape.
void drawStatHintBand(Framebuffer& fb, int window, int windows);

// Build the LOADOUT page's rows (ProseRow, core/ui/prose_page.h. The shared
// name+prose flow this page, BUFFS, and ROCK THE DOCK's opponent sheet all use): a
// MOVES section (the innate default move, tagged, followed by each equipped;
// unlocked-slot. Move) then a MODS section (each equipped mod, or a single
// "- NONE -" row when nothing's equipped). EMPTY slots are skipped entirely:
// nothing to describe. `isEgg` collapses the whole page to a single "- NO LOADOUT -"
// row (MODS is inert for an egg, `Game::slotLocked`), since there is no combat
// loadout to show.
//
// Pure over its two loadouts, which is what lets the ARENA render a rolled opponent's
// kit through the identical builder the player reads their own kit through
// (game_tourney.cpp): an opponent sheet that laid its own page out would be a second
// answer to "what does this move do".
std::vector<ProseRow> buildLoadoutRows(const ContentRegistry& reg,
                                       const MoveLoadout& moveLoad,
                                       const ModSlots& modLoad,
                                       Stage stage, bool isEgg);

// STAT TIERS row model: the investment ladder, as the same flowed prose rows
// LOADOUT and BUFFS use (ProseRow). One `header` row per combat stat carrying that stat's
// point total, then one row per rung: its name, what it does (straight off the shared
// table, core/model/stat_tiers.h) and a tag saying where the player stands on it.
//
// The TAG is the whole point of the page and is deliberately the only channel that
// reports state: "HELD" on a rung already earned, "N TO GO" on the one being climbed,
// and the bare threshold on the rest. Text rather than colour, because the status pages
// have to stay readable in grayscale, and because "how far to the next one" is a NUMBER,
// which no amount of tinting can say.
//
// `statPoints` is TOTAL points per stat (Game::totalStatPoint: earned plus an Epic
// dish's off-level grant), the same count the fight resolves tiers from, so the page can
// never promise a rung the engine will not honour.
std::vector<ProseRow> buildTierRows(const int statPoints[kLevelStatCount]);

// STAT VITALS: name + generation + stage indicator, the three vitals
// gauges (Hunger / Fragmentation / Happiness) with tabular numerics, the creature
// level + an XP bar toward the next level, the care-pip budget, and the
// time-to-next-evolution readout. The vitals and care pips read from the PetModel.
struct StatScreenView {
    const char* name = "";
    Stage stage = Stage::BootSector;
    int generation = 0;
    int level = 0;                // the per-pet creature level
    int combatXp = 0;             // banked XP, filling the bar toward...
    int xpToNext = 0;             // ...the next level's cost
    // False at a terminus (reads MAX). Otherwise `evoRemainMs` is the time to the next
    // boundary: the hatch clock for an egg (labelled HATCH there, EVOLVE otherwise).
    bool hasNextEvo = false;
    uint32_t evoRemainMs = 0;
    int beat = 0;                 // the Critical-pulse phase
    StatPager pager;
};
void drawStatScreen(Framebuffer& fb, const PetModel& m, const StatScreenView& v);

// Rows sit under two headings, ARMED and PERMANENT, because they answer different
// questions (what is running out, and what this pet simply has now). The timed buff
// carries its countdown as the row's suffix.
//
// Build the BUFFS page's rows from what is armed on `pet` (PetGrants, PetArmed, UsbPatch)
// and its standing upgrades. Each armed effect becomes one row naming the ITEM that armed
// it, resolved by scanning the registry for the item whose effect matches the raw state,
// so the display and the item's own 'Pedia/detail text never drift apart even if the
// item roster is renamed or rebalanced later. The USB hold earns its row more than any of
// them: it is the reason STAT's own EVOLVE readout says MAX, and this page is where that
// gets explained. `backupRemainMs` is the Backup Drive's time left (0 = not armed), which
// the pet alone cannot say because its deadline is on the device's uptime clock.
//
// The PERMANENT entries are one row per grant an Epic dish has made this pet
// (core/model/pet_upgrades.h). They arm nothing and never lapse, so they carry no timer
// and no home-screen icon (the idle status row is for states the operator has to act
// on); the BUFFS page is where a pet's standing upgrades are readable, which is the only
// place they belong.
std::vector<ProseRow> buildBuffRows(const ContentRegistry& reg, const Pet& pet,
                                    uint32_t backupRemainMs);

// --- The INDEX -------------------------------------------------------------
//
// One row per destination in STAT, with the readout that destination is about beside
// it. It is the random-access half of the section: the A-cycle walks the six pages in
// their fixed order, which is a lap of the whole reader to reach the last of them, and
// on a levelled pet each flowed page is several screens deep on top of that.
//
// The readouts are the other half of the point. A row that says "TIERS 7/12" has
// answered the question that would otherwise cost the page open plus a scroll, so the
// index is a status screen the way the LOADOUT hub (mods_screen.h) is: you enter a
// page when the summary is not enough, not to find out whether it is.

// The INDEX: `rows` from top to bottom, the focused one banded, each with its readout
// right-aligned. The roster has a ceiling (six pages, four stats, two loadout halves)
// and the pitch is set so all of it fits one screen: nothing here scrolls, which is
// the whole reason it is worth opening.
void drawStatIndex(Framebuffer& fb, const std::vector<StatIndexRow>& rows, int cursor,
                   int beat);

} // namespace mal
