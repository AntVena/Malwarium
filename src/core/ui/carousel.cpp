#include "core/ui/carousel.h"

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

constexpr int kIcon = 28;                       // ASSET_ICON_* are 28x28
constexpr int kIconInset = (kSlotW - kIcon) / 2; // 14 — centre in the 56px column
constexpr int kIconTop = (kTrackH - kIcon) / 2;  // 6  — centre in the 40px track
constexpr int kBoxPad = 3;                       // UI_CURSOR_BOX padding round icon
constexpr int kBoxThick = 2;
constexpr uint8_t kDimAlpha = 140;               // ~50% wash over unfocused icons

// Top track holds cursor 0..3, bottom track 4..7.
int slotCol(int cursor) { return cursor % kSlotCols; }
bool slotIsTop(int cursor) { return cursor < kSlotCols; }
int slotTrackTop(int cursor) { return slotIsTop(cursor) ? 0 : kLivingBottom; }
int slotIconX(int cursor) { return slotCol(cursor) * kSlotW + kIconInset; }
int slotIconY(int cursor) { return slotTrackTop(cursor) + kIconTop; }

// Stroke a rectangle outline (kBoxThick px) — the focus box / shape channel.
void strokeRect(Framebuffer& fb, int x, int y, int w, int h, Rgb565 c) {
    fb.fillRect(x, y, w, kBoxThick, c);
    fb.fillRect(x, y + h - kBoxThick, w, kBoxThick, c);
    fb.fillRect(x, y, kBoxThick, h, c);
    fb.fillRect(x + w - kBoxThick, y, kBoxThick, h, c);
}

// Centre text in [x0, x0+w) at y.
void drawTextCentered(Framebuffer& fb, int x0, int w, int y, const char* s,
                      Rgb565 c, int scale = 1) {
    int tw = textWidth(s, scale);
    drawText(fb, x0 + (w - tw) / 2, y, s, c, scale);
}

} // namespace

const CarouselSlot* carouselSlots() {
    // Cursor order == slot assignment 1..8. All eight slots now route
    // to a real submenu, so no slot carries
    // the carousel ·soon· marker; deferred *content* inside GAMES/EXPL/ARCH/MODS
    // is flagged per-row instead (·soon·/locked tags).
    static const CarouselSlot kSlots[kCarouselSlots] = {
        {"STAT",  &ASSET_ICON_STAT,  false, SubmenuId::Stat},
        {"ITEMS", &ASSET_ICON_ITEMS, false, SubmenuId::Items},
        {"GAMES", &ASSET_ICON_GAMES, false, SubmenuId::Games},
        {"EXPL",  &ASSET_ICON_EXPL,  false, SubmenuId::Expl},
        {"MAINT", &ASSET_ICON_MAINT, false, SubmenuId::Maint},
        {"MODS",  &ASSET_ICON_MODS,  false, SubmenuId::Mods},
        {"ARCH",  &ASSET_ICON_ARCH,  false, SubmenuId::Arch},
        {"CFG",   &ASSET_ICON_CFG,   false, SubmenuId::Cfg},
    };
    return kSlots;
}

