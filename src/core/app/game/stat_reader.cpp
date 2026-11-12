#include "core/app/game/stat_reader.h"

#include <cstring>

#include "core/model/pet.h"
#include "core/ui/stat_pages.h"
#include "core/ui/stat_screen.h"

namespace mal {

const std::vector<const StatPage*>& StatReader::pages() {
    static const std::vector<const StatPage*> kPages = {
        &kStatVitalsPage, &kStatBondPage,  &kStatTiersPage,   &kStatLoadoutPage, &kStatMovesPage,
        &kStatFoodsPage,  &kStatBuffsPage, &kStatSpeciesPage, &kStatLogPage,
    };
    return kPages;
}

const StatPage* StatReader::findPage(const char* title) {
    for (const StatPage* p : pages())
        if (std::strcmp(p->title(), title) == 0) return p;
    return nullptr;
}

StatPager StatReader::pager() const {
    const std::vector<const StatPage*>& all = pages();
    StatPager p;
    p.count = static_cast<int>(all.size());
    for (int i = 0; i < p.count; ++i)
        if (all[i] == page_) p.index = i;
    return p;
}

void StatReader::reset() {
    page_ = pages().front();
    scroll_ = 0;
    screen_ = StatScreen::Page;
    indexRow_ = 0;
}

void StatReader::onButton(StatReaderHost& host, const ButtonEvent& ev) {
    if (screen_ == StatScreen::Index) onIndex(host.statSources(), ev);
    else onPage(host, ev);
}

void StatReader::onPage(StatReaderHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        const std::vector<const StatPage*>& all = pages();
        const StatPager at = pager();
        page_ = all[(at.index + 1) % at.count];
        scroll_ = 0;   // a fresh page opens at its top
    } else if (ev.button == Button::C) {
        host.leaveToCarousel();
        reset();
    } else if (ev.button == Button::B) {
        // B is a tap/hold pair here (the ITEMS filter's shape): the hold opens the INDEX,
        // so the window advance waits for the RELEASE and runs only if the hold never
        // landed (releaseB).
        host.armHoldB();
    }
}

void StatReader::releaseB(StatReaderHost& host) {
    if (screen_ != StatScreen::Page) return;
    const StatSources s = host.statSources();
    if (!s.pet.creature) return;
    const int next = page_->nextScroll(s, scroll_);
    if (next == scroll_) return;
    scroll_ = next;
    host.markDirty();
}

void StatReader::openIndex(const StatSources& s) {
    screen_ = StatScreen::Index;
    // Park the cursor on the last row that opens this page at or before the window the
    // reader is in.
    const std::vector<StatIndexRow> rows = indexRows(s);
    indexRow_ = 0;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        if (rows[i].page == page_ && rows[i].anchor <= scroll_) indexRow_ = i;
}

void StatReader::stepIndex(const StatSources& s, int dir) {
    const int n = static_cast<int>(indexRows(s).size());
    if (n > 0) indexRow_ = ((indexRow_ + dir) % n + n) % n;
}

void StatReader::onIndex(const StatSources& s, const ButtonEvent& ev) {
    // A's REPEAT and C's backward walk are the list contract's (game_listnav.cpp); the
    // tap of each stays this screen's own, as in every list handler: the contract
    // decides how a cursor moves and the screen decides when.
    if (ev.button == Button::A) {
        stepIndex(s, +1);
    } else if (ev.button == Button::B) {
        const std::vector<StatIndexRow> rows = indexRows(s);
        if (indexRow_ >= 0 && indexRow_ < static_cast<int>(rows.size())) {
            const StatIndexRow& r = rows[indexRow_];
            page_ = r.page;
            // The anchor is a ROW of the page being opened, which is what makes a section
            // reachable in one selection: the window the page draws starts there
            // (drawProseRows clamps it), so MODS opens ON the MODS heading.
            scroll_ = r.anchor;
        }
        screen_ = StatScreen::Page;
    } else if (ev.button == Button::C) {
        screen_ = StatScreen::Page;
    }
}

std::vector<StatIndexRow> StatReader::indexRows(const StatSources& s) const {
    std::vector<StatIndexRow> rows;
    for (const StatPage* p : pages()) p->indexRows(s, rows);
    return rows;
}

void StatReader::draw(Framebuffer& fb, const StatSources& s, int beat) const {
    if (!s.pet.creature) return;
    if (screen_ == StatScreen::Index) drawStatIndex(fb, indexRows(s), indexRow_, beat);
    else page_->draw(fb, s, pager(), scroll_, beat);
}

}  // namespace mal
