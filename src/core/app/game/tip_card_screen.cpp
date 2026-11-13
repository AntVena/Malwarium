#include "core/app/game/tip_card_screen.h"

#include <cstdio>

#include "core/model/pet.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/prose_page.h"
#include "core/ui/widgets.h"

namespace mal {

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
     "game pays Bits as well. Answering a guardian on the walk helps, as do some foods."},
};
// Errors happen while nobody is looking (a Lockout expires on an empty room), so these
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
// The second row of that card when the Restore Point is not the advice to give: one is
// already armed, or this pet has had its one. Chosen when the card is drawn
// (tipCardRows), so it is true of the pet the player is looking at.
constexpr TipRowText kShieldArmedRow = {
    "YOUR SHIELD IS UP",
    "A Restore Point is armed and will block the next error. Keep FED up - an empty "
    "stomach is the usual fifth."};
constexpr TipRowText kShieldSpentRow = {
    "NO SHIELD LEFT",
    "This pet has used its one Restore Point, so nothing blocks the next error. Keep FED "
    "up above all - an empty stomach is the usual fifth."};
// The first sealed cache (or Decryptogram) is the first thing in the bag that only the
// Hacker face can use, and that face is reached by a chord nothing else teaches.
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

std::vector<ProseRow> TipCardScreen::rows(const TipCardHost& host) const {
    std::vector<ProseRow> out;
    switch (tip_) {
        case Tip::CareFed:   appendTipRows(out, kCareFedRows); break;
        case Tip::CareFrag:  appendTipRows(out, kCareFragRows); break;
        case Tip::CareHappy: appendTipRows(out, kCareHappyRows); break;
        case Tip::FirstWalk: appendTipRows(out, kFirstWalkRows); break;
        case Tip::FirstError: appendTipRows(out, kFirstErrorRows); break;
        case Tip::NearTheLine: {
            const TipRowText first[] = {kNearTheLineRows[0]};
            appendTipRows(out, first);
            const TipRowText second[] = {host.petRecord().grants.shieldActive ? kShieldArmedRow
                                         : host.petRecord().grants.shieldSpent ? kShieldSpentRow
                                                               : kNearTheLineRows[1]};
            appendTipRows(out, second);
            break;
        }
        case Tip::FirstCache: appendTipRows(out, kFirstCacheRows); break;
        default: break;
    }
    if (markRow_) {
        const TipRowText mark[] = {kMarkRow};
        appendTipRows(out, mark);
    }
    return out;
}

void TipCardScreen::onButton(TipCardHost& host, const ButtonEvent& ev) {
    const std::vector<ProseRow> rows = this->rows(host);
    const int total = static_cast<int>(rows.size());
    if (ev.button == Button::B) {
        scroll_ += proseRowsFitting(rows, scroll_, kTipCardTop);
        if (scroll_ >= total) host.closeTipCard();   // off the end: done
    } else if (ev.button == Button::C) {
        host.closeTipCard();
    }
}

void TipCardScreen::draw(Framebuffer& fb, const TipCardHost& host) const {
    fb.clear(palColor(Pal::PAPER));
    drawHeaderBand(fb, tip_ == Tip::FirstWalk    ? "THE WALK"
                       : tip_ == Tip::FirstCache ? "A CACHE"
                                                     : "YOUR PET",
                   "TIP");
    const std::vector<ProseRow> rows = this->rows(host);
    const bool last = scroll_ + proseRowsFitting(rows, scroll_, kTipCardTop) >=
                      static_cast<int>(rows.size());
    const char* hint = last ? "B GOT IT" : "B NEXT   C SKIP";
    drawProseRows(fb, {rows, scroll_, kTipCardTop, host.screenBeat(), hint});
    // The reader draws its band only for a page that overflows; a card that fits in one
    // window still has to say how it is closed.
    drawHintBand(fb, hint);
}

const char* TipCardScreen::title(Tip t) {
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

}  // namespace mal