const HackerCarouselSlot* hackerCarouselSlots() {
    // roster. PROFILE leads (slot 0) so it maps to the pet face's STAT slot at
    // the same position; CREW/SHOP/VAULT/MERGE follow. SCAN has no icon drawn yet
    // and renders its marker/label alone — a null icon is a legal slot, not a
    // broken one. PROFILE, CREW (enlist + home network), SHOP, and VAULT
    // (sealed-cache decrypt, off pet ITEMS), PEERS (met operators) and LINK (1v1
    // duels) are statically live; SCAN is statically
    // inaccessible (marker + inert B); MERGE (item-combining) starts statically
    // inaccessible too, but drawHackerCarousel's `mergeUnlocked` override flips it
    // live once bought in SHOP — so the face reads as a real, partly-locked console
    // rather than an empty grid, and a purchase visibly unlocks a slot instead of a
    // new one appearing from nowhere.
    static const HackerCarouselSlot kSlots[kCarouselSlots] = {
        {"PROFILE", &ASSET_ICON_PROFILE, true,  HackerSlotId::Profile},
        {"CREW",    &ASSET_ICON_CREW,    true,  HackerSlotId::Crew},
        {"SHOP",    &ASSET_ICON_SHOP,    true,  HackerSlotId::Shop},
        {"VAULT",   &ASSET_ICON_VAULT,   true,  HackerSlotId::Vault},
        {"MRG",     &ASSET_ICON_MRG,     false, HackerSlotId::Merge},
        {"SCAN",    nullptr,             false, HackerSlotId::Scan},
        {"LINK",    &ASSET_ICON_LINK,    true,  HackerSlotId::Link},
        {"PEERS",   &ASSET_ICON_PEERS,   true,  HackerSlotId::Peers},
    };
    return kSlots;
}

void drawHackerCarousel(Framebuffer& fb, int cursor, UiMode mode, int /*beat*/,
                        bool mergeUnlocked, unsigned lockedMask) {
    const HackerCarouselSlot* slots = hackerCarouselSlots();
    const Rgb565 paper = palColor(Pal::PAPER);

    fb.fillRect(0, 0, kActiveW, kTrackH, palColor(Pal::TRACK));
    fb.fillRect(0, kLivingBottom, kActiveW, kTrackH, palColor(Pal::TRACK));

    for (int i = 0; i < kCarouselSlots; ++i) {
        const bool accessible = (slots[i].accessible ||
            (slots[i].id == HackerSlotId::Merge && mergeUnlocked)) &&
            ((lockedMask >> i) & 1u) == 0;
        const bool focused = (i == cursor);
        const int ix = slotIconX(i);
        const int iy = slotIconY(i);

        // IconsLabel swaps the focused slot's own icon for its text label instead
        // of floating a caption over the living area (which collided with
        // whatever draws there — pet sprite, event overlays, …). TextOnly does
        // this for every slot, not just the focused one.
        const bool asText = mode == UiMode::TextOnly ||
            (mode == UiMode::IconsLabel && i == cursor);
        if (asText) {
            const Rgb565 tc = (focused && accessible) ? palColor(Pal::ACCENT)
                                                      : palColor(Pal::INK_DIM);
            const int col = slotCol(i) * kSlotW;
            drawTextCentered(fb, col, kSlotW, slotTrackTop(i) + (kTrackH - kFontH) / 2,
                             slots[i].label, tc);
        } else {
            if (slots[i].icon) {
                drawSprite(fb, *slots[i].icon, 0, ix, iy);
                // An inaccessible slot's icon washes toward paper (like a locked pet
                // slot) so it reads dim; the marker below then names it inaccessible.
                const int washes = accessible ? (focused ? 0 : 1) : 2;
                for (int pass = 0; pass < washes; ++pass)
                    for (int yy = iy; yy < iy + kIcon; ++yy)
                        for (int xx = ix; xx < ix + kIcon; ++xx)
                            fb.blendPixel(xx, yy, paper, kDimAlpha);
            }
            // inaccessible marker: composited centred over the (dimmed) slot icon,
            // or drawn alone for an undesigned slot with no base icon. Dual-coded — a
            // distinct glyph, not just a dim, so the lock survives the grayscale test.
            if (!accessible) {
                const SpriteData& mk = ASSET_ICON_SLOT_INACCESSIBLE;
                const int mx = ix + (kIcon - mk.frameW) / 2;
                const int my = iy + (kIcon - mk.h) / 2;
                drawSprite(fb, mk, 0, mx, my);
            }
        }

        if (i == cursor)
            strokeRect(fb, ix - kBoxPad, iy - kBoxPad, kIcon + 2 * kBoxPad,
                       kIcon + 2 * kBoxPad,
                       accessible ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));
    }
}

