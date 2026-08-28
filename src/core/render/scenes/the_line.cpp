#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE LINE — the pass of a working kitchen, seen from the cook's side. The prize the
// MERGE HUB's recipe ladder pays out, so what it is made of is the ladder's own
// vocabulary: a ticket rail with paper on it, burners with rings alight, a shelf of
// pans, and stainless everywhere else.
//
// STAINLESS IS WHY THIS READS IN VALUE. A steel kitchen has no colour opinion of its
// own — it is the light bouncing off it — so the room is the plain ramp and the only
// tint in the scene is FRAG_HI at the burners, low enough to be a warmth under a pan
// rather than a hue. That is the one warm point of light in a grey room, which is what
// a kitchen at service actually looks like.
//
// It has no distance in it: a horizon indoors is the far wall, so the sky band is
// backsplash tile rather than air, and everything is placed against the two lines the
// screen supplies exactly as an outdoor place would be.
constexpr uint8_t kToneWall = 26;        // the backsplash, behind everything
constexpr uint8_t kToneGrout = 44;       // its tiling, which is the only thing making it a wall
constexpr uint8_t kToneHood = 52;        // the extraction hood hanging over the pass
constexpr uint8_t kToneHoodLip = 96;     // its lit lower edge
constexpr uint8_t kToneBaffle = 34;      // the filter baffles across its face
constexpr uint8_t kToneRail = 74;        // the ticket rail
constexpr uint8_t kToneTicket = 132;     // paper on it — the brightest wide thing here
constexpr uint8_t kTonePan = 84;         // hanging pans
constexpr uint8_t kTonePanLit = 134;     // the side of one facing the hood
constexpr uint8_t kToneRange = 46;       // the range front, under the pass
constexpr uint8_t kToneSteel = 72;       // the pass surface
constexpr uint8_t kToneSeam = 48;
constexpr uint8_t kToneEdge = 126;       // its lit near lip
constexpr uint8_t kToneBurner = 82;      // a ring with nothing on it
constexpr uint8_t kToneFlame = 190;      // ...and one alight
constexpr uint8_t kToneGlow = 52;        // the wash a lit ring throws on the range front

// The backsplash. A tile is 16 across and 12 down, which at this size is the largest
// grid that still reads as tiling rather than as a second floor.
constexpr int kTileW = 16, kTileH = 12;

// The hood, hanging from the top of the canvas. Deep enough to be a hood and shallow
// enough that the rows a screen writes across the upper band still land on wall. The
// baffles are what stop it reading as a black bar: an extraction hood is a rank of
// angled filters, and at this size that is a pitch and a gap.
constexpr int kHoodDepth = 14;
constexpr int kBafflePitch = 12;

// The ticket rail and what is clipped to it: dockets at authored positions, each a
// couple of columns of paper hanging off a wire. Authored rather than rolled for the
// reason every scene's tables are — a screen's copy is read over this, and paper that
// reshuffled between repaints would be the one moving thing behind it.
constexpr int kRailDrop = 26;            // rows below the top of the canvas
struct Ticket { uint8_t x, w, h; };
constexpr Ticket kTickets[] = {
    {18, 9, 13}, {32, 7, 17}, {44, 9, 11}, {66, 8, 16},
    {120, 9, 12}, {134, 7, 18}, {146, 8, 12}, {184, 9, 15}, {198, 7, 11},
};

// The pans, hung on the wall between the rail and the pass. A pan at this size is a
// disc and a stub of handle, and they are given to one side so the middle of the panel
// — where a screen puts its sprite — keeps the plainest wall in the scene.
struct Pan { uint8_t x, r; };
constexpr Pan kPans[] = {{86, 8}, {104, 6}, {170, 7}};
constexpr int kPanDropBelowRail = 22;

// The burners. Four rings across the range top, each a shallow ellipse; a ring is
// ALIGHT on a slow rotation off the beat, so at any moment two of the four are lit and
// the pair walks along the line. A blink, not a slide — the same rule the rack lamps
// keep (mainframe_row.cpp).
constexpr int kBurners = 4;
constexpr int kBurnerR = 9;
constexpr int kBurnerPeriod = 4;

}  // namespace

void drawTheLineScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // The wall, all the way down to the pass. Indoors there is nothing behind it, so
    // it is a flat fill and the tiling is what gives it a surface.
    fb.fillRect(0, 0, kActiveW, g.horizonY, sceneTone(kToneWall));
    const Rgb565 grout = sceneTone(kToneGrout);
    for (int x = 0; x < kActiveW; x += kTileW) fb.fillRect(x, 0, 1, g.horizonY, grout);
    for (int y = kTileH; y < g.horizonY; y += kTileH)
        fb.fillRect(0, y, kActiveW, 1, grout);

    // The hood, and the lit strip along its lower edge — the light source the whole
    // room is lit by, and the reason the pans below it have a bright side.
    fb.fillRect(0, 0, kActiveW, kHoodDepth, sceneTone(kToneHood));
    const Rgb565 baffle = sceneTone(kToneBaffle);
    for (int x = 0; x < kActiveW; x += kBafflePitch)
        fb.fillRect(x, 3, 2, kHoodDepth - 5, baffle);
    fb.fillRect(0, kHoodDepth, kActiveW, 1, sceneTone(kToneHoodLip));

    // The rail, and the dockets hanging off it.
    const Rgb565 rail = sceneTone(kToneRail);
    const Rgb565 paper = sceneTone(kToneTicket);
    fb.fillRect(0, kRailDrop, kActiveW, 1, rail);
    for (const Ticket& t : kTickets) {
        fb.fillRect(t.x, kRailDrop - 2, 1, 3, rail);          // the clip
        fb.fillRect(t.x - t.w / 2, kRailDrop + 1, t.w, t.h, paper);
    }

    // The pans. Walked out from the radius the way every disc in the set is, with the
    // column nearest the hood's light taken a step brighter.
    const Rgb565 pan = sceneTone(kTonePan);
    const Rgb565 panLit = sceneTone(kTonePanLit);
    for (const Pan& p : kPans) {
        const int cy = kRailDrop + kPanDropBelowRail + p.r;
        // Body, then the RIM a step brighter all the way round, then the side facing
        // the hood brighter again. The rim is what makes it a pan: a flat disc at this
        // size is a hole in the wall, and the ring of light round the edge is the only
        // thing that says the object is turned toward you.
        for (int dy = -p.r + 1; dy < p.r; ++dy) {
            int w = 0;
            while ((w + 1) * (w + 1) + dy * dy <= p.r * p.r) ++w;
            fb.fillRect(p.x - w, cy + dy, 2 * w, 1, pan);
            fb.fillRect(p.x - w, cy + dy, 1, 1, panLit);
            fb.fillRect(p.x + w - 1, cy + dy, 1, 1, panLit);
        }
        fb.fillRect(p.x - p.r + 1, cy - p.r + 1, 2 * p.r - 2, 1, panLit);
        fb.fillRect(p.x - 1, cy - p.r - 5, 2, 5, pan);         // the handle, hung up
    }

    // The range front, filling the band between the wall and the pass — the one place
    // the burners' warmth lands anywhere but on the ring itself.
    sceneMiddle(fb, g, kToneRange);

    // The rings, sitting ON the pass line so they read as tops rather than as holes in
    // the front of the range.
    const Rgb565 cold = sceneTone(kToneBurner);
    for (int i = 0; i < kBurners; ++i) {
        const int cx = kActiveW * (2 * i + 1) / (2 * kBurners);
        const bool lit = ((beat + i) % kBurnerPeriod) < 2;
        // A ring is an ellipse half as tall as it is wide: a circle seen at a shallow
        // angle, which is the only thing that puts the range top in the same
        // perspective as the floor under it.
        for (int dy = -kBurnerR / 2; dy <= kBurnerR / 2; ++dy) {
            int w = 0;
            while ((w + 1) * (w + 1) * (kBurnerR / 2) * (kBurnerR / 2) +
                       dy * dy * kBurnerR * kBurnerR <=
                   kBurnerR * kBurnerR * (kBurnerR / 2) * (kBurnerR / 2))
                ++w;
            fb.fillRect(cx - w, g.horizonY - 3 + dy, 2 * w, 1, cold);
        }
        if (!lit) continue;
        // Alight: a short arc of flame across the ring, and a wash of it on the range
        // front below. The flame is the scene's only tint and its only bright pixels,
        // and there are a dozen of them per ring by construction.
        const Rgb565 flame = sceneTint(kToneFlame, Pal::FRAG_HI);
        for (int k = -kBurnerR / 2; k <= kBurnerR / 2; ++k)
            fb.fillRect(cx + k, g.horizonY - 4 - (k * k) / (kBurnerR + 1), 1, 3, flame);
        fb.fillRect(cx - kBurnerR, g.horizonY, 2 * kBurnerR, g.floorY - g.horizonY,
                    sceneTint(kToneGlow, Pal::FRAG_HI));
    }

    // The pass itself: steel, with the seams of the gastronorm wells across it.
    sceneFloor(fb, g, /*seamPitch=*/38, kToneSteel, kToneSeam, kToneEdge);
}

}  // namespace mal
