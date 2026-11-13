// game_onboard.cpp: what the device teaches a first-time player, and how it knows it
// already has.
//
// No one is there to explain the device, so it says what the hint bands cannot: what
// an egg's first minigame is staking, which button opens the menu, which part of the
// menu the pet needs right now, and, the first time each need comes up and the first
// time a walk is armed, WHY. Everything but the "!" marks is a one-time TIP, a
// player-level persisted set (save v66) so a new egg does not re-teach the buttons;
// the marks are a standing readout and are never "seen".
#include "core/app/game.h"

#include <cstdio>
#include <cstring>

#include "core/ui/carousel.h"
#include "core/ui/items_screen.h"
#include "core/ui/prose_page.h"
#include "core/ui/widgets.h"

namespace mal {

// --- The tips set ------------------------------------------------------------

bool Game::tipSeen(Tip t) const {
    const int w = static_cast<int>(t);
    if (w >= kTipWireCap) return false;
    return (op_.reveals.tipsSeen[w / 8] & (1u << (w % 8))) != 0;
}

void Game::markTipSeen(Tip t) {
    const int w = static_cast<int>(t);
    if (w >= kTipWireCap || tipSeen(t)) return;
    op_.reveals.tipsSeen[w / 8] |= static_cast<uint8_t>(1u << (w % 8));
    markSaveDirty();
}

// --- The hatch briefing --------------------------------------------------------

bool Game::hatchGameLive() const {
    if (inArcadeRun() || !inEggPhase()) return false;
    switch (nav_) {
        case Nav::Decryption:
        case Nav::ModalEggPick:
        case Nav::Isolation:
        case Nav::Chroma:
            return true;
        default:
            return false;
    }
}

ProseRow Game::hatchBriefLeadRow() const {
    // Line-agnostic on purpose: every hatch game pays in incubation time and none of
    // them can cost the egg (game_lifecycle.cpp's startHatchGame), so one panel is true
    // of all four. It also names the chord, which is the only way back to this page.
    ProseRow r;
    r.label = "A NEW EGG";
    std::snprintf(r.body.buf, sizeof(r.body.buf), "%s",
                  "Your pet is sealed in this egg. Play well and it hatches sooner. "
                  "Play badly and it still hatches on its own clock - the egg is never "
                  "at risk. A+C brings these rules back mid-game.");
    return r;
}

// --- What the pet needs ---------------------------------------------------------

void Game::careAttention(unsigned& attention, unsigned& urgent) const {
    attention = 0;
    urgent = 0;
    if (!pet_.creature || inEggPhase()) return;
    // Each vital names the ONE slot that fixes it, so the shelf points at the remedy
    // rather than at a readout: food is in ITEMS, a defrag or AV scan in MAINT, and the
    // arcade is the one place Happiness can be bought back on demand. With no meal in
    // the bag, ITEMS cannot fix hunger, so the mark goes to EXPL, where food is found.
    const auto mark = [&](SubmenuId id, Zone zone, bool force) {
        if (zone == Zone::Ok && !force) return;
        for (int i = 0; i < kCarouselSlots; ++i) {
            if (carouselSlots()[i].id != id) continue;
            attention |= 1u << i;
            if (zone == Zone::Critical) urgent |= 1u << i;
        }
    };
    mark(inventoryHoldsMeal(registry_, inventory_) ? SubmenuId::Items : SubmenuId::Expl,
         pet_.model.hungerZone(), false);
    mark(SubmenuId::Maint, pet_.model.fragZone(), pet_.model.hasGhost());
    mark(SubmenuId::Games, pet_.model.happyZone(), false);
    // One error short of failing, or failing: STAT is where the errors are read, and
    // this is the one state where waiting is how the pet is lost. Always urgent.
    if (pet_.model.careMistakes() >= kCareDying - 1)
        for (int i = 0; i < kCarouselSlots; ++i)
            if (carouselSlots()[i].id == SubmenuId::Stat) {
                attention |= 1u << i;
                urgent |= 1u << i;
            }
}

// --- The tip cards -------------------------------------------------------------
//
// A card is a short page of prose shown once, at a moment the player is certainly
// looking: a CARE card when they reach for the menu while that need's "!" is up, and the
// WALK card when they arm their first walk (which then runs hands-off, so this is the
// last moment anyone is guaranteed to be there). Nothing runs on under a card: the walk
// only steps from the idle habitat (Game::tickHeartbeat), and a card is not that.

std::vector<ProseRow> Game::tipCardRows() const { return tipCardScreen_.rows(*this); }

void Game::closeTipCard() { nav_ = tipCardReturn_; }

void Game::openTipCard(Tip t, Nav returnTo) {
    const bool care = t == Tip::CareFed || t == Tip::CareFrag || t == Tip::CareHappy;
    // The "!" explainer only where there is a "!" to explain: CARE ALERTS off draws none.
    const bool markRow = care && settings_.careAlerts && !tipSeen(Tip::CareFed) &&
                         !tipSeen(Tip::CareFrag) && !tipSeen(Tip::CareHappy);
    markTipSeen(t);
    tipCardScreen_.open(t, markRow);
    tipCardReturn_ = returnTo;
    nav_ = Nav::TipCard;
    dirty_ = true;
}

bool Game::openCareTipIfDue() {
    if (!pet_.creature || inEggPhase()) return false;
    // The errors first, nearest the line first: a pet one short of failing is the one
    // thing on this list that cannot wait for the next summon. That card covers what an
    // error is as well, so the first-error card is spent along with it.
    const int errors = pet_.model.careMistakes();
    if (errors >= kCareDying - 1 && errors < kCareDying && !tipSeen(Tip::NearTheLine)) {
        markTipSeen(Tip::FirstError);
        openTipCard(Tip::NearTheLine, Nav::Cursor);
        return true;
    }
    if (errors >= 1 && !tipSeen(Tip::FirstError)) {
        openTipCard(Tip::FirstError, Nav::Cursor);
        return true;
    }
    // In the "!" marks' own order (ITEMS/EXPL, MAINT, GAMES), one card per summon: a
    // second need waits for the next time the menu is opened rather than stacking.
    if (pet_.model.hungerZone() != Zone::Ok && !tipSeen(Tip::CareFed)) {
        openTipCard(Tip::CareFed, Nav::Cursor);
        return true;
    }
    if ((pet_.model.fragZone() != Zone::Ok || pet_.model.hasGhost()) && !tipSeen(Tip::CareFrag)) {
        openTipCard(Tip::CareFrag, Nav::Cursor);
        return true;
    }
    if (pet_.model.happyZone() != Zone::Ok && !tipSeen(Tip::CareHappy)) {
        openTipCard(Tip::CareHappy, Nav::Cursor);
        return true;
    }
    if (!tipSeen(Tip::FirstCache)) {
        for (const ItemDef* d : registry_.allItems()) {
            const bool vaultOnly = d->use == ItemDef::Use::OpenContainer ||
                                   d->use == ItemDef::Use::PlayCryptogram;
            if (vaultOnly && inventory_.count(d->id) > 0) {
                openTipCard(Tip::FirstCache, Nav::Cursor);
                return true;
            }
        }
    }
    return false;
}

// --- CFG > TIPS: the cards again ------------------------------------------------

namespace {
// The replayable cards, in the order a raise tends to meet them.
constexpr Game::Tip kReplayable[] = {
    Game::Tip::CareFrag, Game::Tip::CareFed, Game::Tip::CareHappy, Game::Tip::FirstWalk,
    Game::Tip::FirstCache, Game::Tip::FirstError, Game::Tip::NearTheLine,
};
}  // namespace

const char* Game::tipTitle(Tip t) { return TipCardScreen::title(t); }

std::vector<Game::Tip> Game::replayableTips() const {
    std::vector<Tip> out;
    for (Tip t : kReplayable)
        if (tipSeen(t)) out.push_back(t);
    return out;
}

int32_t Game::failingLeftMs() const {
    if (!pet_.creature || inEggPhase() || pet_.model.careBranch() != CareBranch::Dying) return -1;
    return pet_.dyingElapsedMs < kCsfDyingGraceMs
               ? static_cast<int32_t>(kCsfDyingGraceMs - pet_.dyingElapsedMs)
               : 0;
}

bool Game::yubiReady() const {
    return !pet_.grants.yubiSpent && inventory_.count("yubi_cookie") > 0;
}

// --- WHILE YOU WERE AWAY ----------------------------------------------------------
//
// Lockouts expire, errors land, fights are won and lost, caches turn up, mostly while
// nobody is holding the device. The log records all of it, but a log is somewhere a
// player has to think to go. So the first press after a long quiet is spent on a short
// summary of exactly what changed, and only when something did.

void Game::resetAwayTally() { away_ = AwayTally{}; }

std::vector<Game::AwayRow> Game::awayRows() const {
    std::vector<AwayRow> out;
    const auto add = [&](const char* label, const char* fmt, auto... v) {
        AwayRow r;
        r.label = label;
        std::snprintf(r.value, sizeof r.value, fmt, v...);
        out.push_back(r);
    };
    if (away_.hatched) add("HATCHED INTO", "%s", away_.hatched->displayName);
    if (away_.evolved) add("EVOLVED INTO", "%s", away_.evolved->displayName);
    if (away_.hungry) add("WENT HUNGRY", "%dX", away_.hungry);
    if (away_.missed) add("LOCKOUTS MISSED", "%d", away_.missed);
    if (away_.errors) add("ERRORS", "+%d, NOW %d", away_.errors, pet_.model.careMistakes());
    if (away_.won) add("FIGHTS WON", "%d", away_.won);
    if (away_.lost) add("FIGHTS LOST", "%d", away_.lost);
    if (away_.caches) add("CACHES FOUND", "%d", away_.caches);
    return out;
}

bool Game::openAwayDigestIfDue(uint32_t quietMs) {
    // Only over the habitat: a press on any other screen is the player already looking
    // at something, and a summary would take it out from under them.
    // A failing pet comes first: the press may be the one that feeds it the cookie, and
    // the summary keeps until the next press after the crisis.
    if (failingLeftMs() >= 0) return false;
    const bool due = awayDigestOn_ && quietMs >= kAwayDigestMs && pet_.creature &&
                     face_ == Face::Pet && nav_ == Nav::Idle && !awayRows().empty();
    if (!due) {
        resetAwayTally();   // the player is here: what they missed starts again from now
        return false;
    }
    awayQuietMs_ = quietMs;
    nav_ = Nav::AwayDigest;
    dirty_ = true;
    return true;
}

void Game::onAwayDigest(const ButtonEvent& ev) {
    if (ev.button == Button::B || ev.button == Button::C) {
        resetAwayTally();
        nav_ = Nav::Idle;
        dirty_ = true;
    }
}

void Game::drawAwayDigest(Framebuffer& fb) const {
    fb.clear(palColor(Pal::PAPER));
    drawHeaderBand(fb, "WHILE YOU WERE AWAY");
    char quiet[28];
    std::snprintf(quiet, sizeof quiet, "%u:%02u AWAY (DEVICE ON)",
                  static_cast<unsigned>(awayQuietMs_ / 3600000u),
                  static_cast<unsigned>((awayQuietMs_ / 60000u) % 60u));
    drawText(fb, kMargin, 32, quiet, palColor(Pal::INK_DIM));
    const std::vector<AwayRow> rows = awayRows();
    int y = 52;
    for (const AwayRow& r : rows) {
        // The rows that cost the pet something take the warning ink; the word carries
        // it in grayscale, the colour only grades it.
        const bool bad = std::strcmp(r.label, "LOCKOUTS MISSED") == 0 ||
                         std::strcmp(r.label, "ERRORS") == 0 ||
                         std::strcmp(r.label, "WENT HUNGRY") == 0;
        drawLabelValue(fb, kMargin, y, r.label, palColor(Pal::INK), r.value,
                       bad ? palColor(Pal::WARN) : palColor(Pal::INK_DIM), 0, Overflow::Clip);
        y += 18;
    }
    drawHintBand(fb, "B OK");
}

bool Game::feedYubiToSave() {
    if (!yubiWouldSave()) return false;
    const ItemDef* cookie = registry_.item("yubi_cookie");
    if (!cookie) return false;
    // From the Lockout it is also the meal that clears the Lockout (startFeeding).
    lockoutItemsContext_ = pet_.armed.lockoutActive;
    useItem(*cookie, lockoutItemsContext_ ? ItemUseFrom::Lockout : ItemUseFrom::Bag);
    return nav_ == Nav::ModalFeeding;
}

bool Game::lockoutFoodHeld() const {
    for (const ItemDef* d : registry_.allItems())
        if (itemResolvesLockout(*d) && inventory_.count(d->id) > 0) return true;
    return false;
}

}  // namespace mal
