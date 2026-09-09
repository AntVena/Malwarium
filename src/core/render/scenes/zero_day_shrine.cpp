#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// ZERO DAY SHRINE — a colonnade underground, and a shaft of light on the altar.
//
// No distance: you are inside it, so nothing is drawn on the horizon and the bands are
// taken off the ground the screen hands over.
constexpr Pal kStone = Pal::FRAG_LO;
constexpr Pal kLight = Pal::FRAG_HI;
constexpr Pal kVotive = Pal::NEON_HI;

constexpr uint8_t kToneVault = 10;      // the rock behind everything
constexpr uint8_t kToneDrip = 38;       // stalactites off the roof
constexpr uint8_t kToneWall = 58;       // the pierced wall the bays are cut out of
constexpr uint8_t kToneArch = 96;       // the ring of voussoirs round each opening
constexpr uint8_t kToneBand = 88;       // the impost the arches spring from
constexpr uint8_t kToneShaft = 60;      // the beam, which is WIDE
constexpr uint8_t kToneMote = 126;
constexpr uint8_t kTonePool = 74;       // where it lands
constexpr uint8_t kToneDais = 84;
constexpr uint8_t kToneSlab = 138;
constexpr uint8_t kToneVotive = 186;
constexpr uint8_t kToneFloor = 26;
constexpr uint8_t kToneSeam = 40;
constexpr uint8_t kToneEdge = 82;

// Where the roof stops and where the arches spring from, as fractions of this ground's
// sky — the two bands the room is built between.
constexpr uint8_t kRoofUp = 238;
constexpr uint8_t kSpringUp = 150;

constexpr uint8_t kDrips[] = {7, 12, 5, 16, 9, 4, 13, 6, 18, 8, 3, 11,
                              15, 6, 9, 4, 14, 7, 10, 5, 17, 8, 4, 12};
constexpr int kDripN = static_cast<int>(sizeof(kDrips) / sizeof(kDrips[0]));

// Four bays cut through the wall, by centre. The altar stands in the third, off-centre,
// where nothing a screen draws over this will sit on top of it.
constexpr int kBayX[] = {34, 90, 156, 208};
constexpr int kBayN = static_cast<int>(sizeof(kBayX) / sizeof(kBayX[0]));
constexpr int kBayHalf = 19;
constexpr int kArchRing = 3;
constexpr int kAltarBay = 2;

constexpr int kDaisSteps = 3;
constexpr int kStepH = 4;
constexpr int kStepOut = 6;
constexpr int kSlabW = 22;

constexpr int kShaftTopW = 12;
constexpr int kMotePitch = 8;
constexpr int kVotivePitch = 26;

}  // namespace

void drawZeroDayShrineScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    const int roofY = sceneSkyY(g, kRoofUp);
    const int springY = sceneSkyY(g, kSpringUp);
    const Rgb565 vault = sceneTint(kToneVault, kStone);

    fb.fillRect(0, 0, kActiveW, g.floorY, vault);

    // The wall stops short of the cavern roof, so the room is a building standing INSIDE
    // a cave rather than a facade filling the canvas — and the stalactites have somewhere
    // to hang.
    const int wallTop = springY - kBayHalf - 10;
    fb.fillRect(0, wallTop, kActiveW, g.floorY - wallTop, sceneTint(kToneWall, kStone));
    sceneOverhang(fb, kDrips, kDripN, roofY, kToneDrip, {}, kStone);

    // The bays, cut back out of the wall: an arched opening is a hole, so it is drawn as
    // one rather than as two piers that happen to leave a gap.
    const Rgb565 arch = sceneTint(kToneArch, kStone);
    fb.fillRect(0, wallTop, kActiveW, 2, sceneTint(kToneBand, kStone));   // the cornice
    for (int i = 0; i < kBayN; ++i) {
        const int cx = kBayX[i];
        for (int y = springY; y < g.floorY; ++y)          // the jambs down to the floor
            fb.fillRect(cx - kBayHalf, y, kBayHalf * 2, 1, vault);
        for (int dy = 0; dy <= kBayHalf; ++dy) {          // ...and the head over them
            const int half = static_cast<int>(
                __builtin_sqrt(static_cast<double>(kBayHalf * kBayHalf - dy * dy)));
            fb.fillRect(cx - half, springY - dy, half * 2, 1, vault);
        }
        // The ring, walked one row at a time so it follows the curve it is drawn on.
        for (int dy = 0; dy <= kBayHalf; ++dy) {
            const int half = static_cast<int>(
                __builtin_sqrt(static_cast<double>(kBayHalf * kBayHalf - dy * dy)));
            fb.fillRect(cx - half - kArchRing, springY - dy, kArchRing, 1, arch);
            fb.fillRect(cx + half, springY - dy, kArchRing, 1, arch);
        }
        fb.fillRect(cx - kBayHalf - kArchRing, springY, kArchRing, g.floorY - springY, arch);
        fb.fillRect(cx + kBayHalf, springY, kArchRing, g.floorY - springY, arch);
    }
    fb.fillRect(0, springY - 1, kActiveW, 2, sceneTint(kToneBand, kStone));

    // The beam, through the altar's own bay. Its edges are stepped rather than straight
    // so it reads as light rather than as another piece of masonry.
    const int cx = kBayX[kAltarBay];
    const Rgb565 shaft = sceneTint(kToneShaft, kLight);
    for (int y = roofY; y < g.floorY; ++y) {
        const int t = y - roofY, span = g.floorY - roofY;
        const int half = (kShaftTopW + (kBayHalf * 2 - kShaftTopW) * t / span) / 2;
        const int jitter = ((y >> 1) & 1);
        fb.fillRect(cx - half - jitter, y, half * 2 + jitter * 2, 1, shaft);
    }
    const Rgb565 mote = sceneTint(kToneMote, kLight);
    for (int y = roofY + 3; y < g.floorY - 4; y += kMotePitch) {
        const int t = y - roofY, span = g.floorY - roofY;
        const int half = (kShaftTopW + (kBayHalf * 2 - kShaftTopW) * t / span) / 2;
        fb.fillRect(cx - half + sceneDrift(beat, y, half * 2), y, 2, 1, mote);
    }

    sceneFloor(fb, g, /*seamPitch=*/56, kToneFloor, kToneSeam, kToneEdge, kStone);

    // Where the beam lands, spread across the floor in front of the dais.
    const Rgb565 pool = sceneTint(kTonePool, kLight);
    for (int i = 0; i < 4; ++i)
        fb.fillRect(cx - kBayHalf - i * 3, g.floorY + i * 3, (kBayHalf + i * 3) * 2, 1, pool);

    // The altar: steps, and a lit slab with nothing standing on it.
    const Rgb565 dais = sceneTint(kToneDais, kStone);
    for (int s = 0; s < kDaisSteps; ++s) {
        const int w = kSlabW + (kDaisSteps - s) * kStepOut * 2;
        fb.fillRect(cx - w / 2, g.floorY - (s + 1) * kStepH, w, kStepH, dais);
    }
    fb.fillRect(cx - kSlabW / 2, g.floorY - (kDaisSteps + 1) * kStepH - 1, kSlabW,
                kStepH + 1, sceneTint(kToneSlab, kLight));

    // The votives, the one thing in the room above the wide-tone ceiling.
    const Rgb565 votive = sceneTint(kToneVotive, kVotive);
    for (int i = 0; i < kBayN; ++i) {
        if (i == kAltarBay) continue;
        const int vx = kBayX[i] + kBayHalf + kVotivePitch / 2;
        if (vx + 2 >= kActiveW) continue;
        if (((beat + i) % kBayN) != 0) fb.fillRect(vx, g.floorY - 6, 2, 3, votive);
    }
}

}  // namespace mal
