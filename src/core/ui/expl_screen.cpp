#include "core/ui/expl_screen.h"

#include <algorithm>
#include <cstdio>
#include <cstddef>

#include "core/render/canvas.h"
#include "core/render/font5x7.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {

constexpr int kMargin = 8;
constexpr int kRowTop = 40;
constexpr int kRowH = 28;

// Difficulty pips: `tier` filled diamonds out of 3 (UI_DIFFICULTY_PIPS stub —
// small filled/empty squares so the tier reads in grayscale by count + fill).
void drawDifficulty(Framebuffer& fb, int x, int y, int tier) {
    for (int i = 0; i < 3; ++i) {
        const int px = x + i * 8;
        if (i < tier) fb.fillRect(px, y, 5, 5, palColor(Pal::ACCENT));
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
                          int exploringSub) {
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
        case ExplRowState::SubOpen:                       // arm explore
        case ExplRowState::SubExploring:                  // re-arm the running sub
        case ExplRowState::SubBossReady:                  // fight the sub-area boss
        case ExplRowState::SubCleared:                    // re-arm to FARM
        case ExplRowState::DeepWebOpen:                   // arm the endless dive
        case ExplRowState::DeepWebDiving:                 // re-arm the running dive
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
        case ExplRowState::AreaLocked:   return {"LOCKED",       palColor(Pal::INK_DIM)};
        case ExplRowState::AreaCleared:  return {"CLEARED",      palColor(Pal::CALM)};
        case ExplRowState::AreaBossReady:return {"> AREA BOSS",  palColor(Pal::ACCENT)};
        case ExplRowState::AreaProgress: return {"",             palColor(Pal::INK_DIM)};
        case ExplRowState::SubLocked:    return {"LOCKED",       palColor(Pal::INK_DIM)};
        case ExplRowState::SubExploring: return {"EXPLORING",    palColor(Pal::ACCENT)};
        case ExplRowState::SubBossReady: return {"> FIGHT BOSS", palColor(Pal::ACCENT)};
        case ExplRowState::SubCleared:   return {"CLEARED",      palColor(Pal::CALM)};
        case ExplRowState::SubOpen:      return {"OPEN",         palColor(Pal::CALM)};
        case ExplRowState::DeepWebLocked:return {"LOCKED",       palColor(Pal::INK_DIM)};
        case ExplRowState::DeepWebOpen:  return {"> DIVE",       palColor(Pal::ACCENT)};
        case ExplRowState::DeepWebDiving:return {"DIVING",       palColor(Pal::ACCENT)};
    }
    return {"", palColor(Pal::INK_DIM)};
}
} // namespace

