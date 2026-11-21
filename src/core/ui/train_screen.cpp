#include "core/ui/train_screen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "tunables.h"
#include "core/content/effect_text.h"
#include "core/content/registry.h"
#include "core/model/combat.h"   // simDummyName for the tier label
#include "core/model/move_loadout.h"
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

// Ceiling on the move picker's list, not its fixed height: the list draws only the
// rows it has (drawMovePicker's `visibleRows`) and scrolls past this cap, so the
// slack goes to the description panel below instead of being reserved empty.
constexpr int kMovePickerMaxRows = 5;

const SpriteData& familyIcon(MoveFamily f) {
    switch (f) {
        case MoveFamily::Strike: return ASSET_ICON_MOVE_FAMILY_STRIKE;
        case MoveFamily::Guard:  return ASSET_ICON_MOVE_FAMILY_GUARD;
        case MoveFamily::Stun:   return ASSET_ICON_MOVE_FAMILY_STUN;
        case MoveFamily::Dot:    return ASSET_ICON_MOVE_FAMILY_DOT;
        case MoveFamily::Drain:  return ASSET_ICON_MOVE_FAMILY_DRAIN;
    }
    return ASSET_ICON_MOVE_FAMILY_STRIKE;
}

// `prefix` + `id` upper-cased: the sprite naming convention (ICON_MOVE_<ID>, ICON_LINE_<ID>).
const SpriteData* spriteNamed(const ContentRegistry& reg, const char* prefix, const char* id) {
    char name[48];
    std::snprintf(name, sizeof(name), "%s%s", prefix, id);
    for (char* c = name; *c; ++c)
        if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
    return reg.sprite(name);
}

} // namespace

// The move's own ICON_MOVE_<ID> where one is drawn (like ICON_MOD_*); else a line move's
// ICON_LINE_<LINE>, so a loadout can pick its line's moves out at a glance; else the
// generic move's family glyph. Every row carries an icon.
const SpriteData* moveIcon(const ContentRegistry& reg, const char* id) {
    if (const SpriteData* own = spriteNamed(reg, "ICON_MOVE_", id)) return own;
    const MoveDef* m = reg.move(id);
    if (!m) return nullptr;
    if (m->line)
        if (const SpriteData* line = spriteNamed(reg, "ICON_LINE_", m->line)) return line;
    return &familyIcon(moveFamily(*m));
}

namespace {

// One equipped-move row: glyph, name, and the move's own kind tag at the right edge.
// The tag is enforced to match the slot, so it is the fact the row cannot lose:
// the name yields to it and scrolls when `focused` (widgets.h).
void moveRowText(Framebuffer& fb, const ContentRegistry& reg, const char* id,
                 int x, int y, Rgb565 nameCol, int beat, bool focused) {
    const MoveDef* m = reg.move(id);
    if (const SpriteData* ic = moveIcon(reg, id)) drawSprite(fb, *ic, 0, x, y - 6);
    int nameW = kActiveW - kMargin - (x + 22);
    if (m) {
        const char* tag = moveKindTag(m->kind);
        const int tagX = kActiveW - kMargin - textWidth(tag);
        drawText(fb, tagX, y, tag, palColor(Pal::ACCENT));
        nameW = tagX - kMargin - (x + 22);
    }
    drawTextMarquee(fb, x + 22, y, nameW, m ? m->displayName : id, nameCol, beat,
                    scrollWhen(focused));
}

} // namespace

std::vector<const MoveDef*> ownedMoveList(const ContentRegistry& reg,
                                          const MoveLoadout& load,
                                          MoveDef::Kind requiredKind,
                                          Stage petStage, const char* petLine,
                                          int slot, bool showAll) {
    std::vector<const MoveDef*> out;
    for (const MoveDef* m : reg.allMoves()) {
        // The innate is owned but not slottable (MoveLoadout::isInnate): offering it
        // would be offering to spend a slot on what an empty slot already falls back to.
        // Wrong-kind moves never list here.
        if (!load.owns(m->id) || load.isInnate(m->id) || m->kind != requiredKind) continue;
        if (!showAll) {
            if (!moveUnlockedAtStage(*m, petStage)) continue;
            if (!moveAllowedForLine(*m, petLine)) continue;
            const int elsewhere = load.slotOf(m->id);
            if (elsewhere >= 0 && elsewhere != slot) continue;
        }
        out.push_back(m);
    }
    return out;
}

