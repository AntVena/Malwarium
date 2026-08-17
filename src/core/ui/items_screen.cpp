#include "core/ui/items_screen.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/content/content_recipes.h"  // itemIsRecipeIngredient — the FOOD split
#include "core/content/effect_text.h"
#include "core/content/registry.h"
#include "core/model/inventory.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/spec_sheet.h"
#include "core/ui/theme.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {


// Group key orders the list; lower sorts first. -1 == the Lockout RESOLVE group.
// FOOD's own slot splits into two — cooked dishes, then recipe ingredients — rather
// than the type-order cast used for Buffs/Quest: eating an ingredient directly is
// rare, and interleaved by rarity among the actual meals it was the thing a player
// had to scroll past to find dinner. Derived off content_recipes' own inputs
// (itemIsRecipeIngredient), not a second hand-authored flag on the item row.
int groupKey(const ItemDef& d, bool lockout) {
    if (lockout && itemResolvesLockout(d)) return -1;
    if (d.type == ItemDef::Type::Food)
        return itemIsRecipeIngredient(d.id) ? 1 : 0;
    return itemTypeOrder(d.type) + 1;
}
const char* groupLabel(int key) {
    switch (key) {
        case -1: return "RESOLVE";
        case 0: return "FOOD";
        case 1: return "INGREDIENTS";
        default: return itemTypeName(static_cast<ItemDef::Type>(key - 1));
    }
}

// Does an item pass the active filter? All = everything. Food/Buffs/Quest read the
// TYPE axis; Keys/Tools/Ingredients read the finer CATEGORY axis (Ingredients splits
// Food the same way Keys/Tools split Quest — itemIsRecipeIngredient, not
// itemCategory, since the split is derived from the recipe table rather than the
// row's own category field).
bool filterMatches(ItemFilter f, const ItemDef& d) {
    switch (f) {
        case ItemFilter::All: return true;
        case ItemFilter::Food: return d.type == ItemDef::Type::Food;
        case ItemFilter::Buffs: return d.type == ItemDef::Type::Buff;
        case ItemFilter::Quest: return d.type == ItemDef::Type::Quest;
        case ItemFilter::Keys: return itemCategory(d) == ItemDef::Category::Keys;
        case ItemFilter::Tools: return itemCategory(d) == ItemDef::Category::Tools;
        case ItemFilter::Ingredients:
            return d.type == ItemDef::Type::Food && itemIsRecipeIngredient(d.id);
    }
    return true;
}

// Rarity ladder: four cells, one lit per tier up to the item's own (Common 1 .. Epic
// 4). Lit cells are full-height blocks in the rarity's colour, unlit ones a baseline
// tick — so the tier is COUNTABLE before it is coloured, which is what lets the row
// icon take the tint at all (the rule in core/ui/theme.h). Returns the width drawn.
constexpr int kPipW = 3, kPipGap = 2, kPipCells = 4;
constexpr int kPipStripW = kPipCells * (kPipW + kPipGap) - kPipGap;

void drawRarityPips(Framebuffer& fb, int x, int y, ItemDef::Rarity r) {
    const int lit = static_cast<int>(r) + 1;
    for (int i = 0; i < kPipCells; ++i) {
        const int cx = x + i * (kPipW + kPipGap);
        if (i < lit)
            fb.fillRect(cx, y, kPipW, kFontH, rarityColor(r));
        else
            fb.fillRect(cx, y + kFontH - 1, kPipW, 1, palColor(Pal::INK_DIM));
    }
}

