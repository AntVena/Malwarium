// game_stat.cpp (STAT): the pet's status reader, its six pages and the INDEX over them.
//
// The submenu is READ-ONLY, which is what shapes its whole button contract. Nothing
// here is selected or committed, so the three keys are spent entirely on getting to the
// part of the pet you asked about: A cycles the pages, B advances the flowed ones a
// section at a time, HOLD B opens the INDEX, and C leaves.
//
// STAT opens on VITALS and never on the INDEX. Hunger, frag and the evolve clock are
// what the slot is entered for, several times a session; a menu in front of them would
// charge every one of those visits a press to pay for the occasional long jump. The
// index replaces that walk: one hold, from any page, landing on any page or on any
// SECTION of one (StatIndexRow::anchor, core/ui/stat_screen.h).
//
// The pages' row models are marshalled here too: Game owns the state they report,
// stat_screen.h owns the shape, and these are the seam between the two. The draw and
// the B-advance both read them, which is what keeps the window the engine steps by and
// the window the page drew the same window.
#include "core/app/game.h"

#include "tunables.h"
#include "core/model/move_loadout.h"
#include "core/render/framebuffer.h"
#include "core/ui/collect_screen.h"
#include "core/ui/stat_screen.h"

namespace mal {

// --- The pages' row models --------------------------------------------------

std::vector<ProseRow> Game::statTierRows() const {
    // TOTAL points, not earned ones: an Epic dish's off-level grant counts toward a rung
    // exactly as an earned point does (applyLevelStatPoints resolves the tiers from the
    // same sum), so the page has to read the stat the way the fight does.
    int points[kLevelStatCount];
    for (int i = 0; i < kLevelStatCount; ++i) points[i] = totalStatPoint(i);
    return buildTierRows(points);
}

std::vector<ProseRow> Game::statLoadoutRows() const {
    return buildLoadoutRows(registry_, pet_.moves, pet_.mods,
                            pet_.creature ? pet_.creature->stage : Stage::BootSector, inEggPhase());
}

std::vector<ProseRow> Game::statBuffRows() const {
    const uint32_t backupRemainMs =
        backupShieldArmed() ? (pet_.armed.backupShieldUntilMs - lifetimeUptimeMs()) : 0;
    return buildBuffRows(registry_, pet_.grants.shieldActive, pet_.armed.trojanDivert,
                         backupShieldArmed(), backupRemainMs, pet_.armed.deepWebDepthMultiplier,
                         pet_.armed.deepWebStartDepth != -1,
                         pet_.armed.deepWebStartDepth == PetArmed::kStartDepthUseBest,
                         pet_.armed.deepWebStartDepth, pet_.usb.branch,
                         pet_.usb.soakFactor, pet_.usb.hold, pet_.upgrades);
}

std::vector<StatIndexRow> Game::statIndexRows() const {
    // The two counted rows are counted the way the LOADOUT hub counts them
    // (drawLoadoutHub, core/ui/mods_screen.h): equipped over the slots this stage has
    // actually unlocked, so "4/4" on the index means the same thing it means there.
    const Stage stage = pet_.creature ? pet_.creature->stage : Stage::BootSector;
    const int moveSlots = MoveLoadout::slotsForStage(stage);
    int movesOn = 0;
    for (int i = 0; i < moveSlots; ++i)
        if (pet_.moves.equipped(i)) ++movesOn;
    int modsOn = 0;
    for (int i = 0; i < kModSlots; ++i)
        if (pet_.mods.equipped(i)) ++modsOn;

    // The BUFFS count is BUFFS, not its rows: the page's headings are signage and
    // counting them would tell the reader they have two more buffs than they do.
    const std::vector<ProseRow> buffs = statBuffRows();
    int armed = 0;
    for (const ProseRow& b : buffs)
        if (!b.header) ++armed;

    // The two collection scores, counted off the very rows their pages draw so the
    // index and the page can never report a different number.
    const std::vector<ProseRow> dex = statMoveDexRows();
    int movesKnown = 0, movesLearnable = 0;
    for (const ProseRow& r : dex) {
        if (r.header) continue;
        ++movesLearnable;
        if (r.tag[0]) ++movesKnown;
    }
    int foodsTotal = 0;
    for (const FoodRow& r : statFoodRows()) foodsTotal += r.section ? r.total : 0;

    StatIndexCounts c;
    c.level = pet_.level;
    c.bondBp = bondTotalBp(pet_.upgrades);
    c.movesOn = movesOn;
    c.moveSlots = moveSlots;
    c.modsOn = modsOn;
    c.modSlots = kModSlots;
    c.buffs = armed;
    c.line = pet_.creature ? pet_.creature->line : nullptr;
    c.logEvents = log_.size();
    c.movesKnown = movesKnown;
    c.movesLearnable = movesLearnable;
    c.foodsEaten = petFoodsEaten();
    c.foodsTotal = foodsTotal;
    return buildStatIndexRows(statTierRows(), statLoadoutRows(), c);
}

std::vector<FoodRow> Game::statFoodRows() const {
    return buildFoodRows(registry_, pet_.foodsEaten, foundFavouriteFood());
}

std::vector<ProseRow> Game::statMoveDexRows() const {
    return buildMoveDexRows(registry_, pet_.moves, pet_.creature ? pet_.creature->line : nullptr,
                            pet_.creature ? pet_.creature->stage : Stage::BootSector);
}

Game::StatScrollSpan Game::statScrollSpan() const {
    if (!pet_.creature) return {0, 0};
    switch (statPage_) {
        case StatPage::Tiers: {
            const std::vector<ProseRow> rows = statTierRows();
            return {tierRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
        }
        case StatPage::ModSlots: {
            const std::vector<ProseRow> rows = statLoadoutRows();
            return {loadoutRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
        }
        case StatPage::Moves: {
            const std::vector<ProseRow> rows = statMoveDexRows();
            return {moveDexRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
        }
        case StatPage::Foods: {
            const std::vector<FoodRow> rows = statFoodRows();
            return {foodRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
        }
        case StatPage::Buffs: {
            const std::vector<ProseRow> rows = statBuffRows();
            return {buffRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
        }
        default:
            break;
    }
    return {0, 0};
}

// --- Input ------------------------------------------------------------------

void Game::resetStatReader() {
    statPage_ = StatPage::Vitals;
    statScroll_ = 0;
    statScreen_ = StatScreen::Page;
    statIndexRow_ = 0;
}

void Game::onStatPage(const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        statPage_ = static_cast<StatPage>((static_cast<int>(statPage_) + 1) % kStatPages);
        statScroll_ = 0;         // fresh page -> scroll to the top
    } else if (ev.button == Button::C) {
        nav_ = Nav::Cursor;
        resetStatReader();
    } else if (ev.button == Button::B) {
        // B is a tap/hold pair here (the ITEMS filter's shape): the hold opens the
        // INDEX from tickHeldGestures, so the advance below waits for the RELEASE and
        // runs only if the hold never landed (statIndexReleaseB).
        bHeld_ = true;
        bDownMs_ = nowMs_;
    }
}

void Game::statIndexReleaseB() {
    if (!bHeld_ || face_ != Face::Pet || nav_ != Nav::Submenu ||
        enteredId() != SubmenuId::Stat || statScreen_ != StatScreen::Page)
        return;
    // The window advance. statScrollSpan reports {0, 0} for a page with nothing to
    // scroll and for one whose rows all fit, so B is inert on those. It steps by the
    // window the page actually DREW. Rows are sized to their own text and a section
    // ends a window early (prose_page.h), so that count varies, and wraps to the top
    // past the end.
    const StatScrollSpan span = statScrollSpan();
    if (span.shown > 0 && span.shown < span.total) {
        statScroll_ += span.shown;
        if (statScroll_ >= span.total) statScroll_ = 0;
        dirty_ = true;
    }
}

void Game::openStatIndex() {
    statScreen_ = StatScreen::Index;
    // Park the cursor on what the reader was already looking at: the last row that
    // opens the page they are on at or before the window they are in. So the index
    // opens as a map with "you are here" already marked, and backing straight out of
    // it is a no-op rather than a jump somewhere else.
    const std::vector<StatIndexRow> rows = statIndexRows();
    statIndexRow_ = 0;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        if (rows[i].page == statPage_ && rows[i].anchor <= statScroll_)
            statIndexRow_ = i;
}

void Game::onStatIndex(const ButtonEvent& ev) {
    // A's REPEAT and C's backward walk are the list contract's (game_listnav.cpp); the
    // tap of each stays this screen's own, as in every list handler: the contract
    // decides how a cursor moves and the screen decides when.
    const std::vector<StatIndexRow> rows = statIndexRows();
    const int n = static_cast<int>(rows.size());
    if (ev.button == Button::A) {
        if (n > 0) statIndexRow_ = (statIndexRow_ + 1) % n;
    } else if (ev.button == Button::B) {
        if (statIndexRow_ >= 0 && statIndexRow_ < n) {
            const StatIndexRow& r = rows[statIndexRow_];
            statPage_ = r.page;
            // The anchor is a ROW of the page being opened, which is what makes a
            // section reachable in one selection: the window the page draws starts
            // there (drawProseRows clamps it), so MODS opens ON the MODS heading.
            statScroll_ = r.anchor;
        }
        statScreen_ = StatScreen::Page;
    } else if (ev.button == Button::C) {
        statScreen_ = StatScreen::Page;
    }
}

// --- Render -----------------------------------------------------------------

void Game::drawStat(Framebuffer& fb) const {
    if (!pet_.creature) return;
    if (statScreen_ == StatScreen::Index) {
        drawStatIndex(fb, statIndexRows(), statIndexRow_, beat_);
        return;
    }
    // A cycles the pages in StatPage order; C backs out.
    switch (statPage_) {
        case StatPage::Vitals: {
            StatScreenView v;
            v.name = pet_.creature->displayName;
            v.stage = pet_.creature->stage;
            v.generation = pet_.generation;
            v.level = pet_.level;
            v.combatXp = pet_.xp;
            v.xpToNext = xpToNextLevel();
            v.hasNextEvo = hasNextEvolution();
            v.evoRemainMs = evolveRemainMs();
            v.beat = beat_;
            drawStatScreen(fb, pet_.model, v);
            break;
        }
        case StatPage::Bond: drawBondScreen(fb, pet_.upgrades); break;
        case StatPage::Tiers: drawTiersScreen(fb, statTierRows(), statScroll_, beat_); break;
        case StatPage::ModSlots: drawLoadoutScreen(fb, statLoadoutRows(), statScroll_, beat_); break;
        case StatPage::Moves: {
            const std::vector<ProseRow> rows = statMoveDexRows();
            CollectionPage page;
            page.scrollTop = statScroll_;
            page.beat = beat_;
            for (const ProseRow& r : rows) {
                if (r.header) continue;
                ++page.total;
                if (r.tag[0]) ++page.have;   // a gap carries no tag (buildMoveDexRows)
            }
            drawMoveDexScreen(fb, rows, page);
            break;
        }
        case StatPage::Foods: {
            const std::vector<FoodRow> rows = statFoodRows();
            CollectionPage page;
            page.scrollTop = statScroll_;
            page.have = petFoodsEaten();
            page.beat = beat_;
            for (const FoodRow& r : rows) page.total += r.section ? r.total : 0;
            drawFoodsScreen(fb, registry_, rows, page);
            break;
        }
        case StatPage::Buffs: drawBuffsScreen(fb, statBuffRows(), statScroll_, beat_); break;
        case StatPage::Species: drawSpeciesScreen(fb, *pet_.creature, beat_); break;
        case StatPage::Log:
        case StatPage::Count: drawAuditLog(fb, log_, beat_); break;
    }
}

}  // namespace mal
