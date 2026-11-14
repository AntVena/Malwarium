#include "core/app/game/looks_pages.h"

#include <cstring>

#include "core/app/operator.h"
#include "core/content/content_backgrounds.h"
#include "core/content/content_themes.h"
#include "core/render/palette.h"
#include "core/ui/cfg_screen.h"
#include "core/ui/layout.h"

namespace mal {

namespace {
constexpr const char* kTagAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

// The kBackgrounds row a scene is, as the picker counts them: 0 for AUTO, else 1 + row.
int backgroundRow(SceneId s) {
    const BackgroundDef* b = backgroundFor(s);
    int row = 0;
    if (b)
        for (int i = 0; i < kBackgroundCount; ++i)
            if (kBackgrounds[i].wire == b->wire) row = i + 1;
    return row;
}

// A locked choice is refused, and the page stays on it.
CfgMove leaveIfApplied(bool applied) { return applied ? CfgMove::leave() : CfgMove::stay(); }
}  // namespace

void LooksPages::open(const LooksHost& host, CfgScreen page) {
    switch (page) {
        case CfgScreen::Theme: pick_ = host.themeRow(); break;
        case CfgScreen::Titles: pick_ = host.operatorRecord().world.equippedTitle; break;
        case CfgScreen::Background:
            pick_ = backgroundRow(host.operatorRecord().backgroundPick);
            break;
        case CfgScreen::HackerTag: {
            // The tag padded to the fixed cell count with '_', a valid alphabet char.
            const char* live = host.operatorRecord().hackerTag;
            for (int i = 0; i < kHackerTagMax; ++i) tag_[i] = live[i] == '\0' ? '_' : live[i];
            tag_[kHackerTagMax] = '\0';
            caret_ = 0;
            break;
        }
        default: break;
    }
}

// A walks every row, locked ones included, as BACKGROUND does: the line under the header
// says where the focused set comes from, which is how an operator learns there is more
// to find. A locked row refuses and the page stays, its LOCKED tag being the answer.
// Applying is instant and total, so the page the operator lands back on is the
// confirmation.
CfgMove LooksPages::onTheme(LooksHost& host, const ButtonEvent& ev) {
    return pickerPress(ev, [&] { pick_ = stepRow(pick_, kThemeCount, +1); }, [&] {
        return leaveIfApplied(host.setThemePick(palThemeByName(kThemes[pick_].palette)));
    });
}

// A walks every row, locked ones included: the line under the header names how the
// focused row is earned, which is how the page teaches the economy.
CfgMove LooksPages::onBackground(LooksHost& host, const ButtonEvent& ev) {
    return pickerPress(ev, [&] { pick_ = stepRow(pick_, kBackgroundCount + 1, +1); }, [&] {
        return leaveIfApplied(host.setBackgroundPick(
            pick_ == 0 ? SceneId::None : kBackgrounds[pick_ - 1].scene));
    });
}

// The focus walks NONE (-1) and each unlocked sector Title, skipping locked ones.
CfgMove LooksPages::onTitles(LooksHost& host, const ButtonEvent& ev) {
    return pickerPress(ev, [&] { pick_ = host.nextSelectableTitle(pick_); }, [&] {
        host.equipTitle(pick_);
        return CfgMove::leave();
    });
}

// An arcade editor: A cycles the focused cell's char, B advances the caret (and at the
// confirm cell, saves), C deletes the previous char (or backs out from the first cell).
CfgMove LooksPages::onHackerTag(LooksHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        if (caret_ < kHackerTagMax) {
            const char* pos = std::strchr(kTagAlphabet, tag_[caret_]);
            const int idx = pos ? static_cast<int>(pos - kTagAlphabet) : -1;
            tag_[caret_] = kTagAlphabet[(idx + 1) % static_cast<int>(std::strlen(kTagAlphabet))];
        }
    } else if (ev.button == Button::B) {
        if (caret_ >= kHackerTagMax) {             // the confirm cell
            host.setHackerTag(tag_);
            return CfgMove::leave();
        }
        ++caret_;
    } else if (ev.button == Button::C) {
        if (caret_ == 0) return CfgMove::leave();
        --caret_;
        tag_[caret_] = '_';
    }
    return CfgMove::stay();
}

void LooksPages::drawTheme(Framebuffer& fb, const LooksHost& host) const {
    drawThemePicker(fb, pick_, host.themeRow(), host.themesUnlockedMask());
}

// Ownership is derived, so the mask is built for the draw rather than held: one bit per
// row, in the table's own order.
void LooksPages::drawBackground(Framebuffer& fb, const LooksHost& host) const {
    uint32_t owned = 0;
    for (int i = 0; i < kBackgroundCount; ++i)
        if (host.backgroundOwned(kBackgrounds[i].scene)) owned |= 1u << i;
    drawBackgrounds(fb, pick_, owned, backgroundRow(host.operatorRecord().backgroundPick),
                    host.screenBeat());
}

void LooksPages::drawTitles(Framebuffer& fb, const LooksHost& host) const {
    const OperatorWorld& w = host.operatorRecord().world;
    mal::drawTitles(fb, pick_, w.titlesUnlocked, w.equippedTitle, host.screenBeat());
}

void LooksPages::drawHackerTag(Framebuffer& fb) const { mal::drawHackerTag(fb, tag_, caret_); }

}  // namespace mal
