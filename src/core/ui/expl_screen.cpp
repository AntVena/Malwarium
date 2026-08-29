#include "core/ui/expl_screen.h"

#include <algorithm>
#include <cstdio>
#include <cstddef>

#include "tunables.h"
#include "core/content/content_tournament.h"
#include "core/content/registry.h"
#include "core/render/absorb.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"
#include "generated/assets.h"

namespace mal {

namespace {

// EXPL's list starts below the top carousel track (kTrackH), not under a plain
// header band — hence its own name rather than layout.h's kRowTop.
constexpr int kTrackRowTop = 40;

// Difficulty pips: `filled` of `total` (UI_DIFFICULTY_PIPS stub — small filled/empty
// squares so the tier reads in grayscale by count + fill). `total` is the scale the
// count is read against: 3 for an encounter's difficulty, kAreaCount for a ladder
// depth, so the same widget answers "how deep" and "how hard" without either
// pretending to be the other's scale.
void drawDifficulty(Framebuffer& fb, int x, int y, int filled, int total = 3) {
    for (int i = 0; i < total; ++i) {
        const int px = x + i * 8;
        if (i < filled) fb.fillRect(px, y, 5, 5, palColor(Pal::ACCENT));
        else {
            fb.fillRect(px, y, 5, 5, palColor(Pal::TRACK));
            fb.fillRect(px + 1, y + 1, 3, 3, palColor(Pal::PAPER));
        }
    }
}

} // namespace

// Area/storefront identity itself is owned per-area (src/core/content/areas/) —
// these accessors just resolve a sector INDEX to that area's own AreaDef; out-of-
// range clamps to area 0 (mal::area's own fallback), matching every caller's
// existing expectation.

const char* shopName(int sector) { return area(sector).shop.name; }
const char* modShopName(int sector) { return area(sector).modShop.name; }

bool explSectorOpen(int idx, const bool* sectorCleared) {
    // Linear complete-to-advance: sector 0 always open; sector N>0 opens once
    // the previous sector's boss/gauntlet is cleared. A null array = only sector 0
    // (the honest default for a fresh/migrated save with nothing cleared).
    if (idx < 0 || idx >= kExplSectors) return false;
    if (idx == 0) return true;
    return sectorCleared && sectorCleared[idx - 1];
}
// explSectorTier/explSectorName/sectorTitle/explSubAreaName all return an EMPTY
// default ("" / tier 1) for an out-of-range index — distinct from the shop
// accessors above, which intentionally fall back to area 0's storefront. So these
// bounds-check explicitly rather than relying on mal::area()'s clamp.
int explSectorTier(int idx) {
    return (idx >= 0 && idx < kExplSectors) ? areaTier(idx) : 1;
}
const char* explSectorName(int idx) {
    return (idx >= 0 && idx < kExplSectors) ? area(idx).name : "";
}
const char* sectorTitle(int idx) {
    return (idx >= 0 && idx < kExplSectors) ? area(idx).title : "";
}
const char* explSubAreaName(int sector, int idx) {
    if (sector < 0 || sector >= kExplSectors) return "";
    if (idx < 0 || idx >= kExplSubAreas) return "";
    return area(sector).subAreas[idx];
}

namespace {
// Row-major flag lookup into a [kExplSectors * kExplSubAreas] block (may be null).
bool subFlag(const bool* block, int area, int sub) {
    return block && area >= 0 && area < kExplSectors && sub >= 0 &&
           sub < kExplSubAreas && block[area * kExplSubAreas + sub];
}
// Count of cleared sub-areas in an area (drives the "n/5" progress tag).
int clearedSubCount(const bool* subCleared, int area) {
    int n = 0;
    for (int s = 0; s < kExplSubAreas; ++s) if (subFlag(subCleared, area, s)) ++n;
    return n;
}
} // namespace

ExplRowState explRowState(int row, const bool* areaCleared, const bool* subCleared,
                          const bool* subBossUnlocked, int exploringSector,
                          int exploringSub, bool tourneyRunning) {
    if (explRowIsTourney(row)) {                          // ROCK THE DOCK, the arena
        // Opened by REACHING its water rather than by clearing it: the arena is held in
        // The Pirate Bayou, and arriving there is the invitation. That is the same
        // linear gate every area answers to, asked of the arena's own rung, so the
        // arena needs no unlock flag of its own.
        if (!explSectorOpen(kTourneyAreaIndex, areaCleared))
            return ExplRowState::TourneyLocked;
        return tourneyRunning ? ExplRowState::TourneyRunning : ExplRowState::TourneyOpen;
    }
    if (explRowIsDeepWeb(row)) {                          // the terminal zone
        // Unlocked only by clearing EVERY real area ("beat the exploration game").
        bool allCleared = true;
        for (int a = 0; a < kExplSectors; ++a)
            if (!(areaCleared && areaCleared[a])) { allCleared = false; break; }
        if (!allCleared) return ExplRowState::DeepWebLocked;
        if (exploringSector == kDeepWebSector) return ExplRowState::DeepWebDiving;
        return ExplRowState::DeepWebOpen;
    }
    const int area = explRowArea(row);
    const int sub = explRowSub(row);
    const bool areaOpen = explSectorOpen(area, areaCleared);
    if (sub < 0) {                                        // AREA header row
        if (!areaOpen) return ExplRowState::AreaLocked;
        if (areaCleared && areaCleared[area]) return ExplRowState::AreaCleared;
        if (clearedSubCount(subCleared, area) >= kExplSubAreas)
            return ExplRowState::AreaBossReady;           // all 5 cleared → area boss
        return ExplRowState::AreaProgress;
    }
    // SUB-AREA row. A sub-area is LOCKED only when its AREA is locked; every sub-area
    // of an OPEN area is reachable (you explore them in any order, one at a time).
    if (!areaOpen) return ExplRowState::SubLocked;
    // A CLEARED sub-area stays re-farmable: its boss is done, so it never
    // shows FIGHT BOSS again (cleared wins over boss-unlocked), but the row is still
    // selectable to re-arm. While it's the armed sub it reads EXPLORING (so "which one
    // am I farming" shows); otherwise CLEARED.
    if (subFlag(subCleared, area, sub)) {
        if (area == exploringSector && sub == exploringSub)
            return ExplRowState::SubExploring;
        return ExplRowState::SubCleared;
    }
    // Boss-ready takes priority over exploring so the FIGHT BOSS action stays reachable
    // even on the armed sub-area (the idle badge separately shows "exploring · BOSS
    // READY"). So SubExploring only shows before the streak unlocks the boss.
    if (subFlag(subBossUnlocked, area, sub)) return ExplRowState::SubBossReady;
    if (area == exploringSector && sub == exploringSub)
        return ExplRowState::SubExploring;
    return ExplRowState::SubOpen;
}

bool explRowSelectable(ExplRowState s) {
    switch (s) {
        case ExplRowState::AreaBossReady:                 // area-boss trigger
        case ExplRowState::AreaCleared:                   // re-run the gauntlet
        case ExplRowState::SubOpen:                       // arm explore
        case ExplRowState::SubExploring:                  // re-arm the running sub
        case ExplRowState::SubBossReady:                  // fight the sub-area boss
        case ExplRowState::SubCleared:                    // re-arm to FARM
        case ExplRowState::DeepWebOpen:                   // arm the endless dive
        case ExplRowState::DeepWebDiving:                 // re-arm the running dive
        case ExplRowState::TourneyOpen:                   // draw a fresh bracket
        case ExplRowState::TourneyRunning:                // resume the one in play
            return true;
        default:
            return false;                                 // headers / locked
    }
}

namespace {
// The right-anchored tag + its emphasis colour for a row state (the WORD carries the
// meaning; colour is only emphasis, so a grayscale screenshot stays readable).
struct RowTag { const char* text; Rgb565 col; };
RowTag rowTag(ExplRowState s) {
    switch (s) {
        case ExplRowState::AreaLocked:   return {"LOCKED",  palColor(Pal::INK_DIM)};
        case ExplRowState::AreaCleared:  return {"DONE",    palColor(Pal::CALM)};
        case ExplRowState::AreaBossReady:return {"> BOSS",  palColor(Pal::ACCENT)};
        case ExplRowState::AreaProgress: return {"",        palColor(Pal::INK_DIM)};
        case ExplRowState::SubLocked:    return {"LOCKED",  palColor(Pal::INK_DIM)};
        case ExplRowState::SubExploring: return {"EXPLORING", palColor(Pal::ACCENT)};
        case ExplRowState::SubBossReady: return {"> BOSS",  palColor(Pal::ACCENT)};
        case ExplRowState::SubCleared:   return {"DONE",    palColor(Pal::CALM)};
        case ExplRowState::SubOpen:      return {"OPEN",    palColor(Pal::CALM)};
        case ExplRowState::DeepWebLocked:return {"LOCKED",  palColor(Pal::INK_DIM)};
        case ExplRowState::DeepWebOpen:  return {"> DIVE",  palColor(Pal::ACCENT)};
        case ExplRowState::DeepWebDiving:return {"DIVING",  palColor(Pal::ACCENT)};
        case ExplRowState::TourneyLocked:return {"LOCKED",  palColor(Pal::INK_DIM)};
        case ExplRowState::TourneyOpen:  return {"> ENTER", palColor(Pal::ACCENT)};
        case ExplRowState::TourneyRunning:return {"IN PLAY", palColor(Pal::ACCENT)};
    }
    return {"", palColor(Pal::INK_DIM)};
}

// List geometry. A row is two lines — a TITLE (name + right-anchored state tag) over a
// dim DETAIL line (depth, progress, the boss waiting at the end) — with a 20x20 glyph
// column to its left. The title's x and the tag's right margin are the budget every
// area/sub-area/boss name is held to (test_expl_names_fit_their_rows).
constexpr int kListTop = 26;
constexpr int kListBottom = kActiveH - 16;           // the hint band
constexpr int kIconX = kMargin;
constexpr int kTextX = kIconX + kRowIcon + 2;
constexpr int kRowMax = 30;                          // two lines + breathing room
constexpr int kRowMin = 14;                          // one line, the packed floor
// The widest a level gets: the top level is DeepWeb + every area, inside an area it is
// the boss row + every sub-area. Both grow from area_defs.h, so neither is hand-typed.
constexpr int kLevelRowsMax = kExplLeadRows + kExplTailRows +
    (kExplSectors > kExplSubAreas ? kExplSectors : kExplSubAreas);

// The 20x20 glyph column, for a zone the player can actually see. A zone whose
// ICON_SECTOR_<AREA_ID> hasn't been drawn yet gets an empty frame rather than nothing,
// so the frame means exactly one thing — art pending — and the text beside it sits at
// the same x either way. A LOCKED row draws no glyph at all: it has nothing to show
// yet, and a column of identical frames would drown the one zone that does.
void drawIconSlot(Framebuffer& fb, const SpriteData* icon, int x, int y, Rgb565 tint) {
    if (icon) { drawSpriteTinted(fb, *icon, 0, x, y, tint); return; }
    fb.fillRect(x, y, kRowIcon, kRowIcon, palColor(Pal::TRACK));
    fb.fillRect(x + 1, y + 1, kRowIcon - 2, kRowIcon - 2, palColor(Pal::PAPER));
}

// An area's sector glyph, by the ICON_SECTOR_<AREA_ID> convention (the same
// name-IS-the-lookup rule as itemIcon, items_screen.cpp) — the id is already on the
// AreaDef row, so nothing here maps an area to a picture.
const SpriteData* sectorIcon(const ContentRegistry& reg, int areaIdx) {
    if (areaIdx < 0 || areaIdx >= kExplSectors) return nullptr;
    char name[48];
    std::snprintf(name, sizeof(name), "ICON_SECTOR_%s", area(areaIdx).id);
    for (char* c = name; *c; ++c)
        if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
    return reg.sprite(name);
}
} // namespace

void drawExplList(Framebuffer& fb, const ContentRegistry& reg, const ExplListView& v) {
    // Breadcrumb header — the nav level, spelled out on the band's title. Inside an
    // area the area's NAME is the crumb, which is why the row beneath it is the area's
    // boss and not its name again. The 5x7 font has no '>', so the row cursor's
    // triangle is the separator.
    drawHeaderBand(fb, "EXPL");
    if (v.navArea >= 0) {
        drawRowCursor(fb, kMargin + textWidth("EXPL") + 6, kTitleY,
                      palColor(Pal::INK_DIM));
        drawText(fb, kMargin + textWidth("EXPL") + 16, kTitleY,
                 explSectorName(v.navArea), palColor(Pal::ACCENT));
    }

    // THE LEVEL IS THE LIST (explRowInLevel): the top level draws the DeepWeb row plus
    // one row per AREA, and drilling into an area swaps the whole screen for that area's
    // own block. So the drawn list is ~6 rows instead of the full ladder, and the pitch
    // below can afford the second line each row carries.
    int rowOf[kLevelRowsMax];
    int rows = 0, cursorSlot = 0;
    for (int r = 0, total = explRowCount(); r < total && rows < kLevelRowsMax; ++r) {
        if (!explRowInLevel(r, v.navArea)) continue;
        if (r == v.cursor) cursorSlot = rows;
        rowOf[rows++] = r;
    }

    // ADAPTIVE pitch (the same idiom as drawShop's window): the level's rows share the
    // band, capped at the two-line height and floored at one line. Adding areas shrinks
    // the top level's rows before it ever has to scroll, so the ladder can roughly
    // double before a player has to move a viewport to see all of it.
    const int band = kListBottom - kListTop;
    const int pitch = std::max(kRowMin, std::min(kRowMax, rows > 0 ? band / rows : kRowMax));
    const int visible = std::max(1, std::min(rows, band / pitch));
    const bool twoLine = pitch >= 26;
    // The window keeps a row of LOOKAHEAD on each side of the cursor where there is one:
    // a cursor pinned to the last visible line hides whatever it is about to step onto,
    // which is the whole reason to scroll rather than page.
    int top = 0;
    if (rows > visible) {
        top = cursorSlot - visible + 2;
        if (cursorSlot - 1 < top) top = cursorSlot - 1;
        top = std::max(0, std::min(top, rows - visible));
    }

    for (int slot = top; slot < top + visible && slot < rows; ++slot) {
        const int row = rowOf[slot];
        const int areaIdx = explRowArea(row);
        const int sub = explRowSub(row);
        const ExplRowState st = explRowState(row, v.areaCleared, v.subCleared,
                                             v.subBossUnlocked, v.exploringSector,
                                             v.exploringSub, v.tourneyRunning);
        const bool focused = (row == v.cursor);
        const int y = kListTop + (slot - top) * pitch;
        // One line centres in the row; two put the title above the detail line.
        const int titleY = twoLine ? y + 5 : y + (pitch - kFontH) / 2;
        const int detailY = y + 18;
        if (focused) {
            fb.fillRect(2, y, kActiveW - 4, pitch - 2, palColor(Pal::TRACK));
            drawRowCursor(fb, 3, y + (pitch - 7) / 2, palColor(Pal::ACCENT));
        }
        // A focused ZONE title (an area, the area-boss row, the DeepWeb row) pulses
        // INK <-> ACCENT at the ~1Hz care-pip cadence. A zone title reads like a heading,
        // not a thing you can press — most of the ladder is "??????" early on, so the
        // band alone doesn't say "this one is armed and B enters it". The steady cursor
        // caret is still the non-colour channel; the pulse only draws the eye to it.
        const Rgb565 zoneInk = (focused && ((v.beat / 2) & 1) == 0)
                                   ? palColor(Pal::ACCENT) : palColor(Pal::INK);
        char detail[40];
        detail[0] = '\0';
        const char* title = "";
        Rgb565 titleInk = palColor(Pal::INK);
        RowTag tag = rowTag(st);

        if (explRowIsTourney(row)) {
            // The arena, under the ladder. It borrows its water's sector glyph rather
            // than carrying one of its own — the arena IS held in The Pirate Bayou, so
            // the picture is the true one and no asset exists only to label a menu row.
            // The divider goes ABOVE this row (the dive's goes below its own), so each
            // special row is fenced off from the ladder it sits beside.
            const bool locked = (st == ExplRowState::TourneyLocked);
            fb.fillRect(8, y, kActiveW - 16, 1, palColor(Pal::TRACK));
            if (!locked)
                drawIconSlot(fb, sectorIcon(reg, kTourneyAreaIndex), kIconX,
                             y + (pitch - kRowIcon) / 2, palColor(Pal::INK));
            title = locked ? "??????" : kTourneyName;
            titleInk = locked ? palColor(Pal::INK_DIM) : zoneInk;
            if (locked)
                ;                                    // nothing to promise but the row
            else if (v.tourneyRunning)
                std::snprintf(detail, sizeof(detail), "ROUND %d - %d LEFT",
                              v.tourneyRound + 1, v.tourneyAlive);
            else
                std::snprintf(detail, sizeof(detail), "8 OPERATORS - ONE BRACKET");
        } else if (explRowIsDeepWeb(row)) {
            // The terminal zone, top of the list. A thin divider BELOW it sets it apart
            // from the area ladder that follows; its detail line is the pet's own record,
            // which is the only progress an endless zone has.
            const bool locked = (st == ExplRowState::DeepWebLocked);
            fb.fillRect(8, y + pitch - 1, kActiveW - 16, 1, palColor(Pal::TRACK));
            if (!locked)
                drawIconSlot(fb, reg.sprite(kDeepWebIcon), kIconX,
                             y + (pitch - kRowIcon) / 2, palColor(Pal::INK));
            title = locked ? "??????" : "DEEPWEB DIVE";
            titleInk = locked ? palColor(Pal::INK_DIM) : zoneInk;
            if (!locked && v.bestDeepWebDepth > 0)
                std::snprintf(detail, sizeof(detail), "ENDLESS - BEST DEPTH %d",
                              v.bestDeepWebDepth);
            else if (!locked)
                std::snprintf(detail, sizeof(detail), "ENDLESS - NO DIVE YET");
        } else if (sub < 0 && v.navArea < 0) {
            // TOP level: the area itself, as a zone to pick. Its tier is the ladder depth
            // (areaTier) — how hard, before you commit — over its clear count.
            const bool locked = (st == ExplRowState::AreaLocked);
            if (!locked)
                drawIconSlot(fb, sectorIcon(reg, areaIdx), kIconX,
                             y + (pitch - kRowIcon) / 2, palColor(Pal::INK));
            title = locked ? "??????" : explSectorName(areaIdx);
            titleInk = locked ? palColor(Pal::INK_DIM) : zoneInk;
            if (!locked && twoLine) {
                drawDifficulty(fb, kTextX, detailY, explSectorTier(areaIdx), kExplSectors);
                std::snprintf(detail, sizeof(detail), "%d/%d CLEARED",
                              clearedSubCount(v.subCleared, areaIdx), kExplSubAreas);
                drawText(fb, kTextX + kExplSectors * 8 + 8, detailY, detail,
                         palColor(Pal::INK_DIM));
                detail[0] = '\0';                    // already drawn, beside the pips
            }
        } else if (sub < 0) {
            // INSIDE an area: the header row IS the area gauntlet. The breadcrumb already
            // names the area, so this row spends its title on the BOSS — named only once
            // it's reachable, so the marquee fight stays a reveal rather than a spoiler
            // on a row you can't press yet.
            drawIconSlot(fb, sectorIcon(reg, areaIdx), kIconX, y + (pitch - kRowIcon) / 2,
                         palColor(Pal::INK));
            const bool named = (st == ExplRowState::AreaBossReady ||
                                st == ExplRowState::AreaCleared);
            title = named ? area(areaIdx).areaBossName : "AREA GAUNTLET";
            titleInk = named ? zoneInk : palColor(Pal::INK_DIM);
            fb.fillRect(8, y + pitch - 1, kActiveW - 16, 1, palColor(Pal::TRACK));
            if (st == ExplRowState::AreaCleared) {
                // A cleared gauntlet stays RE-RUNNABLE, the same way a cleared sub-area
                // stays re-farmable — so inside the area this row is an action, not a
                // verdict, and its tag has to say so. At the TOP level the identical
                // state means "this zone is done" and keeps rowTag's plain CLEARED.
                tag = {"> RERUN", palColor(Pal::ACCENT)};
                std::snprintf(detail, sizeof(detail), "TITLE: %s", sectorTitle(areaIdx));
            }
            else if (st == ExplRowState::AreaBossReady)
                std::snprintf(detail, sizeof(detail), "ALL %d BOSSES, NO HEAL",
                              kExplSubAreas);
            else
                std::snprintf(detail, sizeof(detail), "%d/%d SUB-AREAS CLEARED",
                              clearedSubCount(v.subCleared, areaIdx), kExplSubAreas);
        } else {
            // SUB-AREA — the number takes the glyph column, and the detail line answers
            // "what do I get / how far off is it" for whichever state the row is in.
            const bool locked = (st == ExplRowState::SubLocked);
            char num[4];
            std::snprintf(num, sizeof(num), "%d", sub + 1);
            drawText(fb, kIconX + (kRowIcon - textWidth(num)) / 2,
                     y + (pitch - kFontH) / 2, num, palColor(Pal::INK_DIM));
            title = locked ? "??????" : explSubAreaName(areaIdx, sub);
            titleInk = locked ? palColor(Pal::INK_DIM) : palColor(Pal::INK);
            switch (st) {
                case ExplRowState::SubBossReady:
                    std::snprintf(detail, sizeof(detail), "BOSS: %s",
                                  area(areaIdx).subBosses[sub].name);
                    break;
                case ExplRowState::SubExploring:
                    std::snprintf(detail, sizeof(detail), "WINS %d/%d", v.streakWins,
                                  v.winsToBoss);
                    break;
                case ExplRowState::SubCleared:
                    std::snprintf(detail, sizeof(detail), "RE-ARM TO FARM");
                    break;
                case ExplRowState::SubOpen:
                    std::snprintf(detail, sizeof(detail), "BOSS AT %d WINS",
                                  v.winsToBoss);
                    break;
                default: break;                      // locked — the tag says it all
            }
        }

        // Title and detail are both held to the room their row actually leaves — the
        // tag's width varies by state, so the budget is computed here rather than
        // assumed. The focused row scrolls what doesn't fit; the rest clip (widgets.h).
        const int tagW = tag.text[0] ? textWidth(tag.text) + kMargin : 0;
        drawTextMarquee(fb, kTextX, titleY, kActiveW - kMargin - tagW - kTextX, title,
                        titleInk, v.beat, focused);
        if (tag.text[0])
            drawText(fb, kActiveW - kMargin - textWidth(tag.text), titleY, tag.text,
                     tag.col);
        if (twoLine && detail[0])
            drawTextMarquee(fb, kTextX, detailY, kActiveW - kMargin - kTextX, detail,
                            palColor(Pal::INK_DIM), v.beat, focused);
    }

    // Slim scrollbar (UI_SCROLLBAR), matching items/cfg — the non-colour "there's more"
    // channel: a TRACK rail with an ACCENT thumb sized/placed by the window position.
    // Only a ladder long enough to outgrow even the packed pitch ever draws it.
    if (rows > visible) {
        const int barX = kActiveW - 3;
        const int trackH = visible * pitch;
        fb.fillRect(barX, kListTop, 2, trackH, palColor(Pal::TRACK));
        const int thumbH = std::max(8, trackH * visible / rows);
        const int thumbY = kListTop + trackH * top / rows;
        fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::ACCENT));
    }

    // Footer hint — context-sensitive B on the focused row (grayscale-safe).
    const ExplRowState focus =
        (v.cursor >= 0 && v.cursor < explRowCount())
            ? explRowState(v.cursor, v.areaCleared, v.subCleared, v.subBossUnlocked,
                           v.exploringSector, v.exploringSub)
            : ExplRowState::AreaProgress;
    // Two-level footer. TOP level: A cycles the zones; B DIVEs the DeepWeb row or ENTERs
    // an area's sub-areas. INSIDE an area: B acts on the focused sub/boss; C pops back
    // out to the zone list.
    const char* hint;
    if (v.navArea < 0) {
        hint = (focus == ExplRowState::DeepWebOpen ||
                focus == ExplRowState::DeepWebDiving) ? "A ZONE  B DIVE  C BACK"
                                                      : "A ZONE  B ENTER  C BACK";
    } else {
        hint = "A NEXT  B EXPLORE  C BACK";
        if (focus == ExplRowState::SubBossReady) hint = "A NEXT  B FIGHT BOSS  C BACK";
        else if (focus == ExplRowState::AreaBossReady) hint = "A NEXT  B AREA BOSS  C BACK";
        else if (focus == ExplRowState::AreaCleared) hint = "A NEXT  B RERUN BOSS  C BACK";
        else if (focus == ExplRowState::SubCleared) hint = "A NEXT  B FARM  C BACK";
    }
    drawHintBand(fb, hint);
}