void drawExplList(Framebuffer& fb, int cursor, const bool* areaCleared,
                  const bool* subCleared, const bool* subBossUnlocked,
                  int exploringSector, int exploringSub, int navArea, int beat) {
    fb.clear(palColor(Pal::PAPER));
    drawText(fb, kMargin, 6, "EXPL", palColor(Pal::INK));
    fb.fillRect(0, 22, kActiveW, 1, palColor(Pal::TRACK));

    // Nested rows: the DeepWeb Dive row FIRST (row 0, the top farming zone), then area
    // headers + 5 numbered sub-areas each. The ladder outgrew the screen at 3 areas, so
    // it scrolls in a cursor-
    // following viewport (same idiom as items/cfg): a minimal window that keeps the
    // cursor visible, plus a slim scrollbar when there's more than one screenful.
    constexpr int kListTop = 26;
    constexpr int kRow = 14;
    constexpr int kVisibleRows = 13;                 // 26..208, just above the footer
    const int rows = explRowCount();
    int scrollTop = 0;
    if (rows > kVisibleRows) {
        if (cursor >= kVisibleRows) scrollTop = cursor - kVisibleRows + 1;
        const int maxTop = rows - kVisibleRows;
        if (scrollTop > maxTop) scrollTop = maxTop;
        if (scrollTop < 0) scrollTop = 0;
    }
    for (int v = 0; v < kVisibleRows && scrollTop + v < rows; ++v) {
        const int row = scrollTop + v;
        const int area = explRowArea(row);
        const int sub = explRowSub(row);
        const ExplRowState st = explRowState(row, areaCleared, subCleared,
                                             subBossUnlocked, exploringSector,
                                             exploringSub);
        const int y = kListTop + v * kRow;
        const int ty = y + (kRow - kFontH) / 2;
        if (row == cursor) {
            fb.fillRect(2, y, kActiveW - 4, kRow, palColor(Pal::TRACK));
            drawRowCursor(fb, 3, y + (kRow - 7) / 2, palColor(Pal::ACCENT));
        }
        // A focused ZONE title (an area header or the DeepWeb row) pulses INK <-> ACCENT
        // at the ~1Hz care-pip cadence. A zone title reads like a heading, not a thing
        // you can press — most of the ladder is "??????" early on, so the band alone
        // doesn't say "this one is armed and B enters it". The steady cursor caret is
        // still the non-colour channel; the pulse only draws the eye to it.
        const Rgb565 zoneInk = (row == cursor && ((beat / 2) & 1) == 0)
                                   ? palColor(Pal::ACCENT) : palColor(Pal::INK);
        if (explRowIsDeepWeb(row)) {
            // terminal zone — now the FIRST row (top of the list); a thin divider
            // BELOW it sets it apart from the area ladder that follows. Full-strength name
            // when unlocked, "??????" while locked. No sub number.
            const bool locked = (st == ExplRowState::DeepWebLocked);
            fb.fillRect(8, y + kRow - 1, kActiveW - 16, 1, palColor(Pal::TRACK));
            drawText(fb, 12, ty, locked ? "??????" : "DEEPWEB DIVE",
                     locked ? palColor(Pal::INK_DIM) : zoneInk);
        } else if (sub < 0) {
            // AREA header — the area name (or "??????" when locked), full-strength.
            const bool locked = (st == ExplRowState::AreaLocked);
            drawText(fb, 12, ty, locked ? "??????" : explSectorName(area),
                     locked ? palColor(Pal::INK_DIM) : zoneInk);
            if (st == ExplRowState::AreaProgress) {
                char frac[8];
                std::snprintf(frac, sizeof(frac), "%d/%d",
                              clearedSubCount(subCleared, area), kExplSubAreas);
                drawText(fb, kActiveW - kMargin - textWidth(frac), ty, frac,
                         palColor(Pal::INK_DIM));
            }
        } else {
            // SUB-AREA — indented, numbered 1..5, with its name (or "??????" locked).
            const bool locked = (st == ExplRowState::SubLocked);
            char num[4];
            std::snprintf(num, sizeof(num), "%d", sub + 1);
            drawText(fb, 22, ty, num, palColor(Pal::INK_DIM));
            drawText(fb, 34, ty, locked ? "??????" : explSubAreaName(area, sub),
                     locked ? palColor(Pal::INK_DIM) : palColor(Pal::INK));
        }
        const RowTag tag = rowTag(st);
        if (tag.text[0])
            drawText(fb, kActiveW - kMargin - textWidth(tag.text), ty, tag.text,
                     tag.col);
    }

    // Slim scrollbar (UI_SCROLLBAR), matching items/cfg — the non-colour "there's more"
    // channel: a TRACK rail with an ACCENT thumb sized/placed by the window position.
    if (rows > kVisibleRows) {
        const int barX = kActiveW - 3;
        const int trackH = kVisibleRows * kRow;
        fb.fillRect(barX, kListTop, 2, trackH, palColor(Pal::TRACK));
        int thumbH = trackH * kVisibleRows / rows;
        if (thumbH < 8) thumbH = 8;
        const int thumbY = kListTop + trackH * scrollTop / rows;
        fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::ACCENT));
    }

    // Footer hint — context-sensitive B on the focused row (grayscale-safe).
    const ExplRowState focus =
        (cursor >= 0 && cursor < rows)
            ? explRowState(cursor, areaCleared, subCleared, subBossUnlocked,
                           exploringSector, exploringSub)
            : ExplRowState::AreaProgress;
    // Two-level footer. TOP level: A cycles areas + DeepWeb; B DIVEs the
    // DeepWeb row or ENTERs an area's sub-areas. INSIDE an area: B acts on the focused
    // sub/boss; C pops back out to the area list.
    const char* hint;
    if (navArea < 0) {
        hint = (focus == ExplRowState::DeepWebOpen ||
                focus == ExplRowState::DeepWebDiving) ? "A AREA  B DIVE  C BACK"
                                                      : "A AREA  B ENTER  C BACK";
    } else {
        hint = "A NEXT  B EXPLORE  C BACK";
        if (focus == ExplRowState::SubBossReady) hint = "A NEXT  B FIGHT BOSS  C BACK";
        else if (focus == ExplRowState::AreaBossReady) hint = "A NEXT  B AREA BOSS  C BACK";
        else if (focus == ExplRowState::SubCleared) hint = "A NEXT  B FARM  C BACK";
    }
    fb.fillRect(0, kActiveH - 16, kActiveW, 16, palColor(Pal::TRACK));
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kActiveH - 12, hint,
             palColor(Pal::INK));
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
void drawExploreControl(Framebuffer& fb, bool hasWarpKey) {
    const int boxW = 184, boxH = 88;
    const int bx = (kActiveW - boxW) / 2, by = (kActiveH - boxH) / 2;
    fb.fillRect(bx - 2, by - 2, boxW + 4, boxH + 4, palColor(Pal::TRACK));
    fb.fillRect(bx, by, boxW, boxH, palColor(Pal::PAPER));
    drawText(fb, bx + 10, by + 8, "EXPLORE", palColor(Pal::INK));
    fb.fillRect(bx + 8, by + 22, boxW - 16, 1, palColor(Pal::TRACK));
    drawText(fb, bx + 10, by + 30, "A  NETWORK PING", palColor(Pal::INK));
    drawText(fb, bx + 10, by + 46, hasWarpKey ? "B  WARP" : "B  WARP (NO KEY)",
             hasWarpKey ? palColor(Pal::INK) : palColor(Pal::INK_DIM));
    drawText(fb, bx + 10, by + 62, "C  STOP EXPLORE", palColor(Pal::INK));
}