int loadoutSelectableCount(Stage stage) {
    return MoveLoadout::slotsForStage(stage);
}

void drawLoadout(Framebuffer& fb, const ContentRegistry& reg, const MoveLoadout& load,
                 const LoadoutView& v) {
    drawHeaderBand(fb, "MOVES");
    const int unlocked = MoveLoadout::slotsForStage(v.stage);

    // No standalone "default" row: Quick Jab is a per-slot FALLBACK, not a
    // fixture. It's shown inside an empty slot below, never as a slot of its own.
    // Row columns: the slot label on the margin, the state glyph, then the content,
    // with the accepted-kind tag right-aligned opposite. The focused band starts 7px
    // above its row's baseline, so `top` keeps it clear of the header rule.
    // The slot number is a single digit under the MOVES header, so the label column is
    // one cell wide and the move name gets the room "SLOT n" used to take.
    const int rowH = 20, top = kRowTop + 8;
    constexpr int kSlotLabelX = 20;   // clear of the row cursor at the margin
    constexpr int kSlotGlyphX = 36;
    constexpr int kSlotTextX = 60;
    for (int i = 0; i < kMaxMoveSlots; ++i) {
        const int y = top + i * rowH;
        const bool unlockedSlot = i < unlocked;
        if (unlockedSlot && i == v.cursor) {
            fb.fillRect(4, y - 7, kActiveW - 8, rowH - 4, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y - 3, palColor(Pal::ACCENT));
        }
        char slotLbl[4];
        std::snprintf(slotLbl, sizeof(slotLbl), "%d", i + 1);
        drawText(fb, kSlotLabelX, y, slotLbl, palColor(Pal::INK_DIM));
        if (!unlockedSlot) {
            // Locked slot: the path-ahead affordance. The glyph says "shut" and the
            // line says what opens it: "AT SCRIPT" alone read as a property of the
            // slot rather than as the way out of it.
            drawSprite(fb, ASSET_ICON_MOVE_SLOT_LOCKED, 0, kSlotGlyphX, y - 6);
            char lk[28];
            std::snprintf(lk, sizeof(lk), "UNLOCK: %s",
                          stageName(MoveLoadout::stageUnlockingSlot(i)));
            drawText(fb, kSlotTextX, y, lk, palColor(Pal::INK_DIM));
        } else if (const char* id = load.equipped(i)) {
            // The equipped move's own kind tag (moveRowText, right edge) already
            // reads as this slot's type: it's enforced to match.
            moveRowText(fb, reg, id, kSlotGlyphX, y, palColor(Pal::INK), v.beat,
                        i == v.cursor);
        } else {
            // An EMPTY unlocked slot falls back to the innate default (Quick Jab) in
            // combat (per-slot fallback), so show it here IN the slot. Two non-colour
            // channels say it is a stand-in rather than a choice: the empty-slot glyph
            // (distinct from the locked one beside it) and the dimmed name. This is the
            // ONLY place Quick Jab surfaces; there is no dedicated default row.
            drawSprite(fb, ASSET_ICON_MOVE_SLOT_EMPTY, 0, kSlotGlyphX, y - 6);
            const MoveDef* def = reg.move(load.defaultMove());
            drawText(fb, kSlotTextX, y, def ? def->displayName : "QUICK JAB",
                     palColor(Pal::INK_DIM));
            // Keep the slot's accepted-kind tag at the right edge: tells the
            // player what they can equip to replace the fallback.
            const char* tag = moveKindTag(v.slotKinds[i]);
            drawText(fb, kActiveW - kMargin - textWidth(tag), y, tag,
                     palColor(Pal::ACCENT));
        }
    }
    drawHintBand(fb, "A NEXT  B PICK  HOLD B READ");
}