// Detail-page description block: the readout starts at kDetailPanelTop and the prose
// flows under it, down to kDetailPanelBottom. The HAVE row and the action line sit
// below that, near the bottom of a page that has no hint band — so the panel gets the
// room, and the two footer lines take what they need rather than the reverse.
//
// HAVE and the action line are a PAIR and are spaced to say so: their pitch is tighter
// than the prose pitch above them (kFontH + 4 against kLineH), while the gap ABOVE them
// is wider than either. That is the whole grouping channel on this page — neither footer
// line reads as a stray row of the block. It is the same footer pitch drawModDetail
// stacks its gate rows on, so the two pages agree.
//
// The gap is MEASURED, not reserved. The panel's reserve bounds how far the readout and
// prose may reach (a magnitude can never be displaced), but a two-line description ended
// far short of it and the pair stayed pinned to the bottom anyway — so most of the page
// was the fence. drawSpecSheet already reports where it actually stopped, so the pair
// follows that by kDetailFooterGap and is clamped at the reserve, which is exactly its
// old position. A full panel is therefore unchanged and only a short one moves.
//
// The trade: the action line's y now varies by item. It is the same trade drawModDetail
// already makes for its ONE-SHOT caveat, and it buys back most of a screen.
constexpr int kDetailPanelTop = 56;
constexpr int kDetailPanelBottom = 158;
constexpr int kDetailFooterPitch = kFontH + 4;
constexpr int kDetailFooterGap = 14;
constexpr int kDetailHaveMaxY = kDetailPanelBottom + kDetailFooterGap;

// The band with ITEMS' own right label: the n/total position counter, shown only
// once the list is long enough to scroll out from under the cursor.
void listHeader(Framebuffer& fb, const char* title, int sel, int total) {
    char nt[12];
    nt[0] = '\0';
    if (total > kVisibleRows) std::snprintf(nt, sizeof(nt), "%d/%d", sel, total);
    drawHeaderBand(fb, title, nt);
}

}  // namespace

// ICON_ITEM_<UPPER ID> — the per-item inventory glyph naming convention.
const SpriteData* itemIcon(const ContentRegistry& reg, const char* id) {
    char name[40];
    std::snprintf(name, sizeof(name), "ICON_ITEM_%s", id);
    for (char* c = name; *c; ++c)
        if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 'a' + 'A');
    // Every shipped item has its own ICON_ITEM_<ID>, so the name IS the lookup and
    // there is no borrowed-glyph table to keep in step. A null here means a row was
    // added without art, which the row will show as a blank — add the PNG rather
    // than patching an exception back in.
    return reg.sprite(name);
}

// Lockout-resolving items float to the top in Lockout context: any food (the demand
// is hunger), plus any row whose own context says LockoutOnly — which is what that
// context MEANS, so the set is read off the rows instead of being a list of ids here
// (src/core/content/CONTENT_STANDARD.md rule 1). Authoring a second lockout key is a
// row, not an edit to this function.
bool itemResolvesLockout(const ItemDef& d) {
    return d.type == ItemDef::Type::Food ||
           d.context == ItemDef::Context::LockoutOnly;
}

const char* itemFilterLabel(ItemFilter f) {
    switch (f) {
        case ItemFilter::All: return "ALL";
        case ItemFilter::Food: return "FOOD";
        case ItemFilter::Buffs: return "BUFFS";
        case ItemFilter::Quest: return "QUEST";
        case ItemFilter::Keys: return "KEYS";
        case ItemFilter::Tools: return "TOOLS";
        case ItemFilter::Ingredients: return "INGREDIENTS";
    }
    return "ALL";
}

ItemFilter nextItemFilter(ItemFilter f, bool categoryAxis) {
    if (categoryAxis) {
        switch (f) {
            case ItemFilter::All: return ItemFilter::Food;
            case ItemFilter::Food: return ItemFilter::Ingredients;
            case ItemFilter::Ingredients: return ItemFilter::Buffs;
            case ItemFilter::Buffs: return ItemFilter::Keys;
            case ItemFilter::Keys: return ItemFilter::Tools;
            default: return ItemFilter::All;   // Tools, or an off-axis QUEST
        }
    }
    switch (f) {
        case ItemFilter::All: return ItemFilter::Food;
        case ItemFilter::Food: return ItemFilter::Buffs;
        case ItemFilter::Buffs: return ItemFilter::Quest;
        default: return ItemFilter::All;       // Quest, or an off-axis KEYS/TOOLS
    }
}

