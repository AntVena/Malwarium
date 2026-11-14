#include "core/app/game/chroma_screen.h"

#include <cstdio>

#include "tunables.h"
#include "core/model/chromatophore.h"
#include "core/render/camo.h"
#include "core/render/canvas.h"
#include "core/render/font.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"
#include "core/render/sprite.h"

namespace mal {

namespace {

// One skin: the colour it is, and what it is called. Indexed, as the model is:
// kSkins[i] maps a rule to a screen.
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
// active res like other chrome, because nothing here is art with a cell grid to keep.
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
// out of the same ramp, so a plate only ever shows its own colour, and the pattern
// identifies the skin without hue: weed runs vertical, seabed grains stipple, a bloom
// drifts in horizontal bands.
void drawPlate(Framebuffer& fb, int skin, const CamoRamp& ramp) {
    fb.fillRect(kWaterX, kWaterY, kWaterW, kWaterH, ramp.tone[ramp.count / 2]);
    const Rgb565 dark = ramp.tone[0];
    const Rgb565 lit = ramp.tone[ramp.count - 1];
    switch (skin) {
        case 0:   // KELP: standing weed
            for (int x = 0; x < kWaterW; x += 12) {
                fb.fillRect(kWaterX + x, kWaterY, 3, kWaterH, dark);
                fb.fillRect(kWaterX + x + 6, kWaterY + 6, 1, kWaterH - 12, lit);
            }
            break;
        case 1:   // SILT: grains on the bed
            for (int y = 0; y < kWaterH; y += 8)
                for (int x = (y / 8 % 2) * 4; x < kWaterW; x += 8) {
                    fb.fillRect(kWaterX + x, kWaterY + y, 2, 2, dark);
                    fb.fillRect(kWaterX + x + 4, kWaterY + y + 4, 1, 1, lit);
                }
            break;
        default:  // BLOOM: drifting bands
            for (int y = 0; y < kWaterH; y += 14) {
                fb.fillRect(kWaterX, kWaterY + y, kWaterW, 4, dark);
                fb.fillRect(kWaterX, kWaterY + y + 6, kWaterW, 1, lit);
            }
            break;
    }
}

}  // namespace

void ChromaScreen::onButton(ChromaHost& host, const ButtonEvent& ev) {
    if (host.onGameBriefInput(ev)) return;
    Chromatophore& run = host.liveChroma();
    if (!run.running()) {
        // Parked on the result. B banks and leaves. C is disabled, as on every
        // hatch screen, because there is no pet to go back to.
        if (ev.button == Button::B) host.finishChroma();
        return;
    }
    // ALL THREE BUTTONS ARE SKINS. The deviation from the standard A/B/C contract this
    // screen spells out in its hint band, and the reason the game can be played at all:
    // a skin has to be one press, in the order the chips are drawn, or the choice costs
    // more time than the window gives. B is therefore not a commit and C is not a
    // cancel; a run ends by being spotted or by finishing, and B comes back as CONTINUE
    // on the verdict above.
    if (ev.button == Button::A) run.wear(0);
    else if (ev.button == Button::B) run.wear(1);
    else if (ev.button == Button::C) run.wear(2);
}

// --- Render ----------------------------------------------------------------

void ChromaScreen::draw(Framebuffer& fb, const ChromaHost& host) const {
    if (host.gameBriefOpen()) { host.drawGameBrief(fb); return; }
    const Chromatophore& run = host.chroma();
    fb.clear(palColor(Pal::PAPER));

    const Rgb565 ink = palColor(Pal::INK);
    const Rgb565 dim = palColor(Pal::INK_DIM);
    const char* title = "CHROMATOPHORE";
    drawText(fb, (kActiveW - textWidth(title)) / 2, kTitleY, title, ink);

    const CamoRamp plate = skinRamp(run.plate());
    drawPlate(fb, run.plate(), plate);

    // The creature, in whatever it is currently wearing. Clipped to the water rather
    // than scaled to fit it: a Daemon-sized sprite is wider than the panel, and a
    // creature cropped by the edge of the frame still reads as that creature where a
    // resampled one would stop being pixel art.
    const CreatureDef* subject = host.chromaSubject();
    const SpriteData* sprite = subject ? host.creatureSprite(*subject) : nullptr;
    if (sprite) {
        const int w = sprite->frameW * kScaleNum / kScaleDen;
        const int h = sprite->h * kScaleNum / kScaleDen;
        // MID-CHANGE the pixels the front has not reached yet are still wearing the
        // skin being LEFT, not the creature's own colours: on this board the pet is
        // never bare, so a repaint has to read as one skin dissolving into another. Pass
        // the old ramp and FX_CAMO does exactly that (camo.h's `from`).
        const CamoRamp worn = skinRamp(run.worn());
        const CamoRamp leaving = skinRamp(run.leaving());
        const uint8_t level = static_cast<uint8_t>(run.wearPct() * 255 / 100);
        fb.setClip(kWaterX, kWaterY, kWaterW, kWaterH);
        drawSpriteCamo(fb, *sprite, idleFrame(*sprite, host.screenBeat()),
                       kWaterX + (kWaterW - w) / 2, kWaterY + (kWaterH - h) / 2,
                       kScaleNum, kScaleDen, worn, level, /*flashColor=*/0,
                       /*flashAmt=*/0, /*row=*/0, &leaving);
        fb.clearClip();
    }

    // The sweep is drawn over everything in the water, because it looks at the creature
    // and cannot pass behind it. Held at the far edge once the run is over, where it
    // stands as the reason the verdict says what it says. Clipped to the water, because
    // a search light spilling onto the chrome either side would read as part of the
    // frame rather than as something moving through the scene.
    const int sweepX = kWaterX + (kWaterW - kSweepW) * run.sweepPct() / 100;
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
    // offers and named in words. The worn skin gets a frame, a shape cue, so it
    // reads in grayscale and on a chip whose colour is close to the ink's.
    for (int i = 0; i < kChromaSkins; ++i) {
        const int x = kWaterX + i * (kChipW + kChipGap);
        const CamoRamp r = skinRamp(i);
        fb.fillRect(x, kChipY, kChipW, kChipH, r.tone[r.count / 2]);
        if (i == run.worn()) {
            fb.fillRect(x, kChipY, kChipW, 2, ink);
            fb.fillRect(x, kChipY + kChipH - 2, kChipW, 2, ink);
            fb.fillRect(x, kChipY, 2, kChipH, ink);
            fb.fillRect(x + kChipW - 2, kChipY, 2, kChipH, ink);
        }
        // The label takes the ramp's darkest or lightest tone, whichever the chip is
        // not: the only text on this screen that cannot use INK, because the chip
        // under it changes colour per row.
        const Rgb565 label = luminance(r.tone[r.count / 2]) > 0.5f ? r.tone[0]
                                                                   : r.tone[r.count - 1];
        const char* name = kSkins[i].name;
        drawText(fb, x + (kChipW - textWidth(name)) / 2, kChipY + (kChipH - kFontH) / 2,
                 name, label);
    }

    if (!run.running()) {
        const bool clean = run.clean();
        const char* verdict = clean ? "NEVER SEEN" : "SPOTTED - RUN ENDED";
        drawText(fb, (kActiveW - textWidth(verdict)) / 2, kVerdictY, verdict, ink);

        // The effect line is priced in incubation, which an arcade run lacks, so there it
        // reports passes and the payout screen shows the money.
        char effect[32];
        if (host.inArcadeRun())
            std::snprintf(effect, sizeof(effect), "%d / %d PASSES", run.passes(),
                          run.goal());
        else if (host.bootHatchRemainMs() <=
                 static_cast<uint32_t>(run.passes()) * kChromaPassMs)
            std::snprintf(effect, sizeof(effect), "HATCHING NOW");
        else if (run.passes() == 0)
            std::snprintf(effect, sizeof(effect), "HATCHES ON ITS OWN CLOCK");
        else
            std::snprintf(effect, sizeof(effect), "HATCHES %u MIN SOONER",
                          static_cast<unsigned>(static_cast<uint32_t>(run.passes()) *
                                                kChromaPassMs / (60u * 1000u)));
        drawText(fb, (kActiveW - textWidth(effect)) / 2, kEffectY, effect,
                 run.passes() > 0 ? palColor(Pal::ACCENT) : dim);

        const char* hint = "B CONTINUE   C DISABLED";
        drawText(fb, (kActiveW - textWidth(hint)) / 2, kHintY, hint, dim);
        return;
    }

    // Which round this is, and whether the creature is currently safe. HIDDEN is stated
    // in words as well as shown, because "it blends in" is the one thing on the screen a
    // player could reasonably disagree with the board about.
    char status[32];
    std::snprintf(status, sizeof(status), "PASS %d / %d", run.round() + 1,
                  run.goal());
    drawText(fb, (kActiveW - textWidth(status)) / 2, kVerdictY, status, ink);
    const char* state = run.hidden() ? "HIDDEN" : "SHOWING";
    drawText(fb, (kActiveW - textWidth(state)) / 2, kEffectY, state,
             run.hidden() ? palColor(Pal::ACCENT) : ink);

    // UI_HINT_BAND. All three buttons wear a skin here, so the band names them in the
    // order the chips sit: A on the left of the device is the left chip.
    char hint[36];
    std::snprintf(hint, sizeof(hint), "A %s  B %s  C %s", kSkins[0].name, kSkins[1].name,
                  kSkins[2].name);
    drawText(fb, (kActiveW - textWidth(hint)) / 2, kHintY, hint, dim);
}

}  // namespace mal
