#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE SILK LODE — a crypt somebody built down here, and the silk that has had it since.
// The arcade is masonry: bays, piers, courses, an altar off to one side. Everything
// hanging off it is web, and the webs are what the place is now for.
constexpr uint8_t kToneWall = 38;
constexpr uint8_t kToneWallDeep = 16;
constexpr uint8_t kToneCourse = 54;
constexpr uint8_t kToneJoint = 22;
constexpr uint8_t kToneReveal = 30;
constexpr uint8_t kToneVoid = 7;        // inside a bay: the deepest thing here
constexpr uint8_t kToneArris = 84;      // the lit stone edge around one
constexpr uint8_t kToneWeb = 62;
constexpr uint8_t kToneDrape = 48;
constexpr uint8_t kToneCocoon = 92;
constexpr uint8_t kToneWrap = 44;
constexpr uint8_t kToneAltar = 62;
constexpr uint8_t kToneAltarLip = 118;
constexpr uint8_t kToneSlab = 22;
constexpr uint8_t kToneVotive = 186;    // the one lit thing, and the reason to look left
constexpr uint8_t kToneFlag = 44;
constexpr uint8_t kToneSeam = 26;
constexpr uint8_t kToneLip = 112;
constexpr uint8_t kToneDust = 78;

// The arcade, as bay centres and half-widths. Four openings around a wide middle pier,
// so a screen's own subject stands in front of masonry rather than in front of a hole.
struct Bay {
    uint8_t cx, halfW;
};
constexpr Bay kBays[] = {{26, 13}, {74, 14}, {150, 14}, {198, 13}};

// Where the arcade springs and where it points. Both are sky fractions, so the arcade
// keeps its proportions under a fighter's floor and a resting pet's alike.
constexpr uint8_t kSpringUp = 88;
constexpr uint8_t kApexUp = 126;

constexpr int kCoursePitch = 13;

// Web anchored in a corner: rays into the angle and two chords across them. This is the
// drape in the frame's own corners; a bay's web is strung from its apex instead.
constexpr int8_t kWebDirs[][2] = {{0, 8}, {6, 6}, {8, 0}, {7, -4}};
constexpr int kWebDirScale = 8;

// A bay's web: how far below the apex each chord sags, as a share of the arch head.
constexpr int kWebChords[] = {3, 6};
constexpr int kWebChordScale = 10;
constexpr int kWebSag = 4;

struct Cocoon {
    uint8_t x, dropUp, len;
};
constexpr Cocoon kCocoons[] = {{40, 120, 26}, {88, 96, 34}, {134, 132, 22},
                               {176, 88, 30}, {214, 116, 18}};
constexpr int kCocoonSway = 4;

constexpr uint8_t kDust[][2] = {{26, 62},  {51, 128}, {63, 34},  {87, 166}, {105, 56},
                                {119, 200}, {137, 108}, {153, 174}, {175, 78},
                                {192, 134}, {205, 38}, {219, 152}};

// The altar, pushed well left of the middle columns for the same reason the keep's gate
// is: it is the one figure here with any detail in it.
constexpr int kAltarX = 12, kAltarW = 52;
constexpr int kAltarStepH = 5, kAltarSteps = 3, kAltarInset = 7;
constexpr int kSlabH = 13;
constexpr int kVotiveX = 30, kVotiveW = 3, kVotiveH = 5;

// Strung from the apex down across the head: chords sagging between the two arch lines
// and spokes from the point down to them. Drawn against the arch's own geometry rather
// than as a table, so a bay of any width is webbed the same way.
void archWeb(Framebuffer& fb, const Bay& b, int springY, int apexY, uint8_t tone) {
    const Rgb565 c = sceneTone(tone);
    const int rise = springY - apexY;
    for (int ch : kWebChords) {
        const int drop = rise * ch / kWebChordScale;
        const int halfSpan = b.halfW * ch / kWebChordScale;
        for (int dx = -halfSpan; dx <= halfSpan; ++dx) {
            const int t = halfSpan ? (halfSpan - (dx < 0 ? -dx : dx)) : 0;
            const int sagY = halfSpan ? kWebSag * t * t / (halfSpan * halfSpan) : 0;
            fb.fillRect(b.cx + dx, apexY + drop + sagY, 1, 1, c);
        }
        for (int k = 0; k <= drop; ++k) {
            fb.fillRect(b.cx, apexY + k, 1, 1, c);
            fb.fillRect(b.cx - halfSpan * k / (drop ? drop : 1), apexY + k, 1, 1, c);
            fb.fillRect(b.cx + halfSpan * k / (drop ? drop : 1), apexY + k, 1, 1, c);
        }
    }
}

int archTop(const Bay& b, int x, int springY, int apexY) {
    const int dx = x < b.cx ? b.cx - x : x - b.cx;
    return springY - (springY - apexY) * (b.halfW - dx) / b.halfW;
}