std::vector<InvRow> buildInventoryRows(const ContentRegistry& reg,
                                       const Inventory& inv, bool lockoutSort,
                                       ItemFilter filter) {
    struct Owned { const ItemDef* def; int qty; int key; };
    std::vector<Owned> owned;
    for (const ItemDef* d : reg.allItems()) {
        // Sealed caches decrypt in the Hacker VAULT only (see itemUsable's
        // "DECRYPT IN VAULT" gate) — never list them here at all.
        if (d->use == ItemDef::Use::OpenContainer) continue;
        if (!filterMatches(filter, *d)) continue;
        int q = inv.count(d->id);
        if (q > 0) owned.push_back({d, q, groupKey(*d, lockoutSort)});
    }
    // Group order, then RAREST first within a group, alphabetical between equals.
    // Rarity is the closest thing an item row has to "how much this hands you", so
    // the top of every block is the strongest thing the bag holds of that type.
    std::sort(owned.begin(), owned.end(), [](const Owned& a, const Owned& b) {
        if (a.key != b.key) return a.key < b.key;
        if (a.def->rarity != b.def->rarity) return a.def->rarity > b.def->rarity;
        return std::strcmp(a.def->displayName, b.def->displayName) < 0;
    });

    // A category filter narrows WITHIN the Quest type, so its one group header names
    // the category (KEYS/TOOLS) rather than the type it happens to live under. The
    // Lockout RESOLVE group (key -1) always keeps its own label.
    const char* categoryHeader =
        (filter == ItemFilter::Keys || filter == ItemFilter::Tools)
            ? itemFilterLabel(filter) : nullptr;

    std::vector<InvRow> rows;
    int curKey = 0x7fffffff;
    for (const Owned& o : owned) {
        if (o.key != curKey) {  // section header at each group boundary
            curKey = o.key;
            const char* label = (categoryHeader && o.key >= 0) ? categoryHeader
                                                               : groupLabel(o.key);
            rows.push_back({true, label, nullptr, 0, nullptr});
        }
        rows.push_back({false, o.def->displayName, o.def, o.qty,
                        itemIcon(reg, o.def->id)});
    }
    return rows;
}

int firstSelectableRow(const std::vector<InvRow>& rows) {
    for (int i = 0; i < static_cast<int>(rows.size()); ++i)
        if (!rows[i].header) return i;
    return -1;
}

int stepSelectableRow(const std::vector<InvRow>& rows, int cur, int dir) {
    const int n = static_cast<int>(rows.size());
    if (n == 0) return -1;
    for (int step = 0; step < n; ++step) {
        cur = (cur + dir + n) % n;
        if (!rows[cur].header) return cur;
    }
    return cur;  // all headers (shouldn't happen)
}

std::vector<ItemPickRow> buildItemPickerRows(const ContentRegistry& reg,
                                             const Inventory& inv) {
    static const ItemFilter kTiles[] = {ItemFilter::All, ItemFilter::Food,
                                        ItemFilter::Ingredients, ItemFilter::Buffs,
                                        ItemFilter::Keys, ItemFilter::Tools};
    std::vector<ItemPickRow> tiles;
    for (ItemFilter f : kTiles) {
        int units = 0;
        for (const InvRow& r : buildInventoryRows(reg, inv, false, f))
            if (!r.header) units += r.qty;
        tiles.push_back({f, itemFilterLabel(f), units});
    }
    return tiles;
}