namespace {

// Why a picker row cannot be equipped from here. Only reachable when showAll reveals a
// row the filtered default view would have dropped: above the pet's stage, or already
// equipped in a different slot (a mod-style SLOT n tag, mirroring drawModPicker's IN
// SLOT n gate).
struct MoveRowLock {
    int elsewhere = -1;   // the other slot holding it, or -1
    bool inOtherSlot = false;
    bool aboveStage = false;
    bool any() const { return inOtherSlot || aboveStage; }
};

MoveRowLock moveRowLock(const MoveDef& m, const MoveLoadout& load, const MovePickerView& v) {
    const int elsewhere = load.slotOf(m.id);
    return {elsewhere, elsewhere >= 0 && elsewhere != v.slot,
            !moveUnlockedAtStage(m, v.petStage)};
}

void drawMovePickerRow(Framebuffer& fb, const ContentRegistry& reg, const MoveDef& m,
                       const MoveLoadout& load, const MovePickerView& v, int y, bool focused) {
    const MoveRowLock lock = moveRowLock(m, load, v);
    const char* equippedHere = load.equipped(v.slot);
    // Whatever the row's right column is (the reason it is locked, or the kind tag with
    // an ON marker when it is the move already in this slot) is what the row cannot
    // lose. The name yields to it and scrolls (drawLabelValue): a move name drawn at full
    // width runs straight under a tag as wide as "EVO SCRIPT". A locked row dims its name
    // and swaps the ATK/DEF tag for the reason (non-colour channel).
    char lk[20];
    Rgb565 tagCol = palColor(Pal::INK_DIM);
    const char* tag = lk;
    if (lock.inOtherSlot) {
        std::snprintf(lk, sizeof(lk), "SLOT %d", lock.elsewhere + 1);
    } else if (lock.aboveStage) {
        std::snprintf(lk, sizeof(lk), "EVO %s", stageName(m.minStage));
    } else {
        tagCol = palColor(Pal::ACCENT);
        tag = moveKindTag(m.kind);
        if (equippedHere && std::strcmp(equippedHere, m.id) == 0) {
            // ON rides in front of the kind tag rather than beside it, so the pair is
            // one right-hand column the name can measure against.
            std::snprintf(lk, sizeof(lk), "ON %s", moveKindTag(m.kind));
            tag = lk;
            tagCol = palColor(Pal::CALM);
        }
    }
    // The glyph leads the row, as on the slot list, so a pet's own line's moves stand
    // out while choosing (moveIcon).
    if (const SpriteData* ic = moveIcon(reg, m.id)) drawSprite(fb, *ic, 0, 22, y - 2);
    drawLabelValue(fb, 46, y + 5, m.displayName,
                   lock.any() ? palColor(Pal::INK_DIM) : palColor(Pal::INK),
                   tag, tagCol, v.beat, scrollWhen(focused));
}

// The focused move's readout + prose, from `y` down to `bottom`.
// What the focused move's panel holds, worked out before anything draws so the list
// above it can give up rows when the readout needs them (drawMovePicker).
struct MovePanel {
    MoveRowLock lock;
    const MoveDef* swapOut;   // the move the pick would replace, or null
    SpecRows spec;
    // The lead lines and the readout grid: what must fit. The prose takes what is left.
    int fixedHeight() const {
        return (lock.any() ? 12 : 0) + (swapOut ? 12 : 0) +
               gridLines(kActiveW - 2 * kMargin, spec.rows, spec.count) * kLineH;
    }
};

MovePanel movePanel(const ContentRegistry& reg, const MoveDef& m, const MoveLoadout& load,
                    const MovePickerView& v) {
    const MoveRowLock lock = moveRowLock(m, load, v);
    // A move that would replace a different one reads as the SWAP: what it takes out,
    // and every number that changes as NOW -> NEXT against the move in the slot.
    const char* equippedHere = load.equipped(v.slot);
    const MoveDef* current = equippedHere ? reg.move(equippedHere) : nullptr;
    const MoveDef* swapOut = current && current != &m && !lock.any() ? current : nullptr;
    return {lock, swapOut,
            swapOut ? specRowsVersus(specRows(*swapOut), specRows(m)) : specRows(m)};
}

void drawMovePickerPanel(Framebuffer& fb, const MoveDef& m, const MovePanel& panel, int y,
                         int bottom) {
    const MoveRowLock& lock = panel.lock;
    // A locked focus leads with WHY, then still shows the spec: knowing what a move
    // does is exactly what makes an evolution gate worth waiting for.
    if (lock.any()) {
        char buf[36];
        if (lock.inOtherSlot)
            std::snprintf(buf, sizeof(buf), "LOCKED - IN SLOT %d", lock.elsewhere + 1);
        else
            std::snprintf(buf, sizeof(buf), "LOCKED - EVOLVE TO %s", stageName(m.minStage));
        drawText(fb, kMargin, y, buf, palColor(Pal::ACCENT));
        y += 12;
    }
    if (panel.swapOut) {
        char vs[40];
        std::snprintf(vs, sizeof(vs), "SWAPS OUT %s", panel.swapOut->displayName);
        drawText(fb, kMargin, y, vs, palColor(Pal::INK_DIM));
        y += 12;
    }
    const SpecRows& spec = panel.spec;
    const EffectText prose = effectText(m);   // outlives the draw below
    SpecSheet sheet;
    sheet.rows = spec.rows;
    sheet.rowCount = spec.count;
    sheet.prose = prose.c_str();
    drawSpecSheet(fb, kMargin, y, kActiveW - 2 * kMargin, bottom, sheet);
}

// Row 0: take the slot's move out. It names the move it removes, since "unequip" alone
// leaves the player to remember what is in the slot they opened.
void drawRemoveRow(Framebuffer& fb, const ContentRegistry& reg, const MoveLoadout& load,
                   const MovePickerView& v, int y, bool focused) {
    const char* id = load.equipped(v.slot);
    const MoveDef* m = id ? reg.move(id) : nullptr;
    char lbl[40];
    if (m) std::snprintf(lbl, sizeof(lbl), "REMOVE %s", m->displayName);
    else std::snprintf(lbl, sizeof(lbl), "- SLOT EMPTY -");
    drawTextMarquee(fb, 22, y + 5, kActiveW - kMargin - 22, lbl, palColor(Pal::INK_DIM),
                    v.beat, scrollWhen(focused));
}

// What removing does: an empty slot fights with the innate default.
void drawRemovePanel(Framebuffer& fb, const ContentRegistry& reg, const MoveLoadout& load,
                     const MovePickerView& v, int y) {
    const MoveDef* def = reg.move(load.defaultMove());
    char fallback[40];
    std::snprintf(fallback, sizeof(fallback), "EMPTY SLOT USES %s",
                  def ? def->displayName : "QUICK JAB");
    drawText(fb, kMargin, y, load.equipped(v.slot) ? "REMOVE THE EQUIPPED MOVE." : "NOTHING EQUIPPED.",
             palColor(Pal::INK));
    drawText(fb, kMargin, y + 12, fallback, palColor(Pal::INK_DIM));
}

void drawMoveReplaceConfirm(Framebuffer& fb, const ContentRegistry& reg,
                            const MovePickerView& v) {
    fb.fillRect(4, 96, kActiveW - 8, 44, palColor(Pal::TRACK));
    const MoveDef* pend = v.pendingId ? reg.move(v.pendingId) : nullptr;
    char q[34];
    std::snprintf(q, sizeof(q), "REPLACE WITH %s?", pend ? pend->displayName : "MOVE");
    drawText(fb, kMargin, 104, q, palColor(Pal::INK));
    drawCancelConfirm(fb, 122, v.confirmChoice);
}

}  // namespace

