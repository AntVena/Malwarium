// game_story.cpp: the read-set, the fire points, and where a sitting hands back to.
//
// fireStory returns false (carry on) or true (it took the screen, return), so a
// milestone gains a beat without knowing stories exist. The read-set is player-level: a
// new egg is not retold the premise.
#include "core/app/game.h"

#include <algorithm>
#include <cstdio>

#include "tunables.h"
#include "core/content/story.h"
#include "core/ui/prose_page.h"   // ProseRow

namespace mal {

// --- The read-set ------------------------------------------------------------

bool Game::storyRead(const StoryChapterDef* chapter) const {
    if (!chapter || chapter->wire == kStoryWireNone) return false;
    const int w = chapter->wire;
    if (w >= kStoryWireCap) return false;    // a wire this build's set cannot hold
    return (op_.reveals.storyRead[w / 8] & (1u << (w % 8))) != 0;
}

void Game::markStoryRead(const StoryChapterDef* chapter) {
    if (!chapter || chapter->wire == kStoryWireNone) return;
    const int w = chapter->wire;
    if (w >= kStoryWireCap) return;
    op_.reveals.storyRead[w / 8] |= static_cast<uint8_t>(1u << (w % 8));
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
    openStoryChapter(c, nullptr, then, thenArea);
    return true;
}

bool Game::fireStoryPair(int sector, StoryBeat first, StoryBeat second,
                         StoryThen then) {
    const StoryChapterDef* a = storyChapter(sector, first);
    const StoryChapterDef* b = storyChapter(sector, second);
    if (a && storyRead(a)) a = nullptr;
    if (b && storyRead(b)) b = nullptr;
    if (!a && !b) return false;
    if (!a) { a = b; b = nullptr; }
    if (b) markStoryRead(b);        // queued, so it is spent the moment the pair opens
    openStoryChapter(a, b, then, -1);
    return true;
}

void Game::openStoryChapter(const StoryChapterDef* chapter, const StoryChapterDef* next,
                            StoryThen then, int thenArea) {
    if (!chapter) return;
    story_ = StoryRun::opened(chapter, next, then, thenArea, nowMs_);
    // Read on OPEN: re-firing a skipped chapter would make skipping impossible.
    markStoryRead(chapter);
    nav_ = Nav::Story;
    dirty_ = true;
}

// --- The sitting ---------------------------------------------------------------

std::vector<ProseRow> Game::storyRows() const {
    return story_ ? StoryReader::rows(*story_) : std::vector<ProseRow>{};
}

int Game::storyWindows() const { return story_ ? StoryReader::windows(*story_) : 1; }

int Game::storyWindow() const { return story_ ? StoryReader::window(*story_) : 0; }

void Game::turnStoryPage() {
    // One window on, by exactly the rows that were shown, so the next window starts
    // right after what the reader saw. Past the last window the chapter is over.
    if (!story_ || !story_->chapter) { finishStoryChapter(); return; }
    const int total = static_cast<int>(StoryReader::rows(*story_).size());
    if (!story_->turn(StoryReader::rowsShown(*story_), total, nowMs_)) {
        finishStoryChapter();
        return;
    }
    dirty_ = true;
}

void Game::skipStory() {
    if (story_) story_->next = nullptr;
    finishStoryChapter();
}

void Game::finishStoryChapter() {
    dirty_ = true;
    // The queued second half of a paired beat first: it is the same sitting, so it
    // opens in place.
    if (story_ && story_->openNext(nowMs_)) return;
    const StoryThen then = story_ ? story_->then : StoryThen::Walk;
    const int area = story_ ? story_->thenArea : -1;
    story_.reset();
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
    chapters_.open(*this);
    nav_ = Nav::StoryArchive;
    dirty_ = true;
}

void Game::rereadChapter(const StoryChapterDef* chapter) {
    openStoryChapter(chapter, nullptr, StoryThen::Archive, -1);
}

void Game::leaveChapters() {
    // Back to EXPL, parked on the CHAPTERS row the archive was opened from.
    nav_ = Nav::Submenu;
    explCat_ = ExplCat::None;
    explNavArea_ = -1;
    explRow_ = explCatRow(ExplCat::Chapters);
}

}  // namespace mal
