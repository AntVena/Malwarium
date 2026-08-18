// demo_config.h — what the public web demo hides, and why.
//
// The demo is the firmware engine compiled to wasm (src/platform/web/) and handed a
// pre-raised pet, so a visitor lands on the systems rather than on a 30-minute
// incubation clock. Two kinds of slot are wrong to offer there:
//
//   * Settings that configure a DEVICE. CFG is Wi-Fi join, update install, SD mount
//     and Factory Reset — three of those have nothing to act on in a browser and the
//     fourth would wipe the demo out from under the visitor.
//   * Anything that needs the RADIO. The hacker face's SCAN/LINK/PEERS are a Wi-Fi
//     sniff and a peer link; a browser has neither, so they would open onto a screen
//     that can only ever say "nothing found".
//
// Everything else stays live on purpose: EXPL, the arcade, MAINT, MODS, ITEMS and the
// hacker face's SHOP/VAULT/CREW/PROFILE are what the demo exists to show, and the
// VAULT is where a seeded Decryptogram is cashed for a quote board.
//
// A locked slot is DIMMED AND INERT rather than removed, which is the same treatment
// an egg's care slots get (Game::slotLocked) — the shelf still says the feature
// exists, which is the honest thing to show someone deciding whether to buy the
// hardware that has it.
//
// Defined only for the wasm target (tools/build_web.sh passes -DMALWARIUM_DEMO);
// firmware and the native gates never see it.
#pragma once

#ifdef MALWARIUM_DEMO

// UI Mode still needs to be reachable with CFG shut: the icons/label/text-only cycle
// is an accessibility control, not a device setting. The web shell exposes it as a
// page button onto Game::cycleUiMode() instead of through the CFG list.

#endif  // MALWARIUM_DEMO
