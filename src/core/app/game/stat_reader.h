// stat_reader.h: the pet face's STAT slot, as a screen that owns its own state.
//
// STAT is READ-ONLY, which is what shapes its whole button contract. Nothing here is
// selected or committed, so the three keys are spent entirely on getting to the part of
// the pet you asked about: A cycles the pages, B advances the flowed ones a window at a
// time, HOLD B opens the INDEX, and C leaves.
//
// STAT opens on its first page (VITALS) and never on the INDEX. Hunger, frag and the
// evolve clock are what the slot is entered for, several times a session; a menu in front
// of them would charge every one of those visits a press to pay for the occasional long
// jump. The index replaces that walk: one hold, from any page, landing on any page or on
// any SECTION of one (StatIndexRow::anchor, core/ui/stat_pages.h).
//
// The reader holds the one ordered table of pages (pages()): its order is the A order and
// the INDEX order, and its length is the pager's dot count. The pages read the game
// through StatSources; the reader reaches it through StatReaderHost.
#pragma once

#include <cstdint>
#include <vector>

#include "core/device/platform.h"

namespace mal {

class Framebuffer;
class StatPage;
struct StatIndexRow;
struct StatPager;
struct StatSources;

// Which STAT screen is open: one of the pages, or the INDEX that jumps between them.
enum class StatScreen : uint8_t { Page, Index };

// What STAT needs from the game. Game implements it; a test can too.
class StatReaderHost {
public:
    virtual StatSources statSources() const = 0;
    virtual void leaveToCarousel() = 0;   // C on a page
    virtual void armHoldB() = 0;          // B pressed; the release or the hold decides
    virtual void markDirty() = 0;         // repaint

protected:
    ~StatReaderHost() = default;
};

class StatReader {
public:
    // The pages, in A order.
    static const std::vector<const StatPage*>& pages();
    // The page called `title`, or nullptr.
    static const StatPage* findPage(const char* title);

    // Back to the front door: the first page, unscrolled, no index open. Every route
    // that puts the player somewhere else (entering the slot, the tree collapsing, a
    // Lockout resolving out from under the screen) goes through this, so one function
    // decides where STAT opens.
    void reset();
    void onButton(StatReaderHost& host, const ButtonEvent& ev);
    // B released on a page before the hold landed: advance the page's window.
    void releaseB(StatReaderHost& host);
    // B held on a page past kStatIndexHoldMs: open the INDEX with the cursor on the page
    // and window being read, so it opens as a map of where the reader IS and backing
    // straight out is a no-op.
    void openIndex(const StatSources& s);
    // Step the INDEX cursor by `dir`, wrapping. The A tap, the A hold-repeat and the C
    // backward walk (game_listnav.cpp) all land here.
    void stepIndex(const StatSources& s, int dir);
    void draw(Framebuffer& fb, const StatSources& s, int beat) const;

    // The INDEX's rows: each page's own, in table order.
    std::vector<StatIndexRow> indexRows(const StatSources& s) const;

    const StatPage& page() const { return *page_; }
    // Which row of the page the window opens on.
    int scroll() const { return scroll_; }
    StatScreen screen() const { return screen_; }
    int indexRow() const { return indexRow_; }
    StatPager pager() const;

private:
    void onPage(StatReaderHost& host, const ButtonEvent& ev);
    void onIndex(const StatSources& s, const ButtonEvent& ev);

    const StatPage* page_ = pages().front();
    // Row-window scroll offset for the open page. One offset serves every page because a
    // page change resets it.
    int scroll_ = 0;
    StatScreen screen_ = StatScreen::Page;
    int indexRow_ = 0;
};

}  // namespace mal