void drawItemTypePicker(Framebuffer& fb, const std::vector<ItemPickRow>& tiles,
                        int cursor) {
    listHeader(fb, "ITEMS - PICK TYPE", 0, 0);

    // Pitch is set by the tile COUNT against the footer, not chosen for looks: the
    // picker never scrolls (buildItemPickerRows returns a fixed set), so every tile
    // has to fit between kRowTop and the footer rule. Six tiles at the old pitch of
    // 32 put the last row's selection band straight through the rule — the INGREDIENTS
    // tile is what made six. 26 + 6*30 = 206 clears the rule at kActiveH-16.
    constexpr int kPickRowH = 30;
    const int n = static_cast<int>(tiles.size());
    for (int i = 0; i < n; ++i) {
        const ItemPickRow& t = tiles[i];
        const int y = kRowTop + i * kPickRowH;
        if (i == cursor) {  // UI_ROW_SEL band (grayscale-separable from paper)
            fb.fillRect(4, y + 2, kActiveW - 8, kPickRowH - 4, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + (kPickRowH - 7) / 2, palColor(Pal::ACCENT));
        }
        // An empty category dims BOTH its name and its count — the count is the
        // non-colour channel, so the row still reads as empty in grayscale.
        const Rgb565 ink = t.units > 0 ? palColor(Pal::INK) : palColor(Pal::INK_DIM);
        drawText(fb, 24, y + (kPickRowH - kFontH) / 2, t.label, ink);
        char qty[8];
        std::snprintf(qty, sizeof(qty), "x%d", t.units);
        drawText(fb, kActiveW - kMargin - textWidth(qty),
                 y + (kPickRowH - kFontH) / 2, qty, ink);
    }

    fb.fillRect(0, kActiveH - 16, kActiveW, 1, palColor(Pal::TRACK));
    drawText(fb, kMargin, kActiveH - 12, "B - OPEN  C - BACK", palColor(Pal::INK_DIM));
}

void drawItemsList(Framebuffer& fb, const std::vector<InvRow>& rows, int cursor,
                   bool lockout, int beat, ItemFilter filter,
                   bool tabsOwned, bool pickerOwned) {
    const bool filterHintVisible = tabsOwned || pickerOwned;
    fb.clear(palColor(Pal::PAPER));

    // Position indicator counts selectable rows.
    int total = 0, sel = 0;
    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        if (rows[i].header) continue;
        ++total;
        if (i == cursor) sel = total;
    }
    // With either filter upgrade owned the header carries the active-tab chip
    // (ALL/FOOD/BUFFS/QUEST/KEYS/TOOLS), so the narrowing is never invisible.
    const char* base = lockout ? "ITEMS LOCKOUT" : "ITEMS";
    if (filterHintVisible) {
        char title[32];
        std::snprintf(title, sizeof(title), "%s - %s", base, itemFilterLabel(filter));
        listHeader(fb, title, sel, total);
    } else {
        listHeader(fb, base, sel, total);
    }

    // The bottom hint band (exception rule) — each owned upgrade contributes its own
    // word, so an unowned player's ITEMS list carries no band at all. The Lockout
    // context never routes through the picker, so it never offers "C - TYPES".
    char hint[32] = "";
    if (tabsOwned) std::snprintf(hint, sizeof(hint), "HOLD B - FILTER");
    if (pickerOwned && !lockout)
        std::snprintf(hint + std::strlen(hint), sizeof(hint) - std::strlen(hint),
                      "%sC - TYPES", tabsOwned ? "  " : "");
    if (hint[0]) {
        fb.fillRect(0, kActiveH - 16, kActiveW, 1, palColor(Pal::TRACK));
        drawText(fb, kMargin, kActiveH - 12, hint, palColor(Pal::INK_DIM));
    }

    if (rows.empty()) {
        const char* msg = filter != ItemFilter::All ? "- NO MATCHING ITEMS -"
                                                     : "- NO ITEMS -";
        drawText(fb, (kActiveW - textWidth(msg)) / 2, kActiveH / 2, msg,
                 palColor(Pal::INK_DIM));
        return;
    }

    const int n = static_cast<int>(rows.size());
    const int scrollTop = listScrollTop(cursor, n, kVisibleRows);

    for (int v = 0; v < kVisibleRows && scrollTop + v < n; ++v) {
        const int i = scrollTop + v;
        const InvRow& r = rows[i];
        const int y = kRowTop + v * kRowH;
        if (r.header) {
            drawText(fb, kMargin, y + 8, r.label, palColor(Pal::INK_DIM));
            fb.fillRect(kMargin, y + 18, kActiveW - 2 * kMargin - 4, 1,
                        palColor(Pal::TRACK));
            continue;
        }
        if (i == cursor) {  // UI_ROW_SEL band (grayscale-separable from paper)
            fb.fillRect(4, y + 2, kActiveW - 8, kRowH - 4, palColor(Pal::TRACK));
            drawRowCursor(fb, 8, y + (kRowH - 7) / 2, palColor(Pal::ACCENT));
        }
        const int textY = y + (kRowH - kFontH) / 2;
        // Icon takes the rarity colour, exactly as the detail page does; the ladder
        // opposite it is the same fact without colour, so the row survives grayscale.
        if (r.icon)
            drawSpriteTinted(fb, *r.icon, 0, 16, y + (kRowH - kRowIcon) / 2,
                             rarityColor(r.def->rarity));
        char qty[8];
        std::snprintf(qty, sizeof(qty), "x%d", r.qty);
        const int qtyX = kActiveW - kMargin - textWidth(qty);
        const int pipsX = qtyX - 8 - kPipStripW;
        drawText(fb, qtyX, textY, qty, palColor(Pal::INK));
        drawRarityPips(fb, pipsX, textY, r.def->rarity);
        // The ladder and the count own the right end of the row, so the name yields:
        // it scrolls within what is left rather than drawing through them (widgets.h).
        // Only the focused row travels — a list of names all sliding at once is
        // unreadable, and the cursor already says which one the player is asking about.
        drawTextMarquee(fb, 40, textY, std::max(0, pipsX - 8 - 40), r.label,
                        palColor(Pal::INK), beat, i == cursor);
    }

    if (n > kVisibleRows) {  // slim scrollbar (UI_SCROLLBAR)
        const int barX = kActiveW - 3;
        const int trackH = kVisibleRows * kRowH;
        fb.fillRect(barX, kRowTop, 2, trackH, palColor(Pal::TRACK));
        const int thumbH = std::max(8, trackH * kVisibleRows / n);
        const int thumbY = kRowTop + trackH * scrollTop / n;
        fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
    }
}

