#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE CIPHER HUT. The prize for cracking the Decryptogram board, and the room the board
// is about: a wartime codebreakers' hut, timber walls and a plank floor, with a BOMBE
// along the back wall — the machine that cracked the day's key by spinning drums
// through every setting until one fitted. Its drums turn on the heartbeat, each at its
// own rate, which is the scene's one motion and the whole of what the board asks the
// player to do by hand.
//
// The drums sit high on the cabinet, in the band above any sprite's head, because they
// are the picture; the plugboard below them is texture, and a sprite standing in front
// of it costs the scene nothing.
constexpr Pal kBrass = Pal::FRAG_LO;   // the drum faces and the patch cables
constexpr Pal kLit = Pal::FRAG_HI;     // the mark on each drum
constexpr uint8_t kToneWall = 20;
constexpr uint8_t kToneBoard = 28;     // the seams between the wall's boards
constexpr uint8_t kToneCabinet = 40;
constexpr uint8_t kToneCabinetEdge = 66;
constexpr uint8_t kToneDrum = 96;
constexpr uint8_t kToneDrumRim = 54;
constexpr uint8_t kToneMark = 196;
constexpr uint8_t kTonePlug = 70;
constexpr uint8_t kToneCable = 110;
constexpr uint8_t kToneFloor = 46;
constexpr uint8_t kToneFloorSeam = 34;
constexpr uint8_t kToneFloorEdge = 92;

// The wall's boards, laid horizontally, one every `kBoardPitch` rows.
constexpr int kBoardPitch = 9;

// The cabinet: inset from the canvas edges, its top this far up the sky, standing on
// the floor.
constexpr int kCabinetX = 14, kCabinetW = kActiveW - 28;
constexpr uint8_t kCabinetTopUp = 206;

// The drums: three rows of nine, `kDrumPitch` apart, the first row `kDrumTop` rows
// under the cabinet's top. A drum is a disc with a darker rim, and a mark that walks
// round it — eight positions, each row turning at its own rate, the way a Bombe's
// fast, middle and slow drums did.
constexpr int kDrumCols = 9, kDrumRows = 3;
constexpr int kDrumR = 7;
constexpr int kDrumPitch = 20, kDrumRowPitch = 17;
constexpr int kDrumTop = 11;
constexpr int kRowRate[kDrumRows] = {1, 2, 5};   // beats per eighth of a turn
constexpr int kMarkR = 4;
constexpr int kRing[8][2] = {{0, -1}, {1, -1}, {1, 0}, {1, 1},
                             {0, 1},  {-1, 1}, {-1, 0}, {-1, -1}};

// The plugboard under the drums: a grid of sockets, and three cables patched across
// it, each sagging between two sockets.
constexpr int kPlugPitch = 8;
struct Patch { int c0, r0, c1, r1, sag; };
constexpr Patch kPatches[] = {{2, 0, 9, 1, 5}, {6, 2, 15, 0, 4}, {12, 1, 20, 2, 6}};

}  // namespace

void drawCipherHutScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // The hut: timber boards from the ceiling to the floor.
    fb.fillRect(0, 0, kActiveW, g.floorY, sceneTone(kToneWall));
    const Rgb565 seam = sceneTone(kToneBoard);
    for (int y = kBoardPitch; y < g.floorY; y += kBoardPitch)
        fb.fillRect(0, y, kActiveW, 1, seam);

    // The Bombe's cabinet, its edge lit.
    const int top = sceneSkyY(g, kCabinetTopUp);
    fb.fillRect(kCabinetX, top, kCabinetW, g.floorY - top, sceneTone(kToneCabinet));
    const Rgb565 edge = sceneTone(kToneCabinetEdge);
    fb.fillRect(kCabinetX, top, kCabinetW, 1, edge);
    fb.fillRect(kCabinetX, top, 1, g.floorY - top, edge);
    fb.fillRect(kCabinetX + kCabinetW - 1, top, 1, g.floorY - top, edge);

    // The drums, and the mark on each, turning.
    const int left = kCabinetX + (kCabinetW - (kDrumCols - 1) * kDrumPitch) / 2;
    for (int r = 0; r < kDrumRows; ++r) {
        const int cy = top + kDrumTop + kDrumR + r * kDrumRowPitch;
        for (int c = 0; c < kDrumCols; ++c) {
            const int cx = left + c * kDrumPitch;
            sceneDisc(fb, cx, cy, kDrumR, kToneDrumRim, kBrass);
            sceneDisc(fb, cx, cy, kDrumR - 1, kToneDrum, kBrass);
            const int pos = (beat / kRowRate[r] + c * 3) % 8;
            fb.fillRect(cx + kRing[pos][0] * kMarkR, cy + kRing[pos][1] * kMarkR, 1, 2,
                        sceneTint(kToneMark, kLit));
        }
    }

    // The plugboard, from under the last drum row to the floor, and its cables.
    const int boardTop = top + kDrumTop + kDrumRows * kDrumRowPitch + 6;
    const Rgb565 plug = sceneTone(kTonePlug);
    const int cols = (kCabinetW - 12) / kPlugPitch;
    auto sock = [&](int c, int r, int* x, int* y) {
        *x = kCabinetX + 6 + c * kPlugPitch;
        *y = boardTop + r * kPlugPitch;
    };
    for (int r = 0; boardTop + r * kPlugPitch < g.floorY - 2; ++r)
        for (int c = 0; c <= cols; ++c) {
            int x, y;
            sock(c, r, &x, &y);
            fb.fillRect(x, y, 2, 2, plug);
        }
    const Rgb565 cable = sceneTint(kToneCable, kBrass);
    for (const Patch& p : kPatches) {
        int x0, y0, x1, y1;
        sock(p.c0, p.r0, &x0, &y0);
        sock(p.c1, p.r1, &x1, &y1);
        if (y0 >= g.floorY - 2 || y1 >= g.floorY - 2) continue;   // a short wall: no room
        const int span = x1 - x0;
        for (int x = x0; x <= x1; ++x) {
            const int t = x - x0;
            const int y = y0 + (y1 - y0) * t / span + 4 * p.sag * t * (span - t) / (span * span);
            if (y < g.floorY) fb.fillRect(x, y, 1, 1, cable);
        }
    }

    // The floor: planks, their far edge lit.
    sceneFloor(fb, g, /*seamPitch=*/30, kToneFloor, kToneFloorSeam, kToneFloorEdge);
}

}  // namespace mal