void webCorner(Framebuffer& fb, int ax, int ay, int sx, int r0, int r1, uint8_t tone) {
    const Rgb565 c = sceneTone(tone);
    const int n = static_cast<int>(sizeof(kWebDirs) / sizeof(kWebDirs[0]));
    int prev0x = 0, prev0y = 0, prev1x = 0, prev1y = 0;
    for (int i = 0; i < n; ++i) {
        const int x0 = ax + sx * kWebDirs[i][0] * r0 / kWebDirScale;
        const int y0 = ay + kWebDirs[i][1] * r0 / kWebDirScale;
        const int x1 = ax + sx * kWebDirs[i][0] * r1 / kWebDirScale;
        const int y1 = ay + kWebDirs[i][1] * r1 / kWebDirScale;
        for (int k = 0; k <= r1; ++k)
            fb.fillRect(ax + (x1 - ax) * k / r1, ay + (y1 - ay) * k / r1, 1, 1, c);
        if (i) {
            const int steps = r1;
            for (int k = 0; k <= steps; ++k) {
                fb.fillRect(prev0x + (x0 - prev0x) * k / steps,
                            prev0y + (y0 - prev0y) * k / steps, 1, 1, c);
                fb.fillRect(prev1x + (x1 - prev1x) * k / steps,
                            prev1y + (y1 - prev1y) * k / steps, 1, 1, c);
            }
        }
        prev0x = x0; prev0y = y0; prev1x = x1; prev1y = y1;
    }
}

}  // namespace

void drawSilkLodeScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));
    for (int y = 0; y < g.floorY; ++y)
        fb.fillRect(0, y, kActiveW, 1,
                    sceneTone(kToneWallDeep +
                              (kToneWall - kToneWallDeep) * y / g.floorY));

    const int springY = sceneSkyY(g, kSpringUp);
    const int apexY = sceneSkyY(g, kApexUp);

    // Masonry: courses across the face and a joint every other block, offset course to
    // course. Without the joints the courses read as shelving rather than as stone.
    const Rgb565 course = sceneTone(kToneCourse);
    const Rgb565 joint = sceneTone(kToneJoint);
    int ci = 0;
    for (int y = g.floorY - kCoursePitch; y > 0; y -= kCoursePitch) {
        fb.fillRect(0, y, kActiveW, 1, course);
        for (int x = (ci++ % 2) * 14; x < kActiveW; x += 28)
            fb.fillRect(x, y, 1, kCoursePitch, joint);
    }

    const Rgb565 voidC = sceneTone(kToneVoid);
    const Rgb565 arris = sceneTone(kToneArris);
    for (const Bay& b : kBays) {
        for (int dx = -b.halfW; dx <= b.halfW; ++dx) {
            const int x = b.cx + dx;
            if (x < 0 || x >= kActiveW) continue;
            const int top = archTop(b, x, springY, apexY);
            fb.fillRect(x, top, 1, g.floorY - top, voidC);
            fb.fillRect(x, top, 1, 2, arris);
            // The reveal: the opening's own thickness, lit down one jamb only, which is
            // what makes a bay a way in rather than a shape painted on the wall.
            if (dx == -b.halfW + 2) fb.fillRect(x, top + 2, 2, g.floorY - top - 2,
                                                sceneTone(kToneReveal));
        }
        // The jamb edges, so a bay frames as an opening instead of abutting the wall.
        for (int y = springY; y < g.floorY; ++y) {
            fb.fillRect(b.cx - b.halfW, y, 2, 1, arris);
            fb.fillRect(b.cx + b.halfW - 1, y, 2, 1, arris);
        }
        archWeb(fb, b, springY, apexY, kToneWeb);
    }

    // The frame's own top corners, draped: the same construction at a size that says the
    // web has had longer than the bays have.
    webCorner(fb, 0, 0, 1, 15, 30, kToneDrape);
    webCorner(fb, kActiveW - 1, 0, -1, 15, 30, kToneDrape);

    const Rgb565 thread = sceneTone(kToneWeb);
    const Rgb565 cocoon = sceneTone(kToneCocoon);
    int wi = 0;
    for (const Cocoon& c : kCocoons) {
        const int hangY = sceneSkyY(g, c.dropUp);
        const int sway = sceneDrift(beat, wi++, kCocoonSway);
        for (int y = 0; y < hangY; ++y)
            fb.fillRect(c.x + sway * y / (hangY ? hangY : 1), y, 1, 1, thread);
        const Rgb565 wrap = sceneTone(kToneWrap);
        for (int k = 0; k < c.len; ++k) {
            const int w = 3 + 5 * k * (c.len - k) * 4 / (c.len * c.len);
            fb.fillRect(c.x + sway - w / 2, hangY + k, w, 1, k % 5 == 2 ? wrap : cocoon);
        }
    }

    sceneSpecks(fb, kDust, static_cast<int>(sizeof(kDust) / sizeof(kDust[0])), g,
                kToneDust);

    // The altar: steps up to a slab, with one votive still lit in the bay behind it.
    fb.fillRect(kVotiveX, g.floorY - kAltarSteps * kAltarStepH - kSlabH - kVotiveH - 4,
                kVotiveW, kVotiveH, sceneTone(kToneVotive));
    const Rgb565 stone = sceneTone(kToneAltar);
    const Rgb565 lip = sceneTone(kToneAltarLip);
    for (int s = 0; s < kAltarSteps; ++s) {
        const int x = kAltarX + s * kAltarInset;
        const int w = kAltarW - 2 * s * kAltarInset;
        const int y = g.floorY - (s + 1) * kAltarStepH;
        fb.fillRect(x, y, w, kAltarStepH, stone);
        fb.fillRect(x, y, w, 1, lip);
    }
    const int slabX = kAltarX + kAltarSteps * kAltarInset;
    const int slabW = kAltarW - 2 * kAltarSteps * kAltarInset;
    const int slabY = g.floorY - kAltarSteps * kAltarStepH - kSlabH;
    fb.fillRect(slabX, slabY, slabW, kSlabH, sceneTone(kToneSlab));
    fb.fillRect(slabX, slabY, slabW, 1, lip);

    sceneFloor(fb, g, /*seamPitch=*/32, kToneFlag, kToneSeam, kToneLip);
}

}  // namespace mal
