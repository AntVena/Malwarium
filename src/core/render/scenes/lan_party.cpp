#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE LAN PARTY. The prize for meeting other operators over the LINK, and the room
// where meeting other operators used to happen: a hired hall at night, one long row of
// trestle tables down the back wall, every seat with its own screen, and bunting
// strung overhead. The motion is traffic: an activity light hopping screen to screen
// along the row, which is what a room full of peers talking to each other looks like
// from the door.
//
// The bunting is in the sky band, where no sprite stands, and it is the picture's
// landmark; the screens run the whole row, so a sprite in front of some of them still
// leaves a party going on either side.
constexpr Pal kWarm = Pal::FRAG_HI;    // half the pennants, and the activity light
constexpr Pal kCool = Pal::FRAG_LO;    // the other half, and the screens' glow
constexpr uint8_t kToneWall = 16;
constexpr uint8_t kToneTruss = 30;
constexpr uint8_t kToneString = 50;
constexpr uint8_t kTonePennant = 86;
constexpr uint8_t kToneTable = 44;
constexpr uint8_t kToneTableEdge = 80;
constexpr uint8_t kToneMonitor = 30;
constexpr uint8_t kToneScreen = 70;
constexpr uint8_t kToneScreenLine = 96;
constexpr uint8_t kToneActivity = 200;
constexpr uint8_t kToneCarpet = 26;
constexpr uint8_t kToneCable = 14;
constexpr uint8_t kToneCarpetEdge = 60;

// The roof's trusses: a zig-zag lattice across the top of the hall.
constexpr int kTrussH = 10, kTrussPitch = 16;

// The bunting: two strands, each sagging from wall to wall at its own height up the
// sky, with a pennant every `kPennantPitch` columns. Pennants alternate warm and cool,
// and the whole strand stirs by a row on alternate beats.
struct Strand { uint8_t up; int sag; int phase; };
constexpr Strand kStrands[] = {{196, 14, 0}, {150, 10, 7}};
constexpr int kPennantPitch = 14, kPennantW = 7, kPennantH = 7;

// The table along the back wall, `kTableUp` rows above the floor, and the screens on
// it — one every `kSeatPitch` columns, each a monitor with a lit face.
constexpr int kTableUp = 16, kTableH = 3;
constexpr int kSeatPitch = 28;
constexpr int kMonW = 18, kMonH = 13, kStandH = 3;

// The cables under the table, running along the carpet to the floor's edge: one per
// seat, each kinking down at its own row.
constexpr int kCableDrop[] = {5, 9, 3, 12, 7, 4, 10, 6};

// A strand's row at column x.
int strandY(const SceneGround& g, const Strand& s, int x) {
    const int t = x * 2 - kActiveW;
    return sceneSkyY(g, s.up) + s.sag - s.sag * t * t / (kActiveW * kActiveW);
}

}  // namespace

void drawLanPartyScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));

    // The hall, and its roof trusses.
    fb.fillRect(0, 0, kActiveW, g.floorY, sceneTone(kToneWall));
    const Rgb565 truss = sceneTone(kToneTruss);
    fb.fillRect(0, kTrussH, kActiveW, 1, truss);
    for (int x = 0; x < kActiveW; ++x) {
        const int k = x % kTrussPitch;
        const int y = k < kTrussPitch / 2 ? k * 2 * kTrussH / kTrussPitch
                                          : (kTrussPitch - k) * 2 * kTrussH / kTrussPitch;
        fb.fillRect(x, y, 1, 1, truss);
    }

    // The bunting: the string, then a pennant hanging off it every few columns.
    const Rgb565 string = sceneTone(kToneString);
    for (const Strand& s : kStrands) {
        for (int x = 0; x < kActiveW; ++x) fb.fillRect(x, strandY(g, s, x), 1, 1, string);
        const int stir = (beat + s.phase) & 1;
        for (int i = 0, x = s.phase; x < kActiveW; ++i, x += kPennantPitch) {
            const Rgb565 c = sceneTint(kTonePennant, (i & 1) ? kWarm : kCool);
            const int y = strandY(g, s, x + kPennantW / 2) + 1;
            for (int r = 0; r < kPennantH; ++r) {   // a triangle, point down
                const int w = kPennantW - r * kPennantW / kPennantH;
                fb.fillRect(x + (kPennantW - w) / 2 + (r > 3 ? stir : 0), y + r, w, 1, c);
            }
        }
    }

    // The table and its screens, with the activity light hopping seat to seat.
    const int tableY = g.floorY - kTableUp;
    fb.fillRect(0, tableY, kActiveW, kTableH, sceneTone(kToneTable));
    fb.fillRect(0, tableY, kActiveW, 1, sceneTone(kToneTableEdge));
    fb.fillRect(0, tableY + kTableH, kActiveW, g.floorY - tableY - kTableH,
                sceneTone(kToneMonitor));   // the shadow under the table
    const int seats = (kActiveW + kSeatPitch - 1) / kSeatPitch;
    const int active = beat % seats;
    for (int i = 0; i < seats; ++i) {
        const int x = i * kSeatPitch + (kSeatPitch - kMonW) / 2;
        const int y = tableY - kStandH - kMonH;
        fb.fillRect(x, y, kMonW, kMonH, sceneTone(kToneMonitor));
        fb.fillRect(x + 2, y + 2, kMonW - 4, kMonH - 4, sceneTint(kToneScreen, kCool));
        for (int l = 0; l < 3; ++l)   // a few lines of whatever is on the screen
            fb.fillRect(x + 4, y + 4 + l * 2, 4 + (i * 5 + l * 3) % 7, 1,
                        sceneTint(kToneScreenLine, kCool));
        fb.fillRect(x + kMonW / 2 - 1, tableY - kStandH, 3, kStandH, sceneTone(kToneMonitor));
        if (i == active)
            fb.fillRect(x + kMonW - 4, y + kMonH - 3, 2, 1, sceneTint(kToneActivity, kWarm));
    }

    // The carpet, and the cables running along it from under the table.
    sceneFloor(fb, g, /*seamPitch=*/0, kToneCarpet, kToneCarpet, kToneCarpetEdge);
    const Rgb565 cable = sceneTone(kToneCable);
    for (int i = 0; i < seats; ++i) {
        const int x = i * kSeatPitch + kSeatPitch / 2;
        const int drop = kCableDrop[i % static_cast<int>(sizeof(kCableDrop) / sizeof(int))];
        fb.fillRect(x, g.floorY + 1, 1, drop, cable);
        fb.fillRect(x, g.floorY + drop, kSeatPitch / 2, 1, cable);
    }
}

}  // namespace mal
