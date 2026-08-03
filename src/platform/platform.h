// platform.h — the hardware boundary. Core never touches a pin or a driver;
// each target (host SDL now, ESP32/LovyanGFX later) implements these.
#pragma once

#include <cstdint>
#include <vector>

#include "core/render/color.h"

namespace mal {

// Logical game buttons — the whole input vocabulary, three buttons and one chord:
//
//   A  NEXT     advance / cycle forward (and, in the main menu, summon it at slot 1)
//   B  ACCEPT   confirm / enter a submenu
//   C  CANCEL   back / cancel / exit one layer (in the main menu, cycle backward)
//   A+C         the meta-Exploit chord — a high-priority token that bypasses normal
//               menu state and goes straight to the active screen. On the top-level
//               carousel it flips between the Pet and Hacker faces; in combat it opens
//               the once-per-battle override picker; on an egg near hatching it cracks
//               the shell. A screen with nothing to offer ignores it.
//
// A press is held for a short window before it is delivered, so a second button landing
// a few ms later resolves as the chord rather than misfiring a plain NEXT — mechanical
// variance in two fingers is otherwise indistinguishable from intent.
enum class Button { A, B, C };

struct ButtonEvent {
    Button button;
    bool pressed;   // true on press edge, false on release edge
    bool chordAC;   // A and C held together at this edge
};

// Present an already-upscaled active-canvas buffer (kActiveW x kActiveH).
// The implementation is responsible for centring it in the panel + bezel.
struct IDisplay {
    virtual ~IDisplay() = default;
    virtual void present(const Rgb565* active, int w, int h) = 0;
};

struct IClock {
    virtual ~IClock() = default;
    virtual uint32_t millis() = 0;
};

// Persistent save store — the storage boundary, mirroring IDisplay/IClock. The
// engine serializes a versioned save blob (see core/model/save.h); each target
// persists the opaque bytes its own way: ESP32 = NVS (Preferences), host = a
// file in the cwd, tests = in-memory. The core never names a partition or a path.
struct ISaveStore {
    virtual ~ISaveStore() = default;
    // Read the persisted blob; returns an empty vector when no save exists.
    virtual std::vector<uint8_t> load() = 0;
    // Persist `data`, replacing any prior blob. Returns true on success.
    virtual bool save(const std::vector<uint8_t>& data) = 0;
    // Erase the persisted blob (dev reset / Factory Reset).
    virtual void clear() = 0;
};

} // namespace mal