// Explore-mode idle badge: a thin status line under the top track — a
// pulsing cursor + "EXPL <label>" (the armed sub-area) on the left, "WINS n/N" (a
// fraction) or "BOSS READY" (a tag) on the right. Dual-coded (glyph + text +
// fraction), so the running state, which sub-area, and the streak all read in
// grayscale. Drawn over the idle habitat while explore-mode is active.
void drawExploreBadge(Framebuffer& fb, const char* label, int count, int countMax,
                      ExploreBadgeMode mode) {
    const int y = kLivingTop + 4;
    drawRowCursor(fb, kMargin, y, palColor(Pal::ACCENT));
    char left[28];
    std::snprintf(left, sizeof(left), "EXPL %s", label ? label : "");
    drawText(fb, kMargin + 12, y, left, palColor(Pal::INK));
    // Right field, dual-coded (a distinct WORD per mode so it reads in grayscale).
    char right[16];
    Rgb565 rc = palColor(Pal::INK_DIM);
    switch (mode) {
        case ExploreBadgeMode::BossReady:
            std::snprintf(right, sizeof(right), "BOSS READY");
            rc = palColor(Pal::ACCENT); break;
        case ExploreBadgeMode::Farming:
            // count = Bandwidth remaining, countMax = pool. Full loot while it
            // lasts ("FARM n/N", CALM); once depleted the decay applies
            // ("FARM LOW", WARN). Dual-coded: distinct WORD + a fraction/absence.
            if (count > 0) {
                std::snprintf(right, sizeof(right), "FARM %d/%d", count, countMax);
                rc = palColor(Pal::CALM);
            } else {
                std::snprintf(right, sizeof(right), "FARM LOW");
                rc = palColor(Pal::WARN);
            }
            break;
        case ExploreBadgeMode::DeepDive:
            std::snprintf(right, sizeof(right), "DEPTH %d", count);
            rc = palColor(Pal::ACCENT); break;
        case ExploreBadgeMode::Wins:
        default:
            std::snprintf(right, sizeof(right), "WINS %d/%d", count, countMax); break;
    }
    drawText(fb, kActiveW - kMargin - textWidth(right), y, right, rc);
}

