#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE YARD BEHIND THE BUILDING — where a walker is at home, and where an egg sits until
// whatever is in it can stand. It is the fallback the whole chain rests on, so it is
// deliberately the PLAINEST place in the set: a fence, a low plant on the horizon, a
// dish on the roof, hardstanding underfoot. Nothing here competes for attention,
// because this is the backdrop most pets will spend most of their life in front of.
//
// It is also the one place that has to survive being drawn behind absolutely anything —
// the caption band, the hunger alert, an unlock banner, an incubation readout — so its
// whole range sits low and the only bright pixels are three lit windows.
constexpr uint8_t kToneSky = 14;
constexpr uint8_t kToneBuilding = 44;
constexpr uint8_t kToneWindow = 168;
constexpr uint8_t kToneDish = 62;
constexpr uint8_t kToneFence = 70;
constexpr uint8_t kToneMesh = 38;
constexpr uint8_t kToneApron = 30;
constexpr uint8_t kToneSlab = 66;
constexpr uint8_t kToneJoint = 44;
constexpr uint8_t kToneKerb = 122;
constexpr uint8_t kToneLamp = 190;

// The plant behind the fence: a long low block with a stepped end and a stack, which at
// this size is the entire vocabulary of "industrial building". Even-topped on purpose —
// the moors are ragged and the keep is built, and this is built.
constexpr uint8_t kPlant[] = {0, 0, 14, 14, 14, 14, 14, 14, 20, 20, 14, 14,
                              14, 14, 14, 14, 14, 14, 0, 0, 26, 26, 0, 0};

// Three lit windows along the plant's face. Two wide, so they read as windows and not
// as the specks another scene's sky is made of.
struct Window { int x, drop; };
constexpr Window kWindows[] = {{74, 6}, {96, 6}, {150, 6}};
constexpr int kWindowW = 2, kWindowH = 3;

// The dish on the stack: a quarter-disc tipped up, drawn as a run of steps, which is
// what a curve is at this size. On the right, where the plant's stack already is.
constexpr int kDishX = 200, kDishR = 9;

// The chain-link, standing on the horizon in front of everything. Posts every few
// columns and a mesh between them, dim enough that the plant reads THROUGH it — which
// is the only thing that makes it a fence rather than a wall.
constexpr int kFenceH = 22, kPostPitch = 32, kMeshPitch = 4;

// One lamp on a post, off to one side. The scene's single accent and its only motion:
// it buzzes, which is one beat in four with it dark.
constexpr int kLampPost = 1;   // which fence post carries it
constexpr int kLampPeriod = 4;

}  // namespace

void drawServerYardScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));
    sceneGlow(fb, g, /*up=*/56, kToneSky);

    sceneSilhouette(fb, kPlant, static_cast<int>(sizeof(kPlant) / sizeof(kPlant[0])),
                    g.horizonY, kToneBuilding);
    const Rgb565 lit = sceneTone(kToneWindow);
    for (const Window& w : kWindows)
        fb.fillRect(w.x, g.horizonY - w.drop - kWindowH, kWindowW, kWindowH, lit);

    // The dish, as a stack of rows whose width follows the curve — tipped up and open
    // to the left, which is the one asymmetry in an otherwise square scene.
    const Rgb565 dish = sceneTone(kToneDish);
    for (int k = 0; k < kDishR; ++k) {
        const int w = kDishR - k * k / kDishR;
        fb.fillRect(kDishX - w, g.horizonY - 26 - k, w, 1, dish);
    }
    fb.fillRect(kDishX, g.horizonY - 26 - kDishR, 2, kDishR, dish);

    sceneMiddle(fb, g, kToneApron);

    // The fence: posts, then the mesh strung between them one column at a time.
    const Rgb565 post = sceneTone(kToneFence);
    const Rgb565 mesh = sceneTone(kToneMesh);
    const int fenceTop = g.horizonY - kFenceH;
    for (int x = kMeshPitch / 2; x < kActiveW; x += kMeshPitch)
        fb.fillRect(x, fenceTop, 1, kFenceH, mesh);
    fb.fillRect(0, fenceTop, kActiveW, 1, post);
    int posts = 0;
    for (int x = 6; x < kActiveW; x += kPostPitch, ++posts) {
        fb.fillRect(x, fenceTop - 4, 2, kFenceH + 4, post);
        if (posts == kLampPost)
            fb.fillRect(x - 1, fenceTop - 7, 4, 3,
                        sceneTone(beat % kLampPeriod ? kToneLamp : kToneFence));
    }

    sceneFloor(fb, g, /*seamPitch=*/46, kToneSlab, kToneJoint, kToneKerb);
}

}  // namespace mal
