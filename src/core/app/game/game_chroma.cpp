#include "core/app/game.h"

#include <cstdio>

#include "tunables.h"
#include "core/content/creatures/creature_lines.h"
#include "core/render/camo.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"

// game_chroma.cpp — the CHROMATOPHORE, the Metamorphic line's hatch minigame.
//
// A Polystaria egg is a translucent bell with nothing of its own to hide behind, so the
// moment it is laid it rehearses the only defence its line has. The water under it takes
// one of three colours, A/B/C wear one skin each, and a sweep crosses the panel on a
// shrinking clock: be wearing the water when it arrives and the pass is HIDDEN, be
// caught in the wrong skin — or halfway through the change — and the run is over with
// whatever it already earned.
//
// It is the line's own argument played as a game. Every other hatch minigame is about
// finding something (the live egg, the key, the byte); this one is about BECOMING
// something, in time, which is the only thing this family does that the others don't.
//
// THE SAME COLOUR TWICE. A skin is one PAL_CORE token used for both halves of the
// match: camoRampFromTone builds the water's tones AND the palette FX_CAMO repaints the
// creature in, so "wearing it" is literally the same ramp on both, and a retune of the
// hue can never desynchronise the two. What it costs is one call: the pet is drawn with
// drawSpriteCamo instead of drawSpriteUpscaled, and the level it is drawn at is the
// model's own settle (Chromatophore::wearPct) rather than a second animation clock — so
// the scatter crawling across the creature IS the thing the sweep is about to score.
//
// GRAYSCALE. The three skins are a luminance ladder (assets/PAL_CORE.json's camo block)
// and the water is TEXTURED per skin besides — streaks, stipple, bands — so which skin
// is down there survives the colour being taken away, and the chips below name all
// three in words. Colour is the fast channel here, never the only one.
//
// The rules are core/model/chromatophore.h, with no framebuffer and no Game in them.
// This file is the screen, the cadence and the payout.
//
// Entered from Game::startHatchGame the instant the egg is laid; leaves to idle, or
// straight into the hatch when the passes finish the clock.

