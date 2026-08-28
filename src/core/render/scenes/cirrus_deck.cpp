#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// ABOVE THE CLOUD DECK — where a flier is at home, and the only place in the set whose
// FLOOR is not solid. A creature that holds an altitude is not standing on anything, so
// the surface under it is a cloud top: soft-edged, drifting, and the one floor here a
// sprite is allowed to look like it is above rather than on.
//
// The composition is the opposite of the underwater pair beside it. There the light is
// overhead and the value falls away downward; here the light is BELOW, on the deck, and
// the sky above it is the emptiest field in the set — which is what altitude looks like.
constexpr uint8_t kToneHighSky = 8;
constexpr uint8_t kToneStar = 84;      // the ones that survive at this height
constexpr uint8_t kToneThunder = 46;   // heads standing above the deck, far off
constexpr uint8_t kToneDeckLip = 128;  // the lit tops of the cloud
constexpr uint8_t kToneDeck = 54;
constexpr uint8_t kToneDeckDeep = 30;
constexpr uint8_t kToneContrail = 116;

// Stars, and MORE of them than anywhere else on the ladder: at altitude there is less
// air to lose them in, and this scene's whole upper half is sky, so a handful of specks
// in it is an empty panel rather than a high one.
constexpr uint8_t kStars[][2] = {
    {28, 232}, {74, 210}, {118, 240}, {164, 216}, {198, 228}, {52, 190},
    {142, 184}, {12, 218}, {90, 246}, {130, 202}, {182, 244}, {216, 196},
    {40, 168}, {104, 172}, {156, 160}, {206, 176}, {66, 152}, {192, 148},
    {24, 140}, {126, 136}, {170, 128}, {84, 124}};

// The thunderheads: distant, blocky, standing well above the deck they grew out of.
// Ragged and uneven, because a cloud has no built edge — the one silhouette here that
// must not repeat inside a span the eye can hold.
constexpr uint8_t kHeads[] = {4, 9, 19, 27, 21, 11, 5, 3, 7, 5, 3, 8,
                              15, 24, 31, 22, 12, 5, 4, 10, 7, 3};

// The deck itself, in three fills that get deeper toward the viewer — a cloud top is
// lit where it faces the light and shadowed in its own folds, and three bands is enough
// to say so at this size.
constexpr int kDeckBands = 3;

// The billows along the lip: discs sunk into the deck so only their crowns rise above
// it, spaced closer than they are wide so the tops OVERLAP into one lumpy mass. That
// overlap is the whole trick — spaced apart they are a row of beads on a shelf, and a
// row of beads is not a cloud.
struct Billow { int x, r; };
constexpr Billow kBillows[] = {{6, 15}, {30, 11}, {52, 17}, {78, 12}, {104, 19},
                               {134, 13}, {158, 18}, {186, 12}, {210, 16}};
constexpr int kBillowSpan = 10;
constexpr int kBillowSink = 3;   // the crown shows 1/nth of the disc

// One contrail, crossing high and slowly. Something else is up here and it is not
// stopping — the slowest thing on the panel, and dim enough never to fight a glyph.
constexpr uint8_t kTrailUp = 202;
constexpr int kTrailW = 34, kTrailStep = 4;

}  // namespace

void drawCirrusDeckScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    sceneSpecks(fb, kStars, static_cast<int>(sizeof(kStars) / sizeof(kStars[0])), g,
                kToneStar);
    sceneGlow(fb, g, /*up=*/96, kToneHighSky);

    // The contrail, wrapping across the width. Two rows: a stroke and the fainter smear
    // under it, which is the difference between a line and something that left it.
    const int trailX = (beat * kTrailStep) % (kActiveW + kTrailW) - kTrailW;
    const int trailY = sceneSkyY(g, kTrailUp);
    fb.fillRect(trailX, trailY, kTrailW, 1, sceneTone(kToneContrail));
    fb.fillRect(trailX - kTrailW / 3, trailY + 1, kTrailW, 1, sceneTone(kToneHighSky + 24));

    sceneSilhouette(fb, kHeads, static_cast<int>(sizeof(kHeads) / sizeof(kHeads[0])),
                    g.horizonY, kToneThunder);

    // The billows go down FIRST and the deck is banded over them, so all that survives
    // of each disc is the crown standing above the lip. Painted the other way round, a
    // disc's lower half sits on top of the deck's darker bands and hangs under it as a
    // pale scallop — a cloud with the light coming from inside it.
    for (int i = 0; i < static_cast<int>(sizeof(kBillows) / sizeof(kBillows[0])); ++i) {
        const Billow& b = kBillows[i];
        sceneDisc(fb, b.x + sceneDrift(beat, i, kBillowSpan),
                  g.horizonY + b.r - b.r / kBillowSink, b.r, kToneDeck);
    }

    // The deck, deepening toward the viewer.
    for (int i = 0; i < kDeckBands; ++i) {
        const int y0 = g.horizonY + (kActiveH - g.horizonY) * i / kDeckBands;
        const int y1 = g.horizonY + (kActiveH - g.horizonY) * (i + 1) / kDeckBands;
        const uint8_t t = static_cast<uint8_t>(
            kToneDeck + (kToneDeckDeep - kToneDeck) * i / (kDeckBands - 1));
        fb.fillRect(0, y0, kActiveW, y1 - y0, sceneTone(t));
    }

    // One lit row along the top of each crown, which is where the light that is under
    // this scene actually falls.
    for (int i = 0; i < static_cast<int>(sizeof(kBillows) / sizeof(kBillows[0])); ++i) {
        const Billow& b = kBillows[i];
        const int cx = b.x + sceneDrift(beat, i, kBillowSpan);
        fb.fillRect(cx - b.r / 2, g.horizonY - b.r / kBillowSink + 1, b.r, 1,
                    sceneTone(kToneDeckLip));
    }
}

}  // namespace mal
