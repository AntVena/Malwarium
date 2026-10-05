#include "core/render/scenes/draws.h"

#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE MOOR AT NIGHT, FROM THE CAUSEWAY. Wet, flat, empty country with no proper network:
// a handful of farms on the skyline, each talking only to whichever neighbour is in
// range, and the old stone causeway the cable crosses on. No stars and no moon — a moor
// is overcast, and the mist is what the sky is made of here.
//
// THE THREADS ARE THE AREA. Peer to peer means the traffic hangs between the houses
// themselves, so the one figure worth authoring is the run of threads strung farm to
// farm, and the one motion is a worm going down it: a copy crosses each thread in turn,
// and every window it reaches lights and stays lit until the whole moor has it. Then
// the moor goes dark and it starts again, which is the headcount Morris never stopped
// taking.
//
// The keep is on the skyline too, small and on a hill of its own, because the causeway
// leads to it and the next area is standing under its gate.
constexpr uint8_t kToneGlow = 14;
constexpr uint8_t kToneMoor = 36;
constexpr uint8_t kToneHouse = 44;
constexpr uint8_t kToneWindowDark = 26;
constexpr uint8_t kToneWindowLit = 196;
constexpr uint8_t kToneThread = 72;
constexpr uint8_t kTonePacket = 210;
constexpr uint8_t kToneMist = 50;
constexpr uint8_t kToneBog = 24;
constexpr uint8_t kTonePool = 62;
constexpr uint8_t kToneReed = 58;
constexpr uint8_t kToneStone = 60;
constexpr uint8_t kToneJoint = 42;
constexpr uint8_t kToneLip = 120;
constexpr uint8_t kToneCable = 20;

// Where the overcast lift starts. Low and faint: there is no light source out here,
// only the sky being slightly less dark nearer the ground.
constexpr uint8_t kGlowUp = 40;

// The moor: barely a skyline at all. Low and long, two or three rows of rise, which is
// what tells a moor from the ridges and treelines either side of it on the ladder.
constexpr uint8_t kMoorline[] = {2, 3, 3, 4, 3, 2, 2, 3, 4, 4, 3, 2, 3, 3,
                                 2, 2, 3, 4, 3, 3, 2, 3, 4, 3, 2, 2, 3, 3};

// The keep on its hill, two pixels a column: the hill rising out of the moor, the
// crenellated tower on its crown, and the hill falling away again.
constexpr uint8_t kKeepHill[] = {3, 4, 5, 6, 7, 8, 9, 16, 18, 16, 18, 16,
                                 18, 9, 8, 7, 6, 5, 4, 3};
constexpr SceneSpan kKeepAt = {176, 40};
constexpr int kKeepWindowX = 192, kKeepWindowDrop = 13;   // one slit, always lit

// The farms, left to right in the order the worm reaches them. `x` is the house's
// left wall; every house is the same small gabled box, and the thread hangs off its
// ridge. The first is the seeder, so it is lit from the start of every count.
struct Farm { int x, h; };
constexpr Farm kFarms[] = {{18, 6}, {62, 8}, {104, 6}, {146, 7}};
constexpr int kFarmCount = static_cast<int>(sizeof(kFarms) / sizeof(kFarms[0]));
constexpr int kHouseW = 9;
constexpr int kStandRows = 2;   // how far up the moor a house's foot is set

// Each thread sags a little between ridges, deepest at the middle, by this many rows.
// A slack line is a line strung by hand, which is the whole of what peer to peer looks
// like from outside; a taut one would be infrastructure, and the moor has none.
constexpr int kThreadSag = 4;

// The worm's count: `kHopSteps` beats to cross a thread, then `kHoldBeats` with the
// whole moor lit before it goes dark and starts over.
constexpr int kHopSteps = 5;
constexpr int kHoldBeats = 6;

// The mist: long low strokes over the skyline and the bog, drifting. `y` is rows from
// the horizon, negative above it. Drawn over the farms on purpose — a moor that showed
// everything cleanly would not be a moor.
struct Mist { int x, y, w; };
constexpr Mist kMist[] = {{8, -2, 44}, {120, -3, 52}, {70, -1, 30}, {170, -1, 36},
                          {30, 3, 50}, {140, 6, 40}, {96, 8, 26}};
constexpr int kMistSpan = 16;

// The bog between the skyline and the causeway: still pools catching what light there
// is, and reed clumps standing out of the water. A reed clump is (x, rows down from the
// horizon to its foot, how tall its tallest blade is).
struct Pool { int x, y, w; };
constexpr Pool kPools[] = {{14, 5, 18}, {84, 7, 14}, {158, 4, 22}, {196, 8, 12}};
struct Reed { int x, y, h; };
constexpr Reed kReeds[] = {{4, 9, 7}, {50, 6, 4}, {72, 9, 5}, {124, 5, 3},
                           {186, 9, 6}, {214, 9, 8}};

// The causeway's stones: courses `kCourseH` rows deep, joints every `kStoneW` columns,
// each course offset by half a stone from the one above — which is the difference
// between laid stone and the Bayou's planking at a glance.
constexpr int kCourseH = 8;
constexpr int kStoneW = 24;

// The cable runs the length of the causeway just inside its far edge.
constexpr int kCableDrop = 3;

