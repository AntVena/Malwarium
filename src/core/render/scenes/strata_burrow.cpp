#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"

namespace mal {

namespace {

// UNDERGROUND — where a Ground mover is at home. The worms, the grubs and the things
// that slink are all down here, and what is around them is not scenery, it is MATERIAL:
// packed layers with cable running through them and a tunnel already dug.
//
// Its identity is horizontal banding, which nothing else in the set uses as a subject.
// Every other place spends its detail on a silhouette against a field; this one has no
// field at all — the strata ARE the picture, and the tunnel is the one hole in them.
// The layers have to be far enough apart in value to read AS layers. A section drawn in
// four neighbouring greys is a grey slab with lines in it, which is the failure mode a
// gate cannot see.
constexpr uint8_t kToneTopsoil = 26;
constexpr uint8_t kToneStrataA = 58;
constexpr uint8_t kToneStrataB = 20;
constexpr uint8_t kToneGrit = 78;
constexpr uint8_t kToneCable = 88;
constexpr uint8_t kToneCableCore = 150;
constexpr uint8_t kToneTunnel = 10;    // the dug-out dark, and the darkest thing here
constexpr uint8_t kToneRim = 74;
constexpr uint8_t kToneRoot = 70;
constexpr uint8_t kToneBedFill = 56;
constexpr uint8_t kToneBedLip = 116;

// The layers, as a share of the ground above the floor and how each one is packed. A
// band is `t` deep out of 64, so the table reads as a section drawing: thin crust, a
// thick clay, a grit line, a deep bed. `grit` is whether that layer is speckled — the
// only texture here, and it is what tells a loose layer from a set one.
struct Layer { uint8_t depth; uint8_t tone; bool grit; };
constexpr Layer kLayers[] = {{6, kToneTopsoil, false}, {17, kToneStrataA, false},
                             {5, kToneGrit, true},     {20, kToneStrataB, false},
                             {8, kToneStrataA, true},  {8, kToneStrataB, false}};
constexpr int kLayerScale = 64;

// The buried cable, running along one bedding plane the whole width. A live core inside
// a sheath, which is why it is two tones — and the brightest thing down here, because
// it is the only thing that is not dirt.
constexpr uint8_t kCableUp = 128;
constexpr int kCableH = 5;
constexpr int kCableSagPitch = 56, kCableSag = 2;

// The tunnel, off-centre and running back into the wall: a mouth and the throat behind
// it, which is two discs and a run. It is where the pet came from, and putting it out
// of the middle columns is what keeps it out of the way of everything a screen draws.
constexpr int kTunnelX = 42;
constexpr uint8_t kTunnelUp = 52;
constexpr int kTunnelR = 21;

// How far a bedding plane wanders off level, walked per column and wrapped. Ground does
// not settle in rules — a section drawn with straight boundaries is a stacked bar chart,
// and the wobble is the single thing that turns it back into geology. Authored rather
// than rolled, like every other table here: this is drawn behind text that has to stay
// still between repaints.
constexpr int8_t kBedding[] = {0, 1, 1, 2, 2, 1, 0, 0, -1, -2, -2, -1, 0, 1,
                               2, 3, 2, 1, 0, -1, -1, -2, -1, 0, 1, 1, 0, -1};

// Roots working down through the upper layers — the only vertical thing in a scene made
// entirely of horizontals, which is what stops the section reading as a gradient. Each
// wanders as it descends and thins to nothing.
struct Root { int x, top, len, lean; };
constexpr Root kRoots[] = {{22, 4, 62, 1}, {88, 2, 44, -1}, {148, 6, 74, 1},
                           {198, 3, 52, -1}, {120, 8, 34, 1}};

// The tunnel's throat: how far back it runs from the mouth, and which way. A dark disc
// on a wall is a hole painted on it; a hole with somewhere to go is a burrow.
constexpr int kThroatLen = 54, kThroatDir = 1;

// Grit, as fixed specks rather than a roll — the same rule every scene's tables follow.
constexpr uint8_t kSpeckles[] = {7, 19, 33, 41, 58, 66, 79, 91, 103, 118,
                                 131, 144, 157, 169, 182, 195, 207, 217};

}  // namespace

void drawStrataBurrowScene(Framebuffer& fb, int beat, const SceneGround& g) {
    // The layers, stacked down from the top of the frame. Each takes its share of the
    // whole ground column, so the section holds its proportions at either floor.
    const int beds = static_cast<int>(sizeof(kBedding) / sizeof(kBedding[0]));
    int y = 0, li = 0;
    for (const Layer& l : kLayers) {
        const int h = g.floorY * l.depth / kLayerScale;
        // Each column takes the layer down to its own wandering boundary, and each
        // layer reads the table from a different place so two planes never wobble in
        // step — which is what would make the whole section look folded rather than laid.
        const Rgb565 c = sceneTone(l.tone);
        for (int x = 0; x < kActiveW; ++x) {
            const int w = kBedding[(x / 4 + li * 7) % beds];
            fb.fillRect(x, y + w, 1, h + 4, c);
        }
        if (l.grit) {
            const Rgb565 grit = sceneTone(kToneGrit + 30);
            for (uint8_t x : kSpeckles)
                fb.fillRect(x, y + h / 2 + kBedding[(x / 4 + li * 7) % beds], 1, 1, grit);
        }
        y += h;
        ++li;
    }
    // Whatever the layers did not reach, in the deepest of them — the table states a
    // section, not a total, so the last layer simply runs on to the floor.
    if (y < g.floorY) fb.fillRect(0, y, kActiveW, g.floorY - y, sceneTone(kToneStrataB));

    // The roots, working down out of the crust.
    const Rgb565 root = sceneTone(kToneRoot);
    for (const Root& r : kRoots)
        for (int k = 0; k < r.len; ++k)
            fb.fillRect(r.x + r.lean * k / 12, g.floorY * r.top / kLayerScale + k,
                        k < r.len / 2 ? 2 : 1, 1, root);

    // The cable, sagging between whatever is holding it up.
    const int cableY = sceneSkyY(g, kCableUp);
    const Rgb565 sheath = sceneTone(kToneCable);
    const Rgb565 core = sceneTone(kToneCableCore);
    for (int x = 0; x < kActiveW; ++x) {
        const int p = x % kCableSagPitch;
        const int sag = kCableSag * p * (kCableSagPitch - p) * 4 /
                        (kCableSagPitch * kCableSagPitch);
        fb.fillRect(x, cableY + sag, 1, kCableH, sheath);
        fb.fillRect(x, cableY + sag + kCableH / 2, 1, 1, core);
    }

    // The tunnel: the mouth, then a smaller disc set back inside it, so the hole has a
    // depth rather than being a dark circle painted on a wall.
    const int tunnelY = sceneSkyY(g, kTunnelUp);
    // The throat first, tapering back from the mouth — what makes the mouth a way in
    // rather than a circle somebody painted on the wall.
    for (int k = 0; k < kThroatLen; ++k) {
        // Quadratic rather than linear: a throat that narrows fast and then trails off
        // reads as receding, where a straight taper reads as an arrow.
        const int rem = kThroatLen - k;
        const int h = (kTunnelR - 4) * rem * rem / (kThroatLen * kThroatLen);
        fb.fillRect(kTunnelX + kThroatDir * (kTunnelR + k), tunnelY - h, 1, h * 2,
                    sceneTone(kToneTunnel + k / 6));
    }
    sceneDisc(fb, kTunnelX, tunnelY, kTunnelR, kToneRim);
    sceneDisc(fb, kTunnelX, tunnelY, kTunnelR - 3, kToneTunnel);
    // A lit crescent on the upper lip, which is what makes the dark disc a HOLE rather
    // than a circle painted on the wall — the light in here comes from above, so the
    // top of the rim catches it and the bottom does not.
    for (int dx = -kTunnelR + 4; dx < kTunnelR - 3; ++dx) {
        int dy = 0;
        while ((dy + 1) * (dy + 1) + dx * dx <= (kTunnelR - 1) * (kTunnelR - 1)) ++dy;
        fb.fillRect(kTunnelX + dx, tunnelY - dy, 1, 2, sceneTone(kToneRim + 44));
    }
    // ...and one loose clod falling through it, which is the whole of the motion. A
    // burrow is not a place where much happens.
    fb.fillRect(kTunnelX + 6, tunnelY - kTunnelR + 4 + (beat * 5) % (kTunnelR * 2 - 8),
                2, 2, sceneTone(kToneGrit));

    sceneFloor(fb, g, /*seamPitch=*/0, kToneBedFill, kToneBedFill, kToneBedLip);
}

}  // namespace mal