// Explore-control overlay: the A+C chord opens this brief 3-action strip
// over the habitat — A Network Ping (force the next step) · B Warp (if a key is
// held) · C Stop. Standard A/B/C returns, spelled out (grayscale-safe). A bordered
// PAPER panel (TRACK outline via a 2px inset) centered over the living area.
void drawExploreControl(Framebuffer& fb, int cursor, bool hasWarpKey,
                        bool autoProgress) {
    const int boxW = 184, boxH = 124;
    const int bx = (kActiveW - boxW) / 2, by = (kActiveH - boxH) / 2;
    fb.fillRect(bx - 2, by - 2, boxW + 4, boxH + 4, palColor(Pal::TRACK));
    fb.fillRect(bx, by, boxW, boxH, palColor(Pal::PAPER));
    drawText(fb, bx + 10, by + 8, "EXPLORE", palColor(Pal::INK));
    fb.fillRect(bx + 8, by + 22, boxW - 16, 1, palColor(Pal::TRACK));

    // One row per action, cursor-highlighted — the same filled-track + caret idiom every
    // other list uses, so it reads in grayscale. WARP with no key held stays on the list
    // rather than being skipped: the row is what says the action exists at all, and its
    // reason is spelled out beside it.
    static const char* kLabels[kExploreControlRows] = {
        "NETWORK PING", "WARP", "AUTO-PROGRESS", "STOP EXPLORE"};
    const int rowTop = by + 28, rowH = 18;
    for (int i = 0; i < kExploreControlRows; ++i) {
        const int y = rowTop + i * rowH;
        const bool focused = (i == cursor);
        const bool inert = (i == static_cast<int>(ExploreControlRow::Warp)) && !hasWarpKey;
        if (focused) {
            fb.fillRect(bx + 6, y - 3, boxW - 12, rowH - 2, palColor(Pal::TRACK));
            drawRowCursor(fb, bx + 8, y, palColor(Pal::ACCENT));
        }
        drawText(fb, bx + 20, y, kLabels[i],
                 inert ? palColor(Pal::INK_DIM) : palColor(Pal::INK));
        // Right field: the mode's ON/OFF, or WARP's reason for being inert. Both are
        // WORDS — nothing here is carried by colour alone.
        const char* right = nullptr;
        Rgb565 rc = palColor(Pal::INK_DIM);
        if (i == static_cast<int>(ExploreControlRow::AutoProgress)) {
            right = autoProgress ? "ON" : "OFF";
            if (autoProgress) rc = palColor(Pal::ACCENT);
        } else if (inert) {
            right = "NO KEY";
        }
        if (right)
            drawText(fb, bx + boxW - 10 - textWidth(right), y, right, rc);
    }

    const char* hint = "A NEXT  B DO  C BACK";
    drawText(fb, bx + (boxW - textWidth(hint)) / 2, by + boxH - 14, hint,
             palColor(Pal::INK_DIM));
}

