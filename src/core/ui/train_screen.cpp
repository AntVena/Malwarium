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
constexpr int kMovePickerMaxRows = 6;

// ICON_MOVE_<UPPER ID> — the per-move glyph naming convention (like ICON_MOD_*).
const SpriteData* moveIcon(const ContentRegistry& reg, const char* id) {
    char name[40];
    std::snprintf(name, sizeof(name), "ICON_MOVE_%s", id);
    for (char* c = name; *c; ++c)
        if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
    return reg.sprite(name);
}

// One equipped-move row: glyph, name, and the move's own kind tag at the right edge.
// The tag is enforced to match the slot (#12), so it is the fact the row cannot lose —
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
                    focused);
}

} // namespace

std::vector<const MoveDef*> ownedMoveList(const ContentRegistry& reg,
                                          const MoveLoadout& load,
                                          MoveDef::Kind requiredKind,
                                          Stage petStage, const char* petLine,
                                          int slot, bool showAll) {
    std::vector<const MoveDef*> out;
    for (const MoveDef* m : reg.allMoves()) {
        // default isn't owned → excluded; #12: wrong-kind moves never list here.
        if (!load.owns(m->id) || m->kind != requiredKind) continue;
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

void drawLoadout(Framebuffer& fb, const ContentRegistry& reg,
                 const MoveLoadout& load, Stage stage, int cursor,
                 const MoveDef::Kind* slotKinds, int beat) {
    drawHeaderBand(fb, "MOVES");
    const int unlocked = MoveLoadout::slotsForStage(stage);

    // No standalone "default" row: Quick Jab is a per-slot FALLBACK, not a
    // fixture — it's shown inside an empty slot below, never as a slot of its own.
    drawText(fb, kMargin, 30, "EQUIPPED", palColor(Pal::INK_DIM));
    fb.fillRect(kMargin + textWidth("EQUIPPED") + 6, 34, 90, 1, palColor(Pal::TRACK));

    // Row columns: the slot label on the margin, the state glyph, then the content,
    // with the accepted-kind tag right-aligned opposite. `top` clears the LOADOUT
    // caption's own row, which the focused band would otherwise draw over.
    const int rowH = 20, top = 48;
    constexpr int kSlotLabelX = 20;   // clear of the row cursor at the margin
    constexpr int kSlotGlyphX = 72;
    constexpr int kSlotTextX = 96;
    for (int i = 0; i < kMaxMoveSlots; ++i) {
        const int y = top + i * rowH;
        const bool unlockedSlot = i < unlocked;
        if (unlockedSlot && i == cursor) {
            fb.fillRect(4, y - 7, kActiveW - 8, rowH - 4, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y - 3, palColor(Pal::ACCENT));
        }
        char slotLbl[10];
        std::snprintf(slotLbl, sizeof(slotLbl), "SLOT %d", i + 1);
        drawText(fb, kSlotLabelX, y, slotLbl, palColor(Pal::INK_DIM));
        if (!unlockedSlot) {
            // Locked slot: the path-ahead affordance. The glyph says "shut" and the
            // line says what opens it — "AT SCRIPT" alone read as a property of the
            // slot rather than as the way out of it.
            //
            // It is "UNLOCK:" and not "UNLOCK AT" because of the budget: the line
            // starts at kSlotTextX and a locked row draws no kind tag, so it owns
            // everything to the right margin — 15 cells at the mono advance. Slot 0
            // is never locked (kMoveSlotsByStage starts at 1), so the longest stage
            // that can appear here is PROCESS, and "UNLOCK AT PROCESS" is 17.
            drawSprite(fb, ASSET_ICON_MOVE_SLOT_LOCKED, 0, kSlotGlyphX, y - 6);
            char lk[28];
            std::snprintf(lk, sizeof(lk), "UNLOCK: %s",
                          stageName(MoveLoadout::stageUnlockingSlot(i)));
            drawText(fb, kSlotTextX, y, lk, palColor(Pal::INK_DIM));
        } else if (const char* id = load.equipped(i)) {
            // The equipped move's own kind tag (moveRowText, right edge) already
            // reads as this slot's type — it's enforced to match (#12).
            moveRowText(fb, reg, id, kSlotGlyphX, y, palColor(Pal::INK), beat,
                        i == cursor);
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
            // Keep the slot's accepted-kind tag at the right edge (#12) — tells the
            // player what they can equip to replace the fallback.
            const char* tag = moveKindTag(slotKinds[i]);
            drawText(fb, kActiveW - kMargin - textWidth(tag), y, tag,
                     palColor(Pal::ACCENT));
        }
    }
}

void drawMovePicker(Framebuffer& fb, const ContentRegistry& reg,
                    const MoveLoadout& load, int slot, int pick, bool confirmActive,
                    int confirmChoice, const char* pendingId, Stage petStage,
                    const char* petLine, MoveDef::Kind requiredKind, bool showAll,
                    int beat) {
    // #12: the slot's type-lock is right in the header — "SLOT 2 - DEF" — so the
    // player knows why the list only shows one kind before they even scroll it.
    // showAll (hold B) appends " ALL" — the word is the channel, grayscale-safe.
    char title[20];
    std::snprintf(title, sizeof(title), "SLOT %d - %s%s", slot + 1,
                  moveKindTag(requiredKind), showAll ? " ALL" : "");
    drawHeaderBand(fb, title);

    const std::vector<const MoveDef*> owned =
        ownedMoveList(reg, load, requiredKind, petStage, petLine, slot, showAll);
    const int rows = 1 + static_cast<int>(owned.size());   // [0] = unequip
    const char* equippedHere = load.equipped(slot);

    // Position indicator (mirrors items_screen's header) once the list
    // scrolls — lets the player see how far into the roster they are.
    if (rows > kMovePickerMaxRows) {
        char nt[12];
        std::snprintf(nt, sizeof(nt), "%d/%d", pick + 1, rows);
        drawText(fb, kActiveW - kMargin - textWidth(nt), 6, nt, palColor(Pal::INK_DIM));
    }

    // ADAPTIVE window: the list takes only the height it actually needs, capped at
    // kMovePickerMaxRows and scrolled past that. A pet owns 2 rows at Process and 4 at
    // Daemon, so reserving the cap unconditionally left 36-72px of dead screen sitting
    // directly above a description that had nowhere to go.
    const int rowH = 18, rowTop = 26;
    const int visibleRows = std::min(rows, kMovePickerMaxRows);
    const int scrollTop = listScrollTop(pick, rows, kMovePickerMaxRows);
    for (int v = 0; v < visibleRows && scrollTop + v < rows; ++v) {
        const int i = scrollTop + v;
        const int y = rowTop + v * rowH;
        if (i == pick) {
            fb.fillRect(4, y - 1, kActiveW - 8, rowH - 2, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + 4, palColor(Pal::ACCENT));
        }
        if (i == 0) {
            drawText(fb, 22, y + 4, "- EMPTY (UNEQUIP) -", palColor(Pal::INK_DIM));
            continue;
        }
        const MoveDef* m = owned[i - 1];
        // Only reachable when showAll reveals a row the filtered default view would
        // have dropped: above the pet's stage, or already equipped in a different
        // slot (a mod-style SLOT n tag, mirroring drawModPicker's IN SLOT n gate).
        // Dim the name and swap the ATK/DEF tag for the reason (non-colour channel).
        const int elsewhere = load.slotOf(m->id);
        const bool inOtherSlot = elsewhere >= 0 && elsewhere != slot;
        const bool aboveStage = !moveUnlockedAtStage(*m, petStage);
        const bool locked = aboveStage || inOtherSlot;
        // Whatever the row's right column is — the reason it is locked, or the kind tag
        // (with an ON marker when it is the move already in this slot) — is what the row
        // cannot lose. The name yields to it and scrolls (drawLabelValue): a move name
        // drawn at full width runs straight under a tag as wide as "EVO SCRIPT".
        char lk[20];
        Rgb565 tagCol = palColor(Pal::INK_DIM);
        const char* tag = lk;
        if (inOtherSlot) {
            std::snprintf(lk, sizeof(lk), "SLOT %d", elsewhere + 1);
        } else if (aboveStage) {
            std::snprintf(lk, sizeof(lk), "EVO %s", stageName(m->minStage));
        } else {
            tagCol = palColor(Pal::ACCENT);
            tag = moveKindTag(m->kind);
            if (equippedHere && std::strcmp(equippedHere, m->id) == 0) {
                // ON rides in front of the kind tag rather than beside it, so the pair
                // is one right-hand column the name can measure against.
                std::snprintf(lk, sizeof(lk), "ON %s", moveKindTag(m->kind));
                tag = lk;
                tagCol = palColor(Pal::CALM);
            }
        }
        drawLabelValue(fb, 22, y + 4, m->displayName,
                       locked ? palColor(Pal::INK_DIM) : palColor(Pal::INK),
                       tag, tagCol, beat, i == pick);
    }
    if (rows > kMovePickerMaxRows) {   // UI_SCROLLBAR (mirrors drawItemsList)
        const int barX = kActiveW - 3;
        const int trackH = kMovePickerMaxRows * rowH;
        fb.fillRect(barX, rowTop, 2, trackH, palColor(Pal::TRACK));
        const int thumbH = std::max(8, trackH * kMovePickerMaxRows / rows);
        const int thumbY = rowTop + trackH * scrollTop / rows;
        fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
    }

    // Focused move's readout + prose, below a divider that follows the list rather
    // than sitting at a fixed y. Reserve the bottom band for the hold-B hint.
    const int listEnd = rowTop + visibleRows * rowH;
    fb.fillRect(kMargin, listEnd + 4, kActiveW - 2 * kMargin, 1, palColor(Pal::TRACK));
    const int panelY = listEnd + 10;
    const int panelBottom = kActiveH - 14;
    if (pick > 0 && pick - 1 < static_cast<int>(owned.size())) {
        const MoveDef* m = owned[pick - 1];
        const int elsewhere = load.slotOf(m->id);
        const bool inOtherSlot = elsewhere >= 0 && elsewhere != slot;
        const bool aboveStage = !moveUnlockedAtStage(*m, petStage);
        int y = panelY;
        // A locked focus leads with WHY, then still shows the spec — knowing what a
        // move does is exactly what makes an evolution gate worth waiting for.
        if (inOtherSlot || aboveStage) {
            char buf[36];
            if (inOtherSlot)
                std::snprintf(buf, sizeof(buf), "LOCKED - IN SLOT %d", elsewhere + 1);
            else
                std::snprintf(buf, sizeof(buf), "LOCKED - EVOLVE TO %s",
                              stageName(m->minStage));
            drawText(fb, kMargin, y, buf, palColor(Pal::ACCENT));
            y += 12;
        }
        const SpecRows spec = specRows(*m);
        const EffectText prose = effectText(*m);   // outlives the draw below
        SpecSheet sheet;
        sheet.rows = spec.rows;
        sheet.rowCount = spec.count;
        sheet.prose = prose.c_str();
        drawSpecSheet(fb, kMargin, y, kActiveW - 2 * kMargin, panelBottom, sheet);
    } else {
        drawText(fb, kMargin, panelY, "REMOVE THE EQUIPPED MOVE.", palColor(Pal::INK));
    }

    // Inline overwrite confirm: only when replacing a DIFFERENT move.
    if (confirmActive) {
        fb.fillRect(4, 96, kActiveW - 8, 44, palColor(Pal::TRACK));
        const MoveDef* pend = pendingId ? reg.move(pendingId) : nullptr;
        char q[34];
        std::snprintf(q, sizeof(q), "REPLACE WITH %s?",
                      pend ? pend->displayName : "MOVE");
        drawText(fb, kMargin, 104, q, palColor(Pal::INK));
        const int cancelX = 24, confirmX = 120, cy = 122;
        if (confirmChoice == 0) drawRowCursor(fb, cancelX - 12, cy, palColor(Pal::ACCENT));
        else drawRowCursor(fb, confirmX - 12, cy, palColor(Pal::ACCENT));
        drawText(fb, cancelX, cy, "CANCEL",
                 confirmChoice == 0 ? palColor(Pal::ACCENT) : palColor(Pal::INK));
        drawText(fb, confirmX, cy, "CONFIRM",
                 confirmChoice == 1 ? palColor(Pal::ACCENT) : palColor(Pal::INK));
    }
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
                    Stage petStage, bool equippedHere, int proseScroll, int beat) {
    drawHeaderBand(fb, "MOVE");

    // Icon + name + kind tag. Most moves have no glyph yet, so the name only indents
    // when there is actually art to indent past (mirrors drawModDetail's nameX). The
    // kind tag owns the right end and the name yields to it, as on every other detail
    // page — the longest shipped move name is wider than what is left beside it.
    const SpriteData* ic = moveIcon(reg, m.id);
    if (ic) drawSprite(fb, *ic, 0, kMargin, 30);
    drawLabelValue(fb, ic ? kMargin + 26 : kMargin, 36, m.displayName,
                   palColor(Pal::INK), moveKindTag(m.kind), palColor(Pal::ACCENT),
                   beat, true);
    const bool locked = !moveUnlockedAtStage(m, petStage);

    const SpecRows spec = specRows(m);
    const EffectText prose = effectText(m);   // must outlive drawSpecSheet
    SpecSheet sheet;
    sheet.rows = spec.rows;
    sheet.rowCount = spec.count;
    sheet.prose = prose.c_str();
    sheet.proseScroll = proseScroll;
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
        drawRowCursor(fb, kMargin, actionY, palColor(Pal::ACCENT));
        drawText(fb, kMargin + 10, actionY, "EQUIP", palColor(Pal::ACCENT));
    }

    // A already means "advance", so paging the prose with it needs no exception to the
    // A/B/C contract — the band just names what is live on this screen.
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
