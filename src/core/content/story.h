// story.h — the CHAPTERS the walk reads out at the beats it reaches in a zone.
// Skippable and auto-advancing, so a chapter may motivate a mechanic, never explain one.
#pragma once

#include <cstdint>

namespace mal {

// `text` is capped at EffectText::kMaxProse; test_story_chapters_fit_their_page gates it.
struct StoryPanelDef {
    const char* heading;
    const char* text;
};

// AreaIntro: the zone's first wild encounter. AreaOutro: straight after BossOutro.
enum class StoryBeat : uint8_t { AreaIntro, BossIntro, BossOutro, AreaOutro };
constexpr int kStoryBeats = 4;

// Raising this widens a persisted bitset (save.h v65).
constexpr int kStoryWireCap = 128;
constexpr uint8_t kStoryWireNone = 0;   // wires are 1-based

struct StoryChapterDef {
    // Unique, never reused: the ladder can be spliced with no migration.
    uint8_t wire = kStoryWireNone;
    const char* title = nullptr;
    const StoryPanelDef* panels = nullptr;
    int panelCount = 0;

    bool authored() const { return title && panels && panelCount > 0; }
};

// A zeroed beat fires nothing, which is what lets the areas be written one at a time.
struct AreaStoryDef {
    StoryChapterDef chapters[kStoryBeats] = {};
};

// --- The archive index -------------------------------------------------------
//
// Addressed by SECTOR: the ladder is 0..kAreaCount-1 and the endless zones sit
// immediately past it (area_defs.h), so this takes the engine's own `exploreSector_`.

int storyZoneCount();
const AreaStoryDef* storyZone(int sector);
const StoryChapterDef* storyChapter(int sector, StoryBeat beat);

// Null `chapter` = unauthored. Every (zone, beat) pair has a slot, so positions hold.
struct StoryEntry {
    const StoryChapterDef* chapter = nullptr;
    int sector = -1;
    StoryBeat beat = StoryBeat::AreaIntro;
};

// Journey order: each zone's beats in StoryBeat order, zones in sector order.
int storyEntryCount();
StoryEntry storyEntryAt(int index);

const char* storyBeatWord(StoryBeat beat);
const char* storyZoneBadge(int sector);

}  // namespace mal