void drawEncounterIntro(Framebuffer& fb, const char* enemyName, int diffPips,
                        int level, const SpriteData* enemySprite,
                        bool sinkholeAvailable, int choice, int beat) {
    drawHeaderBand(fb, "WILD MALBEAST", nullptr, palColor(Pal::INK_DIM),
                   palColor(Pal::WARN));

    if (enemySprite) {
        const int frame = idleFrame(*enemySprite, beat);
        const int w = enemySprite->frameW * kScaleNum / kScaleDen;
        const int h = enemySprite->h * kScaleNum / kScaleDen;
        const int x = (kActiveW - w) / 2;
        const int y = 96 - h;
        // Turned the way the RIGHT-hand combat seat will hold it (ui/combat_screen.cpp),
        // which is the seat every wild takes: the creature the player is sizing up here
        // is the one they are about to face, and it should not turn round between the two
        // screens. Centred rather than seated, so the mirror is the whole of the change.
        drawSpriteUpscaled(fb, *enemySprite, frame, x, y, kScaleNum, kScaleDen, /*row=*/0,
                           spriteMirrorToFace(*enemySprite, /*faceRight=*/false));
    }
    drawText(fb, kMargin, 100, enemyName, palColor(Pal::INK));
    char lvBuf[8];
    std::snprintf(lvBuf, sizeof(lvBuf), "Lv %d", level);
    drawText(fb, kActiveW - kMargin - textWidth(lvBuf), 100, lvBuf,
             palColor(Pal::INK_DIM));
    drawDifficulty(fb, kMargin, 114, diffPips);

    static const char* kOptions[3] = {"FIGHT", "FLEE", "SINKHOLE"};
    const int n = sinkholeAvailable ? 3 : 2;
    const int top = 140, rowH = 18;
    for (int i = 0; i < n; ++i) {
        const int y = top + i * rowH;
        if (i == choice) drawRowCursor(fb, 8, y, palColor(Pal::ACCENT));
        drawText(fb, 20, y, kOptions[i],
                 i == choice ? palColor(Pal::ACCENT) : palColor(Pal::INK));
    }

    // Mandatory-in-spirit hint band (the wireframe,, shows it) — C's
    // "flee" meaning here isn't the standard back-contract, so it's spelled out.
    const char* hint = "A SWITCH  B CONFIRM  C FLEE";
    drawHintBand(fb, hint);
}

