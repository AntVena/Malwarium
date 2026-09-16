#include "core/content/story.h"

#include "core/content/areas/area_defs.h"
#include "core/content/areas/darkweb_crawl/area.h"
#include "core/content/areas/deepweb_dive/area.h"

namespace mal {

int storyZoneCount() { return kAreaCount + 2; }   // the ladder, the Dive, the Crawl

const AreaStoryDef* storyZone(int sector) {
    // The two endless zones first: their sector numbers sit one and two past the
    // ladder, so testing them before the range check is what keeps the range check a
    // plain "is this a rung".
    if (sector == kDeepWebSector) return &kStoryDeepWeb;
    if (sector == kDarkWebSector) return &kStoryDarkWeb;
    if (sector < 0 || sector >= kAreaCount) return nullptr;
    return &area(sector).story;
}

const StoryChapterDef* storyChapter(int sector, StoryBeat beat) {
    const AreaStoryDef* z = storyZone(sector);
    if (!z) return nullptr;
    const int i = static_cast<int>(beat);
    if (i < 0 || i >= kStoryBeats) return nullptr;
    const StoryChapterDef& c = z->chapters[i];
    return c.authored() ? &c : nullptr;
}

int storyEntryCount() { return storyZoneCount() * kStoryBeats; }

StoryEntry storyEntryAt(int index) {
    if (index < 0 || index >= storyEntryCount()) return {};
    StoryEntry e;
    e.sector = index / kStoryBeats;
    e.beat = static_cast<StoryBeat>(index % kStoryBeats);
    e.chapter = storyChapter(e.sector, e.beat);
    return e;
}

const char* storyBeatWord(StoryBeat beat) {
    switch (beat) {
        case StoryBeat::AreaIntro: return "ARRIVAL";
        case StoryBeat::BossIntro: return "GAUNTLET";
        case StoryBeat::BossOutro: return "CLEARED";
        case StoryBeat::AreaOutro: return "DEPARTURE";
    }
    return "";
}

const char* storyZoneBadge(int sector) {
    if (sector == kDeepWebSector) return kDeepWebBadge;
    if (sector == kDarkWebSector) return kDarkWebBadge;
    if (sector < 0 || sector >= kAreaCount) return "";
    // The same fallback AreaDef::badge documents: a row that authored no short name
    // reads as its display name whole, which fits only by luck and is measured by the
    // same gate that measures every other badge.
    const AreaDef& a = area(sector);
    return a.badge ? a.badge : a.name;
}

}  // namespace mal