namespace mal {

namespace {

// One skin: the colour it is, and what it is called. INDEX-addressed, because that is
// what the model deals in — kSkins[i] is the whole mapping from a rule to a screen.
struct Skin {
    Pal tone;
    const char* name;
};

constexpr Skin kSkins[kChromaSkins] = {
    {Pal::CAMO_KELP, "KELP"},
    {Pal::CAMO_SILT, "SILT"},
    {Pal::CAMO_BLOOM, "BLOOM"},
};
static_assert(sizeof(kSkins) / sizeof(kSkins[0]) == kChromaSkins,
              "one Skin row per skin the board plays with");

// The water, and the three chips under it. Named for what it is rather than as a panel,
// since canvas.h already owns kPanelW/kPanelH for the physical display. Drawn at native
// active res like every other piece of chrome — nothing here is art, so there is no cell
// grid to keep whole.
constexpr int kWaterX = 16;
constexpr int kWaterY = 46;
constexpr int kWaterW = kActiveW - 2 * kWaterX;   // 192
constexpr int kWaterH = 96;
constexpr int kChipY = kWaterY + kWaterH + 8;
constexpr int kChipH = 22;
constexpr int kChipGap = 6;
constexpr int kChipW = (kWaterW - (kChromaSkins - 1) * kChipGap) / kChromaSkins;

// Rows shared with the Clutch Pick and the Isolation buffer, so the three hatch
// minigames sit their text in the same places (title, verdict, effect, UI_HINT_BAND).
constexpr int kTitleY = 14;
constexpr int kVerdictY = 30;
constexpr int kEffectY = 186;
constexpr int kHintY = 204;

// The sweep, in the water's own pixels: a bright edge with a dim leading shadow, so it
// reads as travelling in a direction rather than as a line that moved.
constexpr int kSweepW = 3;
constexpr int kSweepLeadW = 10;

// How many tones a skin's ramp carries. Five is enough for a creature to keep its
// shading through the repaint and few enough that each step is a step the eye can see.
constexpr int kSkinRampSteps = 5;

CamoRamp skinRamp(int skin) {
    if (skin < 0 || skin >= kChromaSkins) skin = 0;
    return camoRampFromTone(palColor(kSkins[skin].tone), kSkinRampSteps);
}

// The water, in one skin. The fill is the ramp's own mid tone and the texture is drawn
// out of the same ramp, so a plate is never anything but the colour it claims to be —
// and the PATTERN is what carries the skin once the hue is gone: weed runs vertical,
// seabed grains stipple, a bloom drifts in horizontal bands.
void drawPlate(Framebuffer& fb, int skin, const CamoRamp& ramp) {
    fb.fillRect(kWaterX, kWaterY, kWaterW, kWaterH, ramp.tone[ramp.count / 2]);
    const Rgb565 dark = ramp.tone[0];
    const Rgb565 lit = ramp.tone[ramp.count - 1];
    switch (skin) {
        case 0:   // KELP — standing weed
            for (int x = 0; x < kWaterW; x += 12) {
                fb.fillRect(kWaterX + x, kWaterY, 3, kWaterH, dark);
                fb.fillRect(kWaterX + x + 6, kWaterY + 6, 1, kWaterH - 12, lit);
            }
            break;
        case 1:   // SILT — grains on the bed
            for (int y = 0; y < kWaterH; y += 8)
                for (int x = (y / 8 % 2) * 4; x < kWaterW; x += 8) {
                    fb.fillRect(kWaterX + x, kWaterY + y, 2, 2, dark);
                    fb.fillRect(kWaterX + x + 4, kWaterY + y + 4, 1, 1, lit);
                }
            break;
        default:  // BLOOM — drifting bands
            for (int y = 0; y < kWaterH; y += 14) {
                fb.fillRect(kWaterX, kWaterY + y, kWaterW, 4, dark);
                fb.fillRect(kWaterX, kWaterY + y + 6, kWaterW, 1, lit);
            }
            break;
    }
}

}  // namespace

// --- Who is standing on the board ------------------------------------------

const CreatureDef* Game::chromaSubject() const {
    // The pet, whenever the pet is something that plausibly does this. Every hatch route
    // in lands here — the egg on the shelf IS the pet, and it is on the family that
    // wears borrowed colours — so the fallback below only ever answers for an arcade
    // cabinet played by somebody else's pet.
    if (pet_) {
        const CreatureLine* fam = creatureLine(pet_->line);
        if (fam && fam->wearsBorrowedColours) return pet_;
    }
    // Otherwise the egg belonging to whichever line runs this game, asked for by its
    // HatchGame rather than by name: the cabinet is a rehearsal, and what rehearses is
    // whatever creature the board was built for.
    for (const EggLineDef* line : registry_.allEggLines()) {
        if (line && line->hatchGame == HatchGame::Chroma)
            return registry_.creature(line->eggCreatureId);
    }
    return nullptr;
}

// --- Lifecycle -------------------------------------------------------------

void Game::startChroma(int rounds, int windowMs, bool switching) {
    rng_ = rng_ * 1664525u + 1013904223u;   // the shared LCG seeds the skin sequence
    chroma_.reset(rng_, rounds, static_cast<uint32_t>(windowMs),
                  static_cast<uint32_t>(kChromaWindowStepMs), switching);
    chromaBanked_ = 0;
    lastChromaMs_ = nowMs_;
    closeGameBrief();   // a stale flag from a previous run must not open paused
    nav_ = Nav::Chroma;
    dirty_ = true;
}

void Game::onChroma(const ButtonEvent& ev) {
    if (onGameBriefInput(ev)) return;
    if (!chroma_.running()) {
        // Parked on the result. B banks and leaves; C is DISABLED, like every other
        // hatch screen — there is no pet to go back to, only an egg to get on with.
        if (ev.button == Button::B) finishChroma();
        return;
    }
    // ALL THREE BUTTONS ARE SKINS — the deviation from the standard A/B/C contract this
    // screen spells out in its hint band, and the reason the game can be played at all:
    // a skin has to be one press, in the order the chips are drawn, or the choice costs
    // more time than the window gives. B is therefore not a commit and C is not a
    // cancel; a run ends by being spotted or by finishing, and B comes back as CONTINUE
    // on the verdict above.
    if (ev.button == Button::A) chroma_.wear(0);
    else if (ev.button == Button::B) chroma_.wear(1);
    else if (ev.button == Button::C) chroma_.wear(2);
    dirty_ = true;
}

void Game::finishChroma() {
    // An arcade run has no clock to spend the passes on: the till takes the score and
    // that is the whole settlement. The cabinet run is ENDLESS, so it has no clean() to
    // report — a win is passing the till's own line (kArcadeChromaWinPasses), which the
    // score is free to go well past.
    if (arcadeRun_) {
        finishArcadeRun(chroma_.passes() >= kArcadeChromaWinPasses, chroma_.passes(),
                        kArcadeChromaWinPasses);
        return;
    }
    // Spend the run. Banked against chromaBanked_ rather than paid straight out, so a
    // second press on the result screen can't buy the same passes twice.
    const int owed = chroma_.passes() - chromaBanked_;
    if (owed > 0) {
        chromaBanked_ = chroma_.passes();
        accelerateEggHatch(static_cast<uint32_t>(owed) * kChromaPassMs);
    }
    // NEVER_SEEN asks for the run that was never spotted, and this is the one place it
    // exists: a clean board, not one that merely earned a lot. The Worm's own clean-run
    // row (WORM_WHISPERER) is the same idea on the same seam.
    if (chroma_.clean()) unlockAchievement(ach::kNeverSeen);
    if (bootHatchRemainMs_ == 0) {
        completeHatch();   // nothing left to incubate — hatch here, not a tick later
        return;
    }
    nav_ = Nav::Idle;
    dirty_ = true;
    markSaveDirty();
}

// --- Render ----------------------------------------------------------------

void Game::drawChroma(Framebuffer& fb) const {
    if (gameBriefOpen_) { drawGameBrief(fb); return; }
    fb.clear(palColor(Pal::PAPER));

    const Rgb565 ink = palColor(Pal::INK);
    const Rgb565 dim = palColor(Pal::INK_DIM);
    const char* title = "CHROMATOPHORE";
    drawText(fb, (kActiveW - textWidth(title)) / 2, kTitleY, title, ink);

    const CamoRamp plate = skinRamp(chroma_.plate());
    drawPlate(fb, chroma_.plate(), plate);

    // The creature, in whatever it is currently wearing. Clipped to the water rather
    // than scaled to fit it: a Daemon-sized sprite is wider than the panel, and a
    // creature cropped by the edge of the frame still reads as that creature where a
    // resampled one would stop being pixel art.
    const CreatureDef* subject = chromaSubject();
    const SpriteData* sprite = subject ? registry_.creatureSprite(*subject) : nullptr;
    if (sprite) {
        const int w = sprite->frameW * kScaleNum / kScaleDen;
        const int h = sprite->h * kScaleNum / kScaleDen;
        // MID-CHANGE the pixels the front has not reached yet are still wearing the
        // skin being LEFT, not the creature's own colours: on this board the pet is
        // never bare, so a repaint has to read as one skin dissolving into another. Pass
        // the old ramp and FX_CAMO does exactly that (camo.h's `from`).
        const CamoRamp worn = skinRamp(chroma_.worn());
        const CamoRamp leaving = skinRamp(chroma_.leaving());
        const uint8_t level = static_cast<uint8_t>(chroma_.wearPct() * 255 / 100);
        fb.setClip(kWaterX, kWaterY, kWaterW, kWaterH);
        drawSpriteCamo(fb, *sprite, idleFrame(*sprite, beat_),
                       kWaterX + (kWaterW - w) / 2, kWaterY + (kWaterH - h) / 2,
                       kScaleNum, kScaleDen, worn, level, /*flashColor=*/0,
                       /*flashAmt=*/0, /*row=*/0, &leaving);
        fb.clearClip();
    }

    // The sweep, over everything in the water — it is looking AT the creature, so it
    // cannot pass behind it. Held at the far edge once the run is over, where it stands
    // as the reason the verdict says what it says. Clipped to the water, because a
    // search light spilling onto the chrome either side would read as part of the
    // frame rather than as something moving through the scene.
    const int sweepX = kWaterX + (kWaterW - kSweepW) * chroma_.sweepPct() / 100;
    fb.setClip(kWaterX, kWaterY, kWaterW, kWaterH);
    // The soft edge runs AHEAD of the bar, brightening into it: the water lights up
    // just before the sweep gets there, which is what makes it read as approaching
    // rather than as a line that has already been and gone.
    for (int i = 0; i < kSweepLeadW; ++i)
        fb.fillRect(sweepX + kSweepW + i, kWaterY, 1, kWaterH,
                    blend(plate.tone[plate.count / 2], ink,
                          static_cast<uint8_t>(24 * (kSweepLeadW - 1 - i))));
    fb.fillRect(sweepX, kWaterY, kSweepW, kWaterH, ink);
    fb.clearClip();

    // The three chips: one per skin, in button order, each filled with the skin it
    // offers and named in words. The WORN one is the one with the frame around it —
    // shape, not brightness, so the answer to "what am I wearing" holds in grayscale
    // and against a chip whose own colour is nearly the ink's.
    for (int i = 0; i < kChromaSkins; ++i) {
        const int x = kWaterX + i * (kChipW + kChipGap);
        const CamoRamp r = skinRamp(i);
        fb.fillRect(x, kChipY, kChipW, kChipH, r.tone[r.count / 2]);
        if (i == chroma_.worn()) {
            fb.fillRect(x, kChipY, kChipW, 2, ink);
            fb.fillRect(x, kChipY + kChipH - 2, kChipW, 2, ink);
            fb.fillRect(x, kChipY, 2, kChipH, ink);
            fb.fillRect(x + kChipW - 2, kChipY, 2, kChipH, ink);
        }
        // The label takes the ramp's darkest or lightest tone, whichever the chip is
        // not — the one piece of text on this screen that cannot use INK, since the
        // chip beneath it is a different colour on every row.
        const Rgb565 label = luminance(r.tone[r.count / 2]) > 0.5f ? r.tone[0]
                                                                   : r.tone[r.count - 1];
        const char* name = kSkins[i].name;
        drawText(fb, x + (kChipW - textWidth(name)) / 2, kChipY + (kChipH - kFontH) / 2,
                 name, label);
    }

    if (!chroma_.running()) {
        const bool clean = chroma_.clean();
        const char* verdict = clean ? "NEVER SEEN" : "SPOTTED - RUN ENDED";
        drawText(fb, (kActiveW - textWidth(verdict)) / 2, kVerdictY, verdict, ink);

        // The effect line is priced in incubation, which an arcade run doesn't have —
        // there it reports the passes themselves and lets the payout screen do the money.
        char effect[32];
        if (arcadeRun_)
            std::snprintf(effect, sizeof(effect), "%d / %d PASSES", chroma_.passes(),
                          chroma_.goal());
        else if (bootHatchRemainMs_ <=
                 static_cast<uint32_t>(chroma_.passes()) * kChromaPassMs)
            std::snprintf(effect, sizeof(effect), "HATCHING NOW");
        else
            std::snprintf(effect, sizeof(effect), "-%u MIN INCUBATION",
                          static_cast<unsigned>(static_cast<uint32_t>(chroma_.passes()) *
                                                kChromaPassMs / (60u * 1000u)));
        drawText(fb, (kActiveW - textWidth(effect)) / 2, kEffectY, effect,
                 chroma_.passes() > 0 ? palColor(Pal::ACCENT) : dim);

        const char* hint = "B CONTINUE   C DISABLED";
        drawText(fb, (kActiveW - textWidth(hint)) / 2, kHintY, hint, dim);
        return;
    }

    // Which round this is, and whether the creature is currently safe. HIDDEN is stated
    // in words as well as shown, because "it blends in" is the one thing on the screen a
    // player could reasonably disagree with the board about.
    char status[32];
    std::snprintf(status, sizeof(status), "PASS %d / %d", chroma_.round() + 1,
                  chroma_.goal());
    drawText(fb, (kActiveW - textWidth(status)) / 2, kVerdictY, status, ink);
    const char* state = chroma_.hidden() ? "HIDDEN" : "SHOWING";
    drawText(fb, (kActiveW - textWidth(state)) / 2, kEffectY, state,
             chroma_.hidden() ? palColor(Pal::ACCENT) : ink);

    // UI_HINT_BAND. All three buttons wear a skin here, so the band names them in the
    // order the chips sit — A on the left of the device is the left chip.
    char hint[36];
    std::snprintf(hint, sizeof(hint), "A %s  B %s  C %s", kSkins[0].name, kSkins[1].name,
                  kSkins[2].name);
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kHintY, hint, dim);
}

}  // namespace mal