namespace {
// The absorb stage: where the pet stands. The dissolve finishes well inside
// kExploreRevealHoldBeats, so the pet is standing with its afterglow when the screen
// auto-continues.
constexpr int kWifiStageCy = 140;

// How much of the glyph each discovery takes. A nibble is deliberately well short of
// half: the network has to still be recognisably standing there afterwards, because
// "some of it" is the whole reading.
constexpr uint8_t kWifiNibbleBite = 96;

uint8_t wifiAbsorbBite(WifiAbsorb a) {
    switch (a) {
        case WifiAbsorb::Whole: return 255;
        case WifiAbsorb::Nibble: return kWifiNibbleBite;
        case WifiAbsorb::Untouched: return 0;
        case WifiAbsorb::None: break;
    }
    return 0;
}
}  // namespace

void drawWifiEvent(Framebuffer& fb, const char* sectorName, const char* banner,
                   const char* outcomeLine, const char* discoveryLine,
                   const SpriteData* pet, WifiAbsorb absorb, int beat, int fxBeat) {
    drawHeaderBand(fb, sectorName);

    // The three lines stack immediately under the header rule so the whole lower half
    // of the canvas belongs to the pet and the glyph it is working on. The
    // real-network-discovery beat (new / familiar / home-turf / empty-queue) is an
    // independent line from the guardian/cache/friendly one, drawn only when
    // resolveNetworkDiscovery() actually set it.
    // The discovery line carries a real SSID, so it is the one that can outrun the
    // canvas — it scrolls inside the margins rather than being centred off both edges.
    if (discoveryLine && discoveryLine[0])
        drawTextMarquee(fb, kMargin, 32, kActiveW - 2 * kMargin, discoveryLine,
                        palColor(Pal::INK_DIM), beat, true);

    if (banner && banner[0])
        drawText(fb, (kActiveW - textWidth(banner)) / 2, 50, banner,
                 palColor(Pal::ACCENT));
    if (outcomeLine && outcomeLine[0])
        drawText(fb, (kActiveW - textWidth(outcomeLine)) / 2, 68, outcomeLine,
                 palColor(Pal::INK_DIM));

    // The pet, and the network going into it. WifiAbsorb::None had nothing queued to
    // find, so there is no glyph to draw at all — the pet just stands there, which is
    // exactly what "WANTS NEW NETWORKS..." means.
    const AbsorbPhase phase = absorbPhase(fxBeat, kAbsorbLeadBeats, kAbsorbBeats,
                                          wifiAbsorbBite(absorb));
    drawPetAbsorbing(fb, pet, kActiveW / 2, kWifiStageCy,
                     absorb == WifiAbsorb::None ? nullptr : &ASSET_ICON_SYS_WIFI,
                     palColor(Pal::ACCENT), phase, beat);

    const char* hint = "B CONTINUE";
    drawHintBand(fb, hint);
}

