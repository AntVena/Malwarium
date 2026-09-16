// game_story.cpp — WHEN a chapter fires, and the reader that shows it.
//
// The EXPL ladder is a sequence of places with nothing between them. A chapter is what
// goes between: the walk hits one of four milestones in a zone (arriving, opening the
// gauntlet, taking it, leaving), and the device stops and says what just happened.
// What a chapter IS lives in the content layer beside the area it is about
// (core/content/story.h); what it LOOKS like is core/ui/story_screen.h. This unit is
// the join — the read-set, the fire points, and the page's own clock.
//
// A FIRE POINT IS ONE LINE. fireStory answers false for a beat that is unauthored or
// already read, and the caller carries on as though no story existed; it answers true
// once it has taken the screen, and the caller returns. That is what lets a beat be
// added to a milestone without the milestone learning anything about stories — see
// startEncounter and startAreaBoss in game_explore.cpp.
//
// AND IT FIRES ONCE, PER DEVICE. The read-set is player-level, not per-pet: the journey
// is the operator's, and hatching a new egg is not grounds for being told the premise
// again. What a player who has walked past a chapter under auto-progress gets instead
// is the ARCHIVE — EXPL's CHAPTERS category, which lists everything the walk has
// already fired and nothing it has not.
#include "core/app/game.h"

#include <algorithm>
#include <cstdio>

#include "tunables.h"
#include "core/content/story.h"
#include "core/ui/prose_page.h"
#include "core/ui/story_screen.h"

