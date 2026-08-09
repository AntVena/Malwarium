#include "core/ui/modals.h"

#include <cstdio>

#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"
#include "core/ui/layout.h"
#include "core/ui/widgets.h"

namespace mal {

namespace {


void drawPetCentered(Framebuffer& fb, const SpriteData* pet, int beat, int cy) {
    if (!pet) return;
    const int frame = idleFrame(*pet, beat);   // breathe, single-frame-safe
    const int petW = pet->frameW * kScaleNum / kScaleDen;
    const int petH = pet->h * kScaleNum / kScaleDen;
    drawSpriteUpscaled(fb, *pet, frame, (kActiveW - petW) / 2, cy - petH / 2,
                       kScaleNum, kScaleDen);
}

void drawCenteredText(Framebuffer& fb, int y, const char* s, Rgb565 c) {
    drawText(fb, (kActiveW - textWidth(s)) / 2, y, s, c);
}

// One vitals gauge on the feeding modal — the same label/ramp language the STAT
// vitals page uses (stat_screen.cpp), so a stat reads identically wherever it is
// drawn. `dir` is which way the food just moved it and `good` whether that was
// in the pet's favour (down is the favour for Fragmentation, up for the rest).
struct FeedGaugeRow {
    const char* label;
    int value;
    Zone zone;
    bool fragRamp;
    int dir;
    bool good;
};

// Does this row pull the given stat at all? Which stats a food TARGETS comes off
// its effects; how far each one actually moved comes off the before/after pair,
// since a magnitude can't know it was clamped at a cap.
bool feedTargets(const ItemDef& d, ItemEffect::Kind k) {
    for (const ItemEffect& e : d.effects)
        if (e.kind == k && e.magnitude != 0) return true;
    return false;
}

// Fill `out` (capacity 3) with a row per stat this food touched, and return how
// many. A food that pulls no vitals at all yields none, which is the honest
// report: whatever it did (arming a buff, say) isn't a gauge.
int buildFeedGaugeRows(const ItemDef& d, const PetModel& m, const FeedVitals& b,
                       FeedGaugeRow* out) {
    auto dir = [](int before, int now) { return now > before ? 1 : (now < before ? -1 : 0); };
    int n = 0;
    if (feedTargets(d, ItemEffect::Kind::Hunger)) {
        const int dv = dir(b.hunger, m.hunger());
        out[n++] = {"HUNGER", m.hunger(), m.hungerZone(), false, dv, dv > 0};
    }
    if (feedTargets(d, ItemEffect::Kind::Frag)) {
        const int dv = dir(b.frag, m.fragmentation());
        out[n++] = {"FRAG", m.fragmentation(), m.fragZone(), true, dv, dv < 0};
    }
    // Happiness has two movers — a flat add, and Null Noodles' pull TOWARD 50%,
    // which numbs or lifts depending purely on the mood it found.
    if (feedTargets(d, ItemEffect::Kind::Happy) ||
        feedTargets(d, ItemEffect::Kind::HappyToward50)) {
        const int dv = dir(b.happy, m.happiness());
        out[n++] = {"HAPPY", m.happiness(), m.happyZone(), false, dv, dv > 0};
    }
    return n;
}

}  // namespace

void drawEvolveModal(Framebuffer& fb, const SpriteData* from, const SpriteData* to,
                     const char* toName, Stage toStage, int phase, int beat) {
    fb.clear(palColor(Pal::PAPER));

    if (phase < 2) {  // hold + flash: title over the current (old) sprite
        drawCenteredText(fb, 16, "EVOLVING", palColor(Pal::INK));
        if (phase == 1) {
            // FX_EVO_FLASH white-out: a solid accent block masks the morph. Bright
            // against the paper field, so the moment still reads in grayscale.
            fb.fillRect(kMargin, 40, kActiveW - 2 * kMargin, 110, palColor(Pal::ACCENT));
        } else {
            drawPetCentered(fb, from, beat, 96);
        }
        return;
    }

    // Reveal: the new sprite, its name, and the stage indicator advanced.
    drawPetCentered(fb, to, beat, 78);
    drawCenteredText(fb, 126, toName, palColor(Pal::INK));
    drawStageIndicator(fb, (kActiveW - 130) / 2, 146, 130, toStage);

    // UI_HINT_BAND — C is disabled here, so the deviation is surfaced.
    drawCenteredText(fb, 200, "B CONTINUE   C DISABLED", palColor(Pal::INK_DIM));
}

void drawCSFModal(Framebuffer& fb, const SpriteData* pet, bool revealed, int beat) {
    fb.clear(palColor(Pal::PAPER));

    // Heaviest visual in the game: dark title ink over a HOT rule.
    drawCenteredText(fb, 14, "CRITICAL SYSTEM FAILURE", palColor(Pal::INK));
    fb.fillRect(kMargin, 30, kActiveW - 2 * kMargin, 1, palColor(Pal::HOT));

    // The pet corrupting out: draw it, then lay FX_CRITICAL_FAIL glitch bands over
    // it (maxed FX_CORRUPTION). The band count/position is the grayscale-safe
    // channel — the crash reads without colour. Jittered per heartbeat.
    drawPetCentered(fb, pet, beat, 96);
    for (int i = 0; i < 6; ++i) {
        const int gy = 58 + i * 13;
        const int gx = kMargin + ((beat * 5 + i * 11) % 48);
        const int gw = 70 + ((beat * 3 + i * 17) % 70);
        fb.fillRect(gx, gy, gw, 3, palColor(Pal::HOT));
    }

    drawCenteredText(fb, 150, "SPECIMEN LOST.", palColor(Pal::INK));
    drawCenteredText(fb, 164, "ARCHIVED AS CORRUPTED.", palColor(Pal::INK_DIM));

    // UI_HINT_BAND — B acknowledges (once the crash has held); C is disabled.
    drawCenteredText(fb, 200, revealed ? "B ACKNOWLEDGE   C DISABLED" : "SYSTEM HALTING",
                     palColor(Pal::INK_DIM));
}

void drawFeedingModal(Framebuffer& fb, const SpriteData* pet, const ItemDef* food,
                      const PetModel& m, const FeedVitals& before, int beat) {
    fb.clear(palColor(Pal::PAPER));
    drawPetCentered(fb, pet, beat, 80);
    drawCenteredText(fb, 132, food ? food->displayName : "", palColor(Pal::INK));

    FeedGaugeRow rows[3];
    const int n = food ? buildFeedGaugeRows(*food, m, before, rows) : 0;

    // The block stays centred on the old single-gauge line, so a one-stat morsel
    // sits exactly where it always did and a three-stat feast grows around it.
    constexpr int kVitalsRowH = 22;   // tighter than the grid: gauges, not text rows
    const int top = 168 - (n - 1) * kVitalsRowH / 2;
    for (int i = 0; i < n; ++i) {
        const FeedGaugeRow& r = rows[i];
        const int y = top + i * kVitalsRowH;
        drawText(fb, kMargin, y, r.label, palColor(Pal::INK));
        drawGauge(fb, 70, y - 2, 110, 10, r.value, r.zone, r.fragRamp, true);
        char num[8];
        std::snprintf(num, sizeof(num), "%3d", r.value);
        drawText(fb, 188, y, num, palColor(Pal::INK));
        // Direction is its own glyph channel, so which way the bar just moved
        // survives grayscale — the colour underneath it is a second coding, not
        // the carrier. A stat the food leaves where it found it says nothing.
        if (r.dir != 0)
            drawText(fb, 210, y, r.dir > 0 ? "+" : "-",
                     palColor(r.good ? Pal::CALM : Pal::WARN));
    }
}

void drawLockoutModal(Framebuffer& fb, const SpriteData* pet, const PetModel& m,
                      int secondsLeft, float remainFrac, bool payOption,
                      bool canPay, int bitsCost, int beat) {
    (void)m;
    fb.clear(palColor(Pal::PAPER));

    // FX_LOCKOUT_BAND: a flashing red banner (shape + text survive grayscale).
    const bool on = ((beat / 2) & 1) == 0;
    const Rgb565 band = on ? palColor(Pal::HOT) : palColor(Pal::INK_DIM);
    fb.fillRect(0, 0, kActiveW, 22, band);
    drawText(fb, kMargin, 8, "SYSTEM LOCKOUT", palColor(Pal::PAPER));
    char cd[8];
    std::snprintf(cd, sizeof(cd), "00:%02d", secondsLeft < 0 ? 0 : secondsLeft);
    drawText(fb, kActiveW - kMargin - textWidth(cd), 8, cd, palColor(Pal::PAPER));

    // Draining countdown bar.
    drawProgressBar(fb, kMargin, 28, kActiveW - 2 * kMargin, 8, remainFrac,
                    palColor(Pal::HOT));

    drawPetCentered(fb, pet, beat, 90);

    drawText(fb, kMargin, 132, "RESOLVE BEFORE TIME EXPIRES:", palColor(Pal::INK));

    // Two-path choice — the focused one carries the row cursor.
    const int yItems = 150, yPay = 166;
    char payLine[20];
    std::snprintf(payLine, sizeof(payLine), "PAY %d BITS", bitsCost);
    Rgb565 payCol = canPay ? palColor(Pal::INK) : palColor(Pal::INK_DIM);
    if (!payOption) {
        drawRowCursor(fb, kMargin, yItems, palColor(Pal::ACCENT));
        drawText(fb, kMargin + 10, yItems, "OPEN ITEMS", palColor(Pal::ACCENT));
        drawText(fb, kMargin + 10, yPay, payLine, payCol);
    } else {
        drawText(fb, kMargin + 10, yItems, "OPEN ITEMS", palColor(Pal::INK));
        drawRowCursor(fb, kMargin, yPay, palColor(Pal::ACCENT));
        drawText(fb, kMargin + 10, yPay, payLine,
                 canPay ? palColor(Pal::ACCENT) : palColor(Pal::INK_DIM));
    }

    // UI_HINT_BAND — C is disabled, so the deviation must be surfaced.
    drawCenteredText(fb, 200, "B SELECT   C DISABLED", palColor(Pal::INK_DIM));
}

}  // namespace mal