namespace {
constexpr int kShopRowTop = 34;
constexpr int kShopRowPitch = 40;
// Ceiling on the storefront list, not its fixed height — see drawShop's adaptive
// window.
constexpr int kShopMaxRows = 3;

// The row's full price as one line — Bits plus every extra item cost it carries
// (an arbitrary-length list, not just Bits alone — docs/CONTENT_STANDARD.md).
void formatShopPrice(char* buf, size_t n, const ShopRowView& row) {
    int off = std::snprintf(buf, n, "%dB", row.bitsPrice);
    for (int i = 0; i < row.costCount && off > 0 && static_cast<size_t>(off) < n; ++i)
        off += std::snprintf(buf + off, n - off, " +%d %s", row.costQty[i], row.costName[i]);
}

// The buy reason's own line, pinned just above the hint band. It is the VERDICT on the
// focused listing, not a tail of the description, so it sits in one place whatever the
// prose ran to and lets the description's unfilled reserve be the gap between the two —
// the same footer grouping drawItemDetail uses for its HAVE/action pair. Set flowing at
// a few px under the prose it read as the description's last line instead.
constexpr int kShopStatusY = kActiveH - 16 - (kFontH + 6);

// Lines the description gets for a list of `visibleRows`: the band between the last
// listing and kShopStatusY. At the row cap this is kShopDescLines (expl_screen.h — the
// worst case a stocked row has to clear); a one-listing storefront gets several times
// that.
int shopDescLines(int visibleRows) {
    const int top = kShopRowTop + visibleRows * kShopRowPitch + 2;
    return std::max(1, (kShopStatusY - top) / (kFontH + 3));
}

// One storefront row: name, full price (Bits + any item costs), and remaining
// stock as a count (so it reads in grayscale — sold out shows STOCK 0, not just a
// greyed row). `selected` draws the row cursor.
void drawShopRow(Framebuffer& fb, int rowY, const ShopRowView& row, bool selected,
                 int beat) {
    fb.fillRect(4, rowY - 2, kActiveW - 8, kShopRowPitch - 6, palColor(Pal::TRACK));
    if (selected) drawRowCursor(fb, 10, rowY + 4, palColor(Pal::ACCENT));
    drawTextMarquee(fb, 24, rowY + 2, kActiveW - kMargin - 24, row.name,
                    palColor(Pal::INK), beat, selected);

    // The price line carries the item costs too, so it is the half that grows; STOCK
    // is the fact a shopper cannot lose, so the price yields to it.
    char priceStr[48];
    formatShopPrice(priceStr, sizeof(priceStr), row);
    char stockStr[16];
    std::snprintf(stockStr, sizeof(stockStr), "STOCK %d", row.stock);
    drawLabelValue(fb, 24, rowY + 16, priceStr, palColor(Pal::INK_DIM), stockStr,
                   row.stock > 0 ? palColor(Pal::CALM) : palColor(Pal::WARN),
                   beat, selected);
}
} // namespace

