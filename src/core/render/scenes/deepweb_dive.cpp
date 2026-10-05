#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE DIVE, LOOKING DOWN THE SHAFT. The one place on the walk with no horizon to stand
// a skyline on, no floor and no silhouette: the dive is a descent, so the picture is
// the shaft itself — relay rings receding to a point, streaming past toward the viewer
// on the heartbeat, which is what going down looks like from inside. A fighter here
// stands on nothing, and that is correct.
//
// It is the scene that proves the primitives are optional. Nothing below calls
// sceneSilhouette, sceneMiddle or sceneFloor; the ground is used for one thing only,
// the vanishing point. That is NOT on the horizon, where sceneConverge would put it: a
// shaft has no horizon, and a point down at the sprites' feet leaves them standing on
// the bottom of it. It sits level with the middle of a standing sprite instead, so a
// fighter or a resting pet is IN the shaft — between the two fighters on a fight, and
// behind the pet in the habitat, with the rings opening out around it.
constexpr uint8_t kToneRail = 26;
constexpr uint8_t kToneRingFar = 18;     // the ring at the bottom of the shaft
constexpr uint8_t kToneRingNear = 112;   // ...at its brightest, on its way up
constexpr uint8_t kToneRingGone = 30;    // ...and passing the viewer, fading out
constexpr uint8_t kToneNode = 150;
constexpr uint8_t kToneNodeLit = 210;

// The shaft's rings, as half-heights in rows, smallest first: each a fifth again the
// last, so the table reads as depth. A ring is drawn at every kStepsPerRing-th entry
// and slides one entry outward per beat, so the whole shaft streams toward the viewer
// without any ring ever jumping. Two steps apart is about 1.4x ring to ring, which is
// close enough to read as a shaft rather than a target.
constexpr uint8_t kRingH[] = {2,  2,  3,  3,  4,  5,  6,  7,  8,  10, 12, 14, 16,
                              19, 23, 27, 32, 38, 45, 54, 64, 76, 91, 108, 128, 152};
constexpr int kRingSteps = static_cast<int>(sizeof(kRingH) / sizeof(kRingH[0]));
constexpr int kStepsPerRing = 2;
// The step a ring is brightest at. Past it a ring is mostly off the canvas — its sides
// gone, only its top and bottom crossing the screen — and a full-tone line across the
// rows a screen writes its header in is a rule, not a ring; so from here it fades.
constexpr int kPeakStep = 18;

// A ring is an octagon: wider than it is tall by this ratio (in eighths), with its
// corners cut at 45 degrees by this share of its half-height (in eighths). A flat
// rectangle reads as a frame and a true ellipse at these sizes is a staircase; a
// chamfered box is the shape a pixel grid draws cleanly at every size in the table.
constexpr int kRingAspect8 = 13;
constexpr int kRingChamfer8 = 3;

// The relay nodes on each ring, at the middle of every side. One node in the whole
// shaft carries the packet on any beat, walking round the ring nearest the viewer, so
// the shaft is a network and not only a tunnel.
constexpr int kNodeSize = 2;

// Where the vanishing point sits, as a row: this far above the floor, which is the
// middle of a standing sprite on either screen.
constexpr int kVanishAboveFloor = 30;
int vanishY(const SceneGround& g) { return g.floorY - kVanishAboveFloor; }

// A ring's tone at a step: up from the bottom of the shaft to the peak, then back down
// as it passes.
uint8_t ringTone(int step) {
    if (step <= kPeakStep)
        return static_cast<uint8_t>(kToneRingFar +
                                    (kToneRingNear - kToneRingFar) * step / kPeakStep);
    const int past = kRingSteps - 1 - kPeakStep;
    return static_cast<uint8_t>(kToneRingNear -
                                (kToneRingNear - kToneRingGone) * (step - kPeakStep) / past);
}

// One ring's outline, centred on (cx, cy) with half-height h.
void drawRing(Framebuffer& fb, int cx, int cy, int h, Rgb565 c) {
    const int w = h * kRingAspect8 / 8;
    const int k = h * kRingChamfer8 / 8;   // how far a corner is cut back
    fb.fillRect(cx - w + k, cy - h, 2 * (w - k) + 1, 1, c);   // top
    fb.fillRect(cx - w + k, cy + h, 2 * (w - k) + 1, 1, c);   // bottom
    fb.fillRect(cx - w, cy - h + k, 1, 2 * (h - k) + 1, c);   // left
    fb.fillRect(cx + w, cy - h + k, 1, 2 * (h - k) + 1, c);   // right
    for (int i = 1; i < k; ++i) {                             // the four corners
        fb.fillRect(cx - w + k - i, cy - h + i, 1, 1, c);
        fb.fillRect(cx + w - k + i, cy - h + i, 1, 1, c);
        fb.fillRect(cx - w + k - i, cy + h - i, 1, 1, c);
        fb.fillRect(cx + w - k + i, cy + h - i, 1, 1, c);
    }
}

}  // namespace

void drawDeepWebDiveScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));
    const int cx = kActiveW / 2, cy = vanishY(g);

    // The rails: the shaft's four corners, running in from the canvas corners to the
    // point. Dotted and faint — they are what tells the rings apart from a target.
    const Rgb565 rail = sceneTone(kToneRail);
    const int corners[4][2] = {{0, 0}, {kActiveW - 1, 0}, {0, kActiveH - 1},
                               {kActiveW - 1, kActiveH - 1}};
    for (const auto& c : corners) {
        const int dx = c[0] - cx, dy = c[1] - cy;
        const int n = (dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy) ? (dx < 0 ? -dx : dx)
                                                                : (dy < 0 ? -dy : dy);
        for (int i = 6; i <= n; i += 3) fb.fillRect(cx + dx * i / n, cy + dy * i / n, 1, 1, rail);
    }

    // The rings, far to near so a near one is drawn over whatever it passes. `shift` is
    // how far through one ring-spacing the shaft has streamed on this beat.
    const int shift = beat % kStepsPerRing;
    const int nearest = (kRingSteps - 1 - shift) / kStepsPerRing;   // ring count - 1
    // The packet rides the brightest ring, a node further round every time the shaft
    // moves a whole ring.
    const int packetRing = (kPeakStep - shift) / kStepsPerRing;
    const int packetNode = (beat / kStepsPerRing) % 4;
    for (int r = 0; r <= nearest; ++r) {
        const int step = r * kStepsPerRing + shift;
        const int h = kRingH[step];
        const uint8_t tone = ringTone(step);
        drawRing(fb, cx, cy, h, sceneTone(tone));

        // The nodes, from the fourth ring out: any nearer the point they are a smudge.
        if (r < 3) continue;
        const int w = h * kRingAspect8 / 8;
        const int nodes[4][2] = {{cx, cy - h}, {cx + w, cy}, {cx, cy + h}, {cx - w, cy}};
        for (int i = 0; i < 4; ++i) {
            const bool lit = r == packetRing && i == packetNode;
            const uint8_t node = tone + 30 > kToneNode ? kToneNode : tone + 30;
            fb.fillRect(nodes[i][0] - kNodeSize / 2, nodes[i][1] - kNodeSize / 2, kNodeSize,
                        kNodeSize, sceneTone(lit ? kToneNodeLit : node));
        }
    }
}

}  // namespace mal
