// arena_screen.h: ROCK THE DOCK's screen, as a feature that owns its own state.
//
// One screen, three reads (TourneyView): the BRACKET is the field, a held B opens the
// focused entrant's SCOUT sheet, and the chord opens the BRIEFING. The screen holds which
// read is up, the bracket's read cursor, the open reader's scroll, and the entrant the
// scout sheet is showing. The bracket itself is the run's (TourneyRun), which the screen
// reads through ArenaHost; the bout it starts and the verdict it clears are the game's.
//
// This header names no core/ui type beyond the prose row the readers return, so game.h
// can hold an ArenaScreen by value.
#pragma once

#include <cstdint>
#include <vector>

#include "core/device/platform.h"
#include "core/model/tournament.h"   // TourneyFighter

namespace mal {

class ContentRegistry;
class Framebuffer;
struct Operator;
struct Pet;
struct ProseRow;
struct TourneyRun;

enum class TourneyView : uint8_t { Bracket, Scout, Brief };

// What the arena needs from the game. Game implements it; a test can too.
class ArenaHost {
public:
    virtual const ContentRegistry& content() const = 0;
    virtual int screenBeat() const = 0;
    virtual const Operator& operatorRecord() const = 0;   // the operator's tag on its row
    virtual const Pet& petRecord() const = 0;             // its level, and the champion
    // The bracket in play, or null between runs (the screen is never up then).
    virtual const TourneyRun* tourneyRun() const = 0;

    virtual void startTourneyMatch() = 0;   // B's tap: the operator's own bout
    // C leaves to EXPL and keeps the run; any key off a verdict clears it first.
    virtual void leaveArena() = 0;
    virtual void abandonArena() = 0;
    virtual void armHoldB() = 0;

protected:
    ~ArenaHost() = default;
};

class ArenaScreen {
public:
    // Every entry, and every return from a bout, opens on the bracket with the cursor on
    // the operator's own slot.
    void open(int ownSlot);

    void onButton(ArenaHost& host, const ButtonEvent& ev);
    // B on the bracket is a tap/hold pair: held past kTourneyScoutHoldMs it opens the
    // focused entrant's sheet, and the tap under it fights the operator's own bout, so
    // both engage the fighter under the cursor.
    bool holdB(ArenaHost& host, uint32_t heldMs);
    void releaseB(ArenaHost& host);
    void draw(Framebuffer& fb, const ArenaHost& host) const;

    // The two readers' row models. The scout sheet is STAT's LOADOUT page pointed at
    // another pet, so an opponent's kit reads exactly like the operator's own.
    std::vector<ProseRow> scoutRows(const ArenaHost& host) const;
    static std::vector<ProseRow> briefRows();

    TourneyView view() const { return view_; }
    int cursor() const { return cursor_; }

private:
    void openScout(const ArenaHost& host);
    void drawBracket(Framebuffer& fb, const ArenaHost& host) const;
    void drawFoot(Framebuffer& fb, const ArenaHost& host) const;   // the match card
    void drawScout(Framebuffer& fb, const ArenaHost& host) const;
    void drawBrief(Framebuffer& fb, const ArenaHost& host) const;

    TourneyView view_ = TourneyView::Bracket;
    // A READS the field: it steps over the eight slots and chooses whose sheet a held B
    // opens. It never picks who you fight: the bracket does that.
    int cursor_ = 0;
    // The open reader's scroll: one field, because only one reader is ever open.
    int scroll_ = 0;
    // The entrant the SCOUT sheet is showing. Held rather than re-derived per repaint:
    // building one walks the whole move and mod tables, which is not a per-frame cost.
    TourneyFighter scouted_;
    int scoutedSlot_ = -1;
};

}  // namespace mal
