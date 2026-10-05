#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE SPECIMEN HALL. The prize for a full bestiary: a natural-history hall after
// closing, where everything the operator has beaten ends up. The landmark is the one
// every such hall has — a great skeleton hung from the ceiling by wires — and here it
// is a WORM's, a long serpent of vertebrae and ribs, since the oldest thing on the 'net
// is a worm. Display cases stand in the two margins with their specimens in them,
// which is where nothing ever stands in front of them; the floor is the hall's own
// chequered stone.
//
// Nothing moves. A museum after hours is the one place that should not.
constexpr Pal kGlass = Pal::FRAG_LO;   // the cases' glass, and the light it catches
constexpr uint8_t kToneWall = 18;
constexpr uint8_t kTonePilaster = 28;
constexpr uint8_t kToneWire = 44;
constexpr uint8_t kToneBone = 120;
constexpr uint8_t kToneRib = 92;
constexpr uint8_t kToneEye = 0;        // the skull's socket: a hole, not a mark
constexpr uint8_t kToneCase = 36;
constexpr uint8_t kToneCaseFrame = 70;
constexpr uint8_t kToneSheen = 90;
constexpr uint8_t kToneSpecimen = 58;
constexpr uint8_t kToneTileA = 40;
constexpr uint8_t kToneTileB = 52;
constexpr uint8_t kToneTileEdge = 96;

// The wall is broken into bays by pilasters, `kBayW` apart.
constexpr int kBayW = 44, kPilasterW = 4;

// The skeleton's spine, as (x, rows below its hanging line) every few columns from
// tail to skull, and how far that line hangs down the sky. A table rather than a sine
// so the curve can be shaped by eye — a hung skeleton is posed, not a waveform.
constexpr uint8_t kHangUp = 168;
constexpr uint8_t kSpine[][2] = {{28, 9},  {40, 6},  {52, 3},  {64, 2},  {76, 4},
                                 {88, 8},  {100, 11}, {112, 12}, {124, 10}, {136, 6},
                                 {148, 3}, {160, 2},  {172, 4},  {182, 7}};
constexpr int kSpineN = static_cast<int>(sizeof(kSpine) / sizeof(kSpine[0]));
constexpr int kWires[] = {3, 7, 11};   // which spine points hang from the ceiling

// The display cases, one in each margin: (x, width, height above the floor), and the
// specimen standing in each, as a 7-wide mask — a stooped malbeast in the left case and
// a creature with a crest in the right one, both dark against their glass.
struct Case { int x, w, h; };
constexpr Case kCases[] = {{4, 30, 58}, {kActiveW - 34, 30, 58}};
constexpr int kSpecW = 9, kSpecH = 10;
constexpr const char* kSpecimens[2][kSpecH] = {
    {"...XXX...", "..XXXXX..", "..X.XXX..", "..XXXXX..", ".XXXXXXX.",
     "XXXXXXXXX", "XXXXXXXXX", ".XXXXXXX.", ".XX...XX.", ".XX...XX."},
    {"X...X...X", ".X.XXX.X.", "..XXXXX..", "..X.X.X..", "..XXXXX..",
     "...XXX...", "..XXXXX..", ".XXXXXXX.", "..XX.XX..", "..XX.XX.."},
};

// The floor's tiles, square, chequered.
constexpr int kTile = 12;

}  // namespace

