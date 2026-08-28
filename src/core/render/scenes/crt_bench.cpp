#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE CRT BENCH — the workshop the rig is actually built on, and the prize the Hacker
// SHOP's upgrade ladder pays out. A pegboard of tools behind, a rank of tube monitors
// along the back of the bench, and the bench top itself underfoot.
//
// THE PHOSPHOR IS THE ONE TINT. A CRT's light is not a value, it is a colour thrown
// into a dark room, so the tubes are anchored on FRAG_LO — the violet the palette
// already carries that no interface state has claimed — and everything the room is
// MADE of stays on the plain ramp. Two tubes carry a live trace; the rest hold a
// resting scanline, so the bench reads as running rather than as switched on.
//
// Like the kitchen, it is an interior: there is no distance, so the band above the
// horizon is the pegboard wall and the horizon is where the bench meets it.
constexpr uint8_t kToneWall = 24;        // the pegboard
constexpr uint8_t kTonePeg = 88;         // its hole pattern — a single pixel, so it can carry
constexpr uint8_t kToneTool = 80;        // what is hung on it
constexpr uint8_t kToneShelf = 58;       // the parts shelf across the top
constexpr uint8_t kToneBin = 78;         // component bins on it
constexpr uint8_t kToneCase = 70;        // a monitor's shell
constexpr uint8_t kToneBezel = 88;       // its lit front edge
constexpr uint8_t kToneTube = 30;        // the glass, dark
constexpr uint8_t kToneScan = 124;       // a resting scanline on it
constexpr uint8_t kToneTrace = 176;      // ...and a live trace on the two that are working
constexpr uint8_t kToneFront = 44;       // the bench's front edge, under the tubes
constexpr uint8_t kToneBench = 78;       // the bench top
constexpr uint8_t kToneSeam = 46;
constexpr uint8_t kToneLip = 124;        // its near edge
constexpr uint8_t kToneIron = 176;       // the soldering iron's tip, the room's one hot point

// The pegboard's holes. A pitch this coarse is what reads as pegboard rather than as
// noise at 224 columns.
constexpr int kPegPitch = 10;

// What is hung on it, as columns of depth hanging DOWN from the shelf: a rack of
// screwdrivers, then a pair of pliers, then a coil of wire, then a spanner or two.
// Authored as a silhouette table because that is what a wall of hanging tools is — a
// ragged strip, the same shape a treeline is, read upside down (sceneOverhang).
//
// The GAPS carry it. A run of similar depths merges into one bar at this size, so no
// tool is next to another of its own length and every group is fenced off by empty
// columns. Read the table: four thin ones, a stubby pair, nothing, a long thin one,
// nothing, a short block, nothing, two more.
constexpr uint8_t kTools[] = {0, 26, 0, 20, 0, 30, 0, 24, 0, 0, 12, 14, 0, 0,
                              0, 0, 34, 0, 0, 0, 16, 16, 0, 0, 0, 22, 0, 28,
                              0, 0, 18, 0};
constexpr int kShelfY = 22;              // rows down from the top of the canvas
constexpr int kShelfH = 4;

// The bins on the shelf: a run of small blocks, which at this size is the whole
// vocabulary of "somewhere the resistors live".
constexpr int kBinPitch = 18, kBinW = 13, kBinH = 8;

// The monitors, standing on the bench line. Each is a case with a tube inset in it;
// widths differ because a bench is what somebody accumulated rather than what somebody
// bought, and the tall one is deliberately off-centre so the middle of the panel — where
// a screen stands its sprite — is the gap between two of them.
struct Tube { uint8_t x, w, h; bool live; };
constexpr Tube kTubes[] = {
    {6, 52, 42, false}, {62, 40, 34, true}, {150, 46, 38, true}, {200, 22, 26, false},
};
constexpr int kInset = 5;                // how far the glass sits inside the case

// The live trace: a sawtooth walked across the glass, one column at a time, travelling
// with the beat. It is the only motion in the scene and it is confined to two rectangles
// of a few hundred pixels each.
constexpr int kTracePeriod = 16;

// The iron, resting in its stand at the near edge of the bench, in the gap the tubes
// leave between the third and the fourth. A stand at this size is two uprights and a
// cradle; what makes it an IRON is the one hot pixel pair on the end of the barrel, and
// it is the brightest thing in the scene precisely because it is four pixels.
constexpr int kIronX = 136;
constexpr int kIronH = 14;

}  // namespace

void drawCrtBenchScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // The wall and its holes, down to the bench.
    fb.fillRect(0, 0, kActiveW, g.horizonY, sceneTone(kToneWall));
    const Rgb565 peg = sceneTone(kTonePeg);
    for (int y = kShelfY + kShelfH + 6; y < g.horizonY; y += kPegPitch)
        for (int x = kPegPitch / 2; x < kActiveW; x += kPegPitch)
            fb.fillRect(x, y, 1, 1, peg);

    // The shelf, the bins standing on it, and the tools hanging under it.
    fb.fillRect(0, kShelfY, kActiveW, kShelfH, sceneTone(kToneShelf));
    const Rgb565 bin = sceneTone(kToneBin);
    for (int x = 4; x + kBinW < kActiveW; x += kBinPitch)
        fb.fillRect(x, kShelfY - kBinH, kBinW, kBinH, bin);
    sceneOverhang(fb, kTools, static_cast<int>(sizeof(kTools) / sizeof(kTools[0])),
                  kShelfY + kShelfH, kToneTool);

    // The monitors. Case, then glass inset in it, then whatever the glass is showing —
    // in that order, so a tube is a hole in a shell rather than a rectangle beside one.
    const Rgb565 shell = sceneTone(kToneCase);
    const Rgb565 bezel = sceneTone(kToneBezel);
    const Rgb565 glass = sceneTone(kToneTube);
    const Rgb565 scan = sceneTint(kToneScan, Pal::FRAG_LO);
    const Rgb565 trace = sceneTint(kToneTrace, Pal::FRAG_LO);
    for (const Tube& t : kTubes) {
        const int top = g.horizonY - t.h;
        fb.fillRect(t.x, top, t.w, t.h, shell);
        fb.fillRect(t.x, top, t.w, 1, bezel);
        const int gx = t.x + kInset, gy = top + kInset;
        const int gw = t.w - 2 * kInset, gh = t.h - 2 * kInset - 2;
        if (gw <= 0 || gh <= 0) continue;
        fb.fillRect(gx, gy, gw, gh, glass);
        if (!t.live) {
            // Idle: one resting line across the middle of the glass, which is what a
            // tube with nothing driving it looks like.
            fb.fillRect(gx, gy + gh / 2, gw, 1, scan);
            continue;
        }
        // Live: a sawtooth. `sceneDrift` is what every other scene's motion rides, so
        // the sweep travels on the same heartbeat the pet wanders on.
        const int phase = sceneDrift(beat, static_cast<int>(t.x), kTracePeriod);
        for (int i = 0; i < gw; ++i) {
            const int k = (i + phase) % kTracePeriod;
            const int dy = (k < kTracePeriod / 2 ? k : kTracePeriod - k) - kTracePeriod / 4;
            const int y = gy + gh / 2 + dy * gh / (2 * kTracePeriod);
            fb.fillRect(gx + i, y, 1, 1, trace);
        }
    }

    // The bench top. No middle band worth the name — a bench meets its wall — so the
    // horizon and the floor are close enough that this is one surface with a lit lip.
    // The front takes its own tone rather than the shells': a monitor has to stand out
    // from the wall behind it AND from the bench under it, and one number cannot do both.
    sceneMiddle(fb, g, kToneFront);
    sceneFloor(fb, g, /*seamPitch=*/52, kToneBench, kToneSeam, kToneLip);

    // The iron in its stand. Two uprights, the cradle between them, the barrel lying in
    // it, and the tip — the whole of the scene's warmth, and four pixels of it.
    const Rgb565 stand = sceneTone(kToneBezel);
    scenePost(fb, kIronX, kIronH, g, kToneBezel);
    scenePost(fb, kIronX + 12, kIronH, g, kToneBezel);
    fb.fillRect(kIronX, g.floorY - kIronH, 14, 2, stand);
    fb.fillRect(kIronX - 6, g.floorY - kIronH - 4, 22, 3, sceneTone(kToneCase));
    fb.fillRect(kIronX - 8, g.floorY - kIronH - 4, 2, 3, sceneTone(kToneIron));
}

}  // namespace mal