// Where a thread hangs at column x between two ridges, or -1 off its span.
int threadY(int x, int x0, int y0, int x1, int y1) {
    if (x < x0 || x > x1 || x1 <= x0) return -1;
    const int span = x1 - x0, t = x - x0;
    return y0 + (y1 - y0) * t / span + 4 * kThreadSag * t * (span - t) / (span * span);
}

}  // namespace

void drawNapstorrentMoorsScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // The overcast, and the moor and keep standing on the horizon under it.
    sceneGlow(fb, g, kGlowUp, kToneGlow);
    sceneSilhouette(fb, kMoorline,
                    static_cast<int>(sizeof(kMoorline) / sizeof(kMoorline[0])),
                    g.horizonY, kToneMoor);
    sceneSilhouette(fb, kKeepHill, static_cast<int>(sizeof(kKeepHill)), g.horizonY,
                    kToneMoor, kKeepAt);
    fb.fillRect(kKeepWindowX, g.horizonY - kKeepWindowDrop, 1, 2,
                sceneTone(kToneWindowLit));

    // Where the count has got to: which thread the worm is crossing and how far along
    // it, or past the last one and holding with every window lit.
    const int cycle = (kFarmCount - 1) * kHopSteps + kHoldBeats;
    const int phase = beat % cycle;
    const int thread = phase / kHopSteps;   // >= kFarmCount - 1 means holding
    const int reached = thread < kFarmCount - 1 ? thread : kFarmCount - 1;

    // The farms, each a gabled box with one window that lights when the worm arrives.
    const Rgb565 house = sceneTone(kToneHouse);
    int ridgeX[kFarmCount], ridgeY[kFarmCount];
    for (int i = 0; i < kFarmCount; ++i) {
        const Farm& f = kFarms[i];
        const int foot = g.horizonY - kStandRows;
        fb.fillRect(f.x, foot - f.h, kHouseW, f.h, house);
        for (int r = 1; r <= kHouseW / 2; ++r)   // the gable, stepped a row per column
            fb.fillRect(f.x + r, foot - f.h - r, kHouseW - 2 * r, 1, house);
        ridgeX[i] = f.x + kHouseW / 2;
        ridgeY[i] = foot - f.h - kHouseW / 2;
        fb.fillRect(f.x + 3, foot - f.h + 2, 2, 2,
                    sceneTone(i <= reached ? kToneWindowLit : kToneWindowDark));
    }

    // The threads, ridge to ridge, dotted: traffic, not a cable. Then the copy in
    // transit on the one it is crossing.
    const Rgb565 threadC = sceneTone(kToneThread);
    for (int i = 0; i + 1 < kFarmCount; ++i)
        for (int x = ridgeX[i] + 1; x < ridgeX[i + 1]; x += 2)
            fb.fillRect(x, threadY(x, ridgeX[i], ridgeY[i], ridgeX[i + 1], ridgeY[i + 1]),
                        1, 1, threadC);
    if (thread < kFarmCount - 1) {
        const int x0 = ridgeX[thread], x1 = ridgeX[thread + 1];
        const int px = x0 + (x1 - x0) * (phase % kHopSteps + 1) / (kHopSteps + 1);
        fb.fillRect(px, threadY(px, x0, ridgeY[thread], x1, ridgeY[thread + 1]) - 1, 2, 2,
                    sceneTone(kTonePacket));
    }

    // The bog: dark water, still pools, reeds.
    sceneMiddle(fb, g, kToneBog);
    const Rgb565 pool = sceneTone(kTonePool);
    for (const Pool& p : kPools) fb.fillRect(p.x, g.horizonY + p.y, p.w, 1, pool);
    const Rgb565 reed = sceneTone(kToneReed);
    for (const Reed& r : kReeds) {
        const int foot = g.horizonY + r.y;
        fb.fillRect(r.x, foot - r.h, 1, r.h, reed);
        fb.fillRect(r.x + 2, foot - r.h + 2, 1, r.h - 2, reed);
        fb.fillRect(r.x - 2, foot - r.h / 2, 1, r.h / 2, reed);
    }

    // The mist over all of it, drifting.
    const Rgb565 mist = sceneTone(kToneMist);
    for (int i = 0; i < static_cast<int>(sizeof(kMist) / sizeof(kMist[0])); ++i)
        fb.fillRect(kMist[i].x + sceneDrift(beat, i, kMistSpan), g.horizonY + kMist[i].y,
                    kMist[i].w, 1, mist);

    // The causeway: laid stone, its far lip lit, the cable along it.
    sceneFloor(fb, g, /*seamPitch=*/0, kToneStone, kToneJoint, kToneLip);
    const Rgb565 joint = sceneTone(kToneJoint);
    for (int c = 0, y = g.floorY + 1; y < kActiveH; ++c, y += kCourseH) {
        if (c > 0) fb.fillRect(0, y, kActiveW, 1, joint);
        const int h = y + kCourseH <= kActiveH ? kCourseH : kActiveH - y;
        for (int x = (c & 1) ? kStoneW / 2 : kStoneW; x < kActiveW; x += kStoneW)
            fb.fillRect(x, y + 1, 1, h - 1, joint);
    }
    fb.fillRect(0, g.floorY + kCableDrop, kActiveW, 1, sceneTone(kToneCable));
}

}  // namespace mal
