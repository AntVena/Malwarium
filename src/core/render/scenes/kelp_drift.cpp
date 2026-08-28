#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"

namespace mal {

namespace {

// UNDER THE SURFACE — where a swimmer is at home. Every other place in the set puts an
// empty field above the horizon and calls it sky; this one fills it, because the whole
// point of being underwater is that there is something between you and the light.
//
// The inversion that makes it read: the water gets DARKER downward rather than lighter
// toward the horizon. That is the opposite of every horizon scene here, and it is the
// single cue that says "you are looking up through this" before any kelp is drawn.
// The water never reaches `paper`. It is tempting to let the deep go to nothing, but
// the pet stands at the FLOOR — silhouetting it against the darkest band on the panel
// costs the creature its own read, which is the one thing a backdrop may never do.
constexpr uint8_t kToneSurface = 50;   // the lit water just under the top
constexpr uint8_t kToneDeep = 26;      // and what it fades to at the bed
constexpr uint8_t kToneShaft = 62;
constexpr uint8_t kToneKelp = 44;
constexpr uint8_t kToneKelpNear = 78;
constexpr uint8_t kToneBlade = 66;
constexpr uint8_t kToneBubble = 118;
constexpr uint8_t kToneBed = 56;
constexpr uint8_t kToneSilt = 44;
constexpr uint8_t kToneBedLip = 108;

// How many bands the column of water is drawn in. Four, the same count the glow uses,
// because at this panel's depth a smooth ramp bands anyway and a placed band is a
// decision rather than an artefact.
constexpr int kWaterBands = 4;

// The light shafts coming down from the surface. Wide, faint and slanted the same way,
// because sunlight through a swell is one direction and not a fan — and kept to the
// outer thirds, where nothing a screen draws needs the contrast.
struct Shaft { int x, w; };
constexpr Shaft kShafts[] = {{14, 9}, {36, 5}, {176, 7}, {200, 11}};
constexpr int kShaftLean = 3;   // columns the shaft slides per quarter of its fall

// The kelp: stands rooted in the bed and rising most of the way up, each a stipe that
// wanders as it goes and throws blades off alternate sides. A stand is one long bend
// rather than a silhouette table — kelp is the one thing here with no straight edge in
// it — and the blades are what stop a stipe reading as a pipe.
struct Stand { int x, h, w; uint8_t tone; int sway; int bladePitch; };
constexpr Stand kStands[] = {
    {18, 126, 5, kToneKelpNear, 7, 11}, {34, 100, 4, kToneKelp, 5, 13},
    {190, 118, 5, kToneKelpNear, 6, 12}, {206, 86, 4, kToneKelp, 4, 14},
    {52, 74, 3, kToneKelp, 4, 15}, {170, 66, 3, kToneKelp, 5, 16}};
constexpr int kBladeLen = 7;

// Bubbles, rising rather than drifting: the one motion in the set that goes UP, which
// is the second thing after the value inversion that says which way is the surface.
constexpr uint8_t kBubbles[][2] = {{26, 60}, {58, 24}, {180, 44}, {198, 12}, {90, 8}};
constexpr int kBubbleRise = 46;

}  // namespace

void drawKelpDriftScene(Framebuffer& fb, int beat, const SceneGround& g) {
    // The water column, lit at the top and lost at the bed. Filled rather than cleared
    // — there is no `paper` anywhere in this scene, which is what being submerged is.
    // The column runs to the FLOOR, not to the horizon: this is one body of water and
    // the bed is its bottom, so there is no middle band to leave between them. Every
    // horizon scene has one; a submerged one does not, and calling sceneMiddle here
    // would be drawing a band that isn't there.
    for (int i = 0; i < kWaterBands; ++i) {
        const int y0 = g.floorY * i / kWaterBands;
        const int y1 = g.floorY * (i + 1) / kWaterBands;
        const uint8_t t = static_cast<uint8_t>(
            kToneSurface + (kToneDeep - kToneSurface) * i / (kWaterBands - 1));
        fb.fillRect(0, y0, kActiveW, y1 - y0, sceneTone(t));
    }

    // The shafts, leaning as they fall.
    const Rgb565 shaft = sceneTone(kToneShaft);
    for (const Shaft& s : kShafts)
        for (int y = 0; y < g.horizonY; ++y)
            fb.fillRect(s.x + y * kShaftLean / (g.horizonY / 4 + 1), y, s.w, 1, shaft);

    // The kelp, swaying on the heartbeat. Each column steps a pixel sideways every few
    // rows, and the phase carries the whole stand — so a stand bends rather than
    // wobbling, which is the difference between weed and static.
    for (int i = 0; i < static_cast<int>(sizeof(kStands) / sizeof(kStands[0])); ++i) {
        const Stand& st = kStands[i];
        const Rgb565 c = sceneTone(st.tone);
        const Rgb565 blade = sceneTone(kToneBlade);
        const int phase = (beat + i) % 3 - 1;
        for (int k = 0; k < st.h; ++k) {
            const int y = g.floorY - k;
            // Further from the root bends further, which is the whole of how a rooted
            // thing moves: the lean is proportional to how much stipe is under it.
            const int lean = st.sway * k / st.h * phase;
            fb.fillRect(st.x + lean, y, st.w, 1, c);
            // A blade every so often, alternating sides and trailing the way the stand
            // is leaning — so the whole plant agrees about which way the water is going.
            if (k % st.bladePitch) continue;
            const int side = (k / st.bladePitch) & 1 ? 1 : -1;
            const int len = kBladeLen - (k % 3);
            for (int b = 1; b <= len; ++b)
                fb.fillRect(st.x + lean + (side < 0 ? -b : st.w - 1 + b),
                            y + b * b / len, 1, 1, blade);
        }
    }

    // The bubbles, wrapping when they reach the top.
    const Rgb565 bubble = sceneTone(kToneBubble);
    for (int i = 0; i < static_cast<int>(sizeof(kBubbles) / sizeof(kBubbles[0])); ++i) {
        const int rise = (beat * 3 + i * 9) % kBubbleRise;
        fb.fillRect(kBubbles[i][0], sceneSkyY(g, kBubbles[i][1]) - rise, 2, 2, bubble);
    }

    // The bed. Seamless — silt has no seams — so the pitch is off and the texture is
    // the lit lip alone.
    sceneFloor(fb, g, /*seamPitch=*/0, kToneBed, kToneSilt, kToneBedLip);
}

}  // namespace mal
