#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// GROUND STATION — a ridge of dishes under a night sky, and the prize the radio's
// wardriving ladder pays out. The one place in the set that is genuinely OUTDOORS and
// genuinely EMPTY: no skyline, no corridor, no room. A mast, three dishes, a fence of
// guy wires, and a lot of sky.
//
// That emptiness is the point rather than a shortfall. Every other prize backdrop is
// somewhere crowded, and a screen that has a long caption band or a full unlock banner
// on it needs one place in the roster whose upper two-thirds is nearly nothing. So the
// silhouette is a PART-WIDTH one (SceneSpan) sitting to the right, the specks are
// sparse, and the middle of the panel is left as sky.
//
// It reads in pure value, like the yard: a station at night is lit by its own beacon
// and by nothing else.
constexpr uint8_t kToneSky = 16;         // the lift toward the horizon
constexpr uint8_t kToneStar = 92;
constexpr uint8_t kToneRidge = 34;       // the far ground behind the station
constexpr uint8_t kToneMast = 58;
constexpr uint8_t kToneGuy = 46;         // its guy wires — dim, so they read THROUGH
constexpr uint8_t kToneBeaconOn = 198;   // the one bright thing for miles
constexpr uint8_t kToneBeaconOff = 46;
constexpr uint8_t kToneDish = 62;
constexpr uint8_t kToneDishRim = 112;    // the lit inside of a dish's mouth
constexpr uint8_t kToneHut = 48;         // the equipment hut at the mast's foot
constexpr uint8_t kToneHutLamp = 150;
constexpr uint8_t kToneScrub = 24;       // the middle ground
constexpr uint8_t kToneGravel = 64;      // the hardstanding
constexpr uint8_t kToneSeam = 42;
constexpr uint8_t kToneKerb = 118;

// The sky. Sparse on purpose and weighted to the top, so the band a screen writes
// across stays as close to `paper` as a painted sky can be.
constexpr uint8_t kStars[][2] = {
    {14, 232}, {31, 198}, {52, 244}, {68, 176}, {87, 214},
    {103, 250}, {118, 188}, {141, 226}, {162, 206}, {177, 240},
    {193, 182}, {209, 220}, {24, 160}, {130, 166}, {216, 150},
};

// The ridge the station stands on: low, even, and only as far as the dishes reach. A
// part-width silhouette is the whole reason SceneSpan exists, and this is what it is
// for — the left half of the horizon is open ground.
constexpr uint8_t kRidge[] = {0, 0, 3, 5, 6, 6, 7, 8, 8, 9, 9, 8, 8, 7, 6, 5, 4, 3, 2, 0};
constexpr int kRidgeX = 96, kRidgeW = 128;

// The mast: a tall thin thing with a beacon on it, standing well off-centre. `kMastX`
// is deliberately in the right third — the middle of the panel is where a fighter
// stands, and a mast up the middle of it would be a pole growing out of a pet's head.
constexpr int kMastX = 176;
constexpr int kMastRise = 92;            // rows above the horizon, at the taller ground
constexpr int kBeaconPeriod = 6;         // slow — an aircraft warning light, not a strobe

// The dishes, as (x, radius, how far above the horizon their centre sits). Three, in
// descending size going left, which is the cheapest way to make a flat ridge read as
// receding.
struct Dish { uint8_t x, r, lift; };
constexpr Dish kDishes[] = {{132, 15, 14}, {104, 11, 10}, {208, 9, 9}};

// The hut at the mast's foot, and the one lit window on it.
constexpr int kHutX = 148, kHutW = 22, kHutH = 13;

}  // namespace

void drawGroundStationScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    sceneSpecks(fb, kStars, static_cast<int>(sizeof(kStars) / sizeof(kStars[0])), g,
                kToneStar);
    sceneGlow(fb, g, /*up=*/44, kToneSky);

    sceneSilhouette(fb, kRidge, static_cast<int>(sizeof(kRidge) / sizeof(kRidge[0])),
                    g.horizonY, kToneRidge, {kRidgeX, kRidgeW});

    // The guy wires first, so the mast stands in front of its own rigging: two lines
    // run down from near the top of the mast to the ground either side of it.
    const Rgb565 guy = sceneTone(kToneGuy);
    const int mastTop = g.horizonY - kMastRise;
    for (int side = -1; side <= 1; side += 2) {
        const int footX = kMastX + side * 44;
        for (int y = mastTop + 12; y < g.horizonY; ++y) {
            const int num = y - (mastTop + 12), den = g.horizonY - (mastTop + 12);
            fb.fillRect(kMastX + (footX - kMastX) * num / den, y, 1, 1, guy);
        }
    }

    // The dishes. TWO DISCS, not one: the body, and a second one offset up and left
    // drawn in the mouth's tone. What is left of the body is a crescent along the lower
    // right, and a crescent is the only thing that reads as "a bowl turned toward you"
    // at nine pixels across — a flat disc is the moon, and this scene already has a sky
    // for that to be mistaken in.
    const Rgb565 dish = sceneTone(kToneDish);
    for (const Dish& d : kDishes) {
        const int cy = g.horizonY - d.lift - d.r;
        sceneDisc(fb, d.x, cy, d.r, kToneDish);
        sceneDisc(fb, d.x - d.r / 5, cy - d.r / 5, d.r - 2, kToneDishRim);
        // ...and the feed at the focus of it, which is what the mouth is pointed at.
        fb.fillRect(d.x - d.r / 5, cy - d.r - 2, 2, 4, dish);
        fb.fillRect(d.x - 1, cy + d.r - 2, 2, d.lift + 2, dish);   // the pedestal
    }

    // The mast, and the beacon on top of it.
    fb.fillRect(kMastX, mastTop, 2, kMastRise, sceneTone(kToneMast));
    for (int y = mastTop + 8; y < g.horizonY; y += 12)               // the lattice
        fb.fillRect(kMastX - 2, y, 6, 1, sceneTone(kToneGuy));
    fb.fillRect(kMastX - 1, mastTop - 3, 4, 3,
                sceneTone(beat % kBeaconPeriod < 2 ? kToneBeaconOn : kToneBeaconOff));

    sceneMiddle(fb, g, kToneScrub);

    // The hut, standing on the middle ground in front of the ridge.
    fb.fillRect(kHutX, g.horizonY - kHutH, kHutW, kHutH, sceneTone(kToneHut));
    fb.fillRect(kHutX + 4, g.horizonY - kHutH + 4, 3, 3, sceneTone(kToneHutLamp));

    sceneFloor(fb, g, /*seamPitch=*/34, kToneGravel, kToneSeam, kToneKerb);
}

}  // namespace mal
