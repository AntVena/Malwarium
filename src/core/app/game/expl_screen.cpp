#include "core/app/game/expl_screen.h"

#include "tunables.h"
#include "core/app/operator.h"
#include "core/model/explore_walk.h"
#include "core/model/pet.h"
#include "core/model/tournament.h"   // tourneyAliveCount
#include "core/ui/expl_screen.h"
#include "core/ui/layout.h"

namespace mal {

namespace {
constexpr int kSubFlags = kExplSectors * kExplSubAreas;

// The durable per-sub flag blocks, flattened to the row-major layout the expl_screen row
// helpers read ([area * kExplSubAreas + sub]).
struct SubFlags {
    bool cleared[kSubFlags];
    bool boss[kSubFlags];
    explicit SubFlags(const Operator& op) {
        for (int a = 0; a < kExplSectors; ++a)
            for (int s = 0; s < kExplSubAreas; ++s) {
                cleared[a * kExplSubAreas + s] = op.world.subCleared[a][s];
                boss[a * kExplSubAreas + s] = op.world.subBossUnlocked[a][s];
            }
    }
};

// Past the lead rows (the two endless zones), then one block per area.
int areaHeaderRow(int area) { return kExplLeadRows + area * kExplRowsPerArea; }

int walkSector(const ExplHost& host) {
    return host.exploreWalk().active ? host.exploreWalk().sector : -1;
}
int walkSub(const ExplHost& host) {
    return host.exploreWalk().active ? host.exploreWalk().sub : -1;
}

ExplRowState rowState(const ExplHost& host, const SubFlags& f, int row) {
    return explRowState(row, host.operatorRecord().world.sectorCleared, f.cleared, f.boss,
                        walkSector(host), walkSub(host), host.tourneyRunning());
}

// The view the picker's lock rules are answered from. The nav and the draw both build it
// here, so the categories the cursor skips are the set drawn as "??????".
ExplListView worldView(const ExplHost& host) {
    ExplListView v;
    v.areaCleared = host.operatorRecord().world.sectorCleared;
    v.exploringSector = walkSector(host);
    v.exploringSub = walkSub(host);
    v.bestDeepWebDepth = host.petRecord().bestDeepWebDepth;
    v.bestDarkWebDepth = host.petRecord().bestDarkWebDepth;
    v.tourneyRunning = host.tourneyRunning();
    v.tourneyAlive = tourneyAliveCount(host.tourneyAlive());
    v.tourneyRound = host.tourneyRound();
    v.storyChapters = host.storyChapterCount();
    return v;
}
}  // namespace

bool ExplScreen::rowLandable(const ExplHost& host, int row) const {
    if (!explRowInLevel(row, cat_, navArea_)) return false;
    const ExplRowState st = rowState(host, SubFlags(host.operatorRecord()), row);
    // An area header at STORY's own level is landable while not selectable: an open area
    // is entered whether or not its gauntlet stands yet, so only AreaLocked refuses.
    if (cat_ == ExplCat::Story && navArea_ < 0) return st != ExplRowState::AreaLocked;
    return explRowSelectable(st);
}

bool ExplScreen::catLandable(const ExplHost& host, int catRow) const {
    if (catRow < 0 || catRow >= kExplCatRows) return false;
    return explCatOpen(explCatAt(catRow), worldView(host));
}

void ExplScreen::open(const ExplHost& host) {
    const ExploreWalk& walk = host.exploreWalk();
    if (walk.active) {
        if (walk.sector >= 0 && walk.sector < kExplSectors) {
            cat_ = ExplCat::Story;
            navArea_ = walk.sector;
            row_ = areaHeaderRow(walk.sector) + 1 + walk.sub;
            if (rowLandable(host, row_)) return;
        } else if (host.inEndlessZone()) {
            cat_ = ExplCat::Endless;
            navArea_ = -1;
            row_ = walk.sector == kDeepWebSector ? 0 : 1;
            if (rowLandable(host, row_)) return;
        }
    }
    // Otherwise the picker opens on STORY, which is never locked.
    cat_ = ExplCat::None;
    navArea_ = -1;
    for (int r = 0; r < kExplCatRows; ++r)
        if (catLandable(host, r)) { row_ = r; return; }
    row_ = 0;                                        // nothing landable: park at 0
}

void ExplScreen::parkOnCategory(ExplCat cat) {
    cat_ = ExplCat::None;
    navArea_ = -1;
    row_ = explCatRow(cat);
}

void ExplScreen::stepList(const ExplHost& host, int dir) {
    // Two cursor spaces behind one list: the picker's rows, and the ladder's inside an
    // open category. Both walk the same way and differ in length and in where they stop.
    if (cat_ == ExplCat::None)
        row_ = stepRow(row_, kExplCatRows, dir, [&](int r) { return catLandable(host, r); });
    else
        row_ = stepRow(row_, explRowCount(), dir, [&](int r) { return rowLandable(host, r); });
}

void ExplScreen::onButton(ExplHost& host, const ButtonEvent& ev) {
    if (cat_ == ExplCat::None) onCategories(host, ev);
    else onRows(host, ev);
}

void ExplScreen::onCategories(ExplHost& host, const ButtonEvent& ev) {
    // The two categories with rows behind them drop into their level; the other two ARE
    // their action, so B goes straight to the bracket or the archive.
    if (ev.button == Button::A) {
        stepList(host, +1);
    } else if (ev.button == Button::B) {
        if (!catLandable(host, row_)) return;
        const ExplCat c = explCatAt(row_);
        if (!explCatHasRows(c)) {
            if (c == ExplCat::Arena) host.openTourney();
            else if (c == ExplCat::Chapters) host.openStoryArchive();
            return;
        }
        cat_ = c;
        navArea_ = -1;
        for (int r = 0, n = explRowCount(); r < n; ++r)
            if (rowLandable(host, r)) { row_ = r; return; }
        row_ = 0;
    } else if (ev.button == Button::C) {
        host.leaveToCarousel();
    }
}

void ExplScreen::onRows(ExplHost& host, const ButtonEvent& ev) {
    const int n = explRowCount();
    if (ev.button == Button::A) {
        stepList(host, +1);
    } else if (ev.button == Button::B) {
        if (row_ < 0 || row_ >= n || !rowLandable(host, row_)) return;
        // STORY's own level, on an area header: drill in to the area's first landable
        // row. ENDLESS's two rows have nothing to drill into and act directly.
        if (cat_ == ExplCat::Story && navArea_ < 0) {
            navArea_ = explRowArea(row_);
            for (int r = 0; r < n; ++r)
                if (rowLandable(host, r)) { row_ = r; break; }
            return;
        }
        const int area = explRowArea(row_);
        const int sub = explRowSub(row_);
        switch (rowState(host, SubFlags(host.operatorRecord()), row_)) {
            case ExplRowState::AreaBossReady:
            case ExplRowState::AreaCleared:     host.startAreaBoss(area); break;
            case ExplRowState::SubBossReady:    host.startSubAreaBoss(area, sub); break;
            case ExplRowState::DeepWebOpen:
            case ExplRowState::DeepWebDiving:   host.offerThenDive(); break;   // bells first
            case ExplRowState::DarkWebOpen:
            case ExplRowState::DarkWebCrawling: host.startDarkWebCrawl(); break;
            default:
                host.startExplore(area, sub);                  // arm or re-arm
                host.explainFirstWalk();
                break;
        }
    } else if (ev.button == Button::C) {
        // One level out, every time, parked on the row just left: inside an area to its
        // category's rows, at a category's rows to the picker. C never skips a level, so
        // the way out of EXPL is the same number of presses as the way in.
        if (navArea_ >= 0) {
            const int a = navArea_;
            navArea_ = -1;
            row_ = areaHeaderRow(a);
        } else {
            parkOnCategory(cat_);
        }
    }
}

void ExplScreen::draw(Framebuffer& fb, const ExplHost& host) const {
    const SubFlags f(host.operatorRecord());
    ExplListView v = worldView(host);
    v.cursor = row_;
    v.subCleared = f.cleared;
    v.subBossUnlocked = f.boss;
    v.cat = cat_;
    v.navArea = navArea_;
    v.streakWins = host.exploreWalk().streak;
    v.winsToBoss = kExploreStreakToBoss;
    // Only the rows this level draws are counted: STORY's zone list pays for the area
    // pools, an open area for its own five rungs, ENDLESS for its two, and the picker,
    // which draws no zone rows, for none. Everything else stays 0, which is what a row
    // nobody is looking at is worth.
    int areaLearn[kExplSectors] = {};
    int gauntletLearn[kExplSectors] = {};
    int subLearn[kSubFlags] = {};
    if (cat_ == ExplCat::Story && navArea_ >= 0 && navArea_ < kExplSectors) {
        gauntletLearn[navArea_] = host.areaGauntletMovesToLearn(navArea_);
        for (int sub = 0; sub < kExplSubAreas; ++sub)
            subLearn[navArea_ * kExplSubAreas + sub] = host.subAreaMovesToLearn(navArea_, sub);
    } else if (cat_ == ExplCat::Story) {
        for (int a = 0; a < kExplSectors; ++a) areaLearn[a] = host.areaMovesToLearn(a);
    } else if (cat_ == ExplCat::Endless) {
        v.deepWebMovesToLearn = host.deepWebMovesToLearn();
        v.darkWebMovesToLearn = host.darkWebMovesToLearn();
    }
    v.areaMovesToLearn = areaLearn;
    v.gauntletMovesToLearn = gauntletLearn;
    v.subMovesToLearn = subLearn;
    v.beat = host.screenBeat();
    drawExplList(fb, host.content(), v);
}

}  // namespace mal