void drawSpecimenHallScene(Framebuffer& fb, int beat, const SceneGround& g) {
    (void)beat;
    fb.clear(palColor(Pal::PAPER));

    // The hall: a dark wall, broken into bays.
    fb.fillRect(0, 0, kActiveW, g.floorY, sceneTone(kToneWall));
    const Rgb565 pilaster = sceneTone(kTonePilaster);
    for (int x = kBayW / 2; x < kActiveW; x += kBayW)
        fb.fillRect(x - kPilasterW / 2, 0, kPilasterW, g.floorY, pilaster);

    // The skeleton: wires up to the ceiling, then the spine, its ribs shrinking toward
    // the tail, and the skull at the head end.
    const int hang = sceneSkyY(g, kHangUp);
    const Rgb565 wire = sceneTone(kToneWire);
    for (int w : kWires)
        fb.fillRect(kSpine[w][0], 0, 1, hang + kSpine[w][1], wire);
    const Rgb565 bone = sceneTone(kToneBone), rib = sceneTone(kToneRib);
    for (int i = 0; i + 1 < kSpineN; ++i) {
        const int x0 = kSpine[i][0], x1 = kSpine[i + 1][0];
        const int y0 = hang + kSpine[i][1], y1 = hang + kSpine[i + 1][1];
        for (int x = x0; x < x1; ++x)
            fb.fillRect(x, y0 + (y1 - y0) * (x - x0) / (x1 - x0), 1, 2, bone);
        // A rib pair at every vertebra: longer toward the head, which is the right.
        const int len = 2 + i / 2;
        for (int k = 0; k < 2; ++k) {
            const int rx = x0 + k * (x1 - x0) / 2;
            const int ry = y0 + (y1 - y0) * k / 2;
            fb.fillRect(rx, ry - len, 1, len, rib);
            fb.fillRect(rx, ry + 2, 1, len, rib);
        }
    }
    const int sx = kSpine[kSpineN - 1][0], sy = hang + kSpine[kSpineN - 1][1];
    fb.fillRect(sx, sy - 4, 12, 7, bone);              // the cranium
    fb.fillRect(sx + 12, sy, 6, 3, bone);              // the snout
    fb.fillRect(sx + 4, sy + 3, 12, 2, rib);           // the jaw, hanging a little open
    fb.fillRect(sx + 7, sy - 2, 2, 2, sceneTone(kToneEye));

    // The cases, with their specimens behind the glass and the light on it.
    const Rgb565 frame = sceneTint(kToneCaseFrame, kGlass);
    const Rgb565 spec = sceneTone(kToneSpecimen);
    const Rgb565 sheen = sceneTint(kToneSheen, kGlass);
    for (int i = 0; i < 2; ++i) {
        const Case& c = kCases[i];
        const int ty = g.floorY - c.h;
        fb.fillRect(c.x, ty, c.w, c.h, sceneTint(kToneCase, kGlass));
        fb.fillRect(c.x, ty, c.w, 1, frame);
        fb.fillRect(c.x, ty, 1, c.h, frame);
        fb.fillRect(c.x + c.w - 1, ty, 1, c.h, frame);
        fb.fillRect(c.x, g.floorY - 10, c.w, 1, frame);      // the plinth's lid
        fb.fillRect(c.x, g.floorY - 10, c.w, 10, sceneTone(kToneCase));
        const int ox = c.x + (c.w - kSpecW) / 2, oy = g.floorY - 10 - kSpecH - 4;
        for (int r = 0; r < kSpecH; ++r)
            for (int k = 0; k < kSpecW; ++k)
                if (kSpecimens[i][r][k] == 'X') fb.fillRect(ox + k, oy + r, 1, 1, spec);
        for (int d = 0; d < 10; ++d)                         // the sheen, a diagonal
            fb.fillRect(c.x + c.w - 6 - d, ty + 4 + d * 2, 1, 2, sheen);
    }

    // The floor: chequered tiles, its far edge lit.
    sceneFloor(fb, g, /*seamPitch=*/0, kToneTileA, kToneTileA, kToneTileEdge);
    const Rgb565 tileB = sceneTone(kToneTileB);
    for (int y = g.floorY + 1, row = 0; y < kActiveH; y += kTile, ++row)
        for (int x = (row & 1) ? kTile : 0; x < kActiveW; x += 2 * kTile)
            fb.fillRect(x, y, kTile, kTile, tileB);
}

}  // namespace mal
