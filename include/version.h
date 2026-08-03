// version.h — firmware identity: what this build calls itself, and the number
// an update check compares against.
//
// One source of truth: the three MAJOR/MINOR/PATCH numbers below. Both public
// constants derive from them, so the string and the number can never disagree.
//
//   kFirmwareVersion     — the human string ("v0.1.0"), drawn on the CFG
//                          System Info FW row (drawSysInfo, cfg_screen.cpp).
//   kFirmwareVersionCode — a monotonic integer (v0.1.0 -> 1000). An update is
//                          offered only when a published build's code is
//                          strictly greater than this one, so this — not the
//                          string — is what actually gates an OTA.
//
// WHAT EACH NUMBER MEANS. MAJOR is a release. MINOR is a new feature or a small
// save bump — the thing you'd write a note about. PATCH is everything else, and
// there are a LOT of those: every publish to a test device needs its own number,
// because a device only takes an artifact whose code is strictly higher than what
// it runs. So PATCH gets three digits and MINOR two, and a feature-to-feature
// stretch has a thousand publishes of room in it.
//
// A release build overrides the numbers from the build system instead of
// editing this file, so a published binary is stamped from the tag that
// produced it. `pio run` takes no -D of its own; the env var is what appends
// to the environment's build_flags:
//
//   PLATFORMIO_BUILD_FLAGS='-DFW_VERSION_MAJOR=0 -DFW_VERSION_MINOR=2 -DFW_VERSION_PATCH=0' \
//     pio run -e waveshare_s3_154
//
// Day to day nobody edits this by hand: `tools/serve_updates.sh --bump fix`
// (or `feature` / `major`) rewrites the three numbers and publishes in one go.
//
// MINOR occupies two digits of the code and PATCH three, so MINOR stays in 0..99
// and PATCH in 0..999. Over-running either is a compile error rather than a silent
// carry into the field above it (a PATCH of 1000 would otherwise read as the next
// MINOR, and a device would decline the update it was just handed).
#pragma once

#include <cstdint>

#ifndef FW_VERSION_MAJOR
#  define FW_VERSION_MAJOR 0
#endif
#ifndef FW_VERSION_MINOR
#  define FW_VERSION_MINOR 1
#endif
#ifndef FW_VERSION_PATCH
#  define FW_VERSION_PATCH 10
#endif

// Two-step expansion: the outer macro forces its argument to expand to a
// number before the inner one stringises it.
#define MAL_VERSION_STR_(x) #x
#define MAL_VERSION_STR(x) MAL_VERSION_STR_(x)

namespace mal {

constexpr const char* kFirmwareVersion = "v" MAL_VERSION_STR(FW_VERSION_MAJOR)
                                         "." MAL_VERSION_STR(FW_VERSION_MINOR)
                                         "." MAL_VERSION_STR(FW_VERSION_PATCH);

static_assert(FW_VERSION_MINOR >= 0 && FW_VERSION_MINOR < 100,
              "FW_VERSION_MINOR must fit two digits (0..99)");
static_assert(FW_VERSION_PATCH >= 0 && FW_VERSION_PATCH < 1000,
              "FW_VERSION_PATCH must fit three digits (0..999) — bump MINOR instead");

constexpr uint32_t kFirmwareVersionCode = FW_VERSION_MAJOR * 100000u +
                                          FW_VERSION_MINOR * 1000u +
                                          FW_VERSION_PATCH;

// True when a published build advertising `code` is newer than this firmware.
// The one place the comparison lives, so the update check and any test agree.
constexpr bool firmwareUpdateAvailable(uint32_t code) {
    return code > kFirmwareVersionCode;
}

}  // namespace mal

#undef MAL_VERSION_STR
#undef MAL_VERSION_STR_