void drawCarousel(Framebuffer& fb, int cursor, UiMode mode, int beat,
                  unsigned lockedMask, unsigned spinMask) {
    const CarouselSlot* slots = carouselSlots();
    const Rgb565 paper = palColor(Pal::PAPER);

    // Track chrome bands (native res). The dimmed living area shows between them.
    fb.fillRect(0, 0, kActiveW, kTrackH, palColor(Pal::TRACK));
    fb.fillRect(0, kLivingBottom, kActiveW, kTrackH, palColor(Pal::TRACK));

    // The carousel recedes so the pet holds attention: every slot is dimmed
    // *unless* it's the focused one. cursor < 0 (resting) focuses nothing, so the
    // whole shelf reads quiet. (Future per-slot "needs attention" highlighting —
    // e.g. a low-stat nudge — would brighten a slot here without a cursor.)
    for (int i = 0; i < kCarouselSlots; ++i) {
        const bool locked = (lockedMask >> i) & 1u;   // inert slot (egg-phase grey)
        const bool focused = (i == cursor) && !locked; // a locked slot can't read active

        const int ix = slotIconX(i);
        const int iy = slotIconY(i);

        // IconsLabel swaps the focused slot's own icon for its text label instead
        // of floating a caption over the living area (which collided with
        // whatever draws there — pet sprite, event overlays, …). TextOnly does
        // this for every slot, not just the focused one.
        const bool asText = mode == UiMode::TextOnly ||
            (mode == UiMode::IconsLabel && i == cursor);
        if (asText) {
            // Terse word in place of the icon: focused ACCENT, the rest (and any
            // locked slot) dimmed.
            const Rgb565 tc = focused ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM);
            const int col = slotCol(i) * kSlotW;
            drawTextCentered(fb, col, kSlotW, slotTrackTop(i) + (kTrackH - kFontH) / 2,
                             slots[i].label, tc);
        } else {
            // A spinning slot cycles its own frames off the shared beat; every other
            // slot rests on frame 0. Sprite frames are the whole animation state, so a
            // slot that hasn't been given more than one just ignores its bit.
            const SpriteData& s = *slots[i].icon;
            const int frame =
                ((spinMask >> i) & 1u) && s.frames > 1 ? beat % s.frames : 0;
            drawSprite(fb, s, frame, ix, iy);
            // Wash the icon toward paper: unfocused = one pass (~50%); a locked slot
            // gets two passes so it reads distinctly greyed-out (disabled) even at
            // rest, when the whole shelf is already receded.
            const int washes = locked ? 2 : (focused ? 0 : 1);
            for (int pass = 0; pass < washes; ++pass)
                for (int yy = iy; yy < iy + kIcon; ++yy)
                    for (int xx = ix; xx < ix + kIcon; ++xx)
                        fb.blendPixel(xx, yy, paper, kDimAlpha);
        }

        // ·soon· marker on any not-yet-built slot, just inside the living-area edge.
        if (slots[i].phase2) {
            const int col = slotCol(i) * kSlotW;
            const int sy = slotIsTop(i) ? kTrackH - kFontH - 1 : kLivingBottom + 1;
            drawTextCentered(fb, col, kSlotW, sy, "SOON", palColor(Pal::INK_DIM));
        }

        // Focus box (shape channel — survives grayscale). A cursor parked on a
        // locked slot still draws a box so the cursor is visible, but muted
        // (INK_DIM, not ACCENT) so the slot reads unavailable.
        if (i == cursor)
            strokeRect(fb, ix - kBoxPad, iy - kBoxPad, kIcon + 2 * kBoxPad,
                       kIcon + 2 * kBoxPad,
                       locked ? palColor(Pal::INK_DIM) : palColor(Pal::ACCENT));
    }
}

void drawPlaceholderSubmenu(Framebuffer& fb, const char* title) {
    drawHeaderBand(fb, title);
    // Centered status tag — this category's submenu isn't built yet.
    drawTextCentered(fb, 0, kActiveW, kActiveH / 2 - 7, "SOON", palColor(Pal::INK_DIM), 2);
}

} // namespace mal
