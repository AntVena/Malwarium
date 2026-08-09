// dev_config.h — development-only start overrides (board-agnostic).
//
// NOT shipped behaviour: this only changes how the host preview and the device
// firmware *start*, so day-to-day dev doesn't pay the egg-hatch every launch.
// The engine and the gates are unaffected (tests pin their own StartMode).
#pragma once

// Shorten a freshly laid egg's incubation clock to this many ms on a fresh boot,
// so dev iteration doesn't wait out the real kBootHatchMs (~30 min) — the hatch
// still runs its own line-select/minigame/random-Process-draw, so the pet that
// comes out is a real, correctly-routed one rather than a fixed stand-in. Comment
// this out to get the untouched real first-boot Hatch.
#define DEV_EGG_TIMER_MS (5u * 60u * 1000u)

// Add an easy-access "Reset to Hatch" row to the CFG submenu that calls
// Game::resetToHatch() in one press — quick to hit while iterating. This is the
// DEV reset, distinct from the spec's hidden Factory Reset (deliberately
// hard to reach). Comment this out for a release build so the dev row does not
// ship (mirrors DEV_EGG_TIMER_MS / the A+B device chord gating).
#define DEV_CFG_RESET_ROW 1
