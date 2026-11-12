// stat_pages.h (STAT): the reader's pages, each an object that draws itself, steps its
// own scroll window and lists its own INDEX rows.
//
// The pages read the pet through `const Pet&` and the registry for names. What is not the
// pet's (the audit log, the level curve, the evolution and Backup Drive clocks) arrives in
// StatSources from the game. The reader that pages through them (StatReader,
// core/app/game/stat_reader.h) holds the one ordered table of them, so a page never knows
// its own position: the reader hands it a StatPager to draw the header's dots from.
//
// The pages are defined beside their draws: VITALS, BOND, TIERS, LOADOUT, BUFFS, SPECIES
// and AUDIT LOG in stat_screen.cpp, MOVES and FOODS in collect_screen.cpp.
#pragma once

#include <cstdint>
#include <vector>

namespace mal {

class ContentRegistry;
class EventLog;
class Framebuffer;
struct Pet;
class StatPage;

// Everything a STAT page reads.
struct StatSources {
    const Pet& pet;
    const ContentRegistry& reg;
    const EventLog& log;
    int xpToNext = 0;              // Game::xpToNextLevel
    // False at a terminus. Otherwise the time to the next boundary (the hatch clock for
    // an egg).
    bool hasNextEvo = false;
    uint32_t evoRemainMs = 0;
    uint32_t backupRemainMs = 0;   // the Backup Drive's time left; 0 = not armed
};

// Which page of how many is open, for the header's dot pager.
struct StatPager {
    int index = 0;
    int count = 0;
};

// One INDEX destination: a page, and the row of that page to open it AT.
//
// `anchor` is what makes a section reachable in one selection rather than in a page open
// plus n scroll steps: it is an index into the flowed page's own rows, so choosing MAX
// HEALTH opens TIERS with MAX HEALTH's heading at the top of the window. 0 on a page's own
// row. `sub` rows are the sections INSIDE a page (a stat's rungs, the MOVES half of a
// loadout) and draw indented under the row that names their page.
// `label` is borrowed (a page title or a row label that outlives the frame), and `value`
// is formatted, so it is stored.
struct StatIndexRow {
    const char* label = "";
    char value[14] = {0};
    bool sub = false;
    const StatPage* page = nullptr;
    int anchor = 0;
};

// A page's own INDEX row, and a row for a section inside it, with a printf readout.
StatIndexRow pageIndexRow(const StatPage& page, const char* fmt, ...);
StatIndexRow sectionIndexRow(const StatPage& page, const char* label, int anchor,
                             const char* fmt, ...);

class StatPage {
public:
    // The page's name: its INDEX row's label, and how dump_frame and the tests find it.
    virtual const char* title() const = 0;
    virtual void draw(Framebuffer& fb, const StatSources& s, StatPager pager, int scrollTop,
                      int beat) const = 0;
    // The row B's window advance opens on, from the window drawn at `scrollTop`: the
    // same fit the draw used, wrapping to 0 past the end. A page that does not scroll
    // stays where it is.
    virtual int nextScroll(const StatSources& s, int scrollTop) const;
    // Append this page's INDEX rows: its own, then one per section it anchors. Counted
    // off the rows the page draws, so the index and the page report the same numbers.
    virtual void indexRows(const StatSources& s, std::vector<StatIndexRow>& out) const = 0;

protected:
    ~StatPage() = default;
};

extern const StatPage& kStatVitalsPage;
extern const StatPage& kStatBondPage;
extern const StatPage& kStatTiersPage;
extern const StatPage& kStatLoadoutPage;
extern const StatPage& kStatMovesPage;
extern const StatPage& kStatFoodsPage;
extern const StatPage& kStatBuffsPage;
extern const StatPage& kStatSpeciesPage;
extern const StatPage& kStatLogPage;

}  // namespace mal