int itemDetailPanelLines() {
    return (kDetailPanelBottom - kDetailPanelTop) / kLineH;
}

void drawItemDetail(Framebuffer& fb, const ItemDef& def, const SpriteData* icon,
                    int qty, bool usable, const char* gateMsg, int beat) {
    drawHeaderBand(fb, "ITEMS");

    // Icon + name + rarity tag. The icon takes the rarity's colour, which is pure
    // decoration on top of the word printed opposite it — see core/ui/theme.h.
    if (icon) drawSpriteTinted(fb, *icon, 0, kMargin, 30, rarityColor(def.rarity));
    const int nameX = icon ? kMargin + kRowIcon + 6 : kMargin;
    // Rarity tag (the font has no brackets — colour + word carry it). It owns the
    // right end; the name yields and scrolls, since the tier is the shorter fact.
    drawLabelValue(fb, nameX, 36, def.displayName, palColor(Pal::INK),
                   rarityName(def.rarity), rarityColor(def.rarity), beat, true);

    // The readout as an aligned grid, then the prose under it (spec_sheet.h): the grid
    // reports every magnitude the row hands the engine whether or not the prose
    // mentions it, and gets reserved room so a long description can't displace it.
    const SpecRows spec = specRows(def);
    const EffectText prose = effectText(def);   // must outlive the draw
    SpecSheet sheet;
    sheet.rows = spec.rows;
    sheet.rowCount = spec.count;
    sheet.prose = prose.c_str();
    const int panelEnd = drawSpecSheet(fb, kMargin, kDetailPanelTop,
                                       kActiveW - 2 * kMargin, kDetailPanelBottom,
                                       sheet).endY;

    // The footer pair, flowed under what the panel actually drew (see the note on
    // kDetailFooterGap). The clamp is what keeps a full panel where it always was.
    const int haveY = std::min(kDetailHaveMaxY, panelEnd + kDetailFooterGap);
    const int actionY = haveY + kDetailFooterPitch;

    // Quantity.
    char have[16];
    std::snprintf(have, sizeof(have), "HAVE x%d", qty);
    drawText(fb, kMargin, haveY, have, palColor(Pal::INK_DIM));

    // Action / gate line.
    if (usable) {
        drawRowCursor(fb, kMargin, actionY, palColor(Pal::ACCENT));
        // A re-roll reads ROLLBACK; the egg accelerator reads DECRYPT
        // everything else USE. (Sealed caches decrypt from the Hacker VAULT,
        // so no OPEN verb appears here.)
        const char* verb = def.use == ItemDef::Use::DecryptEgg ? "DECRYPT"
                         : def.use == ItemDef::Use::Rollback   ? "ROLLBACK"
                                                               : "USE";
        drawText(fb, kMargin + 10, actionY, verb, palColor(Pal::ACCENT));
    } else {
        drawText(fb, kMargin, actionY, gateMsg ? gateMsg : "- UNUSABLE HERE -",
                 palColor(Pal::INK_DIM));
    }
}