void drawEncounterIntro(Framebuffer& fb, const char* enemyName, int diffPips,
                        int level, const SpriteData* enemySprite,
                        bool sinkholeAvailable, int choice, int beat) {
    fb.clear(palColor(Pal::PAPER));
    drawText(fb, kMargin, 6, "WILD MALBEAST", palColor(Pal::WARN));
    fb.fillRect(0, 22, kActiveW, 1, palColor(Pal::TRACK));

    if (enemySprite) {
        const int frame = idleFrame(*enemySprite, beat);
        const int w = enemySprite->frameW * kScaleNum / kScaleDen;
        const int h = enemySprite->h * kScaleNum / kScaleDen;
        const int x = (kActiveW - w) / 2;
        const int y = 96 - h;
        drawSpriteUpscaled(fb, *enemySprite, frame, x, y, kScaleNum, kScaleDen);
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
    fb.fillRect(0, kActiveH - 16, kActiveW, 16, palColor(Pal::TRACK));
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kActiveH - 12, hint,
             palColor(Pal::INK));
}

void drawWifiEvent(Framebuffer& fb, const char* sectorName,
                   const char* outcomeLine, const char* discoveryLine) {
    fb.clear(palColor(Pal::PAPER));
    drawText(fb, kMargin, 6, sectorName, palColor(Pal::INK));
    fb.fillRect(0, 22, kActiveW, 1, palColor(Pal::TRACK));

    // The real-network-discovery beat (new / familiar / home-turf / empty-queue)
    // sits in the gap above the guardian/cache/friendly banner — an independent
    // line, drawn only when resolveNetworkDiscovery() actually set one.
    if (discoveryLine && discoveryLine[0])
        drawText(fb, (kActiveW - textWidth(discoveryLine)) / 2, 50, discoveryLine,
                 palColor(Pal::INK_DIM));

    const char* banner = "NEW WI-FI NETWORK";
    drawText(fb, (kActiveW - textWidth(banner)) / 2, 90, banner,
             palColor(Pal::ACCENT));
    if (outcomeLine && outcomeLine[0])
        drawText(fb, (kActiveW - textWidth(outcomeLine)) / 2, 106, outcomeLine,
                 palColor(Pal::INK_DIM));

    const char* hint = "B CONTINUE";
    fb.fillRect(0, kActiveH - 16, kActiveW, 16, palColor(Pal::TRACK));
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kActiveH - 12, hint,
             palColor(Pal::INK));
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

// Lines the description gets for a list of `visibleRows`: the band between the last
// listing and the centred buy reason above the hint band. At the row cap this is
// kShopDescLines (expl_screen.h — the worst case a stocked row has to clear); a
// one-listing storefront gets several times that.
int shopDescLines(int visibleRows) {
    const int top = kShopRowTop + visibleRows * kShopRowPitch + 2;
    const int bottom = kActiveH - 16 - (kFontH + 6);   // hint band + the status line
    return std::max(1, (bottom - top) / (kFontH + 3));
}

