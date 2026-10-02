// game_onboard.cpp — what the device teaches a first-time player, and how it knows it
// already has.
//
// Nobody is standing beside a new operator, so the device has to say the things the
// hint bands cannot: what an egg's first minigame is staking, which button opens the
// menu, which part of the menu the pet needs right now — and, the first time each need
// comes up and the first time a walk is armed, WHY. Everything but the "!" marks is a
// one-time TIP, a player-level persisted set (save v66) so a new egg does not re-teach
// the buttons; the marks are a standing readout and are never "seen".
#include "core/app/game.h"

#include <cstdio>

#include "core/ui/carousel.h"
#include "core/ui/items_screen.h"
#include "core/ui/prose_page.h"
#include "core/ui/widgets.h"

namespace mal {

// --- The tips set ------------------------------------------------------------

bool Game::tipSeen(Tip t) const {
    const int w = static_cast<int>(t);
    if (w >= kTipWireCap) return false;
    return (tipsSeen_[w / 8] & (1u << (w % 8))) != 0;
}

void Game::markTipSeen(Tip t) {
    const int w = static_cast<int>(t);
    if (w >= kTipWireCap || tipSeen(t)) return;
    tipsSeen_[w / 8] |= static_cast<uint8_t>(1u << (w % 8));
    markSaveDirty();
}

// --- The hatch briefing --------------------------------------------------------

bool Game::hatchGameLive() const {
    if (arcadeRun_ || !inEggPhase()) return false;
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
    if (!pet_ || inEggPhase()) return;
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
         model_.hungerZone(), false);
    mark(SubmenuId::Maint, model_.fragZone(), model_.hasGhost());
    mark(SubmenuId::Games, model_.happyZone(), false);
    // One error short of failing, or failing: STAT is where the errors are read, and
    // this is the one state where waiting is how the pet is lost. Always urgent.
    if (model_.careMistakes() >= kCareDying - 1)
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

namespace {

struct TipRowText {
    const char* label;
    const char* text;
};

// The "!" explainer, which leads the first CARE card a device shows.
constexpr TipRowText kMarkRow = {
    "THE ! MARK",
    "A ! beside a menu means something there needs doing. A blinking red one is urgent. "
    "Cards like this one show once each."};

constexpr TipRowText kCareFedRows[] = {
    {"HUNGRY",
     "FED drops a little every few minutes the device is on. Open ITEMS and FEED it a "
     "meal. At zero it locks up and costs an error. More food turns up on the walk."},
};
constexpr TipRowText kCareFragRows[] = {
    {"GLITCHY",
     "Fights and the walk fragment it. Run a DEFRAG from MAINT - the TOOL option never "
     "fails, and a first pet comes with one tool. An AV scan clears a ghost."},
};
constexpr TipRowText kCareHappyRows[] = {
    {"BORED",
     "Its mood drifts down over time. Play anything in GAMES to cheer it up - every "
     "game pays Bits as well."},
};
// Errors happen while nobody is looking — a Lockout expires on an empty room — so these
// two wait for the next time the menu is opened, like every other card.
constexpr TipRowText kFirstErrorRows[] = {
    {"AN ERROR",
     "Your pet has taken a care error - STAT's AUDIT LOG page says what. Errors stay "
     "for this pet's whole life; STAT's ERRORS row shows where they lead."},
    {"THE PATH",
     "0-2 errors keep it on the GOOD path, 3-4 the BAD one: stronger, but it glitches "
     "faster. At 5 it starts failing, and is lost in half an hour."},
    {"HEADING IT OFF",
     "Keep it fed - starving is the usual cause. A Restore Point from ITEMS blocks the "
     "next error, once per pet."},
};
constexpr TipRowText kNearTheLineRows[] = {
    {"ONE MORE ERROR",
     "This pet has 4 errors. One more and it starts failing, with half an hour of "
     "powered-on time to save it."},
    {"BLOCK THE NEXT ONE",
     "Arm a Restore Point from ITEMS now: it stops the next error, once per pet. And "
     "keep FED up - an empty stomach is the usual fifth."},
};
// The first sealed cache (or Decryptogram) is the first thing in the bag that only the
// Hacker face can use — and that face is reached by a chord nothing else teaches.
constexpr TipRowText kFirstCacheRows[] = {
    {"YOUR SIDE",
     "A+C on the home screen flips to your side of the device: your profile, the "
     "SHOP's rig upgrades, the VAULT and more. A+C again flips back."},
    {"THE VAULT",
     "Sealed caches found on the walk open in the VAULT - food, Bits and rare finds "
     "come out. A Decryptogram is cashed in there too."},
    {"MID-WALK",
     "While a walk is running, A+C opens the walk's controls instead. Stop the walk "
     "there first, then A+C reaches your side."},
};
constexpr TipRowText kFirstWalkRows[] = {
    {"IT WALKS ITSELF",
     "Your pet now roams this area on its own, menu open or not. Wild malbeasts pick "
     "fights along the way, and the fights play out by themselves."},
    {"WINS AND LOSSES",
     "A win pays Bits and XP. A loss adds FRAG and ends the walk. Win 10 in a row and "
     "the area's boss opens up."},
    {"IN A FIGHT",
     "A skips ahead, B shows both sides, C runs. A+C opens your Exploit command."},
    {"ALONG THE WAY",
     "Food, items and Bits turn up as it walks. A+C at home opens the walk's controls - "
     "stop it from there."},
};

template <size_t N>
void appendTipRows(std::vector<ProseRow>& out, const TipRowText (&rows)[N]) {
    for (const TipRowText& t : rows) {
        ProseRow r;
        r.label = t.label;
        std::snprintf(r.body.buf, sizeof(r.body.buf), "%s", t.text);
        out.push_back(r);
    }
}

// Where the card's prose starts: under drawHeaderBand, as the RULES page's does.
constexpr int kTipCardTop = 46;

}  // namespace

std::vector<ProseRow> Game::tipCardRows() const {
    std::vector<ProseRow> out;
    switch (tipCard_) {
        case Tip::CareFed:   appendTipRows(out, kCareFedRows); break;
        case Tip::CareFrag:  appendTipRows(out, kCareFragRows); break;
        case Tip::CareHappy: appendTipRows(out, kCareHappyRows); break;
        case Tip::FirstWalk: appendTipRows(out, kFirstWalkRows); break;
        case Tip::FirstError: appendTipRows(out, kFirstErrorRows); break;
        case Tip::NearTheLine: appendTipRows(out, kNearTheLineRows); break;
        case Tip::FirstCache: appendTipRows(out, kFirstCacheRows); break;
        default: break;
    }
    if (tipCardMarkRow_) {
        const TipRowText mark[] = {kMarkRow};
        appendTipRows(out, mark);
    }
    return out;
}

void Game::openTipCard(Tip t, Nav returnTo) {
    const bool care = t == Tip::CareFed || t == Tip::CareFrag || t == Tip::CareHappy;
    // The "!" explainer only where there is a "!" to explain: CARE ALERTS off draws none.
    tipCardMarkRow_ = care && careAlerts_ && !tipSeen(Tip::CareFed) &&
                      !tipSeen(Tip::CareFrag) && !tipSeen(Tip::CareHappy);
    markTipSeen(t);
    tipCard_ = t;
    tipCardScroll_ = 0;
    tipCardReturn_ = returnTo;
    nav_ = Nav::TipCard;
    dirty_ = true;
}

bool Game::openCareTipIfDue() {
    if (!pet_ || inEggPhase()) return false;
    // The errors first, nearest the line first: a pet one short of failing is the one
    // thing on this list that cannot wait for the next summon. That card covers what an
    // error is as well, so the first-error card is spent along with it.
    const int errors = model_.careMistakes();
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
    if (model_.hungerZone() != Zone::Ok && !tipSeen(Tip::CareFed)) {
        openTipCard(Tip::CareFed, Nav::Cursor);
        return true;
    }
    if ((model_.fragZone() != Zone::Ok || model_.hasGhost()) && !tipSeen(Tip::CareFrag)) {
        openTipCard(Tip::CareFrag, Nav::Cursor);
        return true;
    }
    if (model_.happyZone() != Zone::Ok && !tipSeen(Tip::CareHappy)) {
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

void Game::onTipCard(const ButtonEvent& ev) {
    const std::vector<ProseRow> rows = tipCardRows();
    const int total = static_cast<int>(rows.size());
    if (ev.button == Button::B) {
        tipCardScroll_ += proseRowsFitting(rows, tipCardScroll_, kTipCardTop);
        if (tipCardScroll_ >= total) nav_ = tipCardReturn_;   // off the end: done
    } else if (ev.button == Button::C) {
        nav_ = tipCardReturn_;
    }
    dirty_ = true;
}

void Game::drawTipCard(Framebuffer& fb) const {
    fb.clear(palColor(Pal::PAPER));
    drawHeaderBand(fb, tipCard_ == Tip::FirstWalk    ? "THE WALK"
                       : tipCard_ == Tip::FirstCache ? "A CACHE"
                                                     : "YOUR PET",
                   "TIP");
    const std::vector<ProseRow> rows = tipCardRows();
    const bool last = tipCardScroll_ + proseRowsFitting(rows, tipCardScroll_, kTipCardTop) >=
                      static_cast<int>(rows.size());
    const char* hint = last ? "B GOT IT" : "B NEXT   C SKIP";
    drawProseRows(fb, rows, tipCardScroll_, kTipCardTop, beat_, hint);
    // The reader draws its band only for a page that overflows; a card that fits in one
    // window still has to say how it is closed.
    drawHintBand(fb, hint);
}

// --- CFG > TIPS: the cards again ------------------------------------------------

namespace {
// The replayable cards, in the order a raise tends to meet them.
constexpr Game::Tip kReplayable[] = {
    Game::Tip::CareFrag, Game::Tip::CareFed, Game::Tip::CareHappy, Game::Tip::FirstWalk,
    Game::Tip::FirstCache, Game::Tip::FirstError, Game::Tip::NearTheLine,
};
}  // namespace

const char* Game::tipTitle(Tip t) {
    switch (t) {
        case Tip::CareFed: return "HUNGRY";
        case Tip::CareFrag: return "GLITCHY";
        case Tip::CareHappy: return "BORED";
        case Tip::FirstWalk: return "THE WALK";
        case Tip::FirstCache: return "CACHES + THE VAULT";
        case Tip::FirstError: return "ERRORS";
        case Tip::NearTheLine: return "ONE MORE ERROR";
        default: return "";
    }
}

std::vector<Game::Tip> Game::replayableTips() const {
    std::vector<Tip> out;
    for (Tip t : kReplayable)
        if (tipSeen(t)) out.push_back(t);
    return out;
}

void Game::onTipsList(const ButtonEvent& ev) {
    const std::vector<Tip> tips = replayableTips();
    const int n = static_cast<int>(tips.size());
    if (ev.button == Button::A && n > 0) {
        cfgTipsRow_ = (cfgTipsRow_ + 1) % n;
        dirty_ = true;
    } else if (ev.button == Button::B && n > 0) {
        if (cfgTipsRow_ >= n) cfgTipsRow_ = 0;
        openTipCard(tips[cfgTipsRow_], Nav::Detail);   // back here when it is read
    } else if (ev.button == Button::C) {
        leaveCfgScreen();
    }
}

void Game::drawTipsList(Framebuffer& fb) const {
    fb.clear(palColor(Pal::PAPER));
    drawHeaderBand(fb, "TIPS");
    const std::vector<Tip> tips = replayableTips();
    const int n = static_cast<int>(tips.size());
    if (n == 0) {
        const char* none = "- NONE SHOWN YET -";
        drawText(fb, (kActiveW - textWidth(none)) / 2, kActiveH / 2, none,
                 palColor(Pal::INK_DIM));
        drawHintBand(fb, "C BACK");
        return;
    }
    const int cursor = cfgTipsRow_ < n ? cfgTipsRow_ : 0;
    const int scrollTop = listScrollTop(cursor, n, kVisibleRows);
    for (int v = 0; v < kVisibleRows && scrollTop + v < n; ++v) {
        const int i = scrollTop + v;
        const int y = kRowTop + v * kRowH;
        if (i == cursor) {
            fb.fillRect(4, y + 2, kActiveW - 8, kRowH - 4, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + (kRowH - 7) / 2, palColor(Pal::ACCENT));
        }
        drawText(fb, 22, y + (kRowH - kFontH) / 2, tipTitle(tips[i]),
                 i == cursor ? palColor(Pal::ACCENT) : palColor(Pal::INK));
    }
    drawHintBand(fb, "A NEXT  B READ  C BACK");
}

int32_t Game::failingLeftMs() const {
    if (!pet_ || inEggPhase() || model_.careBranch() != CareBranch::Dying) return -1;
    return dyingElapsedMs_ < kCsfDyingGraceMs
               ? static_cast<int32_t>(kCsfDyingGraceMs - dyingElapsedMs_)
               : 0;
}

bool Game::yubiReady() const {
    return !yubiConsumed_ && inventory_.count("yubi_cookie") > 0;
}

bool Game::lockoutFoodHeld() const {
    for (const ItemDef* d : registry_.allItems())
        if (itemResolvesLockout(*d) && inventory_.count(d->id) > 0) return true;
    return false;
}

}  // namespace mal
