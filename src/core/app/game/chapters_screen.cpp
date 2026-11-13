#include "core/app/game/chapters_screen.h"

#include "core/ui/layout.h"
#include "core/ui/story_screen.h"

namespace mal {

void ChaptersScreen::open(const ChaptersHost& host) {
    if (row_ >= static_cast<int>(host.storyArchiveEntries().size())) row_ = 0;
}

void ChaptersScreen::stepList(const ChaptersHost& host, int dir) {
    row_ = stepRow(row_, static_cast<int>(host.storyArchiveEntries().size()), dir);
}

void ChaptersScreen::onButton(ChaptersHost& host, const ButtonEvent& ev) {
    const std::vector<StoryEntry> rows = host.storyArchiveEntries();
    const int n = static_cast<int>(rows.size());
    if (ev.button == Button::A) {
        row_ = stepRow(row_, n, +1);
    } else if (ev.button == Button::B) {
        if (row_ >= 0 && row_ < n) host.rereadChapter(rows[row_].chapter);
    } else if (ev.button == Button::C) {
        host.leaveChapters();
    }
}

void ChaptersScreen::draw(Framebuffer& fb, const ChaptersHost& host) const {
    const std::vector<StoryEntry> entries = host.storyArchiveEntries();
    std::vector<StoryArchiveRow> rows;
    rows.reserve(entries.size());
    for (const StoryEntry& e : entries) {
        StoryArchiveRow r;
        r.title = e.chapter->title;
        r.zone = storyZoneBadge(e.sector);
        r.beat = storyBeatWord(e.beat);
        rows.push_back(r);
    }
    drawStoryArchive(fb, rows.data(), static_cast<int>(rows.size()), row_,
                     host.screenBeat());
}

}  // namespace mal
