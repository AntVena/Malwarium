// peer_ledger.h — persistent roster of the operators this device has met.
//
// PURE + HOST-TESTABLE, and file-backed the same way NetworkLedger is: plain POSIX
// stdio, which is the real filesystem on host and the mounted /sdcard VFS on-device.
// A missing file is not an error — the index starts empty and every mutator still
// works, so a device with no SD card degrades to a session-only roster instead of
// failing. Fed by decoded HELLO beacons (peer_link.h) via Game::registerPeer; read
// by the Hacker-face PEERS screen (game_peers.cpp).
//
// DEVICE-level, not pet-level: a met operator outlives any one pet, like the
// HackerTag and the earned Titles, so this file is never cleared by a hatch, a
// death, or a rack swap. The key is the peer's radio MAC (packPeerKey), which is
// what makes "the same person again" recognisable across all of that.
//
// The mutable/immutable split is the interesting part, and it is the opposite of
// NetworkLedger's. A network's name is essentially fixed, so that ledger only ever
// upgrades an empty one. An operator's PET, CREW and RANK all change between
// meetings — that churn IS the content of the roster — so record() refreshes them
// to the latest broadcast every time, while the key, the first-met marker, and the
// meeting tally accumulate.
//
// Entries are UNAUTHENTICATED: everything but `key` and `timesMet` is whatever a
// device in range claimed about itself (see peer_link.h). Sanitized to printable
// ASCII at decode, but never verified.
#pragma once

#include <cstdint>
#include <cstdio>
#include <vector>

#include "core/net/peer_link.h"

namespace mal {

class PeerLedger {
public:
    struct Entry {
        uint64_t key = 0;                   // packed 48-bit radio MAC (packPeerKey)
        char tag[kPeerTagCap] = {0};        // their HackerTag
        char petName[kPeerPetCap] = {0};    // species displayName as of the last sighting
        char crewName[kPeerCrewCap] = {0};  // empty = they were not enlisted
        uint8_t stage = 0;                  // their pet's Stage, last sighting
        uint8_t rank = 0;                   // their Hacker Rank, last sighting
        bool crewRed = false;               // their crew's team, last sighting
        uint32_t timesMet = 0;              // distinct meetings (>=1)
    };

    // Read an existing roster file into the in-RAM index, remembering `path` for
    // later write-throughs. Returns false for a missing/unset file — not an error;
    // the mutators below work in-RAM regardless of whether this ever succeeds.
    bool load(const char* path);

    bool lookup(uint64_t key, Entry* out) const;

    // Record a sighting. A key that is new is appended at timesMet 1; a known key
    // has its tally bumped and its pet/crew/rank/tag overwritten from `hello` —
    // the roster always shows who someone is NOW, not who they were the first time.
    // Returns true only for a genuinely new operator, which is what the caller
    // turns into the "new peer" log line and reward.
    //
    // Callers are expected to rate-limit: this counts every call as a meeting, and
    // a beacon repeats for as long as two devices sit next to each other. Game
    // holds the live-sighting set that collapses a continuous encounter into one.
    bool record(uint64_t key, const PeerHello& hello);

    // Every known operator, in first-met order. The PEERS screen enumerates this,
    // sorting for display without disturbing the stored order.
    const std::vector<Entry>& entries() const { return entries_; }

    size_t size() const { return entries_.size(); }

private:
    void writeThrough() const;

    std::vector<Entry> entries_;
    char path_[64] = {0};
};

}  // namespace mal
