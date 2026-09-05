#include "core/ui/collect_screen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/content/registry.h"
#include "core/model/move_loadout.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/stat_screen.h"   // drawStatHintBand — STAT's pages end alike
#include "core/ui/widgets.h"
#include "tunables.h"

namespace mal {

namespace {

// --- The FOODS grid ---------------------------------------------------------

constexpr int kGridTop = 26;
constexpr int kCellPitch = 24;                       // 20px icon tier + 4px air
constexpr int kCellRowH = kCellPitch;
constexpr int kGridW = kFoodCols * kCellPitch - (kCellPitch - kRowIcon);
constexpr int kGridX = (kActiveW - kGridW) / 2;      // centred, so both gutters match
// A heading takes the flowed pages' own measurements (prose_page.h) so the two kinds of
// page break their sections at the same rhythm.
constexpr int kSectionH = kProseHeaderH + 4;
constexpr int kSectionLead = kProseGroupLead;

// ICON_ITEM_<UPPER ID> — the per-item glyph, the same convention the ITEMS bag draws its
// rows from (items_screen.cpp), so a dish looks the same wherever it is met.
const SpriteData* foodIcon(const ContentRegistry& reg, const char* id) {
    char name[40];
    std::snprintf(name, sizeof(name), "ICON_ITEM_%s", id);
    for (char* c = name; *c; ++c)
        if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
    return reg.sprite(name);
}

int foodRowHeight(const FoodRow& r, bool firstInWindow) {
    if (r.section) return (firstInWindow ? 0 : kSectionLead) + kSectionH;
    return kCellRowH;
}

}  // namespace

std::vector<FoodRow> buildFoodRows(const ContentRegistry& reg,
                                   const std::vector<const ItemDef*>& eatenSet) {
    auto tasted = [&](const ItemDef* d) {
        for (const ItemDef* e : eatenSet)
            if (e && d && std::strcmp(e->id, d->id) == 0) return true;
        return false;
    };

    std::vector<FoodRow> out;
    static const ItemDef::Rarity kOrder[] = {
        ItemDef::Rarity::Common, ItemDef::Rarity::Uncommon, ItemDef::Rarity::Rare,
        ItemDef::Rarity::Epic};
    const std::vector<const ItemDef*> all = reg.allItems();

    for (ItemDef::Rarity rarity : kOrder) {
        std::vector<const ItemDef*> group;
        for (const ItemDef* d : all)
            if (d && itemCategory(*d) == ItemDef::Category::Food && d->rarity == rarity)
                group.push_back(d);
        if (group.empty()) continue;   // an empty tier is not a section

        FoodRow head{};
        head.section = rarityName(rarity);
        head.total = static_cast<int>(group.size());
        for (const ItemDef* d : group)
            if (tasted(d)) ++head.have;
        out.push_back(head);

        for (std::size_t i = 0; i < group.size(); i += kFoodCols) {
            FoodRow row{};
            for (std::size_t c = 0; c < kFoodCols && i + c < group.size(); ++c) {
                row.cells[c] = group[i + c];
                row.eaten[c] = tasted(group[i + c]);
                ++row.count;
            }
            out.push_back(row);
        }
    }
    return out;
}

int foodRowsFitting(const std::vector<FoodRow>& rows, int top) {
    int y = kGridTop;
    int n = 0;
    for (int i = top; i < static_cast<int>(rows.size()); ++i) {
        // The flowed pages' section rule, and the same half-full clause with it: a
        // heading opens a window once there is a screen's worth behind it, and is packed
        // in behind a short tail rather than spending a screen on one row of dishes.
        if (n > 0 && rows[i].section && y - kGridTop >= (kProseBottom - kGridTop) / 2)
            break;
        const int h = foodRowHeight(rows[i], i == top);
        if (n > 0 && y + h > kProseBottom) break;
        y += h;
        ++n;
    }
    return n;
}

int foodWindowCount(const std::vector<FoodRow>& rows) {
    int n = 0;
    for (int top = 0; top < static_cast<int>(rows.size()); ++n) {
        const int shown = foodRowsFitting(rows, top);
        if (shown <= 0) break;
        top += shown;
    }
    return n;
}

int foodWindowIndex(const std::vector<FoodRow>& rows, int scrollTop) {
    int n = 0;
    for (int top = 0; top < static_cast<int>(rows.size()); ++n) {
        if (top >= scrollTop) return n;
        const int shown = foodRowsFitting(rows, top);
        if (shown <= 0) break;
        top += shown;
    }
    return n;
}

void drawFoodsScreen(Framebuffer& fb, const ContentRegistry& reg,
                     const std::vector<FoodRow>& rows, int scrollTop, int eaten,
                     int total, int beat) {
    // The score rides the header band, because it is the one number the page is for and
    // the reader should not have to add the sections up to get it.
    char score[16];
    std::snprintf(score, sizeof(score), "%d/%d", eaten, total);
    drawHeaderBand(fb, "FOODS", score);

    if (rows.empty()) {
        drawText(fb, kMargin, 60, "- NO DISHES -", palColor(Pal::INK_DIM));
        drawStatHintBand(fb, 1, 1);
        return;
    }

    const int count = static_cast<int>(rows.size());
    const int top = std::max(0, std::min(scrollTop, count - 1));
    const int shown = foodRowsFitting(rows, top);

    int y = kGridTop;
    for (int v = 0; v < shown; ++v) {
        const FoodRow& r = rows[top + v];
        if (r.section) {
            const int hy = y + (v == 0 ? 0 : kSectionLead);
            char tag[12];
            std::snprintf(tag, sizeof(tag), "%d/%d", r.have, r.total);
            // Dim, like every other section heading on the status pages: a fence, not a
            // row — and its tally is the reason a reader stops here at all.
            drawLabelValue(fb, kMargin, hy, r.section, palColor(Pal::INK_DIM), tag,
                           palColor(Pal::INK_DIM), beat, /*scroll=*/false);
        } else {
            for (int c = 0; c < r.count; ++c) {
                const SpriteData* icon = foodIcon(reg, r.cells[c]->id);
                if (!icon) continue;
                const int cx = kGridX + c * kCellPitch;
                // The one channel this page reports on, and it is a VALUE difference
                // rather than a hue: a tasted dish is drawn in ink, an untasted one in
                // the dim, at a coverage low enough that the grid reads as a plate half
                // filled in from across the room. Both survive the grayscale gate,
                // which nothing built on colour alone would.
                if (r.eaten[c])
                    drawSpriteTinted(fb, *icon, 0, cx, y, palColor(Pal::INK));
                else
                    drawSpriteTinted(fb, *icon, 0, cx, y, palColor(Pal::INK_DIM), 0,
                                     false, /*alpha=*/90);
            }
        }
        y += foodRowHeight(r, v == 0);
    }

    drawStatHintBand(fb, foodWindowIndex(rows, top) + 1, foodWindowCount(rows));

    if (foodRowsFitting(rows, 0) < count) {
        const int barX = kActiveW - 3;
        const int trackH = kProseBottom - kGridTop;
        fb.fillRect(barX, kGridTop, 2, trackH, palColor(Pal::TRACK));
        const int thumbH = std::max(8, trackH * shown / count);
        fb.fillRect(barX, kGridTop + trackH * top / count, 2, thumbH,
                    palColor(Pal::INK_DIM));
    }
}

// --- The MOVES roster -------------------------------------------------------

std::vector<ProseRow> buildMoveDexRows(const ContentRegistry& reg,
                                       const MoveLoadout& moves, const char* petLine,
                                       Stage stage) {
    std::vector<ProseRow> out;
    const std::vector<const MoveDef*> all = reg.allMoves();

    // The pet's OWN line first. It is the half a player can complete by raising this
    // creature at all, where the generic pool below is earned one wild win at a time,
    // and a page that mixed them would say nothing about which of the two you are short
    // on. A lineless pet simply has no first section.
    auto section = [&](const char* label, bool lineHalf) {
        std::vector<const MoveDef*> group;
        for (const MoveDef* m : all) {
            if (!m || !moveAllowedForLine(*m, petLine)) continue;
            const bool isLine = m->line != nullptr;
            if (isLine != lineHalf) continue;
            group.push_back(m);
        }
        if (group.empty()) return;

        int known = 0;
        for (const MoveDef* m : group)
            if (moves.owns(m->id) || moves.isInnate(m->id)) ++known;

        ProseRow head{};
        head.header = true;
        head.label = label;
        setProseTag(head, "%d/%d", known, static_cast<int>(group.size()));
        out.push_back(head);

        for (const MoveDef* m : group) {
            ProseRow row{};
            row.label = m->displayName;
            // Three states, all in the tag, because the tag is the only channel a
            // grayscale screen can read a state off: held, held but not yet fieldable at
            // this stage, or a gap. A gap says nothing rather than "LOCKED" — it is not
            // locked, it is simply not found yet, and that is the whole invitation.
            if (moves.owns(m->id) || moves.isInnate(m->id))
                setProseTag(row, moveUnlockedAtStage(*m, stage) ? "KNOWN" : "AT %s",
                            stageName(m->minStage));
            out.push_back(row);
        }
    };

    // A LITERAL, not the line id upper-cased into a buffer: a ProseRow's label is
    // BORROWED (prose_page.h) and would outlive any local this builder could uppercase
    // into. Which line it is, is not in doubt on a page about this pet — SPECIES and the
    // index both name it — and what the heading has to say here is which HALF of the
    // roster follows: the moves a pet gets for being what it is, or the pool anyone can
    // win off a wild.
    section("LINE MOVES", /*lineHalf=*/true);
    section("COMMON POOL", /*lineHalf=*/false);
    return out;
}

int moveDexRowsFitting(const std::vector<ProseRow>& rows, int top) {
    return proseRowsFitting(rows, top, kGridTop);
}

void drawMoveDexScreen(Framebuffer& fb, const std::vector<ProseRow>& rows, int scrollTop,
                       int known, int total, int beat) {
    char score[16];
    std::snprintf(score, sizeof(score), "%d/%d", known, total);
    drawHeaderBand(fb, "MOVES", score);
    drawProseRows(fb, rows, scrollTop, kGridTop, beat, nullptr);
    drawStatHintBand(fb, proseWindowIndex(rows, scrollTop, kGridTop) + 1,
                     proseWindowCount(rows, kGridTop));
}

}  // namespace mal
