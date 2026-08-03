// radio_status.h — which single consumer currently owns the Wi-Fi radio.
//
// Three toggles can be switched on at once (AUDIT listens, LINK transmits, the
// 'Pedia AP hosts) and an update job can outrank all of them, but only ONE of them
// can hold the radio: platform/esp32/radio_arbiter.h resolves them against a fixed
// priority and stands the rest down. Without this reading the CFG screens would each
// report their own toggle as live while the arbiter had quietly parked it — so the
// arbiter pushes the resolved owner in (Game::setRadioOwner) and the RADIO group +
// System Info report it. Device-only in practice: a host build never mounts an
// arbiter, so it reads None ("IDLE"), the same way SD and BATT read absent there.
#pragma once

#include <cstdint>

namespace mal {

// In the arbiter's own priority order, lowest first. `Update` is the home-network
// association — the arbiter's top owner, and named for the job that raises it,
// because a running check or install is the only thing that ever does. There is no
// standing toggle behind it to name it after.
enum class RadioOwner : uint8_t { None, Scan, Capture, Link, Ap, Update };

// The player-facing name. Words, not colour — the System Info line reads the same
// in grayscale.
inline const char* radioOwnerName(RadioOwner o) {
    switch (o) {
        case RadioOwner::Scan: return "AUDIT SCAN";
        case RadioOwner::Capture: return "AUDIT CAPTURE";
        case RadioOwner::Link: return "LINK";
        case RadioOwner::Ap: return "PEDIA AP";
        case RadioOwner::Update: return "UPDATE";
        case RadioOwner::None: break;
    }
    return "IDLE";
}

} // namespace mal
