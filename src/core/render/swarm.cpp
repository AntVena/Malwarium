#include "core/render/swarm.h"

#include "core/render/framebuffer.h"

namespace mal {
namespace {

// The heading is in 1/16 px per step (kFlockShift, core/model/flock.h). This file works
// in whole px, so a streak is walked in sixteenths and rounded down as it goes — which is
// what lets a mark drifting a third of a pixel a step still trail in the right direction.
constexpr int kVelShift = 4;

// How long a streak may run. Three px on a panel where a creature is sixty across: enough
// to say which way a mark is going, short enough that a fast flock is still made of marks
// rather than of lines.
constexpr int kStreakMax = 3;

// The alpha a mark at the body's CENTRE is drawn at, and the one out at the fringe.
// Nothing is ever drawn opaque: a swarm sits over a backdrop and reads as something
// hanging in front of it, and a solid mark reads as a hole punched in the screen.
//
// The fringe is not far below the core, and deliberately: two dozen marks is not many,
// and a fringe faint enough to be tasteful is a fringe nobody sees — which leaves a
// dozen bright dots and no creature around them.
constexpr uint8_t kCoreAlpha = 240;
constexpr uint8_t kFringeAlpha = 132;

// The wash around a body mark, as a fraction of that mark's own alpha. Two core marks a
// few px apart have overlapping haloes, so a node reads as having size rather than as a
// lit pixel. The fringe gets none — the edge of the creature has to stay granular or the
// whole silhouette softens into a cloud with no marks in it.
constexpr int kHaloNum = 1, kHaloDen = 3;

// THE MESH, and the thing that makes this a creature rather than a constellation.
//
// Two dozen marks cannot fill a cell this size by themselves: spaced far enough apart to
// occupy it, they read as stars. Drawing a thread between marks that are near each other
// gives the swarm connective tissue — so it has an interior, its shape is legible at a
// glance, and it comes apart and re-forms as the flock moves. It is also the truest
// picture of what the thing is: a guardian is what has been watching a NETWORK, and the
// mesh is the only part of this drawing that says so.
//
// kLinkR is how near two marks must be, in px. kLinkPerMark caps how many threads one
// mark may open — a tight flock has every pair in range, and the cap is what keeps the
// draw's cost flat between a knot and a cloud instead of quadrupling exactly when the
// creature is densest.
constexpr int kLinkR = 15;
constexpr int kLinkPerMark = 4;
constexpr uint8_t kLinkAlpha = 118;

int absv(int v) { return v < 0 ? -v : v; }
int chebyshev(int dx, int dy) { const int a = absv(dx), b = absv(dy); return a > b ? a : b; }

uint8_t lerpAlpha(uint8_t from, uint8_t to, int num, int den) {
    if (den <= 0) return from;
    if (num >= den) return to;
    if (num <= 0) return from;
    return static_cast<uint8_t>(from + (to - from) * num / den);
}

// A blended segment, Bresenham. Local to this effect rather than a Framebuffer method on
// purpose: nothing else on the device draws a diagonal — every other screen is built out
// of rects, text and sprites — so a general primitive here would be a shared surface with
// exactly one caller.
void blendLine(Framebuffer& fb, int x0, int y0, int x1, int y1, Rgb565 c, uint8_t a) {
    int dx = absv(x1 - x0), dy = -absv(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        fb.blendPixel(x0, y0, c, a);
        if (x0 == x1 && y0 == y1) return;
        const int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

}  // namespace

void drawSwarm(Framebuffer& fb, const SwarmView& v, int x, int y, int w, int h,
               Rgb565 core, Rgb565 fringe) {
    if (v.n <= 0 || w <= 0 || h <= 0) return;

    // The cell is a hard edge. The model bounces its boids off its own walls, but a
    // streak reaches PAST a mark, so the clip is what actually guarantees a swarm cannot
    // write into the line of text under it.
    fb.setClip(x, y, w, h);

    // The radius the body is measured against. Twice the mean spread, so roughly the
    // inner half of the flock draws as core and the rest thins out — and floored, because
    // a swarm that has collapsed to a point would otherwise divide by nothing and draw
    // every mark as fringe, which is the opposite of what it is doing.
    const int bodyR = (v.spread > 1 ? v.spread : 1) * 2;
    const int n = v.n < kSwarmMarksMax ? v.n : kSwarmMarksMax;

    // The mesh first, so the marks sit ON it rather than under it — a node with a thread
    // drawn over it stops reading as a node.
    for (int i = 0; i < n; ++i) {
        int opened = 0;
        for (int j = i + 1; j < n && opened < kLinkPerMark; ++j) {
            const int dx = v.marks[j].x - v.marks[i].x;
            const int dy = v.marks[j].y - v.marks[i].y;
            const int d = chebyshev(dx, dy);
            if (d > kLinkR) continue;
            ++opened;
            // A thread fades with length, so the mesh thickens where the flock is dense
            // and thins to nothing at the reach — which is what stops the cap above from
            // reading as an arbitrary edge somewhere in the middle of the creature.
            const uint8_t la = static_cast<uint8_t>(kLinkAlpha * (kLinkR - d) / kLinkR);
            // Core colour only where BOTH ends are inside the body: a thread out to the
            // fringe belongs to the fringe, or the creature grows a bright halo of spokes.
            const bool bothIn = chebyshev(v.marks[i].x - v.cx, v.marks[i].y - v.cy) * 2 < bodyR &&
                                chebyshev(v.marks[j].x - v.cx, v.marks[j].y - v.cy) * 2 < bodyR;
            blendLine(fb, x + v.marks[i].x, y + v.marks[i].y,
                      x + v.marks[j].x, y + v.marks[j].y, bothIn ? core : fringe, la);
        }
    }

    for (int i = 0; i < n; ++i) {
        const SwarmMark& m = v.marks[i];
        const int px = x + m.x;
        const int py = y + m.y;

        // How far out of the body this mark is, 0 at the centre and bodyR at the edge.
        // Everything about how it draws comes off this one number, which is what keeps
        // the core and the fringe reading as one creature and not as two effects.
        const int out = chebyshev(m.x - v.cx, m.y - v.cy);
        const bool inBody = out * 2 < bodyR;
        const uint8_t a = lerpAlpha(kCoreAlpha, kFringeAlpha, out, bodyR);
        const Rgb565 c = inBody ? core : fringe;

        // The mark. A 2x2 block in the body and a single pixel outside it — the size IS
        // the density channel, so the creature still has a middle in a grayscale shot.
        fb.blendPixel(px, py, c, a);
        if (inBody) {
            fb.blendPixel(px + 1, py, c, a);
            fb.blendPixel(px, py + 1, c, a);
            fb.blendPixel(px + 1, py + 1, c, a);
            // ...and the wash that lets it join the marks beside it (see kHaloNum).
            const uint8_t ha = static_cast<uint8_t>(a * kHaloNum / kHaloDen);
            fb.blendPixel(px - 1, py, c, ha);
            fb.blendPixel(px + 2, py, c, ha);
            fb.blendPixel(px - 1, py + 1, c, ha);
            fb.blendPixel(px + 2, py + 1, c, ha);
            fb.blendPixel(px, py - 1, c, ha);
            fb.blendPixel(px + 1, py - 1, c, ha);
            fb.blendPixel(px, py + 2, c, ha);
            fb.blendPixel(px + 1, py + 2, c, ha);
        }

        // The streak, walked BACKWARD along the heading in sixteenths. Its length is the
        // mark's own speed, so a flock holding station has none and an agitated one is
        // all streak — the mood is legible from the strokes before it is legible from
        // anything else.
        const int speed = chebyshev(m.vx, m.vy);
        int tail = speed >> kVelShift;
        if (tail > kStreakMax) tail = kStreakMax;
        for (int s = 1; s <= tail; ++s) {
            const int tx = px - (m.vx * s) / (1 << kVelShift);
            const int ty = py - (m.vy * s) / (1 << kVelShift);
            // Each step of the tail is fainter than the last, and the whole tail is
            // fainter than its mark: a streak is where the thing WAS, and drawing it as
            // brightly as where it is turns the swarm into a tangle of threads.
            fb.blendPixel(tx, ty, c, static_cast<uint8_t>(a / (s + 1)));
        }
    }

    fb.clearClip();
}

}  // namespace mal
