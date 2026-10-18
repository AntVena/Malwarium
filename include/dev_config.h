// dev_config.h: development-only start overrides (board-agnostic).
//
// Off unless the build defines MALWARIUM_DEV, so a release ships the real start with
// no hand edit. To turn them on for local iteration, add -DMALWARIUM_DEV to the build:
// `build_flags` in platformio.ini for the device, or
// `cmake -DCMAKE_CXX_FLAGS=-DMALWARIUM_DEV` for the host. The gates pass either way;
// test_cfg_dev_reset_row checks the CFG row against the macro in both states.
#pragma once

#ifdef MALWARIUM_DEV

// Shorten a freshly laid egg's incubation clock to this many ms on a fresh boot, so
// iteration doesn't wait out the real kBootHatchMs (~30 min). The hatch still runs its
// own line-select, minigame and Process draw, so the pet that comes out is a real one.
#define DEV_EGG_TIMER_MS (5u * 60u * 1000u)

// A "RESET TO HATCH" row in CFG that calls Game::resetToHatch() in one press. The
// shipped wipe is the hidden Factory Reset, which is deliberately hard to reach.
#define DEV_CFG_RESET_ROW 1

#endif  // MALWARIUM_DEV
