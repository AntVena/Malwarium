// expl_screen.h: the EXPL slot's list, as a feature that owns its own state.
//
// Three levels behind one list: the ACTIVITY PICKER (STORY, ENDLESS, the arena, the
// chapters), a category's own rows, and the inside of one STORY area. The screen holds
// the cursor, the open category and the open area, and decides which rows the cursor
// stops on. The world it reads (what is cleared, the walk, the bracket) and what a row
// starts are the game's, reached through ExplHost.
//
// This header names no core/ui type beyond ui_state.h, so game.h can hold an ExplScreen
// by value.
#pragma once

#include <cstdint>

#include "core/device/platform.h"
#include "core/ui/ui_state.h"   // ExplCat

namespace mal {

class ContentRegistry;
class Framebuffer;
struct ExploreWalk;
struct Operator;
struct Pet;

// What EXPL needs from the game. Game implements it; a test can too.
class ExplHost {
public:
    virtual const ContentRegistry& content() const = 0;
    virtual const Operator& operatorRecord() const = 0;   // what the ladder has cleared
    virtual const Pet& petRecord() const = 0;             // the endless zones' best depths
    virtual const ExploreWalk& exploreWalk() const = 0;   // the walk armed, if any
    virtual bool inEndlessZone() const = 0;
    virtual bool tourneyRunning() const = 0;
    virtual uint8_t tourneyAlive() const = 0;
    virtual int tourneyRound() const = 0;
    virtual int storyChapterCount() const = 0;
    virtual int areaMovesToLearn(int area) const = 0;
    virtual int areaGauntletMovesToLearn(int area) const = 0;
    virtual int subAreaMovesToLearn(int area, int sub) const = 0;
    virtual int deepWebMovesToLearn() const = 0;
    virtual int darkWebMovesToLearn() const = 0;
    virtual int screenBeat() const = 0;

    virtual void startExplore(int sector, int sub) = 0;
    virtual void explainFirstWalk() = 0;   // the walk's tip card, the first time one is armed
    virtual void startAreaBoss(int area) = 0;
    virtual void startSubAreaBoss(int area, int sub) = 0;
    virtual void offerThenDive() = 0;
    virtual void startDarkWebCrawl() = 0;
    virtual void openTourney() = 0;
    virtual void openStoryArchive() = 0;
    virtual void leaveToCarousel() = 0;

protected:
    ~ExplHost() = default;
};

class ExplScreen {
public:
    // A running walk resumes where the pet is, at the level it stands on, so the picker
    // adds no press to returning to it. Otherwise the picker opens on its first open row.
    void open(const ExplHost& host);
    // Back on the picker, on `cat`'s row: where leaving the arena or the archive lands.
    void parkOnCategory(ExplCat cat);

    void onButton(ExplHost& host, const ButtonEvent& ev);
    void stepList(const ExplHost& host, int dir);
    void draw(Framebuffer& fb, const ExplHost& host) const;

    // The picker's four rows while no category is open, else the ladder's row space.
    int row() const { return row_; }

private:
    void onCategories(ExplHost& host, const ButtonEvent& ev);
    void onRows(ExplHost& host, const ButtonEvent& ev);
    // Does the cursor stop on `row` at the current level? On picker row `catRow`?
    bool rowLandable(const ExplHost& host, int row) const;
    bool catLandable(const ExplHost& host, int catRow) const;

    int row_ = 0;
    ExplCat cat_ = ExplCat::None;
    int navArea_ = -1;   // inside a STORY area, or -1 at a category's own rows
};

}  // namespace mal
