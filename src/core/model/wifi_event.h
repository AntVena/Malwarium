// wifi_event.h: one Wi-Fi network event on a walk, as a run.
//
// The event has two halves that one screen shows together: what the radio's sighting
// queue turned up (the discovery beat) and what the pet then met there (the rolled
// outcome, which B plays out). Both are this event's result and nothing else's, so they
// live here from the roll until the outcome is played out, and the next event starts
// from a fresh one.
//
// The discovery is resolved by the game, which owns the sighting queue and the ledger it
// is checked against; the event is handed to it to fill in. The outcome roll is the
// event's own rule, given the draw and whether the operator is broadcasting.
#pragma once

#include <cstdint>

#include "tunables.h"

namespace mal {

// What the pet meets on the event. Sleeping and Open both pay the loot pool, Awakened
// is a wild fight, Friendly is an ally buff for the next few battles.
enum class WifiOutcome { SleepingGuardian, AwakenedGuardian, OpenCache, FriendlyVisit };

// What the discovery beat actually was, as a value rather than only as the sentence
// that says it. The screen draws the pet taking the network in (FX_ABSORB), and how much
// of the glyph it gets through is the picture of the same reward the line states in
// words: a new network is eaten whole, one it is merely fond of is nibbled, its own home
// turf is left standing, and an empty queue has nothing to show at all.
enum class NetDiscovery : uint8_t { None, New, Fond, HomeTurf };

struct WifiEvent {
    NetDiscovery discovery = NetDiscovery::None;
    // The sentence the discovery beat wrote, naming the network; "" for an empty queue.
    // Sized so every template fits around a display name capped at 18 characters.
    char discoveryLine[40] = "";
    WifiOutcome outcome = WifiOutcome::SleepingGuardian;

    // `roll` is 0..99. A guardian only wakes for an operator who is broadcasting after
    // a real handshake capture: discovering a network never makes them hot, so an
    // operator who is not sneaks the cache instead of being attacked.
    void rollOutcome(int roll, bool broadcasting) {
        if (roll < kWifiSleepingPct)
            outcome = WifiOutcome::SleepingGuardian;
        else if (roll < kWifiSleepingPct + kWifiAwakenedPct)
            outcome = broadcasting ? WifiOutcome::AwakenedGuardian : WifiOutcome::SleepingGuardian;
        else if (roll < kWifiSleepingPct + kWifiAwakenedPct + kWifiOpenCachePct)
            outcome = WifiOutcome::OpenCache;
        else
            outcome = WifiOutcome::FriendlyVisit;
    }

    // The line under the banner that says what was met.
    const char* outcomeLine() const {
        switch (outcome) {
            case WifiOutcome::SleepingGuardian: return "GUARDIAN'S ASLEEP...";
            case WifiOutcome::AwakenedGuardian: return "GUARDIAN AWAKENS!";
            case WifiOutcome::OpenCache: return "CACHE WIDE OPEN...";
            case WifiOutcome::FriendlyVisit: return "A FRIEND STOPS BY...";
        }
        return "";
    }
};

}  // namespace mal
