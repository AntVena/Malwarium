// game_stat.cpp — STAT: the pet's status reader, its six pages and the INDEX over them.
//
// The submenu is READ-ONLY, which is what shapes its whole button contract. Nothing
// here is selected or committed, so the three keys are spent entirely on getting to the
// part of the pet you asked about: A cycles the pages, B advances the flowed ones a
// section at a time, HOLD B opens the INDEX, and C leaves.
//
// STAT opens on VITALS and never on the INDEX. Hunger, frag and the evolve clock are
// what the slot is entered for, several times a session; a menu in front of them would
// charge every one of those visits a press to pay for the occasional long jump. The
// index is what that jump costs instead — one hold, from any page, landing on any page
// or on any SECTION of one (StatIndexRow::anchor, core/ui/stat_screen.h).
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
    return buildLoadoutRows(registry_, moveLoadout_, loadout_,
                            pet_ ? pet_->stage : Stage::BootSector, inEggPhase());
}

std::vector<BuffRow> Game::statBuffRows() const {
    const uint32_t backupRemainMs =
        backupShieldArmed() ? (backupShieldUntilMs_ - lifetimeUptimeMs()) : 0;
    return buildBuffRows(registry_, mistakeShieldActive_, forceTrojanDivert_,
                         backupShieldArmed(), backupRemainMs, deepWebDepthMultiplier_,
                         pendingDeepWebStartDepth_ != -1,
                         pendingDeepWebStartDepth_ == kDeepWebStartDepthUseBest,
                         pendingDeepWebStartDepth_, evolveBranchOverride_,
                         evolveSoakFactor_, evolveHold_, upgrades_);
}

std::vector<StatIndexRow> Game::statIndexRows() const {
    // The two counted rows are counted the way the LOADOUT hub counts them
    // (drawLoadoutHub, core/ui/mods_screen.h): equipped over the slots this stage has
    // actually unlocked, so "4/4" on the index means the same thing it means there.
    const Stage stage = pet_ ? pet_->stage : Stage::BootSector;
    const int moveSlots = MoveLoadout::slotsForStage(stage);
    int movesOn = 0;
    for (int i = 0; i < moveSlots; ++i)
        if (moveLoadout_.equipped(i)) ++movesOn;
    int modsOn = 0;
    for (int i = 0; i < kModSlots; ++i)
        if (loadout_.equipped(i)) ++modsOn;

    // The BUFFS count is BUFFS, not its rows: the page's headings are signage and
    // counting them would tell the reader they have two more buffs than they do.
    const std::vector<BuffRow> buffs = statBuffRows();
    int armed = 0;
    for (const BuffRow& b : buffs)
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

    return buildStatIndexRows(statTierRows(), statLoadoutRows(), combatLevel_, movesOn,
                              moveSlots, modsOn, kModSlots, armed,
                              pet_ ? pet_->line : nullptr, log_.size(), movesKnown,
                              movesLearnable, petFoodsEaten(), foodsTotal);
}

std::vector<FoodRow> Game::statFoodRows() const {
    return buildFoodRows(registry_, petFoodsEaten_);
}

std::vector<ProseRow> Game::statMoveDexRows() const {
    return buildMoveDexRows(registry_, moveLoadout_, pet_ ? pet_->line : nullptr,
                            pet_ ? pet_->stage : Stage::BootSector);
}

Game::StatScrollSpan Game::statScrollSpan() const {
    if (!pet_) return {0, 0};
    if (statPage_ == 1) {
        const std::vector<ProseRow> rows = statTierRows();
        return {tierRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
    }
    if (statPage_ == 2) {
        const std::vector<ProseRow> rows = statLoadoutRows();
        return {loadoutRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
    }
    if (statPage_ == 3) {
        const std::vector<ProseRow> rows = statMoveDexRows();
        return {moveDexRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
    }
    if (statPage_ == 4) {
        const std::vector<FoodRow> rows = statFoodRows();
        return {foodRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
    }
    if (statPage_ == 5) {
        const std::vector<BuffRow> rows = statBuffRows();
        return {buffRowsFitting(rows, statScroll_), static_cast<int>(rows.size())};
    }
    return {0, 0};
}

// --- Input ------------------------------------------------------------------

void Game::resetStatReader() {
    statPage_ = 0;
    statScroll_ = 0;
    statScreen_ = StatScreen::Page;
    statIndexRow_ = 0;
}

void Game::onStatPage(const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        statPage_ = (statPage_ + 1) % kStatPages;
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
    // window the page actually DREW — rows are sized to their own text and a section
    // ends a window early (prose_page.h), so that count varies — and wraps to the top
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
    // tap of each is still this screen's own, the shape every list handler on the device
    // has — the contract owns how a cursor moves, not when.
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
    if (!pet_) return;
    if (statScreen_ == StatScreen::Index) {
        drawStatIndex(fb, statIndexRows(), statIndexRow_, beat_);
        return;
    }
    // 8 pages: 0 pet vitals (the landing) · 1 the investment ladder — which stat tiers
    // this pet holds and what the next costs · 2 the equipped loadout (moves + mods,
    // WITH their effect text) · 3 every move this pet could learn · 4 every dish it
    // could eat · 5 currently-armed item buffs · 6 the pet's own species lore · 7 audit
    // log. A cycles; C backs out.
    switch (statPage_) {
        case 0:
            drawStatScreen(fb, model_, pet_->displayName, pet_->stage, generation_,
                           combatLevel_, combatXp_, xpToNextLevel(), beat_,
                           hasNextEvolution(), evolveRemainMs());
            break;
        case 1: drawTiersScreen(fb, statTierRows(), statScroll_, beat_); break;
        case 2: drawLoadoutScreen(fb, statLoadoutRows(), statScroll_, beat_); break;
        case 3: {
            const std::vector<ProseRow> rows = statMoveDexRows();
            int known = 0, total = 0;
            for (const ProseRow& r : rows) {
                if (r.header) continue;
                ++total;
                if (r.tag[0]) ++known;   // a gap carries no tag (buildMoveDexRows)
            }
            drawMoveDexScreen(fb, rows, statScroll_, known, total, beat_);
            break;
        }
        case 4: {
            const std::vector<FoodRow> rows = statFoodRows();
            int total = 0;
            for (const FoodRow& r : rows) total += r.section ? r.total : 0;
            drawFoodsScreen(fb, registry_, rows, statScroll_, petFoodsEaten(), total,
                            beat_);
            break;
        }
        case 5: drawBuffsScreen(fb, statBuffRows(), statScroll_, beat_); break;
        case 6:
            drawSpeciesScreen(fb, pet_->displayName, pet_->line, pet_->hint,
                              pet_->context, beat_);
            break;
        default: drawAuditLog(fb, log_, beat_); break;
    }
}

}  // namespace mal
