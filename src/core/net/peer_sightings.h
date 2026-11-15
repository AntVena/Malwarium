// peer_sightings.h: the peers the radio can hear right now. A snapshot of kPeerVisibleCap
// aged out by kPeerVisibleFreshMs, holding each peer's FULL last HELLO so a PEERS row shows
// what someone is raising this second.
//
// RAM only: the SD-backed roster of everyone ever met is PeerLedger, which takes only the
// ARRIVAL edge this snapshot reports, so timesMet counts meetings rather than frames.
// Game::registerPeer feeds both. NetworkSightings is the same snapshot for networks.
#pragma once

#include <cstdint>

#include "core/net/peer_link.h"
#include "tunables.h"

namespace mal {

class PeerSightings {
public:
    struct Sighting {
        uint64_t key = 0;
        PeerHello hello;
        uint32_t lastSeenMs = 0;
    };

    // Record or refresh one sighting. Returns true when the peer ARRIVED (absent, or
    // present but aged out) rather than being re-heard. A live row always shows the latest
    // HELLO. Full, it evicts the stalest entry, so a room that has churned through more
    // than kPeerVisibleCap devices still tracks whoever is present now.
    bool note(uint64_t key, const PeerHello& hello, uint32_t nowMs) {
        int stalest = -1;
        for (int i = 0; i < count_; ++i) {
            Sighting& p = seen_[i];
            if (p.key == key) {
                const bool arrived = !fresh(p, nowMs);
                p.hello = hello;
                p.lastSeenMs = nowMs;
                return arrived;
            }
            if (stalest < 0 || nowMs - p.lastSeenMs > nowMs - seen_[stalest].lastSeenMs)
                stalest = i;
        }
        const int at = count_ < kPeerVisibleCap ? count_++ : (stalest >= 0 ? stalest : 0);
        seen_[at] = {key, hello, nowMs};
        return true;
    }

    // The peers in range at `nowMs`, in snapshot order, up to `max`. Returns the count.
    int inRange(const Sighting** out, int max, uint32_t nowMs) const {
        int n = 0;
        for (int i = 0; i < count_ && n < max; ++i)
            if (fresh(seen_[i], nowMs)) out[n++] = &seen_[i];
        return n;
    }
    int inRangeCount(uint32_t nowMs) const {
        int n = 0;
        for (int i = 0; i < count_; ++i)
            if (fresh(seen_[i], nowMs)) ++n;
        return n;
    }
    // `key`'s last HELLO while it is in range, else nullptr.
    const PeerHello* find(uint64_t key, uint32_t nowMs) const {
        for (int i = 0; i < count_; ++i)
            if (seen_[i].key == key && fresh(seen_[i], nowMs)) return &seen_[i].hello;
        return nullptr;
    }

private:
    // Unsigned subtraction, so a clock wrap behaves like every other elapsed-time check.
    static bool fresh(const Sighting& p, uint32_t nowMs) {
        return nowMs - p.lastSeenMs <= kPeerVisibleFreshMs;
    }

    Sighting seen_[kPeerVisibleCap];
    int count_ = 0;
};

}  // namespace mal
