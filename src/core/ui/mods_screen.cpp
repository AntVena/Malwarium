#include "core/ui/mods_screen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/effect_text.h"
#include "core/content/registry.h"
#include "core/model/loadout.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/spec_sheet.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

namespace mal {

namespace {

// Ceiling on the mod picker's list, not its fixed height (mirrors train_screen's
// kMovePickerMaxRows): the list draws only the rows it has and scrolls past this cap,
// so the slack goes to the spec panel below instead of being reserved empty.
constexpr int kModPickerMaxRows = 6;

// ICON_MOD_<UPPER ID> — the per-mod glyph naming convention.
const SpriteData* modIcon(const ContentRegistry& reg, const char* id) {
    char name[40];
    std::snprintf(name, sizeof(name), "ICON_MOD_%s", id);
    for (char* c = name; *c; ++c)
        if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
    return reg.sprite(name);
}

// Uppercased copy of a line id ("phishing" -> "PHISHING") for the LOCKED reason text —
// mirrors modIcon's uppercasing idiom above.
void upperLine(char* out, size_t outSize, const char* s) {
    size_t i = 0;
    for (const char* p = s; *p && i + 1 < outSize; ++p, ++i)
        out[i] = (*p >= 'a' && *p <= 'z') ? static_cast<char>(*p - 'a' + 'A') : *p;
    out[i] = '\0';
}

// The hard line gate (niche-flavour pass, ModDef::requiresLine): true iff `m` requires
// a line the active pet doesn't carry. nullptr `petLine` (no pet / lineless) fails any
// requirement.
bool lineLocked(const ModDef* m, const char* petLine) {
    return m->requiresLine &&
           (!petLine || std::strcmp(m->requiresLine, petLine) != 0);
}

// Detail-page prose reserve: the prose starts under the name/readout block and has to
// leave room for the slot readout + gate rows below it. modDetailProseLines() is what
// the prose ACTUALLY gets — this reserve minus the gap the readout leaves behind it.
constexpr int kDetailProseLines = 5;
// The footer under the panel. SLOT/HAVE, the gate rows and the verdict line answer ONE
// question between them — can this go on, and where — so they stack at a single pitch,
// tighter than the prose pitch above them, with the panel's unfilled reserve as the gap
// that fences the group off. Spacing them unevenly is what made the dim SLOT/HAVE row
// read as a tail of the block above rather than the head of this one. Same footer pitch
// drawItemDetail stacks its HAVE/action pair on, so the two detail pages agree. The page
// has no hint band, so the group takes what it needs off the bottom.
constexpr int kDetailFooterPitch = kFontH + 4;
constexpr int kDetailSlotY = 160;
constexpr int kDetailGateY = kDetailSlotY + kDetailFooterPitch;

} // namespace

std::vector<const ModDef*> ownedModList(const ContentRegistry& reg,
                                        const Loadout& load) {
    std::vector<const ModDef*> out;
    for (const ModDef* m : reg.allMods())
        if (load.owns(m->id)) out.push_back(m);
    return out;
}

void drawModsList(Framebuffer& fb, const ContentRegistry& reg,
                  const Loadout& load, int cursor) {
    drawHeaderBand(fb, "MODS");
    for (int i = 0; i < kModSlots; ++i) {
        const int y = kRowTop + i * kRowH;
        if (i == cursor) {
            fb.fillRect(4, y + 2, kActiveW - 8, kRowH - 4, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + (kRowH - 7) / 2, palColor(Pal::ACCENT));
        }
        char slotLbl[10];
        std::snprintf(slotLbl, sizeof(slotLbl), "SLOT %d", i + 1);
        const char* id = load.equipped(i);
        if (id) {
            const ModDef* m = reg.mod(id);
            const SpriteData* icon = modIcon(reg, id);
            if (icon) drawSprite(fb, *icon, 0, 16, y + (kRowH - kRowIcon) / 2);
            drawText(fb, 40, y + (kRowH - kFontH) / 2,
                     m ? m->displayName : id, palColor(Pal::INK));
            if (m)
                drawText(fb, kActiveW - kMargin - textWidth(m->effectTag),
                         y + (kRowH - kFontH) / 2, m->effectTag,
                         palColor(Pal::ACCENT));
        } else {
            drawSprite(fb, ASSET_ICON_MODS_SLOT_EMPTY, 0, 16,
                       y + (kRowH - kRowIcon) / 2);
            drawText(fb, 40, y + (kRowH - kFontH) / 2, "- EMPTY -",
                     palColor(Pal::INK_DIM));
        }
        // Slot number sits dim under the mod name (a stable per-row anchor).
        drawText(fb, 40, y + kRowH - kFontH, slotLbl, palColor(Pal::INK_DIM));
    }
}

void drawModPicker(Framebuffer& fb, const ContentRegistry& reg,
                   const Loadout& load, int slot, int pick, bool confirmActive,
                   int confirmChoice, const char* pendingId, int petLevel,
                   const char* petLine) {
    char title[12];
    std::snprintf(title, sizeof(title), "SLOT %d", slot + 1);
    drawHeaderBand(fb, title);

    const std::vector<const ModDef*> owned = ownedModList(reg, load);
    const int rows = static_cast<int>(owned.size());       // available spares to install

    // Position indicator (mirrors drawMovePicker's header) once the list
    // scrolls — lets the player see how far into the held-mod pool they are.
    if (rows > kModPickerMaxRows) {
        char nt[12];
        std::snprintf(nt, sizeof(nt), "%d/%d", pick + 1, rows);
        drawText(fb, kActiveW - kMargin - textWidth(nt), 6, nt, palColor(Pal::INK_DIM));
    }

    // List the available (un-equipped) mods — mods are permanent (D3), so there is
    // no unequip option; equipping consumes the mod into the slot. Compact rows so
    // the effect text fits. A mod the pet is too low-level to field shows
    // its required level ("L n") in place of the effect tag and dims — a number channel
    // that reads without colour.
    //
    // ADAPTIVE window (mirrors drawMovePicker, train_screen.cpp): the list takes only
    // the height it needs, capped at kModPickerMaxRows and scrolled past that, so an
    // early-game pool of one or two mods hands its slack to the panel below instead of
    // reserving it empty.
    const int rowH = 18;
    if (rows == 0)
        drawText(fb, 22, kRowTop + 4, "- NO MODS HELD -", palColor(Pal::INK_DIM));
    const int visibleRows = std::max(1, std::min(rows, kModPickerMaxRows));
    int scrollTop = 0;
    if (rows > kModPickerMaxRows) {
        if (pick < scrollTop) scrollTop = pick;
        if (pick >= scrollTop + kModPickerMaxRows)
            scrollTop = pick - kModPickerMaxRows + 1;
        scrollTop = std::max(0, std::min(scrollTop, rows - kModPickerMaxRows));
    }
    for (int v = 0; v < visibleRows && scrollTop + v < rows; ++v) {
        const int i = scrollTop + v;
        const int y = kRowTop + v * rowH;
        if (i == pick) {
            fb.fillRect(4, y - 1, kActiveW - 8, rowH - 2, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + 4, palColor(Pal::ACCENT));
        }
        const ModDef* m = owned[i];
        const int req = modEquipLevel(*m);
        const bool wrongLine = lineLocked(m, petLine);
        const int elsewhere = load.slotOf(m->id);
        const bool inOtherSlot = elsewhere >= 0 && elsewhere != slot;
        const bool locked = req > petLevel || wrongLine || inOtherSlot;
        char name[40];
        const int qty = load.countOf(m->id);
        if (qty > 1) std::snprintf(name, sizeof(name), "%s x%d", m->displayName, qty);
        else std::snprintf(name, sizeof(name), "%s", m->displayName);
        drawText(fb, 22, y + 4, name,
                 locked ? palColor(Pal::INK_DIM) : palColor(Pal::INK));
        if (inOtherSlot) {
            char rl[10];
            std::snprintf(rl, sizeof(rl), "SLOT %d", elsewhere + 1);
            drawText(fb, kActiveW - kMargin - textWidth(rl), y + 4, rl,
                     palColor(Pal::HOT));
        } else if (locked) {
            char rl[8];
            if (wrongLine) std::snprintf(rl, sizeof(rl), "LINE");
            else std::snprintf(rl, sizeof(rl), "L%d", req);
            drawText(fb, kActiveW - kMargin - textWidth(rl), y + 4, rl,
                     palColor(Pal::HOT));
        } else {
            drawText(fb, kActiveW - kMargin - textWidth(m->effectTag), y + 4,
                     m->effectTag, palColor(Pal::ACCENT));
        }
    }
    if (rows > kModPickerMaxRows) {   // UI_SCROLLBAR (mirrors drawMovePicker)
        const int barX = kActiveW - 3;
        const int trackH = kModPickerMaxRows * rowH;
        fb.fillRect(barX, kRowTop, 2, trackH, palColor(Pal::TRACK));
        const int thumbH = std::max(8, trackH * kModPickerMaxRows / rows);
        const int thumbY = kRowTop + trackH * scrollTop / rows;
        fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
    }

    // Focused mod's spec panel, below a divider that follows the list rather than
    // sitting at a fixed y, plus its equip-level gate underneath.
    const int listEnd = kRowTop + visibleRows * rowH;
    fb.fillRect(kMargin, listEnd + 4, kActiveW - 2 * kMargin, 1, palColor(Pal::TRACK));
    if (rows > 0 && pick >= 0 && pick < rows) {
        const ModDef* fm = owned[pick];
        const SpecRows spec = specRows(*fm);
        const EffectText prose = effectText(*fm);   // must outlive the draw
        SpecSheet sheet;
        sheet.rows = spec.rows;
        sheet.rowCount = spec.count;
        sheet.prose = prose.c_str();
        // Leave the gate line its own row at the bottom of the panel band.
        const int afterProse =
            drawSpecSheet(fb, kMargin, listEnd + 10, kActiveW - 2 * kMargin,
                          kActiveH - 14, sheet).endY;
        const int req = modEquipLevel(*fm);
        const bool wrongLine = lineLocked(fm, petLine);
        const int elsewhere = load.slotOf(fm->id);
        const bool inOtherSlot = elsewhere >= 0 && elsewhere != slot;
        char rl[40];
        if (inOtherSlot) {
            std::snprintf(rl, sizeof(rl), "LOCKED - IN SLOT %d", elsewhere + 1);
        } else if (wrongLine) {
            char up[16];
            upperLine(up, sizeof(up), fm->requiresLine);
            std::snprintf(rl, sizeof(rl), "LOCKED - REQUIRES %s LINE", up);
        } else if (req > petLevel) {
            std::snprintf(rl, sizeof(rl), "LOCKED - NEEDS LVL %d", req);
        } else {
            std::snprintf(rl, sizeof(rl), "EQUIP LVL %d - OK", req);
        }
        drawText(fb, kMargin, std::min(afterProse + 2, kActiveH - 10), rl,
                 (req > petLevel || wrongLine || inOtherSlot) ? palColor(Pal::HOT)
                                                               : palColor(Pal::INK_DIM));
    }

    // Inline overwrite confirm (D3): shown only when the slot already holds a
    // DIFFERENT mod. The overwrite is IRREVERSIBLE — it discards the current mod for
    // good — so the warning names it and stamps PERMANENT (a grayscale-safe word,
    // not a colour). Default focus is Cancel.
    if (confirmActive) {
        fb.fillRect(4, 88, kActiveW - 8, 64, palColor(Pal::TRACK));
        const ModDef* pend = pendingId ? reg.mod(pendingId) : nullptr;
        const ModDef* curMod = reg.mod(load.equipped(slot));
        char q[40];
        std::snprintf(q, sizeof(q), "EQUIP %s",
                      pend ? pend->displayName : "MOD");
        drawText(fb, kMargin, 94, q, palColor(Pal::INK));
        char w[40];
        std::snprintf(w, sizeof(w), "DISCARDS %s",
                      curMod ? curMod->displayName : "CURRENT");
        drawText(fb, kMargin, 108, w, palColor(Pal::HOT));
        drawText(fb, kMargin, 120, "- PERMANENT -", palColor(Pal::HOT));
        const int cancelX = 24, confirmX = 120, cy = 136;
        if (confirmChoice == 0) drawRowCursor(fb, cancelX - 12, cy, palColor(Pal::ACCENT));
        else drawRowCursor(fb, confirmX - 12, cy, palColor(Pal::ACCENT));
        drawText(fb, cancelX, cy, "CANCEL",
                 confirmChoice == 0 ? palColor(Pal::ACCENT) : palColor(Pal::INK));
        drawText(fb, confirmX, cy, "CONFIRM",
                 confirmChoice == 1 ? palColor(Pal::ACCENT) : palColor(Pal::INK));
    }
}

int modDetailProseLines() {
    // The band between where the grid ends (grid lines + kBlockGap) and proseFloor.
    // Both move with the grid, so the difference is fixed.
    return (kDetailProseLines * kLineH - kSpecBlockGap) / kLineH;
}

void drawModDetail(Framebuffer& fb, const ContentRegistry& reg, const Loadout& load,
                   const ModDef& mod, bool equippedHere, int slot,
                   int reqLevel, int petLevel, const char* petLine, int storageCap) {
    drawHeaderBand(fb, "MODS");

    // Icon + name + the effect TAG (the stat-delta shorthand, e.g. "+DEF").
    const SpriteData* icon = modIcon(reg, mod.id);
    if (icon) drawSprite(fb, *icon, 0, kMargin, 30);
    const int nameX = icon ? kMargin + kRowIcon + 6 : kMargin;
    drawText(fb, nameX, 36, mod.displayName, palColor(Pal::INK));
    drawText(fb, kActiveW - kMargin - textWidth(mod.effectTag), 36, mod.effectTag,
             palColor(Pal::ACCENT));

    // The readout as an aligned grid, then the prose under it (spec_sheet.h): every
    // magnitude the row hands combat is reserved room and drawn, and the prose takes
    // what's left — so a retune can never outrun the description, and a long
    // description can never push a magnitude off the screen.
    const SpecRows spec = specRows(mod);
    const EffectText prose = effectText(mod);   // must outlive the draw
    SpecSheet sheet;
    sheet.rows = spec.rows;
    sheet.rowCount = spec.count;
    sheet.prose = prose.c_str();
    const int proseFloor = 56 + kDetailProseLines * kLineH +
                           gridLines(kActiveW - 2 * kMargin, spec.rows, spec.count) * kLineH;
    const int afterStats =
        drawSpecSheet(fb, kMargin, 56, kActiveW - 2 * kMargin, proseFloor, sheet).endY;

    // ONE-SHOT flag — a consumed-on-trigger mod carries a distinct, grayscale-safe
    // caveat line (the word itself is the channel; HOT tints it too). It sits at a
    // fixed y so short descriptions all agree on where to look, but a long one
    // pushes it down rather than being written over.
    if (mod.oneShot)
        drawText(fb, kMargin, std::max(138, afterStats + 4),
                 "ONE-SHOT: CONSUMED ON USE", palColor(Pal::HOT));

    // Target slot readout + the rolled equip-LEVEL gate + the hard line gate
    // (niche-flavour pass, ModDef::requiresLine) + the one-slot-per-pet gate
    // (already installed in a DIFFERENT slot on this pet). REQUIRES LVL n is the
    // per-instance rolled requirement (a number channel); a mod with requiresLine
    // adds a second REQUIRES <LINE> LINE row. Any gate failing turns the action
    // line LOCKED instead of EQUIP (word channel, grayscale-safe).
    const int elsewhere = load.slotOf(mod.id);
    const bool inOtherSlot = !equippedHere && elsewhere >= 0;
    const bool levelLocked = !equippedHere && reqLevel > petLevel;
    const bool wrongLine = !equippedHere && lineLocked(&mod, petLine);
    const bool locked = levelLocked || wrongLine || inOtherSlot;
    char slotLbl[16];
    std::snprintf(slotLbl, sizeof(slotLbl), "SLOT %d", slot + 1);
    drawText(fb, kMargin, kDetailSlotY, slotLbl, palColor(Pal::INK_DIM));
    // Held spares against the cap that bounds them. ITEMS says "HAVE xN" because an
    // inventory stack has no ceiling; a mod pool does, and a player at it needs to see
    // that the number stopped climbing on purpose — that is the MOD STORAGE row's whole
    // pitch, and the only place it gets made.
    char have[16];
    const int held = load.countOf(mod.id);
    std::snprintf(have, sizeof(have), "HAVE %d/%d", held, storageCap);
    drawText(fb, kActiveW - kMargin - textWidth(have), kDetailSlotY, have,
             held >= storageCap ? palColor(Pal::WARN) : palColor(Pal::INK_DIM));
    char req[24];
    std::snprintf(req, sizeof(req), "REQUIRES LVL %d", reqLevel);
    int y = kDetailGateY;
    drawText(fb, kMargin, y, req, levelLocked ? palColor(Pal::HOT) : palColor(Pal::INK_DIM));
    y += kDetailFooterPitch;
    if (mod.requiresLine) {
        char up[16];
        upperLine(up, sizeof(up), mod.requiresLine);
        char lineReq[32];
        std::snprintf(lineReq, sizeof(lineReq), "REQUIRES %s LINE", up);
        drawText(fb, kMargin, y, lineReq, wrongLine ? palColor(Pal::HOT) : palColor(Pal::INK_DIM));
        y += kDetailFooterPitch;
    }

    // Action line: EQUIPPED (dim, already here) / LOCKED (level, line, or already
    // installed elsewhere on this pet) / EQUIP + cursor.
    if (equippedHere) {
        drawText(fb, kMargin, y, "EQUIPPED HERE", palColor(Pal::INK_DIM));
    } else if (locked) {
        char lk[28];
        if (inOtherSlot) std::snprintf(lk, sizeof(lk), "LOCKED - IN SLOT %d", elsewhere + 1);
        else if (wrongLine) std::snprintf(lk, sizeof(lk), "LOCKED - WRONG LINE");
        else std::snprintf(lk, sizeof(lk), "LOCKED - NEEDS LVL %d", reqLevel);
        drawText(fb, kMargin, y, lk, palColor(Pal::HOT));
    } else {
        drawRowCursor(fb, kMargin, y, palColor(Pal::ACCENT));
        drawText(fb, kMargin + 10, y, "EQUIP", palColor(Pal::ACCENT));
    }
}

} // namespace mal