void drawMovePicker(Framebuffer& fb, const ContentRegistry& reg, const MoveLoadout& load,
                    const MovePickerView& v) {
    // The slot's type-lock is right in the header ("SLOT 2 - DEF"), so the player knows
    // why the list only shows one kind before they even scroll it. showAll (hold B)
    // appends " ALL": the word is the channel, grayscale-safe.
    char title[20];
    std::snprintf(title, sizeof(title), "SLOT %d - %s%s", v.slot + 1,
                  moveKindTag(v.requiredKind), v.showAll ? " ALL" : "");
    drawHeaderBand(fb, title);

    const std::vector<const MoveDef*> owned = ownedMoveList(
        reg, load, v.requiredKind, v.petStage, v.petLine, v.slot, v.showAll);
    const int rows = 1 + static_cast<int>(owned.size());   // [0] = unequip

    // Position indicator (mirrors items_screen's header) once the list scrolls: lets
    // the player see how far into the roster they are.
    if (rows > kMovePickerMaxRows) {
        char nt[24];
        std::snprintf(nt, sizeof(nt), "%d/%d", v.pick + 1, rows);
        drawText(fb, kActiveW - kMargin - textWidth(nt), 6, nt, palColor(Pal::INK_DIM));
    }

    // ADAPTIVE window: the list takes only the height it actually needs, capped at
    // kMovePickerMaxRows and scrolled past that. A pet owns 2 rows at Process and 4 at
    // Daemon, so reserving the cap unconditionally would leave 36-72px of dead screen
    // directly above a description that had nowhere to go.
    const int rowH = 20, rowTop = 26;
    const int panelBottom = kActiveH - kHintBandH - 2;
    const MoveDef* focus =
        v.pick > 0 && v.pick - 1 < static_cast<int>(owned.size()) ? owned[v.pick - 1] : nullptr;
    const MovePanel panel = focus ? movePanel(reg, *focus, load, v) : MovePanel{};
    // A swap's NOW -> NEXT readout can be taller than the room under a full list, and the
    // readout is the reason the panel is there, so the list gives up rows to it (down to
    // three, one each side of the cursor) and scrolls instead.
    const int roomForList = panelBottom - 10 - panel.fixedHeight() - rowTop;
    const int listCap = std::max(3, std::min(kMovePickerMaxRows, roomForList / rowH));
    const int visibleRows = std::min(rows, listCap);
    const int scrollTop = listScrollTop(v.pick, rows, listCap);
    for (int slot = 0; slot < visibleRows && scrollTop + slot < rows; ++slot) {
        const int i = scrollTop + slot;
        const int y = rowTop + slot * rowH;
        if (i == v.pick) {
            fb.fillRect(4, y - 1, kActiveW - 8, rowH - 2, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + 5, palColor(Pal::ACCENT));
        }
        if (i == 0) drawRemoveRow(fb, reg, load, v, y, i == v.pick);
        else drawMovePickerRow(fb, reg, *owned[i - 1], load, v, y, i == v.pick);
    }
    drawScrollbar(fb, rowTop, listCap * rowH, rows, scrollTop, listCap);

    // The panel sits below a divider that follows the list rather than a fixed y, and
    // leaves the bottom band for the hold-B hint.
    const int listEnd = rowTop + visibleRows * rowH;
    fb.fillRect(kMargin, listEnd + 4, kActiveW - 2 * kMargin, 1, palColor(Pal::TRACK));
    const int panelY = listEnd + 10;
    if (focus) drawMovePickerPanel(fb, *focus, panel, panelY, panelBottom);
    else
        drawRemovePanel(fb, reg, load, v, panelY);

    if (v.confirmActive) drawMoveReplaceConfirm(fb, reg, v);
    drawHintBand(fb, v.confirmActive ? "A TOGGLE  B COMMIT  C CANCEL"
                                     : "A NEXT  B VIEW  C BACK");
}