void drawShop(Framebuffer& fb, const char* storeName, int walletBits,
              const ShopRowView* rows, int rowCount, int cursor,
              const char* selectedDescription, const char* statusLine, int beat) {
    // Storefront banner + wallet header (dual-coded: the numbers carry meaning,
    // colour is only emphasis). The banner scrolls when the wallet crowds it —
    // a store name is the screen's whole identity, so it is not the half that gives.
    char wallet[16];
    std::snprintf(wallet, sizeof(wallet), "%dB", walletBits);
    drawHeaderBand(fb, "", wallet, palColor(Pal::ACCENT));
    drawTextMarquee(fb, kMargin, kTitleY,
                    kActiveW - 2 * kMargin - textWidth(wallet) - kMargin, storeName,
                    palColor(Pal::INK), beat, true);

    // Windowed list (mirrors the Rig Shop, game_hacker.cpp's drawHackerSubmenu) —
    // an arbitrary listing count scrolls past kShopMaxRows instead of growing
    // the screen.
    // ADAPTIVE window: a storefront stocks one or two listings, so drawing only the
    // rows it has hands 40-80px back to the description below instead of reserving the
    // cap empty. Past the cap it scrolls as before.
    const int visibleRows = std::max(1, std::min(rowCount, kShopMaxRows));
    int scrollTop = 0;
    if (rowCount > kShopMaxRows) {
        if (cursor < scrollTop) scrollTop = cursor;
        if (cursor >= scrollTop + kShopMaxRows) scrollTop = cursor - kShopMaxRows + 1;
        scrollTop = std::max(0, std::min(scrollTop, rowCount - kShopMaxRows));
    }
    for (int v = 0; v < visibleRows && scrollTop + v < rowCount; ++v) {
        const int row = scrollTop + v;
        drawShopRow(fb, kShopRowTop + v * kShopRowPitch, rows[row], row == cursor, beat);
    }
    if (rowCount > kShopMaxRows) {   // UI_SCROLLBAR
        const int barX = kActiveW - 3;
        const int trackH = kShopMaxRows * kShopRowPitch;
        fb.fillRect(barX, kShopRowTop, 2, trackH, palColor(Pal::TRACK));
        const int thumbH = std::max(8, trackH * kShopMaxRows / rowCount);
        const int thumbY = kShopRowTop + trackH * scrollTop / rowCount;
        fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
    }

    // Selected row's description + the buy-availability reason, spelled out so
    // both survive grayscale — "SOLD OUT" / "NOT ENOUGH BITS" / "NOT ENOUGH ITEMS" /
    // "B TO BUY".
    // The description is a content row's own prose (core/content/content_*.cpp), far
    // wider than the 208px here, so it wraps into the band above kShopStatusY; the buy
    // reason takes that line whatever the prose ran to.
    const int descY = kShopRowTop + visibleRows * kShopRowPitch + 2;
    if (selectedDescription && selectedDescription[0])
        drawTextWrapped(fb, kMargin, descY, kActiveW - 2 * kMargin,
                        selectedDescription, palColor(Pal::INK_DIM),
                        kFontH + 3, shopDescLines(visibleRows));
    if (statusLine && statusLine[0])
        drawText(fb, (kActiveW - textWidth(statusLine)) / 2, kShopStatusY, statusLine,
                 palColor(Pal::INK_DIM));

    const char* hint = rowCount > 1 ? "A NEXT  B BUY  C LEAVE" : "B BUY   C LEAVE";
    drawHintBand(fb, hint);
}

