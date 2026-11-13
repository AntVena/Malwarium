#include "core/app/game/story_reader.h"

#include <algorithm>
#include <cstdio>

#include "core/content/story.h"
#include "core/ui/prose_page.h"
#include "core/ui/story_screen.h"

namespace mal {

std::vector<ProseRow> StoryReader::rows(const StoryRun& run) {
    std::vector<ProseRow> out;
    if (!run.chapter) return out;
    out.reserve(static_cast<size_t>(run.chapter->panelCount));
    for (int i = 0; i < run.chapter->panelCount; ++i) {
        ProseRow r;
        r.label = run.chapter->panels[i].heading;
        std::snprintf(r.body.buf, sizeof(r.body.buf), "%s", run.chapter->panels[i].text);
        out.push_back(r);
    }
    return out;
}

int StoryReader::rowsShown(const StoryRun& run) {
    return proseRowsFitting(rows(run), run.scroll, kStoryBodyTop);
}

int StoryReader::window(const StoryRun& run) {
    if (!run.chapter) return 0;
    return proseWindowIndex(rows(run), run.scroll, kStoryBodyTop);
}

int StoryReader::windows(const StoryRun& run) {
    if (!run.chapter) return 1;
    return std::max(1, proseWindowCount(rows(run), kStoryBodyTop));
}

void StoryReader::onButton(StoryReaderHost& host, const ButtonEvent& ev) {
    // A is inert: a second key doing B's job is worse than a key doing nothing.
    if (ev.button == Button::B) host.turnStoryPage();
    else if (ev.button == Button::C) host.skipStory();
}

void StoryReader::draw(Framebuffer& fb, const StoryReaderHost& host) const {
    const StoryRun* run = host.storyRun();
    if (!run || !run->chapter) return;
    // Built once, since each of the window reads would build its own.
    const std::vector<ProseRow> r = rows(*run);
    StoryPageView v;
    v.title = run->chapter->title;
    v.scrollTop = run->scroll;
    v.window = proseWindowIndex(r, run->scroll, kStoryBodyTop);
    v.windows = std::max(1, proseWindowCount(r, kStoryBodyTop));
    // How much of this window's budget is left, as the countdown rule's fill. Clamped:
    // the deadline can be a beat in the past between the tick that passed it and the
    // turn it causes.
    const uint32_t now = host.uptimeMs();
    const uint32_t left = run->deadlineMs > now ? run->deadlineMs - now : 0;
    v.remainingPct = static_cast<int>(left * 100 / kStoryPanelMs);
    v.beat = host.screenBeat();
    drawStoryPage(fb, r, v);
}

}  // namespace mal
