// carousel.h — the 8-slot L1 carousel: slot table + native-chrome renderer.
//
// The slot table is the registration seam: each category is a data row,
// so new modes drop in without
// rewiring nav. Cursor order is the canonical slot assignment 1..8 (STAT, ITEMS,
// TRAIN, EXPL, MAINT, MODS, ARCH, CFG); top track = cursor 0..3, bottom = 4..7.
// Book-wrap is then just (cursor±1) mod 8 — the 4->5 / 8->1 cross-screen jumps
// fall out of the linear index.
#pragma once

#include "core/ui/ui_state.h"  // SubmenuId, HackerSlotId, UiMode

namespace mal {

class Framebuffer;
struct SpriteData;

// A slot with no built screen renders the ·soon· placeholder.
struct CarouselSlot {
    const char* label;       // full word: CAP_SLOT_LABEL + Text-only slot text
    const SpriteData* icon;  // 28x28 native icon (ASSET_ICON_*)
    bool phase2;             // renders the ·soon· marker; routes to placeholder
    SubmenuId id;            // B destination (dispatch seam)
};

constexpr int kCarouselSlots = 8;

// The 8 slots in cursor order (== slot assignment 1..8).
const CarouselSlot* carouselSlots();

// A Hacker-face carousel slot. `accessible` false = the inaccessible
// marker is composited over the icon and B is inert (the slot exists but isn't usable
// yet). `icon` may be null for an undesigned slot (renders the marker alone).
struct HackerCarouselSlot {
    const char* label;
    const SpriteData* icon;   // 28x28 native icon, or null (marker-only)
    bool accessible;          // false → inaccessible marker + inert B
    HackerSlotId id;          // B destination on the hacker face
};

// The 8 Hacker-face slots in cursor order (top track 0..3, bottom 4..7).
const HackerCarouselSlot* hackerCarouselSlots();

// Draw the always-on carousel chrome: track bands, icons/text, ·soon· tags.
// `cursor` < 0 is the resting state (all slots equal, no focus box). 0..7 focuses
// that slot (box + dimmed neighbours); in IconsLabel mode the focused slot's icon
// is swapped for its text label in place, so the label never floats over the
// living area where the pet/overlays draw. TextOnly renders every slot as text.
// `lockedMask` disables slots by index (bit i set = slot i inert): the icon/text
// stays dimmed even when focused and the focus box reads muted — the Boot-Sector
// egg greys out the care/combat slots (nothing to do to an egg).
// `spinMask` animates slots by index (bit i set = slot i's icon runs its frames off
// `beat` instead of resting on frame 0), so a running background mode is legible from
// the shelf itself: EXPL's globe turns while auto-progress is armed. A slot whose icon
// is single-frame ignores its bit.
void drawCarousel(Framebuffer& fb, int cursor, UiMode mode, int beat,
                  unsigned lockedMask = 0, unsigned spinMask = 0);

// Draw the Hacker-face carousel: the same track/geometry as drawCarousel,
// but the hacker slot roster + the inaccessible marker over any slot whose
// HackerCarouselSlot::accessible is false. `cursor` < 0 rests (no focus box).
// `mergeUnlocked` overrides the MERGE slot's static (false) accessible bit once
// the Hacker-SHOP Merge Hub purchase is owned — the only slot with a runtime-gated
// lock (every other slot's accessibility is fixed at compile time).
void drawHackerCarousel(Framebuffer& fb, int cursor, UiMode mode, int beat,
                        bool mergeUnlocked = false);

// A header-band placeholder for any slot whose real submenu isn't built yet.
// Title + centered tag.
void drawPlaceholderSubmenu(Framebuffer& fb, const char* title);

} // namespace mal