void drawWarpPicker(Framebuffer& fb, const char* const* keyNames, int keyCount,
                    int cursor) {
    // Header: what this is, spelled out so it reads without colour.
    drawHeaderBand(fb, "WARP KEY");
    drawText(fb, kMargin, 28, "JUMP TO...", palColor(Pal::INK_DIM));

    // One row per held key, cursor-highlighted (a filled track + the row cursor —
    // the same idiom as the sector list / shop row, so it reads in grayscale).
    for (int i = 0; i < keyCount; ++i) {
        const int y = kTrackRowTop + i * kRowH;
        if (i == cursor) {
            fb.fillRect(4, y + 2, kActiveW - 8, kRowH - 4, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + (kRowH - 7) / 2, palColor(Pal::ACCENT));
        }
        drawText(fb, 24, y + (kRowH - kFontH) / 2, keyNames[i],
                 palColor(Pal::INK));
    }

    const char* hint = "A NEXT  B USE  C BACK";
    drawHintBand(fb, hint);
}

// Post-encounter status readout: a brief, informational-only
// overlay — no decision, so the hint band spells out the one break from the
// standard A/B/C contract (ANY button dismisses, not just B/C).
void drawPostEncounter(Framebuffer& fb, int bwBefore, int bwAfter, int bwMax,
                       PostEncounterFragState fragState, int fragDelta,
                       const char* levelLine) {
    drawHeaderBand(fb, "STATUS");

    // Bandwidth line: current/max + this encounter's delta, dual-coded by the
    // explicit signed number — except when there was nothing left to spend
    // (bwBefore == 0), where a "+0"/"-0" delta would misleadingly read as "no
    // change happened" instead of "nothing to spend" (feedback intent).
    char bwLine[32];
    if (bwBefore <= 0) {
        std::snprintf(bwLine, sizeof(bwLine), "BANDWIDTH %d/%d  LOW", bwAfter, bwMax);
        drawText(fb, kMargin, 60, bwLine, palColor(Pal::WARN));
    } else {
        std::snprintf(bwLine, sizeof(bwLine), "BANDWIDTH %d/%d  %+d", bwAfter, bwMax,
                     bwAfter - bwBefore);
        drawText(fb, kMargin, 60, bwLine, palColor(Pal::INK));
    }

    // Fragmentation line: omitted entirely when frag wasn't in play this
    // encounter (None) — a distinct WORD (not colour alone) carries SHIELDED vs
    // the rise amount.
    if (fragState != PostEncounterFragState::None) {
        char fragLine[24];
        if (fragState == PostEncounterFragState::Shielded) {
            std::snprintf(fragLine, sizeof(fragLine), "FRAG SHIELDED");
            drawText(fb, kMargin, 80, fragLine, palColor(Pal::CALM));
        } else {
            std::snprintf(fragLine, sizeof(fragLine), "FRAG +%d", fragDelta);
            drawText(fb, kMargin, 80, fragLine, palColor(Pal::HOT));
        }
    }

    // Level-up line: naming the stat a fight's XP raised so growth isn't
    // silent. A gain is good news → the CALM zone; the stat WORD + "+n" carry the
    // read without colour. Sits below the frag row (which tops out at y=80).
    if (levelLine && levelLine[0])
        drawText(fb, kMargin, 100, levelLine, palColor(Pal::CALM));

    const char* hint = "ANY BUTTON";
    drawHintBand(fb, hint);
}

} // namespace mal
