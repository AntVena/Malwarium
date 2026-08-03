// dev_config.h — development-only start overrides (board-agnostic).
//
// NOT shipped behaviour: this only changes how the host preview and the device
// firmware *start*, so day-to-day dev doesn't pay the egg-hatch every launch.
// The engine and the gates are unaffected (tests pin their own StartMode).
#pragma once

// Boot already-hatched on this creature id (skips the Decryption Hatch). Set to
// the creature you want to iterate on — currently Paypup, the most fleshed-out
// pet. Change the id to start on a different one, or comment the line out to get
// the real first-boot Hatch (which installs the egg-line creature, CryptoShell).
#define DEV_START_CREATURE "paypup"

// Add an easy-access "Reset to Hatch" row to the CFG submenu that calls
// Game::resetToHatch() in one press — quick to hit while iterating. This is the
// DEV reset, distinct from the spec's hidden Factory Reset (deliberately
// hard to reach). Comment this out for a release build so the dev row does not
// ship (mirrors DEV_START_CREATURE / the A+B device chord gating).
#define DEV_CFG_RESET_ROW 1