void drawCacheYield(Framebuffer& fb, const ItemDef& cache, int bits,
                    const ItemDef* const* items, const SpriteData* const* icons,
                    int count) {
    // Header: the decoded container + its rarity word (colour is emphasis only).
    drawHeaderBand(fb, "CACHE DECODED", rarityName(cache.rarity),
                   rarityColor(cache.rarity));

    // Which container it was.
    drawText(fb, kMargin, 30, cache.displayName, palColor(Pal::INK_DIM));

    // "YIELD:" then the Bits line + one row per item (icon + name). Bits always
    // appear (every open pays Bits); items are 0–2.
    drawText(fb, kMargin, 52, "YIELD", palColor(Pal::INK_DIM));
    int y = 70;
    char bitsStr[20];
    std::snprintf(bitsStr, sizeof(bitsStr), "+%d BITS", bits);
    drawText(fb, kMargin + 4, y, bitsStr, palColor(Pal::ACCENT));
    y += 22;

    if (count <= 0) {
        drawText(fb, kMargin + 4, y, "- NO ITEMS -", palColor(Pal::INK_DIM));
    } else {
        for (int i = 0; i < count; ++i) {
            if (icons && icons[i])
                drawSprite(fb, *icons[i], 0, kMargin + 4, y - 6);
            const int nx = (icons && icons[i]) ? kMargin + 4 + kRowIcon + 6
                                               : kMargin + 4;
            // The rarity word owns the right end and the name yields to it. Nothing on
            // this page is focused — it is a reveal, not a list — so no row scrolls;
            // a name too long for the room is clipped rather than drawn through the
            // word that says what came out.
            drawLabelValue(fb, nx, y, items[i] ? items[i]->displayName : "?",
                           palColor(Pal::INK),
                           items[i] ? rarityName(items[i]->rarity) : nullptr,
                           items[i] ? rarityColor(items[i]->rarity)
                                    : palColor(Pal::INK_DIM),
                           /*beat=*/0, /*scroll=*/false);
            y += 24;
        }
    }

    const char* hint = "B  OK";
    drawHintBand(fb, hint);
}

