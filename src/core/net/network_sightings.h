// network_sightings.h: the networks the passive scan has heard this boot, held two ways
// for two questions, plus the streak of EXPL Wi-Fi events that found nothing to resolve.
//
//   In range. What the radio can hear right now: a snapshot of kNetVisibleCap aged out
//     by kNetVisibleFreshMs. Feeds the CREW picker and nothing else, so no reward
//     bookkeeping can shape what the picker offers.
//   Pending. What still owes the player a reward: a FIFO of kPendingNetworkQueueCap
//     drained one entry per EXPL Wi-Fi event (Game::resolveNetworkDiscovery). Evicts
//     oldest-first when full, so an unwalked backlog goes stale rather than deaf.
//
// RAM only: the SD-backed history of what a pet has actually walked past is NetworkLedger.
// Game::registerNetwork feeds it; nothing here grants a reward.
#pragma once

#include <cstdint>
#include <cstring>

#include "tunables.h"

namespace mal {

class NetworkSightings {
public:
    struct Sighting {
        uint64_t key = 0;
        char name[33] = {0};
        uint32_t lastSeenMs = 0;   // in-range snapshot only
    };

    // Record or refresh one sighting in the in-range snapshot. Returns true when the
    // network ARRIVED (absent, or present but aged out) rather than being re-heard, so
    // a network that stays in range counts once. The label refreshes too: a hidden
    // network that later broadcasts its SSID stops reading as NET-xxxxxx.
    bool noteInRange(uint64_t key, const char* name, uint32_t nowMs) {
        for (int i = 0; i < inRangeCount_; ++i) {
            Sighting& v = inRange_[i];
            if (v.key != key) continue;
            const bool wasStale = !fresh(v, nowMs);
            setName(v, name);
            v.lastSeenMs = nowMs;
            return wasStale;
        }
        int at = inRangeCount_;
        if (at >= kNetVisibleCap) {
            // Full: evict the stalest. Anything still in range is refreshed every sweep,
            // so what goes is never something the player is standing in front of.
            at = 0;
            for (int i = 1; i < inRangeCount_; ++i)
                if (inRange_[i].lastSeenMs < inRange_[at].lastSeenMs) at = i;
        } else {
            ++inRangeCount_;
        }
        Sighting& v = inRange_[at];
        v.key = key;
        setName(v, name);
        v.lastSeenMs = nowMs;
        return true;
    }

    // The networks in range at `nowMs`, in snapshot order, up to `max`. Returns the count.
    int inRange(const Sighting** out, int max, uint32_t nowMs) const {
        int n = 0;
        for (int i = 0; i < inRangeCount_ && n < max; ++i)
            if (fresh(inRange_[i], nowMs)) out[n++] = &inRange_[i];
        return n;
    }
    int inRangeCount(uint32_t nowMs) const {
        int n = 0;
        for (int i = 0; i < inRangeCount_; ++i)
            if (fresh(inRange_[i], nowMs)) ++n;
        return n;
    }

    // Queue a sighting for a reward. False if it is already owed one. A full queue drops
    // its OLDEST entry: a player who hasn't walked in a while loses the tail of a backlog
    // they were never going to reach, and the queue never goes deaf to something new.
    bool queue(uint64_t key, const char* name) {
        for (int i = 0; i < pendingCount_; ++i)
            if (pending_[i].key == key) return false;
        if (pendingCount_ >= kPendingNetworkQueueCap) dropOldestPending();
        Sighting& slot = pending_[pendingCount_++];
        slot.key = key;
        setName(slot, name);
        return true;
    }
    // How close the queue is to kPendingNetworkQueueCap, which the Hacker face shows.
    int pendingCount() const { return pendingCount_; }

    // Take the oldest owed sighting, in scan order. A dry queue counts toward
    // emptyStreak() and returns false; a find resets the streak.
    bool popPending(Sighting* out) {
        if (pendingCount_ == 0) {
            ++emptyStreak_;
            return false;
        }
        emptyStreak_ = 0;
        *out = pending_[0];
        dropOldestPending();
        return true;
    }

    // Consecutive EXPL Wi-Fi events that found the queue dry. Game::startWifiEvent
    // summons the guardian off it. A fresh walk resets it, so a drought from an earlier
    // walk cannot summon on this walk's first miss.
    int emptyStreak() const { return emptyStreak_; }
    void resetEmptyStreak() { emptyStreak_ = 0; }

private:
    // Unsigned subtraction, so a clock wrap behaves like every other elapsed-time check.
    static bool fresh(const Sighting& v, uint32_t nowMs) {
        return nowMs - v.lastSeenMs <= kNetVisibleFreshMs;
    }
    static void setName(Sighting& s, const char* name) {
        std::strncpy(s.name, name, sizeof(s.name) - 1);
        s.name[sizeof(s.name) - 1] = '\0';
    }
    void dropOldestPending() {
        for (int i = 1; i < pendingCount_; ++i) pending_[i - 1] = pending_[i];
        --pendingCount_;
    }

    Sighting inRange_[kNetVisibleCap];
    int inRangeCount_ = 0;
    Sighting pending_[kPendingNetworkQueueCap];
    int pendingCount_ = 0;
    int emptyStreak_ = 0;
};

}  // namespace mal
