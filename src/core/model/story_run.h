// story_run.h: one sitting of the STORY reader, as a run.
//
// A chapter fires in front of something (the encounter that triggered it, the gauntlet
// it introduces), so the run carries what it interrupted and hands back to it when the
// chapter runs out. It also carries the second chapter of a paired beat, read in the
// same sitting, and its own page clock: a window turns itself over once kStoryPanelMs
// passes unread, so a hands-off walk is never stopped dead by a page. How many rows a
// window shows is the reader screen's layout, so a turn is told it.
#pragma once

#include <cstdint>

#include "tunables.h"

namespace mal {

struct StoryChapterDef;

// What the reader hands back to when the sitting ends.
//   Walk:      the idle habitat, explore-mode still running. What every outro hands
//              back to, since the thing it followed is already over.
//   Encounter: the wild encounter whose first-in-this-zone roll opened the chapter.
//   AreaBoss:  the area gauntlet the chapter was the threshold of (thenArea).
//   Archive:   opened from the CHAPTERS list; back to the list.
enum class StoryThen : uint8_t { Walk, Encounter, AreaBoss, Archive };

struct StoryRun {
    const StoryChapterDef* chapter = nullptr;
    const StoryChapterDef* next = nullptr;   // the queued second half of a paired beat
    // Which window is on screen, as the prose row it opens on: the reader steps by the
    // rows it showed, as STAT's pages do (core/ui/prose_page.h).
    int scroll = 0;
    uint32_t deadlineMs = 0;   // when this window turns itself over
    StoryThen then = StoryThen::Walk;
    int thenArea = -1;         // StoryThen::AreaBoss's gauntlet

    static StoryRun opened(const StoryChapterDef* chapter, const StoryChapterDef* next,
                           StoryThen then, int thenArea, uint32_t nowMs) {
        StoryRun r;
        r.chapter = chapter;
        r.next = next;
        r.then = then;
        r.thenArea = thenArea;
        r.armPanel(nowMs);
        return r;
    }
    // Every press re-arms the clock, so it only ever overtakes a page nobody is reading.
    void armPanel(uint32_t nowMs) { deadlineMs = nowMs + kStoryPanelMs; }
    // Past the `shown` rows of the window up, out of `total`. False once the chapter is
    // over.
    bool turn(int shown, int total, uint32_t nowMs) {
        scroll += shown;
        if (scroll >= total) return false;
        armPanel(nowMs);
        return true;
    }
    // The chapter is over: open the queued one in place, the same sitting. False with
    // none queued.
    bool openNext(uint32_t nowMs) {
        if (!next) return false;
        chapter = next;
        next = nullptr;
        scroll = 0;
        armPanel(nowMs);
        return true;
    }
};

}  // namespace mal