void drawBulkYield(Framebuffer& fb, const ItemDef& cache, int cachesOpened, int bits,
                   const BulkYieldRow* rows, int count, int cursor, int beat) {
    // Header: how many of the rarity group were opened + the rarity word.
    char hdr[36];
    std::snprintf(hdr, sizeof(hdr), "OPENED %d %s", cachesOpened, rarityName(cache.rarity));
    drawHeaderBand(fb, hdr, "CACHES", rarityColor(cache.rarity));

    char bitsStr[20];
    std::snprintf(bitsStr, sizeof(bitsStr), "+%d BITS", bits);
    drawText(fb, kMargin, 30, bitsStr, palColor(Pal::ACCENT));

    // Windowed tally list (mirrors drawItemsList/drawMovePicker): capped
    // + scrolled so an arbitrarily long tally never overruns the screen.
    constexpr int kBulkVisibleRows = 6;
    const int rowTop = 50, rowH = 22;
    if (count <= 0) {
        drawText(fb, kMargin, rowTop, "- NO ITEMS -", palColor(Pal::INK_DIM));
    } else {
        const int cur = (cursor >= 0 && cursor < count) ? cursor : 0;
        const int scrollTop = listScrollTop(cur, count, kBulkVisibleRows);
        for (int v = 0; v < kBulkVisibleRows && scrollTop + v < count; ++v) {
            const int i = scrollTop + v;
            const int y = rowTop + v * rowH;
            const BulkYieldRow& r = rows[i];
            if (i == cur) {
                fb.fillRect(4, y - 2, kActiveW - 8, rowH - 2, palColor(Pal::TRACK));
                drawRowCursor(fb, 8, y + 3, palColor(Pal::ACCENT));
            }
            if (r.icon) drawSprite(fb, *r.icon, 0, 20, y - 4);
            // The count is what this tally exists to report, so it owns the right end
            // and the name measures against it (drawLabelValue) rather than running
            // under it — the same rule the ITEMS list rows follow.
            char cnt[8];
            std::snprintf(cnt, sizeof(cnt), "x%d", r.count);
            drawLabelValue(fb, 46, y, r.def ? r.def->displayName : "?",
                           palColor(Pal::INK), cnt, palColor(Pal::INK), beat, i == cur);
        }
        if (count > kBulkVisibleRows) {   // UI_SCROLLBAR
            const int barX = kActiveW - 3;
            const int trackH = kBulkVisibleRows * rowH;
            fb.fillRect(barX, rowTop, 2, trackH, palColor(Pal::TRACK));
            const int thumbH = std::max(8, trackH * kBulkVisibleRows / count);
            const int thumbY = rowTop + trackH * scrollTop / count;
            fb.fillRect(barX, thumbY, 2, thumbH, palColor(Pal::INK_DIM));
        }
    }

    const char* hint = count > kBulkVisibleRows ? "A SCROLL  B/C OK" : "B/C  OK";
    drawHintBand(fb, hint);
}

void drawRollbackPicker(Framebuffer& fb, const int points[4], int cursor) {
    static const char* const kStatLabel[4] = {"POWER", "DEFENSE", "SPEED", "MAX-HP"};
    listHeader(fb, "ROLLBACK", 0, 0);
    // Purpose line (dual-coded: the whole action is spelled out in text).
    drawText(fb, kMargin, 30, "SHED 1 STAT (-1 LVL) TO", palColor(Pal::INK_DIM));
    drawText(fb, kMargin, 42, "RE-ROLL IT", palColor(Pal::INK_DIM));

    for (int i = 0; i < 4; ++i) {
        const int y = 66 + i * 22;
        const bool eligible = points[i] > 0;
        const bool sel = (i == cursor);
        const Rgb565 c = eligible ? palColor(Pal::INK) : palColor(Pal::INK_DIM);
        if (sel && eligible) drawRowCursor(fb, kMargin, y, palColor(Pal::ACCENT));
        drawText(fb, kMargin + 12, y, kStatLabel[i], c);
        if (eligible) {
            char pts[8];
            std::snprintf(pts, sizeof(pts), "+%d", points[i]);
            drawText(fb, kActiveW - kMargin - textWidth(pts), y, pts, c);
        } else {
            const char* none = "CAN'T REDUCE";
            drawText(fb, kActiveW - kMargin - textWidth(none), y, none,
                     palColor(Pal::INK_DIM));
        }
    }

    drawHintBand(fb, "A CYCLE  B SHED  C CANCEL");
}

}  // namespace mal
