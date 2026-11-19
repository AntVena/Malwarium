#include "core/app/game/dive_prep_screen.h"

#include <cstdio>

#include "core/content/defs.h"
#include "core/content/effect_text.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {

// Two lines a row: the row's name with its setting on the right, and under it what the
// setting means right now. An item name is too long to share a line with its slot's name.
constexpr int kTop = 32;
constexpr int kPitch = 2 * kLineH + 4;
constexpr int kValueX = 24;

DiveSlot slotOf(DivePrepRow r) { return r == DivePrepRow::Bell ? DiveSlot::Bell : DiveSlot::Device; }

const char* pickWord(DivePickKind k) {
    switch (k) {
        case DivePickKind::None: return "NONE";
        case DivePickKind::Best: return "BEST";
        case DivePickKind::Item: return "SET";
    }
    return "";
}

struct RowText {
    const char* name = "";
    const char* setting = nullptr;   // right of the name; nullptr for none
    bool settingOn = false;          // ACCENT for a live switch, the word carries it too
    char value[32] = "";
};

RowText slotRow(const DivePrepHost& host, DiveSlot s) {
    RowText t;
    t.name = s == DiveSlot::Bell ? "BELL" : "DEVICE";
    const DivePick& p = host.diveLoadout().pick(s);
    t.setting = pickWord(p.kind);
    // An Item pick that has run out still names its item, at x0, so the player sees why
    // nothing will arm.
    const ItemDef* d = p.kind == DivePickKind::Item ? p.item : host.divePickItem(s);
    if (p.kind == DivePickKind::None)
        std::snprintf(t.value, sizeof(t.value), "NOTHING ARMED");
    else if (d)
        std::snprintf(t.value, sizeof(t.value), "%s x%d", d->displayName, host.heldCount(d->id));
    else
        std::snprintf(t.value, sizeof(t.value), "NONE HELD");
    return t;
}

RowText rowText(const DivePrepHost& host, DivePrepRow r) {
    RowText t;
    switch (r) {
        case DivePrepRow::DiveNow: {
            t.name = "DIVE NOW";
            const bool bell = host.divePickItem(DiveSlot::Bell) != nullptr;
            const bool device = host.divePickItem(DiveSlot::Device) != nullptr;
            std::snprintf(t.value, sizeof(t.value), "%s",
                          bell && device ? "ARMS BELL + DEVICE"
                          : bell         ? "ARMS THE BELL"
                          : device       ? "ARMS THE DEVICE"
                                         : "NOTHING ARMED");
            return t;
        }
        case DivePrepRow::Bell: return slotRow(host, DiveSlot::Bell);
        case DivePrepRow::Device: return slotRow(host, DiveSlot::Device);
        case DivePrepRow::AutoRedive: {
            const bool on = host.diveLoadout().autoRedive;
            t.name = "AUTO-REDIVE";
            t.setting = on ? "ON" : "OFF";
            t.settingOn = on;
            std::snprintf(t.value, sizeof(t.value), "%s",
                          on ? "A LOSS DIVES AGAIN" : "A LOSS ENDS IT");
            return t;
        }
    }
    return t;
}

// What the focused row does, at length, between the rows and the hint band.
void drawFocusNote(Framebuffer& fb, const DivePrepHost& host, DivePrepRow r, int top) {
    const int maxLines = (kActiveH - kHintBandH - top) / kLineH;
    const ItemDef* d = (r == DivePrepRow::Bell || r == DivePrepRow::Device)
                           ? host.divePickItem(slotOf(r)) : nullptr;
    if (d) {
        const EffectText t = effectText(*d);
        drawTextWrapped(fb, kMargin, top, kActiveW - 2 * kMargin, t.c_str(),
                        palColor(Pal::INK_DIM), kLineH, maxLines);
        return;
    }
    const char* note = "";
    switch (r) {
        case DivePrepRow::DiveNow:
            note = "Arms the loadout below, then dives. It stands for every dive after.";
            break;
        case DivePrepRow::Bell:
        case DivePrepRow::Device:
            note = "B steps the pick: none, the best you hold, then each item held.";
            break;
        case DivePrepRow::AutoRedive:
            note = "After a lost dive the next starts by itself, with this loadout.";
            break;
    }
    drawTextWrapped(fb, kMargin, top, kActiveW - 2 * kMargin, note, palColor(Pal::INK_DIM),
                    kLineH, maxLines);
}

}  // namespace

void DivePrepScreen::stepList(const DivePrepHost&, int dir) {
    row_ = stepRow(row_, kDivePrepRows, dir);
}

void DivePrepScreen::onButton(DivePrepHost& host, const ButtonEvent& ev) {
    if (ev.button == Button::A) {
        row_ = stepRow(row_, kDivePrepRows, +1);
    } else if (ev.button == Button::B) {
        switch (static_cast<DivePrepRow>(row_)) {
            case DivePrepRow::DiveNow: host.diveWithLoadout(); break;
            case DivePrepRow::Bell: host.cycleDivePick(DiveSlot::Bell); break;
            case DivePrepRow::Device: host.cycleDivePick(DiveSlot::Device); break;
            case DivePrepRow::AutoRedive: host.toggleAutoRedive(); break;
        }
    } else if (ev.button == Button::C) {
        host.leaveDivePrep(backTo_);
    }
}

void DivePrepScreen::draw(Framebuffer& fb, const DivePrepHost& host) const {
    drawHeaderBand(fb, "DIVE PREP");
    for (int i = 0; i < kDivePrepRows; ++i) {
        const RowText t = rowText(host, static_cast<DivePrepRow>(i));
        const int y = kTop + i * kPitch;
        const bool focused = i == row_;
        if (focused) {
            fb.fillRect(4, y - 3, kActiveW - 8, kPitch - 2, palColor(Pal::TRACK));
            drawRowCursor(fb, kMargin, y, palColor(Pal::ACCENT));
        }
        drawText(fb, 24, y, t.name, palColor(focused ? Pal::INK : Pal::INK_DIM));
        if (t.setting)
            drawText(fb, kActiveW - kMargin - textWidth(t.setting), y, t.setting,
                     palColor(t.settingOn ? Pal::ACCENT : Pal::INK_DIM));
        drawText(fb, kValueX, y + kLineH, t.value, palColor(Pal::INK_DIM));
    }
    const DivePrepRow focused = static_cast<DivePrepRow>(row_);
    drawFocusNote(fb, host, focused, kTop + kDivePrepRows * kPitch + 2);
    drawHintBand(fb, focused == DivePrepRow::DiveNow ? "A NEXT  B DIVE  C BACK"
                                                     : "A NEXT  B CHANGE  C BACK");
}

}  // namespace mal