namespace {
// The detail page's panel band: the readout starts here and the prose flows under it,
// down to the action line above the hint band. A move's readout is at most 4 grid
// lines, so the prose band's height is stable enough for the scroll bounds below to
// be a pure function of the move. The prose pays for the reserve and can afford to:
// it pages on A, where the readout has nowhere to go.
constexpr int kDetailPanelTop = 66;
constexpr int kDetailGridLines = 4;
constexpr int kDetailPanelBottom = kActiveH - 34;
}  // namespace

int moveDetailProseLines() {
    return (kDetailPanelBottom - kDetailPanelTop) / kLineH - kDetailGridLines;
}

int moveProseLines(const MoveDef& m) {
    const EffectText prose = effectText(m);
    return proseLineCount(kActiveW - 2 * kMargin, prose.c_str());
}

void drawMoveDetail(Framebuffer& fb, const ContentRegistry& reg, const MoveDef& m,
                    const MoveDetailView& v) {
    const bool equippedHere = v.equippedHere;
    drawHeaderBand(fb, "MOVE");

    // Icon + name + kind tag. Most moves have no glyph yet, so the name only indents
    // when there is actually art to indent past (mirrors drawModDetail's nameX). The
    // kind tag owns the right end and the name yields to it, as on every other detail
    // page: the longest shipped move name is wider than what is left beside it.
    const SpriteData* ic = moveIcon(reg, m.id);
    if (ic) drawSprite(fb, *ic, 0, kMargin, 30);
    drawLabelValue(fb, ic ? kMargin + 26 : kMargin, 36, m.displayName,
                   palColor(Pal::INK), moveKindTag(m.kind), palColor(Pal::ACCENT),
                   v.beat, Overflow::Scroll);
    const bool locked = !moveUnlockedAtStage(m, v.petStage);

    const SpecRows spec = specRows(m);
    const EffectText prose = effectText(m);   // must outlive drawSpecSheet
    SpecSheet sheet;
    sheet.rows = spec.rows;
    sheet.rowCount = spec.count;
    sheet.prose = prose.c_str();
    sheet.proseScroll = v.proseScroll;
    const SpecSheetLayout laid =
        drawSpecSheet(fb, kMargin, kDetailPanelTop, kActiveW - 2 * kMargin,
                      kDetailPanelBottom, sheet);

    // Action / gate line, mirroring drawItemDetail's verb row so the two detail pages
    // read the same way. The word carries it, not the colour (grayscale-safe).
    const int actionY = kActiveH - 30;
    if (locked) {
        char lk[36];
        std::snprintf(lk, sizeof(lk), "LOCKED - EVOLVE TO %s", stageName(m.minStage));
        drawText(fb, kMargin, actionY, lk, palColor(Pal::HOT));
    } else if (equippedHere) {
        drawText(fb, kMargin, actionY, "EQUIPPED", palColor(Pal::CALM));
    } else {
        // The verb names what it swaps out, so the page answers "instead of what".
        char act[48];
        if (v.replaces)
            std::snprintf(act, sizeof(act), "EQUIP - SWAPS OUT %s", v.replaces->displayName);
        else
            std::snprintf(act, sizeof(act), "EQUIP");
        drawRowCursor(fb, kMargin, actionY, palColor(Pal::ACCENT));
        drawTextMarquee(fb, kMargin + 10, actionY, kActiveW - 2 * kMargin - 10, act,
                        palColor(Pal::ACCENT), v.beat, scrollWhen(true));
    }

    // A already means "advance", so paging the prose with it needs no exception to the
    // A/B/C contract: the band just names what is live on this screen.
    const char* hint = laid.proseScrollable
                           ? (locked || equippedHere ? "A MORE   C BACK"
                                                     : "A MORE  B EQUIP  C BACK")
                           : (locked || equippedHere ? "C BACK" : "B EQUIP   C BACK");
    drawHintBand(fb, hint);
}

void drawSimTier(Framebuffer& fb, int tier, int tierCount) {
    drawHeaderBand(fb, "SIM-BATTLE");
    char line[28];
    std::snprintf(line, sizeof(line), "< %s >", simDummyName(tier));
    drawText(fb, kMargin, 40, "DUMMY TIER:", palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 56, line, palColor(Pal::INK));
    drawText(fb, kMargin, 80, "SAFE - NO INJURY.", palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 92, "BUILDS COMBAT STATS.", palColor(Pal::INK_DIM));
    drawRowCursor(fb, 8, 116, palColor(Pal::ACCENT));
    drawText(fb, 20, 116, "START", palColor(Pal::INK));
    char tc[20];
    std::snprintf(tc, sizeof(tc), "TIER %d/%d", tier + 1, tierCount);
    drawText(fb, kActiveW - kMargin - textWidth(tc), 40, tc, palColor(Pal::INK_DIM));
}

} // namespace mal
