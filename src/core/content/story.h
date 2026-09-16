// story.h — the JOURNEY, as chapters the device reads out at the beats that earn them.
//
// The EXPL ladder is a sequence of places with nothing between them: a pet clears
// CITRUS CIRCUIT's gauntlet and the next row simply stops saying "??????". What is
// missing is the only thing a ladder cannot state on its own — WHY the walk is
// happening, what the operator has just learned, and what the next water is going to
// be like. That is what a chapter is for, and it is why chapters fire on the ladder's
// own milestones rather than sitting in a menu waiting to be read.
//
// A CHAPTER IS A RUN OF PANELS, and a PANEL IS A SCREEN. The panels flow through
// core/ui/prose_page.h exactly as STAT's LOADOUT page and ROCK THE DOCK's briefing do,
// so a chapter needs no layout of its own and a reader who can read one page of this
// device can read all of them: a heading, its prose underneath, and B for the next
// screenful. What the flow decides is where the breaks fall — an author writes panels,
// not pixels.
//
// WHY A WIRE NUMBER. Which chapters have been read is persisted (save.h), so each one
// needs a stable id that survives the table being reordered or an area being spliced
// into the middle of the ladder — the same argument AchievementDef::wire and
// QuoteDef::wire make for themselves. Wires are assigned per chapter, never reused, and
// a native gate fails a duplicate rather than letting two chapters share a bit.
//
// WHERE A CHAPTER LIVES. In the folder of the place it is about
// (src/core/content/areas/<id>/area.cpp), hung off that area's own AreaDef — the same
// rule its bosses, its shop and its guardian follow. Nothing about one area's identity
// is split across another file, and that includes what it MEANS.
//
// Consumers: core/app/game/game_story.cpp (which chapter fires when, and the read-set),
// core/ui/story_screen.h (the reader and the archive).
#pragma once

#include <cstdint>

namespace mal {

// One PANEL: a heading and the prose under it. The pair is the shape every flowed page
// on this device is built from (prose_page.h's ProseRow), so a panel is authored as a
// paragraph with a name and never as a screen with a layout.
//
// `text` is held to EffectText::kMaxProse like every other authored string here, and
// the native gate fails a panel that reaches the cap rather than letting its tail
// disappear.
struct StoryPanelDef {
    const char* heading;
    const char* text;
};

// WHEN a chapter fires. Four beats per zone, in the order the walk reaches them:
//
//   AreaIntro  — the FIRST wild encounter in this zone. The pet has just arrived and
//                the malwarium has just started beeping; this is the chapter that says
//                what it is looking at.
//   BossIntro  — the area GAUNTLET, opened and not yet fought. The confrontation named.
//   BossOutro  — that gauntlet won. The fight's own aftermath.
//   AreaOutro  — and the place closing behind the pet, read STRAIGHT AFTER BossOutro as
//                the second half of the same sitting. Two chapters rather than one
//                because they are two different subjects: what was just beaten, and
//                what the operator is taking away from this water.
//
// A zone may leave any of the four unauthored (see StoryChapterDef::authored) — an
// empty beat simply never fires, which is what lets the ladder's areas be written one
// at a time without a half-finished one interrupting a walk.
enum class StoryBeat : uint8_t { AreaIntro, BossIntro, BossOutro, AreaOutro };
constexpr int kStoryBeats = 4;

// The ceiling on a wire number, and so on how many chapters the save's read-set can
// carry. Sized for the whole ladder several times over (the shipped set is
// kStoryZoneCount * kStoryBeats), because raising it later widens a persisted bitset —
// cheap here, a save note there. 16 bytes on the wire.
constexpr int kStoryWireCap = 128;
// The value a chapter that has not been given a wire holds. Wires are 1-based so that
// "no wire" and "the first chapter" are not the same number; bit 0 of the read-set is
// simply never used.
constexpr uint8_t kStoryWireNone = 0;

// One chapter — a titled run of panels, fired once at its beat and readable forever
// after from the archive.
struct StoryChapterDef {
    // Stable, unique, never reused — the bit this chapter occupies in the read-set.
    uint8_t wire = kStoryWireNone;
    // What the archive lists it as and what the reader's own header carries, e.g.
    // "CHAPTER 1: DAY ZERO". Held to the archive row's width by a native gate.
    const char* title = nullptr;
    const StoryPanelDef* panels = nullptr;
    int panelCount = 0;

    // Is there anything here to read? The one test every consumer asks, so an
    // unauthored beat is a zeroed row and never a special case spelled out per caller.
    bool authored() const { return title && panels && panelCount > 0; }
};

// One zone's four chapters, indexed by StoryBeat. A plain array rather than four named
// fields so a beat is looked up by its enum and adding one is a constant and a row,
// not a fifth accessor.
struct AreaStoryDef {
    StoryChapterDef chapters[kStoryBeats] = {};
};

// --- The archive index -------------------------------------------------------
//
// Every chapter in the game, in journey order, addressed by the SECTOR it belongs to.
// Sector numbering is already contiguous over everything that has a story — the
// ladder's rungs are 0..kAreaCount-1 and the two endless zones are kDeepWebSector and
// kDarkWebSector, one and two past the end (area_defs.h) — so the zone axis needs no
// mapping table of its own and the engine can hand this the same `exploreSector_` it
// walks with.

// How many zones carry a story block: the ladder, plus the Dive and the Crawl.
int storyZoneCount();

// `sector`'s story block, or nullptr if that sector has none (an out-of-range index, or
// a zone that is not a place you walk). Bounds-checked rather than clamped: handing
// back area 0's chapters for a bad index would be a silent wrong answer, where nullptr
// is a visible one.
const AreaStoryDef* storyZone(int sector);

// The chapter at `sector`'s `beat`, or nullptr when that beat is unauthored. The one
// lookup the fire points and the archive share, so a chapter can never be reachable
// from the menu and unreachable on the walk.
const StoryChapterDef* storyChapter(int sector, StoryBeat beat);

// One slot of the flat archive index: a chapter with the zone and beat it belongs to,
// so a browser can label a row without a second lookup. `chapter` is nullptr for an
// unauthored slot — the index covers every (zone, beat) PAIR rather than only the
// written ones, which is what keeps a slot's position stable as the areas are filled
// in one at a time.
struct StoryEntry {
    const StoryChapterDef* chapter = nullptr;
    int sector = -1;
    StoryBeat beat = StoryBeat::AreaIntro;
};

// The index's length, and the slot at `index`. Journey order: each zone's four beats in
// StoryBeat order, zones in sector order, so reading the index start to finish is
// reading the game in the order it is played.
int storyEntryCount();
StoryEntry storyEntryAt(int index);

// What a beat is CALLED where a chapter has to say which of the four it is — the
// archive's dim second line, under the chapter title. A word rather than a number
// because the archive is read out of order by definition, and "GAUNTLET" places a
// chapter in its zone's arc where "beat 2" only places it in an enum.
const char* storyBeatWord(StoryBeat beat);

// The SHORT name of the zone a chapter belongs to — an area's own badge (AreaDef::badge,
// the walk-badge field) or one of the two endless zones' — for the archive row's
// right-anchored tag. Short because the row's title is the chapter and the zone is only
// where it happened; "" for a sector with no story block.
const char* storyZoneBadge(int sector);

}  // namespace mal