// One storefront row: name, full price (Bits + any item costs), and remaining
// stock as a count (so it reads in grayscale — sold out shows STOCK 0, not just a
// greyed row). `selected` draws the row cursor.
void drawShopRow(Framebuffer& fb, int rowY, const ShopRowView& row, bool selected) {
    fb.fillRect(4, rowY - 2, kActiveW - 8, kShopRowPitch - 6, palColor(Pal::TRACK));
    if (selected) drawRowCursor(fb, 10, rowY + 4, palColor(Pal::ACCENT));
    drawText(fb, 24, rowY + 2, row.name, palColor(Pal::INK));

    char priceStr[48];
    formatShopPrice(priceStr, sizeof(priceStr), row);
    drawText(fb, 24, rowY + 16, priceStr, palColor(Pal::INK_DIM));
    char stockStr[16];
    std::snprintf(stockStr, sizeof(stockStr), "STOCK %d", row.stock);
    drawText(fb, kActiveW - kMargin - textWidth(stockStr), rowY + 16, stockStr,
             row.stock > 0 ? palColor(Pal::CALM) : palColor(Pal::WARN));
}
} // namespace

void drawShop(Framebuffer& fb, const char* storeName, int walletBits,
              const ShopRowView* rows, int rowCount, int cursor,
              const char* selectedDescription, const char* statusLine) {
    fb.clear(palColor(Pal::PAPER));
    // Storefront banner + wallet header (dual-coded: the numbers carry meaning,
    // colour is only emphasis).
    drawText(fb, kMargin, 6, storeName, palColor(Pal::INK));
    char wallet[16];
    std::snprintf(wallet, sizeof(wallet), "%dB", walletBits);
    drawText(fb, kActiveW - kMargin - textWidth(wallet), 6, wallet,
             palColor(Pal::ACCENT));
    fb.fillRect(0, 22, kActiveW, 1, palColor(Pal::TRACK));

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
        drawShopRow(fb, kShopRowTop + v * kShopRowPitch, rows[row], row == cursor);
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
    // wider than the 208px here, so it wraps; the buy reason then flows under whatever
    // it took, keeping both clear of the hint band.
    const int descY = kShopRowTop + visibleRows * kShopRowPitch + 2;
    int statusY = descY;
    if (selectedDescription && selectedDescription[0])
        statusY = drawTextWrapped(fb, kMargin, descY, kActiveW - 2 * kMargin,
                                  selectedDescription, palColor(Pal::INK_DIM),
                                  kFontH + 3, shopDescLines(visibleRows));
    if (statusLine && statusLine[0])
        drawText(fb, (kActiveW - textWidth(statusLine)) / 2, statusY + 4, statusLine,
                 palColor(Pal::INK_DIM));

    const char* hint = rowCount > 1 ? "A NEXT  B BUY  C LEAVE" : "B BUY   C LEAVE";
    fb.fillRect(0, kActiveH - 16, kActiveW, 16, palColor(Pal::TRACK));
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kActiveH - 12, hint,
             palColor(Pal::INK));
}

void drawWarpPicker(Framebuffer& fb, const char* const* keyNames, int keyCount,
                    int cursor) {
    fb.clear(palColor(Pal::PAPER));
    // Header: what this is, spelled out so it reads without colour.
    drawText(fb, kMargin, 6, "WARP KEY", palColor(Pal::INK));
    fb.fillRect(0, 22, kActiveW, 1, palColor(Pal::TRACK));
    drawText(fb, kMargin, 28, "JUMP TO...", palColor(Pal::INK_DIM));

    // One row per held key, cursor-highlighted (a filled track + the row cursor —
    // the same idiom as the sector list / shop row, so it reads in grayscale).
    for (int i = 0; i < keyCount; ++i) {
        const int y = kRowTop + i * kRowH;
        if (i == cursor) {
            fb.fillRect(4, y + 2, kActiveW - 8, kRowH - 4, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + (kRowH - 7) / 2, palColor(Pal::ACCENT));
        }
        drawText(fb, 24, y + (kRowH - kFontH) / 2, keyNames[i],
                 palColor(Pal::INK));
    }

    const char* hint = "A NEXT  B USE  C BACK";
    fb.fillRect(0, kActiveH - 16, kActiveW, 16, palColor(Pal::TRACK));
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kActiveH - 12, hint,
             palColor(Pal::INK));
}

// Post-encounter status readout: a brief, informational-only
// overlay — no decision, so the hint band spells out the one break from the
// standard A/B/C contract (ANY button dismisses, not just B/C).
void drawPostEncounter(Framebuffer& fb, int bwBefore, int bwAfter, int bwMax,
                       PostEncounterFragState fragState, int fragDelta,
                       const char* levelLine) {
    fb.clear(palColor(Pal::PAPER));
    drawText(fb, kMargin, 6, "STATUS", palColor(Pal::INK));
    fb.fillRect(0, 22, kActiveW, 1, palColor(Pal::TRACK));

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
    fb.fillRect(0, kActiveH - 16, kActiveW, 16, palColor(Pal::TRACK));
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kActiveH - 12, hint,
             palColor(Pal::INK));
}

} // namespace mal
