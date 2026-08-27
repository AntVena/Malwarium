#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE KEEP, FROM UNDER THE GATE — and the scene that inverts everything the others do.
// There is no distance here and no horizon to put it on: a wall fills the whole band,
// and the only sky is the strip left above its crenellation. That is the claustrophobia,
// and it is also the proof that the primitives are general rather than a set of calls
// that only make horizon pictures. sceneMiddle is deliberately never called — a flat
// fill between the horizon and the floor is exactly the band this place does not have.
//
// Nothing moves. A keep is the one place on the ladder that should be completely still,
// so the draw takes no beat at all.
constexpr uint8_t kToneWall = 46;
constexpr uint8_t kToneMerlon = 62;
constexpr uint8_t kToneCourse = 34;    // the mortar lines across the face
constexpr uint8_t kToneSlit = 20;      // an unlit arrow slit: darker than the wall
constexpr uint8_t kToneSlitLit = 190;
constexpr uint8_t kToneGate = 12;      // the opening, and the darkest thing here
constexpr uint8_t kTonePortcullis = 96;
constexpr uint8_t kToneFlag = 70;
constexpr uint8_t kToneSeam = 40;
constexpr uint8_t kToneLip = 132;

// How far up the sky the wall's head reaches. High enough that the strip above it is a
// strip rather than a second band, low enough that a screen's own copy still has an
// empty field to sit on.
constexpr uint8_t kWallTopUp = 200;

// The crenellation: sixteen columns, alternating merlon and gap. Even rather than
// ragged on purpose — every other silhouette on the ladder is broken up, and this one
// is built, which is the difference between a landscape and a fortification.
constexpr uint8_t kMerlonH = 9;
constexpr uint8_t kCrenellation[] = {9, 0, 9, 0, 9, 0, 9, 0, 9, 0, 9, 0, 9, 0, 9, 0};

// The mortar courses down the face, and how far apart they run. Wide, because a course
// every few rows on a wall this tall is texture the eye reads as noise.
constexpr int kCoursePitch = 15;

// The arrow slits. Two pixels wide is the whole of the figure — a slit is a slit
// because it is narrow, so the tone does the rest. `lit` is the two with something
// behind them, and they are the only bright pixels in the scene.
struct Slit { int x, drop; bool lit; };
constexpr Slit kSlits[] = {{88, 34, false}, {112, 52, true},  {136, 34, false},
                           {160, 52, false}, {184, 34, true}, {200, 52, false}};
constexpr int kSlitW = 2, kSlitH = 11;

// The gate, and the portcullis raised into its head. Pushed well left of centre, which
// is both where a keep's gate can honestly be and what keeps the one figure with any
// detail in it out of the columns a screen puts fighters and copy in.
constexpr int kGateX = 14, kGateW = 48;
constexpr int kGateH = 70;          // rows of opening above the floor
constexpr uint8_t kTeeth[] = {5, 9, 5, 9, 5, 9, 5, 9};
constexpr int kToothW = 3;

}  // namespace

void drawCastleRapidscareScene(Framebuffer& fb, int, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // The wall: one face from the foot of the crenellation all the way to the floor,
    // crossing the horizon without noticing it.
    const int headY = sceneSkyY(g, kWallTopUp);
    const int faceY = headY + kMerlonH;
    fb.fillRect(0, faceY, kActiveW, g.floorY - faceY, sceneTone(kToneWall));
    sceneSilhouette(fb, kCrenellation,
                    static_cast<int>(sizeof(kCrenellation) / sizeof(kCrenellation[0])),
                    faceY, kToneMerlon);

    const Rgb565 course = sceneTone(kToneCourse);
    for (int y = faceY + kCoursePitch; y < g.floorY; y += kCoursePitch)
        fb.fillRect(0, y, kActiveW, 1, course);

    for (const Slit& s : kSlits)
        fb.fillRect(s.x, faceY + s.drop, kSlitW, kSlitH,
                    sceneTone(s.lit ? kToneSlitLit : kToneSlit));

    // The gate, cut through everything above, and the portcullis hanging into its head.
    // The teeth are the one place this scene is allowed a ragged edge, and they point
    // the wrong way round from every silhouette beside them — which is what says the
    // thing overhead is holding itself up rather than standing on something.
    const int gateY = g.floorY - kGateH;
    fb.fillRect(kGateX, gateY, kGateW, kGateH, sceneTone(kToneGate));
    sceneOverhang(fb, kTeeth, static_cast<int>(sizeof(kTeeth) / sizeof(kTeeth[0])),
                  gateY, kTonePortcullis, {kGateX, kGateW});
    const Rgb565 bars = sceneTone(kTonePortcullis);
    for (int x = kGateX; x < kGateX + kGateW; x += 6)
        fb.fillRect(x, gateY, kToothW, 4, bars);

    // Flagstones: the widest seam pitch in the set, which is what a floor made of
    // metre-wide slabs looks like once the far edge is lit.
    sceneFloor(fb, g, /*seamPitch=*/38, kToneFlag, kToneSeam, kToneLip);
}

}  // namespace mal