namespace mal {

// --- The read-set ------------------------------------------------------------

bool Game::storyRead(const StoryChapterDef* chapter) const {
    if (!chapter || chapter->wire == kStoryWireNone) return false;
    const int w = chapter->wire;
    if (w >= kStoryWireCap) return false;    // a wire this build's set cannot hold
    return (storyRead_[w / 8] & (1u << (w % 8))) != 0;
}

void Game::markStoryRead(const StoryChapterDef* chapter) {
    if (!chapter || chapter->wire == kStoryWireNone) return;
    const int w = chapter->wire;
    if (w >= kStoryWireCap) return;
    storyRead_[w / 8] |= static_cast<uint8_t>(1u << (w % 8));
    markSaveDirty();
}

int Game::storyChapterCount() const {
    int n = 0;
    for (int i = 0, total = storyEntryCount(); i < total; ++i)
        if (storyRead(storyEntryAt(i).chapter)) ++n;
    return n;
}

// --- Firing ------------------------------------------------------------------

bool Game::fireStory(int sector, StoryBeat beat, StoryThen then, int thenArea) {
    const StoryChapterDef* c = storyChapter(sector, beat);
    if (!c || storyRead(c)) return false;
    storyNext_ = nullptr;
    storyThenArea_ = thenArea;
    openStoryChapter(c, then);
    return true;
}

bool Game::fireStoryPair(int sector, StoryBeat first, StoryBeat second,
                         StoryThen then) {
    // Two chapters, read back to back as one sitting. Either half may be unauthored or
    // already read, so the pair collapses to whichever halves are actually left — which
    // is what lets an area author only its departure, or a re-cleared gauntlet fire
    // nothing at all, without the caller testing for any of it.
    const StoryChapterDef* a = storyChapter(sector, first);
    const StoryChapterDef* b = storyChapter(sector, second);
    if (a && storyRead(a)) a = nullptr;
    if (b && storyRead(b)) b = nullptr;
    if (!a && !b) return false;
    storyThenArea_ = -1;
    if (!a) { a = b; b = nullptr; }
    storyNext_ = b;
    if (b) markStoryRead(b);        // queued, so it is spent the moment the pair opens
    openStoryChapter(a, then);
    return true;
}

void Game::openStoryChapter(const StoryChapterDef* chapter, StoryThen then) {
    if (!chapter) return;
    storyChapter_ = chapter;
    storyThen_ = then;
    storyScroll_ = 0;
    // Marked read on OPEN rather than on finish, and that is deliberate: a chapter the
    // player skipped on the first press has still had its turn, and re-firing it at the
    // next encounter would make skipping impossible. The archive is where a skipped
    // chapter is recovered from, which is exactly what it is for. A chapter opened FROM
    // the archive is already read and this is a no-op.
    markStoryRead(chapter);
    armStoryPanel();
    nav_ = Nav::Story;
    dirty_ = true;
}

void Game::armStoryPanel() { storyPanelDeadlineMs_ = nowMs_ + kStoryPanelMs; }

// --- The reader --------------------------------------------------------------

std::vector<ProseRow> Game::storyRows() const {
    std::vector<ProseRow> out;
    if (!storyChapter_) return out;
    out.reserve(static_cast<size_t>(storyChapter_->panelCount));
    for (int i = 0; i < storyChapter_->panelCount; ++i) {
        ProseRow r;
        r.label = storyChapter_->panels[i].heading;
        std::snprintf(r.body.buf, sizeof(r.body.buf), "%s",
                      storyChapter_->panels[i].text);
        out.push_back(r);
    }
    return out;
}

int Game::storyWindows() const {
    if (!storyChapter_) return 1;
    return std::max(1, proseWindowCount(storyRows(), kStoryBodyTop));
}

int Game::storyWindow() const {
    if (!storyChapter_) return 0;
    return proseWindowIndex(storyRows(), storyScroll_, kStoryBodyTop);
}

void Game::advanceStoryPanel() {
    // One window on, by exactly the rows that were shown — the step prose_page exports
    // proseRowsFitting for, so what the reader saw and what the next window starts at
    // can never be two different answers. Past the last window the chapter is over.
    if (!storyChapter_) { finishStoryChapter(); return; }
    const std::vector<ProseRow> rows = storyRows();
    const int total = static_cast<int>(rows.size());
    const int shown = proseRowsFitting(rows, storyScroll_, kStoryBodyTop);
    storyScroll_ += shown;
    if (storyScroll_ >= total) { finishStoryChapter(); return; }
    armStoryPanel();
    dirty_ = true;
}

void Game::finishStoryChapter() {
    // The queued second half of a paired beat first — it is the same sitting, so it
    // opens in place rather than handing back and being fired again.
    if (storyNext_) {
        const StoryChapterDef* next = storyNext_;
        storyNext_ = nullptr;
        storyChapter_ = next;
        storyScroll_ = 0;
        armStoryPanel();
        dirty_ = true;
        return;
    }
    storyChapter_ = nullptr;
    const StoryThen then = storyThen_;
    const int area = storyThenArea_;
    storyThen_ = StoryThen::Walk;
    storyThenArea_ = -1;
    dirty_ = true;
    switch (then) {
        // The thing the chapter stood in front of, now that it is done. Each of these
        // re-enters the call the fire point returned out of, and finds the chapter
        // already read this time, so nothing loops.
        case StoryThen::Encounter: startEncounter(); return;
        case StoryThen::AreaBoss:  startAreaBoss(area); return;
        case StoryThen::Archive:   openStoryArchive(); return;
        case StoryThen::Walk:      break;
    }
    returnToExplore();
}

void Game::onStory(const ButtonEvent& ev) {
    // B is the page key on every reader on this device, and here it is the only one
    // that moves forward: a tap turns the window, and on the last window it closes the
    // chapter. C SKIPS — the whole chapter, and anything queued behind it — because a
    // chapter is not a menu and "cancel" on a thing you are being told is "I am done
    // being told". A is deliberately inert: there is nothing on this page to step
    // between, and a key that does nothing is better than a second key that does what
    // B already does.
    if (ev.button == Button::B) {
        advanceStoryPanel();
    } else if (ev.button == Button::C) {
        storyNext_ = nullptr;
        storyChapter_ = nullptr;
        finishStoryChapter();
    }
}

void Game::drawStoryScreen(Framebuffer& fb) const {
    if (!storyChapter_) return;
    // Built ONCE per repaint and then asked all three questions — which window this is,
    // how many there are, and what to draw. The accessors above each build their own
    // (they are called from gates, one question at a time); a repaint asking them
    // separately would flow the same chapter three times a frame.
    const std::vector<ProseRow> rows = storyRows();
    StoryPageView v;
    v.title = storyChapter_->title;
    v.scrollTop = storyScroll_;
    v.window = proseWindowIndex(rows, storyScroll_, kStoryBodyTop);
    v.windows = std::max(1, proseWindowCount(rows, kStoryBodyTop));
    // How much of this window's budget is left, as the countdown rule's fill. Clamped
    // rather than allowed to go negative: the deadline can be a beat in the past
    // between the tick that passed it and the advance it causes.
    const uint32_t left = storyPanelDeadlineMs_ > nowMs_ ? storyPanelDeadlineMs_ - nowMs_
                                                         : 0;
    v.remainingPct = static_cast<int>(left * 100 / kStoryPanelMs);
    v.beat = beat_;
    drawStoryPage(fb, rows, v);
}

// --- The archive -------------------------------------------------------------

std::vector<StoryEntry> Game::storyArchiveEntries() const {
    // Journey order (storyEntryAt), filtered to what the walk has actually fired. The
    // filter is the whole reason this can be a menu at all: everything listed has been
    // read once already, so the archive can spoil nothing.
    std::vector<StoryEntry> out;
    for (int i = 0, total = storyEntryCount(); i < total; ++i) {
        const StoryEntry e = storyEntryAt(i);
        if (storyRead(e.chapter)) out.push_back(e);
    }
    return out;
}

void Game::openStoryArchive() {
    const int n = static_cast<int>(storyArchiveEntries().size());
    if (storyArchiveRow_ >= n) storyArchiveRow_ = 0;
    nav_ = Nav::StoryArchive;
    dirty_ = true;
}

void Game::onStoryArchive(const ButtonEvent& ev) {
    const std::vector<StoryEntry> rows = storyArchiveEntries();
    const int n = static_cast<int>(rows.size());
    if (ev.button == Button::A) {
        if (n > 0) storyArchiveRow_ = (storyArchiveRow_ + 1) % n;
    } else if (ev.button == Button::B) {
        if (storyArchiveRow_ >= 0 && storyArchiveRow_ < n) {
            storyNext_ = nullptr;
            storyThenArea_ = -1;
            openStoryChapter(rows[storyArchiveRow_].chapter, StoryThen::Archive);
        }
    } else if (ev.button == Button::C) {
        // Back to EXPL, parked on the CHAPTERS row the archive was opened from.
        nav_ = Nav::Submenu;
        explCat_ = ExplCat::None;
        explNavArea_ = -1;
        listRow_ = explCatRow(ExplCat::Chapters);
    }
    dirty_ = true;
}

void Game::drawStoryArchiveScreen(Framebuffer& fb) const {
    const std::vector<StoryEntry> entries = storyArchiveEntries();
    std::vector<StoryArchiveRow> rows;
    rows.reserve(entries.size());
    for (const StoryEntry& e : entries) {
        StoryArchiveRow r;
        r.title = e.chapter->title;
        r.zone = storyZoneBadge(e.sector);
        r.beat = storyBeatWord(e.beat);
        rows.push_back(r);
    }
    drawStoryArchive(fb, rows.data(), static_cast<int>(rows.size()), storyArchiveRow_,
                     beat_);
}

}  // namespace mal
