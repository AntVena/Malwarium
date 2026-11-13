// chapters_screen.h: EXPL's CHAPTERS archive, as a feature that owns its own state.
//
// Every STORY chapter the walk has already fired, in journey order, for reading again.
// The screen holds its cursor; which chapters have been read, and opening one in the
// reader, are the game's (game_story.cpp), reached through ChaptersHost.
#pragma once

#include <vector>

#include "core/content/story.h"   // StoryEntry
#include "core/device/platform.h"

namespace mal {

class Framebuffer;

// What CHAPTERS needs from the game. Game implements it; a test can too.
class ChaptersHost {
public:
    virtual int screenBeat() const = 0;
    // Journey order, filtered to what the walk has fired, so the archive spoils nothing.
    virtual std::vector<StoryEntry> storyArchiveEntries() const = 0;

    virtual void rereadChapter(const StoryChapterDef* chapter) = 0;
    virtual void leaveChapters() = 0;   // back to EXPL, on the CHAPTERS row

protected:
    ~ChaptersHost() = default;
};

class ChaptersScreen {
public:
    // Opening keeps the cursor where the last visit left it, if that row still exists.
    void open(const ChaptersHost& host);
    void onButton(ChaptersHost& host, const ButtonEvent& ev);
    void stepList(const ChaptersHost& host, int dir);
    void draw(Framebuffer& fb, const ChaptersHost& host) const;

private:
    int row_ = 0;
};

}  // namespace mal
