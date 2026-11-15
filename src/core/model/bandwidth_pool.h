// bandwidth_pool.h: Bandwidth, the farming resource. A fight that would take a re-farm
// penalty spends a point to shield it; the pool refills over real active time, and an
// item or a Rig Shop level tops it up. Session-level: neither a pet's nor saved, so a boot
// starts it full.
//
// The cap and the refill interval are the rig's and the pet's (Game::bandwidthMax,
// Game::bandwidthRegenMinutes), so each call that can cross the cap is handed it.
#pragma once

#include <cstdint>

namespace mal {

class BandwidthPool {
public:
    explicit BandwidthPool(int level) : level_(level) {}

    int level() const { return level_; }

    // Set the pool, clamped to [0, cap].
    void set(int n, int cap) { level_ = n < 0 ? 0 : (n > cap ? cap : n); }
    // A top-up, capped: overfilling would give shielded fights nobody paid for.
    void add(int n, int cap) { set(level_ + n, cap); }
    // Spend one point. False, and nothing spent, when the pool is dry.
    bool spendOne() {
        if (level_ <= 0) return false;
        --level_;
        return true;
    }

    // What one refill tick restores out of a `spent` hole: a flat point, or with the Link
    // Aggregator `aggregatorPct`% of the hole. The slice rounds up, so the +1 floor is
    // only reached by a hole small enough to deserve it.
    static int tickAmount(int spent, bool aggregator, int aggregatorPct) {
        if (!aggregator) return 1;
        const int slice = (spent * aggregatorPct + 99) / 100;
        return slice > 1 ? slice : 1;
    }

    // Carry `elapsedMs` of active time toward the next tick, one tick per `stepMs`. The
    // amount is worked out per tick, so the Aggregator's slice shrinks as the pool fills.
    // A full pool carries nothing, so a spend does not find a refill already banked.
    void refill(uint32_t elapsedMs, uint32_t stepMs, int cap, bool aggregator,
                int aggregatorPct) {
        if (level_ >= cap) return;
        carryMs_ += elapsedMs;
        while (carryMs_ >= stepMs && level_ < cap) {
            carryMs_ -= stepMs;
            add(tickAmount(cap - level_, aggregator, aggregatorPct), cap);
        }
    }

private:
    int level_;
    uint32_t carryMs_ = 0;   // active time toward the next tick
};

}  // namespace mal
