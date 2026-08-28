#include "core/render/scenes/draws.h"

#include "core/render/canvas.h"
#include "core/render/framebuffer.h"
#include "core/render/palette.h"

namespace mal {

namespace {

// THE LOCKED LOT — the Ransomware line's own place. Its creatures mostly walk, so the
// yard would have answered; what the yard cannot say is the only thing this line is
// about, which is that everything worth having is behind a shutter and somebody else
// has the key.
//
// The composition is a run of roller doors, all closed but one. The open one is the
// subject: it is the darkest hole in the picture, it is off-centre, and it is what the
// eye finds after it has read the row of identical shut ones. A line whose whole
// business is denying access gets a scene that denies it too.
constexpr uint8_t kToneSky = 12;
constexpr uint8_t kToneFacade = 40;
constexpr uint8_t kToneShutter = 58;
constexpr uint8_t kToneSlat = 34;      // the corrugation across each door
constexpr uint8_t kToneJamb = 82;      // the frame between two of them
constexpr uint8_t kToneOpen = 8;       // the one that is up: darker than the sky
constexpr uint8_t kToneChain = 116;
constexpr uint8_t kToneLock = 196;
constexpr uint8_t kToneTarmac = 62;
constexpr uint8_t kToneBay = 40;
constexpr uint8_t kToneLine = 118;

// How far up the sky the facade reaches, and how far down the doors run from it. The
// facade is a band rather than a silhouette: this place has no distance in it, the same
// way the keep has none, and for the same reason — you are standing right in front of it.
constexpr uint8_t kFacadeUp = 168;
constexpr int kLintel = 9;             // the header course above every door

// Seven doors across the width, and which one is up. Off-centre, and on the left, where
// nothing a screen draws is going to sit on top of it.
constexpr int kDoors = 7;
constexpr int kOpenDoor = 1;
constexpr int kSlatPitch = 6;
constexpr int kJambW = 3;

// The chain and padlock on one of the SHUT doors — the accent, and the whole joke. On
// the door beside the open one, so the eye reads "this one is shut, that one is not" in
// one movement rather than hunting.
constexpr int kLockDoor = 2;
constexpr int kLockDrop = 26;

// Bay markings on the tarmac: one line per door, running back to it. The floor is
// covered on the pet face and half-covered in a fight, so it carries no identity — but
// it does have to stop the doors looking like they are floating.
constexpr int kBayLineH = 12;

}  // namespace

void drawRansomLotScene(Framebuffer& fb, int beat, const SceneGround& g) {
    fb.clear(palColor(Pal::PAPER));
    sceneGlow(fb, g, /*up=*/48, kToneSky);

    const int facadeY = sceneSkyY(g, kFacadeUp);
    fb.fillRect(0, facadeY, kActiveW, g.floorY - facadeY, sceneTone(kToneFacade));

    const Rgb565 shutter = sceneTone(kToneShutter);
    const Rgb565 slat = sceneTone(kToneSlat);
    const Rgb565 jamb = sceneTone(kToneJamb);
    const Rgb565 open = sceneTone(kToneOpen);
    const int doorTop = facadeY + kLintel;
    for (int d = 0; d < kDoors; ++d) {
        const int x0 = d * kActiveW / kDoors + kJambW;
        const int x1 = (d + 1) * kActiveW / kDoors - kJambW;
        fb.fillRect(x0 - kJambW, facadeY, kJambW, g.floorY - facadeY, jamb);
        if (d == kOpenDoor) {
            // The one that is up. Its shutter is rolled into the head of the opening,
            // which is the four rows of slats left across the top — a door standing
            // open with nothing above it would read as a doorway somebody built.
            fb.fillRect(x0, doorTop, x1 - x0, g.floorY - doorTop, open);
            fb.fillRect(x0, doorTop, x1 - x0, 4, shutter);
            continue;
        }
        fb.fillRect(x0, doorTop, x1 - x0, g.floorY - doorTop, shutter);
        for (int y = doorTop + kSlatPitch; y < g.floorY; y += kSlatPitch)
            fb.fillRect(x0, y, x1 - x0, 1, slat);
    }

    // The chain across one shut door, and the lock hanging in it. The lock swings a
    // column either way on the beat, which is the only thing moving in the whole scene —
    // and a padlock is exactly the right amount of motion for a place like this.
    const int lockX = (kLockDoor * kActiveW / kDoors + (kLockDoor + 1) * kActiveW / kDoors) / 2;
    const int lockY = doorTop + kLockDrop;
    fb.fillRect(lockX - 9, lockY, 18, 1, sceneTone(kToneChain));
    const int swing = (beat % 3) - 1;
    fb.fillRect(lockX + swing - 1, lockY + 1, 3, 2, sceneTone(kToneChain));
    fb.fillRect(lockX + swing - 2, lockY + 3, 5, 4, sceneTone(kToneLock));

    sceneFloor(fb, g, /*seamPitch=*/0, kToneTarmac, kToneBay, kToneLine);
    // One bay line running back to each door, which ties the tarmac to the facade.
    const Rgb565 bay = sceneTone(kToneBay);
    for (int d = 0; d <= kDoors; ++d)
        fb.fillRect(d * kActiveW / kDoors, g.floorY + 1, 1, kBayLineH, bay);
}

}  // namespace mal
