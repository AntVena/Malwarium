// sprite_anim.h — named animation clips over a multi-row sprite sheet.
//
// A SpriteData sheet can stack more than one row (see sprite.h); this table
// says which row plays which named loop and how many of that row's columns
// it uses. A sprite with no entry here has no authored clip — callers fall
// back to idleFrame()'s single-row breathe/blink heuristic on row 0.
#pragma once

#include <cstring>

namespace mal {

struct AnimClip {
    const char* sprite;   // asset name, e.g. "SPR_PET_KEYLOGGERHEAD"
    const char* name;     // "idle", "attack", ...
    int row;              // sheet row this clip plays from
    int frames;           // frame count used in that row (columns 0..frames-1)
    int holdBeats = 1;    // heartbeats (kHeartbeatMs each) each frame holds before advancing
};

// Add a row here to give a sprite sheet a named, full-length animation loop
// instead of the default breathe/blink heuristic. Raise holdBeats to slow a
// clip's playback (e.g. holdBeats=2 halves its speed) without touching frames.
inline const AnimClip kAnimClips[] = {
    {"SPR_PET_KEYLOGGERHEAD", "idle", 0, 8, /*holdBeats=*/4},
    // Row 1 is a duplicate of row 0 in the source PNG, kept only to prove the
    // multi-row grid reads correctly end to end; nothing draws "attack" yet.
    {"SPR_PET_KEYLOGGERHEAD", "attack", 1, 4},
};
inline constexpr int kAnimClipCount =
    static_cast<int>(sizeof(kAnimClips) / sizeof(kAnimClips[0]));

// Look up a named clip for a sprite; nullptr if none registered.
inline const AnimClip* findAnimClip(const char* spriteName, const char* clipName) {
    if (!spriteName || !clipName) return nullptr;
    for (const auto& c : kAnimClips) {
        if (std::strcmp(c.sprite, spriteName) == 0 &&
            std::strcmp(c.name, clipName) == 0) {
            return &c;
        }
    }
    return nullptr;
}

// Cycles forward through a clip's frames, holding each for `holdBeats`
// heartbeats before advancing, wrapping every frames*holdBeats beats.
inline int clipFrame(const AnimClip& clip, int beat) {
    if (clip.frames <= 0) return 0;
    const int hold = clip.holdBeats > 0 ? clip.holdBeats : 1;
    return (beat / hold) % clip.frames;
}

} // namespace mal
