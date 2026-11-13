// story_reader.h: the STORY reader, the thin screen over a StoryRun.
//
// The run (core/model/story_run.h) holds the sitting: the chapter, the one queued behind
// it, the window up, the page clock and what to hand back to. The reader holds nothing
// of its own. It lays the chapter out in windows, draws the one up, and turns B and C
// into a page turn or a skip, which the game carries out because the end of a sitting
// re-enters the walk, the encounter or the boss.
//
// This header names no core/ui type beyond the prose row it returns, so game.h can hold
// a StoryReader by value.
#pragma once

#include <cstdint>
#include <vector>

#include "core/device/platform.h"
#include "core/model/story_run.h"

namespace mal {

class Framebuffer;
struct ProseRow;

// What the reader needs from the game. Game implements it; a test can too.
class StoryReaderHost {
public:
    virtual const StoryRun* storyRun() const = 0;   // null with no sitting open
    virtual uint32_t uptimeMs() const = 0;          // the page clock's reading
    virtual int screenBeat() const = 0;
    virtual void turnStoryPage() = 0;
    virtual void skipStory() = 0;                   // the sitting over, queue and all

protected:
    ~StoryReaderHost() = default;
};

class StoryReader {
public:
    void onButton(StoryReaderHost& host, const ButtonEvent& ev);
    void draw(Framebuffer& fb, const StoryReaderHost& host) const;

    // The chapter's panels as prose rows, and how the reader's layout windows them.
    static std::vector<ProseRow> rows(const StoryRun& run);
    static int rowsShown(const StoryRun& run);   // in the window up now
    static int window(const StoryRun& run);
    static int windows(const StoryRun& run);
};

}  // namespace mal
